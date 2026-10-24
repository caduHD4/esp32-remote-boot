using System;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Threading;

namespace RemoteBoot {
public static class DiscoverySelfTest {
    static void Assert(bool condition,string message="discovery self-test assertion failed"){if(!condition)throw new Exception(message);}
    static string Temp(){string path=Path.Combine(Path.GetTempPath(),"remote-boot-discovery-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(path);return path;}
    static (ECDsa,byte[]) Key() {
        var key=ECDsa.Create(ECCurve.NamedCurves.nistP256);var parameters=key.ExportParameters(false);var pub=new byte[65];pub[0]=4;
        parameters.Q.X.CopyTo(pub,1);parameters.Q.Y.CopyTo(pub,33);return (key,pub);
    }
    static DiscoveryProfile ProfileFromFixture(JsonElement fixture) {
        var profile=DiscoveryProfile.FromJson(fixture.GetProperty("profile"));
        return profile;
    }
    static void Fixture() {
        using var fixtureStream=typeof(DiscoverySelfTest).Assembly.GetManifestResourceStream("RemoteBoot.DiscoveryFixture") ?? throw new Exception("Embedded discovery fixture missing");
        using var document=JsonDocument.Parse(fixtureStream);var root=document.RootElement;var profile=ProfileFromFixture(root);
        var nonce=DiscoveryProtocol.HexBytes(root.GetProperty("nonce_hex").GetString());
        var reply=DiscoveryProtocol.HexBytes(root.GetProperty("reply_hex").GetString());
        var source=new IPEndPoint(IPAddress.Parse(root.GetProperty("source_ipv4").GetString()),DiscoveryProtocol.Port);
        Assert(DiscoveryProtocol.TryParse(reply,source,nonce,profile,false,out var parsed),"fixture reply rejected");
        Assert(parsed.DeviceId==profile.DeviceId&&parsed.Fingerprint==profile.Fingerprint&&parsed.HttpPort==80&&parsed.WsPort==81);
        Assert(DiscoveryProtocol.TryParse(reply,source,nonce,null,false,out _),"unbound fixture rejected");
        Assert(!DiscoveryProtocol.TryParse(reply,source,new byte[16],profile,false,out _),"wrong nonce accepted");
        Assert(!DiscoveryProtocol.TryParse(reply,new IPEndPoint(IPAddress.Parse("192.168.50.81"),DiscoveryProtocol.Port),nonce,profile,false,out _),"wrong source accepted");
        var mutated=(byte[])reply.Clone();mutated[176]^=1;Assert(!DiscoveryProtocol.TryParse(mutated,source,nonce,profile,false,out _),"bad signature accepted");
        var wrong=(byte[])reply.Clone();wrong[48]^=1;Assert(!DiscoveryProtocol.TryParse(wrong,source,nonce,profile,false,out _),"wrong key accepted");
        foreach(var invalid in root.GetProperty("invalid").EnumerateArray()) {
            var bytes=DiscoveryProtocol.HexBytes(invalid.GetProperty("reply_hex").GetString());
            Assert(!DiscoveryProtocol.TryParse(bytes,source,nonce,profile,false,out _),"invalid fixture accepted: "+invalid.GetProperty("name").GetString());
        }
        var other=ProfileFromFixture(root);other.PublicKey="04"+new string('0',128);other.Fingerprint=DiscoveryProtocol.Fingerprint(other.PublicKey);
        Assert(!DiscoveryProtocol.TryParse(reply,source,nonce,other,false,out _),"foreign pin accepted");
        var query=DiscoveryProtocol.Query(nonce,DiscoveryProtocol.HexBytes(profile.DeviceId));
        Assert(query.Length==40&&Encoding.ASCII.GetString(query,0,4)=="RBD1"&&query[7]==40);
        profile.Validate();
        var broken=ProfileFromFixture(root);broken.Fingerprint=new string('0',64);
        try{broken.Validate();throw new Exception("bad fingerprint accepted");}catch(Exception){}
        using var extra=JsonDocument.Parse("{\"profile_version\":1,\"protocol\":2,\"discovery_version\":1,\"pc_id\":\""+profile.PcId+"\",\"blind_mode\":false,\"mode_revision\":1,\"device_id\":\""+profile.DeviceId+"\",\"public_key\":\""+profile.PublicKey+"\",\"fingerprint\":\""+profile.Fingerprint+"\",\"last_ipv4\":\"192.168.50.80\",\"http_port\":80,\"ws_port\":81,\"hostname\":\"remote-boot-1a1b1c1d1e1f.local\",\"secret\":\"x\"}");
        try{DiscoveryProfile.FromJson(extra.RootElement);throw new Exception("unknown profile field accepted");}catch(Exception){}
    }
    static void Configuration() {
        string directory=Temp();try {
            string legacy=Path.Combine(directory,"legacy.json");
            File.WriteAllText(legacy,"{\"protocol\":2,\"url\":\"http://192.168.50.80\",\"pc_id\":\""+new string('1',32)+"\",\"agent_id\":\""+new string('2',32)+"\",\"token\":\""+new string('a',64)+"\",\"allow_reboot\":false,\"allow_shutdown\":false,\"ws_port\":81}");
            var config=Config.Read(legacy);Assert(!config.BlindMode&&config.DeviceId==""&&config.Token==new string('a',64));
            config.Save(legacy);var text=File.ReadAllText(legacy);Assert(!text.Contains("blind_mode",StringComparison.Ordinal)&&!text.Contains("device_id",StringComparison.Ordinal));
            var blind=Config.Read(legacy);blind.BlindMode=true;blind.ModeRevision=2;blind.DeviceId=new string('3',32);blind.DiscoveryVersion=1;
            using(var key=ECDsa.Create(ECCurve.NamedCurves.nistP256)){var parameters=key.ExportParameters(false);var pub=new byte[65];pub[0]=4;parameters.Q.X.CopyTo(pub,1);parameters.Q.Y.CopyTo(pub,33);
                blind.PublicKey=DiscoveryProtocol.Hex(pub);blind.Fingerprint=DiscoveryProtocol.Fingerprint(blind.PublicKey);}
            blind.Url=new Uri("http://192.168.50.80");blind.Save(legacy);
            var round=Config.Read(legacy);Assert(round.BlindMode&&round.DeviceId==blind.DeviceId&&round.PublicKey==blind.PublicKey&&round.Fingerprint==blind.Fingerprint&&round.ModeRevision==2);
            var bad=File.ReadAllText(legacy).Replace(blind.Fingerprint,new string('0',64));File.WriteAllText(legacy,bad);
            try{Config.Read(legacy);throw new Exception("corrupt blind config accepted");}catch(Exception){}
            blind.Save(legacy);
            using var document=JsonDocument.Parse("{\"pc_id\":\""+blind.PcId+"\",\"discovery_profile\":{\"profile_version\":1,\"protocol\":2,\"discovery_version\":1,\"pc_id\":\""+blind.PcId+"\",\"blind_mode\":true,\"mode_revision\":2,\"device_id\":\""+blind.DeviceId+"\",\"public_key\":\""+blind.PublicKey+"\",\"fingerprint\":\""+blind.Fingerprint+"\",\"last_ipv4\":\"192.168.50.80\",\"http_port\":80,\"ws_port\":81,\"hostname\":\"remote-boot-1a1b1c1d1e1f.local\"}}");
            var pairing=Config.Read(legacy);pairing.BlindMode=false;pairing.ApplyPairingProfile(document.RootElement);Assert(!pairing.BlindMode&&pairing.DeviceId==blind.DeviceId);
        } finally {Directory.Delete(directory,true);}
    }
    static void Respond(Socket server,ECDsa signer,byte[] publicKey,string deviceId,IPAddress signedIp) {
        var query=new byte[512];EndPoint remote=new IPEndPoint(IPAddress.Any,0);int length=server.ReceiveFrom(query,ref remote);Assert(length==40);
        var packet=new byte[177];Encoding.ASCII.GetBytes("RBD1").CopyTo(packet,0);packet[4]=2;packet[5]=0;packet[6]=0;packet[7]=177;
        Array.Copy(query,8,packet,8,16);DiscoveryProtocol.HexBytes(deviceId).CopyTo(packet,24);signedIp.GetAddressBytes().CopyTo(packet,40);
        packet[44]=0;packet[45]=80;packet[46]=0;packet[47]=81;publicKey.CopyTo(packet,48);
        var signature=signer.SignHash(SHA256.HashData(packet.AsSpan(0,113)),DSASignatureFormat.IeeeP1363FixedFieldConcatenation);signature.CopyTo(packet,113);
        server.SendTo(packet,remote);
    }
    static DiscoveryProfile LoopbackProfile(byte[] publicKey,string lastIpv4="192.168.50.80") {
        var profile=new DiscoveryProfile{ProfileVersion=1,Protocol=2,DiscoveryVersion=1,ModeRevision=1,BlindMode=true,PcId=new string('1',32),DeviceId=new string('3',32),
            PublicKey=DiscoveryProtocol.Hex(publicKey),LastIpv4=lastIpv4,HttpPort=80,WsPort=81,Hostname="remote-boot-"+new string('3',32).Substring(20)+".local"};
        profile.Fingerprint=DiscoveryProtocol.Fingerprint(profile.PublicKey);return profile;
    }
    static IPAddress LanAddress() {
        foreach(var network in System.Net.NetworkInformation.NetworkInterface.GetAllNetworkInterfaces()) {
            if(network.OperationalStatus!=System.Net.NetworkInformation.OperationalStatus.Up||network.NetworkInterfaceType==System.Net.NetworkInformation.NetworkInterfaceType.Loopback||network.NetworkInterfaceType==System.Net.NetworkInformation.NetworkInterfaceType.Tunnel)continue;
            string name=network.Name.ToLowerInvariant();if(name.Contains("tailscale")||name.StartsWith("wg")||name.Contains("vpn"))continue;
            foreach(var unicast in network.GetIPProperties().UnicastAddresses)if(unicast.Address.AddressFamily==AddressFamily.InterNetwork&&DiscoveryProtocol.ValidIpv4(unicast.Address.GetAddressBytes()))return unicast.Address;
        }
        return null;
    }
    static void Network() {
        var (key,publicKey)=Key();var profile=LoopbackProfile(publicKey);
        using var server=new Socket(AddressFamily.InterNetwork,SocketType.Dgram,ProtocolType.Udp);
        server.Bind(new IPEndPoint(IPAddress.Loopback,DiscoveryProtocol.Port));server.ReceiveTimeout=3000;
        var responder=new Thread(()=>Respond(server,key,publicKey,profile.DeviceId,IPAddress.Loopback));responder.Start();
        var client=new DiscoveryClient(true,new[]{new IPEndPoint(IPAddress.Loopback,DiscoveryProtocol.Port)});
        var replies=client.Discover(profile,2000,CancellationToken.None);responder.Join();Assert(replies.Count==1&&replies[0].DeviceId==profile.DeviceId&&replies[0].Address.Equals(IPAddress.Loopback));
        server.Dispose();
        var (wrongKey,wrongPublic)=Key();
        using var wrongServer=new Socket(AddressFamily.InterNetwork,SocketType.Dgram,ProtocolType.Udp);
        wrongServer.Bind(new IPEndPoint(IPAddress.Loopback,DiscoveryProtocol.Port));wrongServer.ReceiveTimeout=3000;
        var wrongResponder=new Thread(()=>Respond(wrongServer,wrongKey,wrongPublic,profile.DeviceId,IPAddress.Loopback));wrongResponder.Start();
        var rejected=client.Discover(profile,400,CancellationToken.None);wrongResponder.Join();Assert(rejected.Count==0,"wrong key accepted");
        wrongServer.Dispose();
        var timeoutClient=new DiscoveryClient(true,new[]{new IPEndPoint(IPAddress.Loopback,DiscoveryProtocol.Port)});
        var none=timeoutClient.Discover(profile,300,CancellationToken.None);Assert(none.Count==0,"timeout returned a reply");
        key.Dispose();wrongKey.Dispose();
    }
    static void BlindEndpoint() {
        string directory=Temp();
        var (key,publicKey)=Key();var profile=LoopbackProfile(publicKey);profile.LastIpv4="192.168.50.80";
        using var server=new Socket(AddressFamily.InterNetwork,SocketType.Dgram,ProtocolType.Udp);
        server.Bind(new IPEndPoint(IPAddress.Loopback,DiscoveryProtocol.Port));server.ReceiveTimeout=3000;
        var responder=new Thread(()=>Respond(server,key,publicKey,profile.DeviceId,IPAddress.Loopback));responder.Start();
        try {
            string configPath=Path.Combine(directory,"agent.json");
            var config=new Config{Url=new Uri("http://192.168.50.80"),Token=new string('a',64),PcId=profile.PcId,AgentId=new string('2',32),WsPort=81,
                DeviceId=profile.DeviceId,PublicKey=profile.PublicKey,Fingerprint=profile.Fingerprint,DiscoveryVersion=1,ModeRevision=1,BlindMode=true};
            config.Save(configPath);
            var client=new DiscoveryClient(true,new[]{new IPEndPoint(IPAddress.Loopback,DiscoveryProtocol.Port)});
            DiscoveryRuntime.EnsureEndpoint(config,configPath,CancellationToken.None,client);
            responder.Join();var saved=Config.Read(configPath);
            Assert(saved.Url.Host==IPAddress.Loopback.ToString()&&saved.BlindMode&&saved.WsPort==81,"blind endpoint was not persisted");
        } finally {server.Dispose();Directory.Delete(directory,true);key.Dispose();}
    }
    static void ApplyProfileNormal() {
        string directory=Temp();try {
            var (key,publicKey)=Key();var profile=LoopbackProfile(publicKey);profile.BlindMode=false;profile.LastIpv4="192.168.50.80";
            string profilePath=Path.Combine(directory,"profile.json");string configPath=Path.Combine(directory,"agent.json");
            File.WriteAllText(profilePath,"{\"profile_version\":1,\"protocol\":2,\"discovery_version\":1,\"pc_id\":\""+profile.PcId+"\",\"blind_mode\":false,\"mode_revision\":1,\"device_id\":\""+profile.DeviceId+"\",\"public_key\":\""+profile.PublicKey+"\",\"fingerprint\":\""+profile.Fingerprint+"\",\"last_ipv4\":\"192.168.50.80\",\"http_port\":80,\"ws_port\":81,\"hostname\":\""+profile.Hostname+"\"}");
            File.WriteAllText(configPath,"{\"protocol\":2,\"url\":\"http://192.168.50.80\",\"pc_id\":\""+profile.PcId+"\",\"agent_id\":\""+new string('2',32)+"\",\"token\":\""+new string('a',64)+"\",\"allow_reboot\":false,\"allow_shutdown\":false,\"ws_port\":81}");
            DiscoveryRuntime.ApplyProfile(profilePath,configPath,profile.Fingerprint);var config=Config.Read(configPath);
            Assert(!config.BlindMode&&config.DeviceId==profile.DeviceId&&config.PublicKey==profile.PublicKey&&config.Url.Host=="192.168.50.80");
            key.Dispose();
        } finally {Directory.Delete(directory,true);}
    }
    static string Header(Stream stream) {
        var text=new StringBuilder();var one=new byte[1];while(text.Length<16000){if(stream.Read(one,0,1)==0)throw new Exception("unexpected EOF");text.Append((char)one[0]);if(text.ToString().EndsWith("\r\n\r\n",StringComparison.Ordinal))return text.ToString();}throw new Exception("header too large");
    }
    static void Preparation() {
        string directory=Temp();try {
            var listener=new TcpListener(IPAddress.Loopback,0);listener.Start();int port=((IPEndPoint)listener.LocalEndpoint).Port;
            var (key,publicKey)=Key();var profile=LoopbackProfile(publicKey);profile.BlindMode=false;
            string configPath=Path.Combine(directory,"agent.json");string manifestPath=Path.Combine(directory,"manifest.json");
            var config=new Config{Url=new Uri("http://127.0.0.1:"+port),Token=new string('a',64),PcId=profile.PcId,AgentId=new string('2',32),WsPort=81,
                DeviceId=profile.DeviceId,PublicKey=profile.PublicKey,Fingerprint=profile.Fingerprint,DiscoveryVersion=1,ModeRevision=1,BlindMode=false};
            config.Save(configPath);
            File.WriteAllText(manifestPath,"{\"pc_id\":\""+profile.PcId+"\",\"device_id\":\""+profile.DeviceId+"\",\"public_key\":\""+profile.PublicKey+"\",\"fingerprint\":\""+profile.Fingerprint+"\",\"mode_revision\":1,\"blind_mode\":false,\"discovery_version\":1,\"ipxe_sha256\":\""+new string('b',64)+"\",\"loader_sha256\":\""+new string('c',64)+"\"}");
            var server=System.Threading.Tasks.Task.Run(()=>{using var tcp=listener.AcceptTcpClient();var stream=tcp.GetStream();string header=Header(stream);Assert(header.StartsWith("POST /api/v2/agent/preparation ",StringComparison.Ordinal));Assert(header.Contains("Authorization: Bearer "+new string('a',64),StringComparison.Ordinal));
                int length=0;foreach(string line in header.Split("\r\n"))if(line.StartsWith("Content-Length:",StringComparison.OrdinalIgnoreCase))length=int.Parse(line.Substring(15).Trim());
                var body=new byte[length];stream.ReadExactly(body,0,length);using var doc=JsonDocument.Parse(body);var d=doc.RootElement;
                Assert(Json.Text(d,"pc_id")==profile.PcId&&Json.Text(d,"agent_id")==new string('2',32)&&Json.Bool(d,"boot_prepared")&&Json.Bool(d,"blind_mode")==false);
                Assert(Json.Text(d,"ipxe_sha256")==new string('b',64)&&Json.Text(d,"loader_sha256")==new string('c',64));
                stream.Write(Encoding.ASCII.GetBytes("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\n{}"));
            });
            DiscoveryRuntime.ReportPreparation(manifestPath,configPath,CancellationToken.None).GetAwaiter().GetResult();server.GetAwaiter().GetResult();key.Dispose();
        } finally {Directory.Delete(directory,true);}
    }
    static void DnsDeadline() {
        var (key,pub)=Key();using(key) {
            var profile=LoopbackProfile(pub);profile.Hostname="remote-boot-"+profile.DeviceId.Substring(20)+".local";
            int calls=0;var pending=new System.Threading.Tasks.TaskCompletionSource<IPAddress[]>();
            var client=new DiscoveryClient(true,new[]{new IPEndPoint(IPAddress.Loopback,DiscoveryProtocol.Port)},(host,token)=>{calls++;return pending.Task;});
            var timer=System.Diagnostics.Stopwatch.StartNew();
            var replies=client.Discover(profile,100,CancellationToken.None);
            Assert(replies.Count==0&&calls==1&&timer.ElapsedMilliseconds<1500,"stalled DNS escaped discovery deadline");
        }
    }
    public static int Run() {
        Fixture();Configuration();DnsDeadline();Network();BlindEndpoint();ApplyProfileNormal();Preparation();
        Console.WriteLine("PASS: shared signed discovery fixture, profile/config pinning, legacy compatibility, atomic config, loopback/production-LAN UDP success/wrong-key/timeout, blind endpoint persistence, normal profile apply and authenticated preparation report");
        return 0;
    }
}
}
