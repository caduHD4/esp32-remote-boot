using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace RemoteBoot {
public sealed class DiscoveryReply {
    public string DeviceId="",PublicKey="",Fingerprint="",Hostname="";public IPAddress Address;public int HttpPort,WsPort;
}
public sealed class DiscoveryProfile {
    public int ProfileVersion,Protocol,DiscoveryVersion,ModeRevision,HttpPort=80,WsPort=81;
    public string PcId="",DeviceId="",PublicKey="",Fingerprint="",LastIpv4="",Hostname="";
    public bool BlindMode;
    public static DiscoveryProfile Read(string path) {
        if(new FileInfo(path).Length>16384)throw new Exception("Discovery profile too large");
        using var doc=JsonDocument.Parse(File.ReadAllText(path));return FromJson(doc.RootElement);
    }
    public static DiscoveryProfile FromJson(JsonElement d) {
        if(d.ValueKind!=JsonValueKind.Object)throw new Exception("Invalid discovery profile");
        var p=new DiscoveryProfile{ProfileVersion=Int(d,"profile_version"),Protocol=Int(d,"protocol"),DiscoveryVersion=Int(d,"discovery_version"),
            ModeRevision=Int(d,"mode_revision"),BlindMode=Bool(d,"blind_mode"),PcId=Text(d,"pc_id"),DeviceId=Text(d,"device_id"),
            PublicKey=Text(d,"public_key"),Fingerprint=Text(d,"fingerprint"),LastIpv4=Text(d,"last_ipv4"),Hostname=Text(d,"hostname"),
            HttpPort=Int(d,"http_port"),WsPort=Int(d,"ws_port")};
        foreach(var property in d.EnumerateObject())if(!new[]{"profile_version","protocol","discovery_version","pc_id","blind_mode","mode_revision","device_id","public_key","fingerprint","last_ipv4","http_port","ws_port","hostname"}.Contains(property.Name))
            throw new Exception("Unknown discovery profile field");
        p.Validate();return p;
    }
    public static DiscoveryProfile FromConfig(Config c) {
        if(c==null||!c.BlindMode)throw new Exception("Blind mode is not configured");
        var p=new DiscoveryProfile{ProfileVersion=1,Protocol=2,DiscoveryVersion=c.DiscoveryVersion,ModeRevision=(int)c.ModeRevision,BlindMode=true,
            PcId=c.PcId,DeviceId=c.DeviceId,PublicKey=c.PublicKey,Fingerprint=c.Fingerprint,HttpPort=c.Url?.IsDefaultPort==false?c.Url.Port:80,
            WsPort=c.WsPort,LastIpv4=c.Url?.Host??"",Hostname="remote-boot-"+c.DeviceId.Substring(Math.Max(0,c.DeviceId.Length-12))+".local"};
        p.Validate();return p;
    }
    public void Validate() {
        if(ProfileVersion!=1||Protocol!=2||DiscoveryVersion!=1||ModeRevision<=0||HttpPort<1||HttpPort>65535||WsPort<1||WsPort>65535)
            throw new Exception("Invalid discovery profile version or ports");
        if(!AgentIdentity.ValidHex(PcId,32)||!AgentIdentity.ValidHex(DeviceId,32))throw new Exception("Invalid discovery profile IDs");
        if(PublicKey.Length!=130||!PublicKey.StartsWith("04",StringComparison.Ordinal)||!AgentIdentity.ValidHex(PublicKey,130))
            throw new Exception("Invalid discovery public key");
        if(!AgentIdentity.ValidHex(Fingerprint,64)||Fingerprint!=DiscoveryProtocol.Fingerprint(PublicKey))
            throw new Exception("Invalid discovery fingerprint");
        if(!IPAddress.TryParse(LastIpv4,out var ip)||ip.AddressFamily!=AddressFamily.InterNetwork||!DiscoveryProtocol.ValidIpv4(ip.GetAddressBytes(),false))
            throw new Exception("Invalid discovery IPv4");
        if(Hostname.Length<14||Hostname.Length>63||!Hostname.StartsWith("remote-boot-",StringComparison.Ordinal)||!Hostname.EndsWith(".local",StringComparison.Ordinal))
            throw new Exception("Invalid discovery hostname");
    }
    static int Int(JsonElement d,string key){if(!d.TryGetProperty(key,out var v)||v.ValueKind!=JsonValueKind.Number||!v.TryGetInt32(out int n))throw new Exception("Invalid discovery profile field");return n;}
    static bool Bool(JsonElement d,string key){if(!d.TryGetProperty(key,out var v)||(v.ValueKind!=JsonValueKind.True&&v.ValueKind!=JsonValueKind.False))throw new Exception("Invalid discovery profile field");return v.GetBoolean();}
    static string Text(JsonElement d,string key){if(!d.TryGetProperty(key,out var v)||v.ValueKind!=JsonValueKind.String||string.IsNullOrEmpty(v.GetString()))throw new Exception("Invalid discovery profile field");return v.GetString();}
}
public static class DiscoveryProtocol {
    public const int Port=49192;
    public static byte[] Query(byte[] nonce,byte[] deviceId) {
        if(nonce==null||nonce.Length!=16||deviceId==null||deviceId.Length!=16)throw new ArgumentException("Invalid discovery query");
        var q=new byte[40];Encoding.ASCII.GetBytes("RBD1").CopyTo(q,0);q[4]=1;q[5]=0;q[6]=0;q[7]=40;nonce.CopyTo(q,8);deviceId.CopyTo(q,24);return q;
    }
    public static byte[] HexBytes(string hex) { if(hex==null||hex.Length%2!=0)throw new ArgumentException("Invalid hex");var b=new byte[hex.Length/2];for(int i=0;i<b.Length;i++)b[i]=Convert.ToByte(hex.Substring(i*2,2),16);return b; }
    public static string Hex(byte[] data) { var chars=new char[data.Length*2];const string digits="0123456789abcdef";for(int i=0;i<data.Length;i++){chars[i*2]=digits[data[i]>>4];chars[i*2+1]=digits[data[i]&15];}return new string(chars); }
    public static string Fingerprint(string publicKeyHex)=>Hex(SHA256.HashData(HexBytes(publicKeyHex)));
    public static bool ValidIpv4(byte[] b,bool allowLoopback=false) {
        if(b==null||b.Length!=4||b[0]==0||b[0]>=224)return false;
        if(!allowLoopback&&b[0]==127)return false;
        return true;
    }
    public static bool TryParse(byte[] packet,IPEndPoint source,byte[] nonce,DiscoveryProfile expected,bool allowLoopback,out DiscoveryReply reply) {
        reply=null;
        if(packet==null||packet.Length!=177||source==null||nonce==null||nonce.Length!=16)return false;
        if(packet[0]!=(byte)'R'||packet[1]!=(byte)'B'||packet[2]!=(byte)'D'||packet[3]!=(byte)'1'||packet[4]!=2||packet[5]!=0||packet[6]!=0||packet[7]!=177)return false;
        if(!packet.AsSpan(8,16).SequenceEqual(nonce))return false;
        var deviceId=packet.AsSpan(24,16);if(deviceId.IndexOfAnyExcept((byte)0)<0)return false;
        var ip=packet.AsSpan(40,4).ToArray();if(!ValidIpv4(ip,allowLoopback))return false;
        if(!source.Address.GetAddressBytes().SequenceEqual(ip)||source.Port!=Port)return false;
        int http=(packet[44]<<8)|packet[45],ws=(packet[46]<<8)|packet[47];if(http<1||ws<1)return false;
        if(packet[48]!=4)return false;
        var key=packet.AsSpan(48,65).ToArray();
        var fingerprint=Fingerprint(Hex(key));
        if(expected!=null) {
            if(Hex(deviceId.ToArray())!=expected.DeviceId||Hex(key)!=expected.PublicKey||fingerprint!=expected.Fingerprint||http!=expected.HttpPort||ws!=expected.WsPort)return false;
        }
        if(!VerifySignature(packet,key))return false;
        reply=new DiscoveryReply{DeviceId=Hex(deviceId.ToArray()),PublicKey=Hex(key),Fingerprint=fingerprint,
            Address=new IPAddress(ip),HttpPort=http,WsPort=ws,Hostname="remote-boot-"+Hex(deviceId.ToArray()).Substring(20)+".local"};
        return true;
    }
    static bool VerifySignature(byte[] packet,byte[] key) {
        try {
            var parameters=new ECParameters{Curve=ECCurve.NamedCurves.nistP256,Q=new ECPoint{X=key.AsSpan(1,32).ToArray(),Y=key.AsSpan(33,32).ToArray()}};
            using var ecdsa=ECDsa.Create(parameters);
            return ecdsa.VerifyData(packet.AsSpan(0,113),packet.AsSpan(113,64),HashAlgorithmName.SHA256,DSASignatureFormat.IeeeP1363FixedFieldConcatenation);
        } catch(CryptographicException){return false;}
    }
}
public sealed class DiscoveryClient {
    readonly bool allowLoopback;readonly IPEndPoint[] testDestinations;
    readonly Func<string,CancellationToken,Task<IPAddress[]>> resolver;
    public DiscoveryClient(bool allowLoopbackForTests=false,IPEndPoint[] destinations=null,Func<string,CancellationToken,Task<IPAddress[]>> dnsResolver=null){allowLoopback=allowLoopbackForTests;testDestinations=destinations;resolver=dnsResolver??((host,token)=>Dns.GetHostAddressesAsync(host,token));}
    public List<DiscoveryReply> Discover(DiscoveryProfile expected,int timeoutMs,CancellationToken token) {
        if(timeoutMs<100||timeoutMs>60000)throw new ArgumentOutOfRangeException(nameof(timeoutMs));
        var nonce=RandomNumberGenerator.GetBytes(16);var id=expected==null?new byte[16]:DiscoveryProtocol.HexBytes(expected.DeviceId);
        var query=DiscoveryProtocol.Query(nonce,id);var timer=Stopwatch.StartNew();
        using var dnsCancellation=CancellationTokenSource.CreateLinkedTokenSource(token);
        dnsCancellation.CancelAfter(timeoutMs);
        Task<IPAddress[]> dns=null;
        if(expected!=null&&expected.Hostname.Length>0) {
            try{dns=resolver(expected.Hostname,dnsCancellation.Token);}catch(SocketException){}catch(ArgumentException){}
        }
        var sockets=CreateSockets();
        try {
            void SendAll(){foreach(var (socket,broadcast) in sockets)foreach(var destination in Destinations(broadcast,expected,dns)){try{socket.SendTo(query,destination);}catch(SocketException){}}}
            var results=new List<DiscoveryReply>();var seen=new HashSet<string>(StringComparer.Ordinal);
            long idleUntil=long.MaxValue,nextSend=0;var buffer=new byte[512];
            while(timer.ElapsedMilliseconds<timeoutMs) {
                token.ThrowIfCancellationRequested();
                var now=timer.ElapsedMilliseconds;if(results.Count>0&&now>=idleUntil)break;
                if(results.Count==0&&now>=nextSend){SendAll();nextSend=now+1000;}
                int remain=(int)Math.Max(1,timeoutMs-timer.ElapsedMilliseconds);
                int untilSend=(int)Math.Max(1,nextSend-timer.ElapsedMilliseconds);
                var readable=new List<Socket>(sockets.Count);foreach(var (socket,_) in sockets)readable.Add(socket);
                int wait=Math.Min(remain,Math.Min(results.Count>0?500:remain,untilSend));
                try{Socket.Select(readable,null,null,wait*1000);}catch(SocketException){break;}
                foreach(var socket in readable) {
                    EndPoint remote=new IPEndPoint(IPAddress.Any,0);int length;
                    try{length=socket.ReceiveFrom(buffer,ref remote);}catch(SocketException){continue;}
                    if(remote is not IPEndPoint source)continue;
                    if(!DiscoveryProtocol.TryParse(buffer.AsSpan(0,length).ToArray(),source,nonce,expected,allowLoopback,out var reply)||!seen.Add(reply.DeviceId))continue;
                    token.ThrowIfCancellationRequested();if(timer.ElapsedMilliseconds>=timeoutMs)break;
                    results.Add(reply);if(expected!=null)return results;idleUntil=timer.ElapsedMilliseconds+500;
                }
            }
            return results;
        } finally {dnsCancellation.Cancel();if(dns!=null)_=dns.ContinueWith(task=>{_ = task.Exception;},TaskContinuationOptions.OnlyOnFaulted);foreach(var (socket,_) in sockets)socket.Dispose();}
    }
    List<(Socket socket,IPAddress broadcast)> CreateSockets() {
        var sockets=new List<(Socket,IPAddress)>();
        if(testDestinations!=null) {
            var socket=NewSocket();socket.Bind(new IPEndPoint(IPAddress.Any,0));sockets.Add((socket,null));return sockets;
        }
        foreach(var (address,broadcast) in LocalInterfaces()) {
            var socket=NewSocket();
            try{socket.Bind(new IPEndPoint(address,0));sockets.Add((socket,broadcast));}catch(SocketException){socket.Dispose();}
        }
        if(sockets.Count==0){var socket=NewSocket();socket.Bind(new IPEndPoint(IPAddress.Any,0));sockets.Add((socket,null));}
        return sockets;
    }
    static Socket NewSocket()=>new Socket(AddressFamily.InterNetwork,SocketType.Dgram,ProtocolType.Udp){EnableBroadcast=true};
    static List<(IPAddress address,IPAddress broadcast)> LocalInterfaces() {
        var result=new List<(IPAddress,IPAddress)>();
        foreach(var network in NetworkInterface.GetAllNetworkInterfaces()) {
            if(network.OperationalStatus!=OperationalStatus.Up||network.NetworkInterfaceType==NetworkInterfaceType.Loopback||network.NetworkInterfaceType==NetworkInterfaceType.Tunnel)continue;
            string name=network.Name.ToLowerInvariant();if(name.Contains("tailscale")||name.StartsWith("wg")||name.Contains("vpn"))continue;
            foreach(var unicast in network.GetIPProperties().UnicastAddresses) {
                if(unicast.Address.AddressFamily!=AddressFamily.InterNetwork||unicast.IPv4Mask==null)continue;
                byte[] ip=unicast.Address.GetAddressBytes(),mask=unicast.IPv4Mask.GetAddressBytes(),broadcast=new byte[4];
                for(int i=0;i<4;i++)broadcast[i]=(byte)(ip[i]|~mask[i]);
                if(DiscoveryProtocol.ValidIpv4(broadcast))result.Add((unicast.Address,new IPAddress(broadcast)));
            }
        }
        return result;
    }
    List<IPEndPoint> Destinations(IPAddress broadcast,DiscoveryProfile expected,Task<IPAddress[]> dns) {
        if(testDestinations!=null)return testDestinations.ToList();
        var list=new List<IPEndPoint>();var seen=new HashSet<string>(StringComparer.Ordinal);
        void Add(IPAddress address){if(address==null)return;var key=address.ToString();if(seen.Add(key))list.Add(new IPEndPoint(address,DiscoveryProtocol.Port));}
        if(expected!=null&&IPAddress.TryParse(expected.LastIpv4,out var cached))Add(cached);
        if(dns!=null&&dns.IsCompletedSuccessfully)foreach(var address in dns.Result)if(address.AddressFamily==AddressFamily.InterNetwork)Add(address);
        Add(IPAddress.Broadcast);Add(broadcast);return list;
    }
}
public static class DiscoveryRuntime {
    public static void EnsureEndpoint(Config config,string configPath,CancellationToken token,DiscoveryClient client=null) {
        if(config==null||!config.BlindMode)return;
        if(string.IsNullOrEmpty(configPath))throw new Exception("Blind configuration path is required");
        var profile=DiscoveryProfile.FromConfig(config);var replies=(client??new DiscoveryClient()).Discover(profile,10000,token);
        if(replies.Count==0)throw new Exception("ESP discovery failed");
        var reply=replies[0];config.Url=new UriBuilder("http",reply.Address.ToString(),reply.HttpPort).Uri;config.WsPort=reply.WsPort;config.Save(configPath);
    }
    public static void ApplyProfile(string profilePath,string configPath,string confirmFingerprint) {
        var profile=DiscoveryProfile.Read(profilePath);var config=Config.Read(configPath);
        if(!string.Equals(confirmFingerprint,profile.Fingerprint,StringComparison.Ordinal))throw new Exception("Fingerprint confirmation mismatch");
        if(profile.PcId!=config.PcId)throw new Exception("Discovery profile belongs to another PC");
        if(config.DeviceId.Length>0&&config.DeviceId!=profile.DeviceId)throw new Exception("Discovery profile changes the pinned device");
        if(config.PublicKey.Length>0&&config.PublicKey!=profile.PublicKey)throw new Exception("Discovery profile changes the pinned key");
        if(config.Fingerprint.Length>0&&config.Fingerprint!=profile.Fingerprint)throw new Exception("Discovery profile changes the pinned fingerprint");
        config.DeviceId=profile.DeviceId;config.PublicKey=profile.PublicKey;config.Fingerprint=profile.Fingerprint;
        config.DiscoveryVersion=profile.DiscoveryVersion;config.ModeRevision=(uint)profile.ModeRevision;config.BlindMode=profile.BlindMode;config.WsPort=profile.WsPort;
        config.Url=new UriBuilder("http",profile.LastIpv4,profile.HttpPort).Uri;
        if(profile.BlindMode) {
            var verified=DiscoveryProfile.FromConfig(config);var replies=new DiscoveryClient().Discover(verified,10000,CancellationToken.None);
            if(replies.Count==0)throw new Exception("ESP discovery failed; profile not applied");
            config.Url=new UriBuilder("http",replies[0].Address.ToString(),replies[0].HttpPort).Uri;config.WsPort=replies[0].WsPort;
        }
        config.Save(configPath);
    }
    public static async Task ReportPreparation(string manifestPath,string configPath,CancellationToken token) {
        var config=Config.Read(configPath);var manifest=JsonDocument.Parse(File.ReadAllText(manifestPath)).RootElement;
        string pc=Json.Text(manifest,"pc_id"),device=Json.Text(manifest,"device_id"),key=Json.Text(manifest,"public_key"),fingerprint=Json.Text(manifest,"fingerprint"),ipxe=Json.Text(manifest,"ipxe_sha256"),loader=Json.Text(manifest,"loader_sha256");
        int revision=manifest.TryGetProperty("mode_revision",out var r)&&r.TryGetInt32(out int rv)?rv:0;
        bool blind=manifest.TryGetProperty("blind_mode",out var b)&&b.ValueKind==JsonValueKind.True;
        int discoveryVersion=manifest.TryGetProperty("discovery_version",out var d)&&d.TryGetInt32(out int dv)?dv:0;
        if(pc!=config.PcId||device!=config.DeviceId||key!=config.PublicKey||fingerprint!=config.Fingerprint||revision!=(int)config.ModeRevision||blind!=config.BlindMode||discoveryVersion!=1||
            !AgentIdentity.ValidHex(ipxe,64)||!AgentIdentity.ValidHex(loader,64))throw new Exception("Manifest does not match the prepared configuration");
        if(config.BlindMode)EnsureEndpoint(config,configPath,token);
        var body=Json.Build(w=>{w.WriteString("pc_id",config.PcId);w.WriteString("agent_id",config.AgentId);w.WriteNumber("mode_revision",config.ModeRevision);w.WriteBoolean("blind_mode",config.BlindMode);
            w.WriteNumber("discovery_version",1);w.WriteBoolean("boot_prepared",true);w.WriteString("ipxe_sha256",ipxe);w.WriteString("loader_sha256",loader);});
        using var handler=new HttpClientHandler{UseProxy=false,AllowAutoRedirect=false};
        using var http=new HttpClient(handler){Timeout=TimeSpan.FromSeconds(8)};
        using var request=new HttpRequestMessage(HttpMethod.Post,new Uri(config.Url,"/api/v2/agent/preparation"));
        request.Headers.Authorization=new AuthenticationHeaderValue("Bearer",config.Token);request.Content=new ByteArrayContent(body);request.Content.Headers.ContentType=new MediaTypeHeaderValue("application/json");
        using var response=await http.SendAsync(request,token);
        if((int)response.StatusCode>=300&&(int)response.StatusCode<400)throw new InvalidOperationException("Preparation redirect refused");
        response.EnsureSuccessStatusCode();
    }
    public static string JsonCandidates(IEnumerable<DiscoveryReply> replies,bool pinned) {
        using var stream=new MemoryStream();using(var writer=new Utf8JsonWriter(stream)) {
            writer.WriteStartArray();foreach(var reply in replies){writer.WriteStartObject();writer.WriteString("device_id",reply.DeviceId);writer.WriteString("public_key",reply.PublicKey);
                writer.WriteString("fingerprint",reply.Fingerprint);writer.WriteString("ipv4",reply.Address.ToString());writer.WriteNumber("http_port",reply.HttpPort);
                writer.WriteNumber("ws_port",reply.WsPort);writer.WriteString("hostname",reply.Hostname);writer.WriteBoolean("signature_valid",true);writer.WriteBoolean("pinned",pinned);writer.WriteEndObject();}
            writer.WriteEndArray();
        }
        return Encoding.UTF8.GetString(stream.ToArray());
    }
}
}
