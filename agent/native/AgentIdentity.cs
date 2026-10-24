using System;
using System.IO;
using System.Net;
using System.Text.Json;

namespace RemoteBoot {
public static class AgentIdentity {
    public static bool ValidHex(string value,int length) {
        if(value==null||value.Length!=length)return false;
        foreach(char c in value)if(!(c>='0'&&c<='9'||c>='a'&&c<='f'))return false;
        return true;
    }
    public static bool Matches(JsonElement message,Config config,string session)=>
        Json.Text(message,"pc_id")==config.PcId&&Json.Text(message,"agent_id")==config.AgentId&&Json.Text(message,"session_id")==session;
    public static void Write(Utf8JsonWriter writer,Config config,string session) {
        writer.WriteString("pc_id",config.PcId);writer.WriteString("agent_id",config.AgentId);writer.WriteString("session_id",session);
    }
}
public sealed class Config {
    public Uri Url;public string Token,PcId,AgentId;public int Protocol=2;public bool AllowShutdown,AllowReboot;public int WsPort=81;
    public bool BlindMode;public uint ModeRevision=1;public string DeviceId="",PublicKey="",Fingerprint="";public int DiscoveryVersion;
    public static bool ValidToken(string token)=>AgentIdentity.ValidHex(token,64);
    public static void ValidateUrl(Uri url) {
        if(url==null||url.Scheme!="http"||!IPAddress.TryParse(url.Host,out var ip)||ip.AddressFamily!=System.Net.Sockets.AddressFamily.InterNetwork||
            url.AbsolutePath!="/"||url.Query!=""||url.Fragment!=""||url.UserInfo!="")throw new Exception("Invalid server URL");
    }
    public void Validate() {
        ValidateUrl(Url);
        if(Protocol!=2||!AgentIdentity.ValidHex(PcId,32)||!AgentIdentity.ValidHex(AgentId,32)||!ValidToken(Token)||WsPort<1||WsPort>65535)
            throw new InvalidOperationException("Invalid protocol 2 agent configuration");
        bool pinned=DeviceId.Length>0||PublicKey.Length>0||Fingerprint.Length>0||DiscoveryVersion!=0;
        if(pinned) {
            if(!AgentIdentity.ValidHex(DeviceId,32)||PublicKey.Length!=130||!PublicKey.StartsWith("04",StringComparison.Ordinal)||!AgentIdentity.ValidHex(PublicKey,130)||
                !AgentIdentity.ValidHex(Fingerprint,64)||Fingerprint!=DiscoveryProtocol.Fingerprint(PublicKey)||DiscoveryVersion!=1||ModeRevision==0)
                throw new InvalidOperationException("Invalid discovery pin");
        }
        if(BlindMode&&!pinned)throw new InvalidOperationException("Blind mode requires a complete discovery pin");
    }
    public static Config Read(string path) {
        if(new FileInfo(path).Length>16384)throw new Exception("Configuration too large");
        using var doc=JsonDocument.Parse(File.ReadAllText(path));var d=doc.RootElement;
        var c=new Config{Protocol=d.GetProperty("protocol").GetInt32(),Url=new Uri(d.GetProperty("url").GetString()),Token=Json.Text(d,"token"),
            PcId=Json.Text(d,"pc_id"),AgentId=Json.Text(d,"agent_id"),AllowShutdown=Json.Bool(d,"allow_shutdown"),AllowReboot=Json.Bool(d,"allow_reboot")};
        if(d.TryGetProperty("ws_port",out var port))c.WsPort=port.GetInt32();
        if(d.TryGetProperty("blind_mode",out var blind)){if(blind.ValueKind!=JsonValueKind.True&&blind.ValueKind!=JsonValueKind.False)throw new Exception("Invalid blind_mode");c.BlindMode=blind.GetBoolean();}
        if(d.TryGetProperty("mode_revision",out var revision)){if(revision.ValueKind!=JsonValueKind.Number||!revision.TryGetUInt32(out uint value)||value==0)throw new Exception("Invalid mode_revision");c.ModeRevision=value;}
        if(d.TryGetProperty("discovery_version",out var discovery)){if(discovery.ValueKind!=JsonValueKind.Number||!discovery.TryGetInt32(out int value))throw new Exception("Invalid discovery_version");c.DiscoveryVersion=value;}
        c.DeviceId=OptionalText(d,"device_id");c.PublicKey=OptionalText(d,"public_key");c.Fingerprint=OptionalText(d,"fingerprint");
        c.Validate();return c;
    }
    public void Save(string path) {
        Validate();string temporary=path+"."+Guid.NewGuid().ToString("N")+".tmp";
        try {
            var options=new FileStreamOptions{Mode=FileMode.CreateNew,Access=FileAccess.Write,Share=FileShare.None};
            if(!OperatingSystem.IsWindows())options.UnixCreateMode=UnixFileMode.UserRead|UnixFileMode.UserWrite;
            using(var file=new FileStream(temporary,options)) {
                var data=Json.Build(w=>{w.WriteNumber("protocol",2);w.WriteString("url",Url.AbsoluteUri);w.WriteString("pc_id",PcId);w.WriteString("agent_id",AgentId);
                    w.WriteString("token",Token);w.WriteBoolean("allow_reboot",AllowReboot);w.WriteBoolean("allow_shutdown",AllowShutdown);w.WriteNumber("ws_port",WsPort);
                    if(BlindMode||DeviceId.Length>0||PublicKey.Length>0||Fingerprint.Length>0||DiscoveryVersion!=0||ModeRevision!=1) {
                        w.WriteBoolean("blind_mode",BlindMode);w.WriteNumber("mode_revision",ModeRevision);
                        if(DeviceId.Length>0)w.WriteString("device_id",DeviceId);if(PublicKey.Length>0)w.WriteString("public_key",PublicKey);if(Fingerprint.Length>0)w.WriteString("fingerprint",Fingerprint);
                        if(DiscoveryVersion!=0)w.WriteNumber("discovery_version",DiscoveryVersion);
                    }});
                file.Write(data);file.Flush(true);
            }
            File.Move(temporary,path,true);
        } finally {if(File.Exists(temporary))File.Delete(temporary);}
    }
    public void ApplyPairingProfile(JsonElement root) {
        if(!root.TryGetProperty("discovery_profile",out var profile)||profile.ValueKind!=JsonValueKind.Object)return;
        try {
            var parsed=DiscoveryProfile.FromJson(profile);
            if(parsed.PcId!=PcId)return;
            DeviceId=parsed.DeviceId;PublicKey=parsed.PublicKey;Fingerprint=parsed.Fingerprint;DiscoveryVersion=parsed.DiscoveryVersion;ModeRevision=(uint)parsed.ModeRevision;
        } catch(Exception) { /* optional metadata must not break an approved legacy pairing */ }
    }
    static string OptionalText(JsonElement d,string key){if(!d.TryGetProperty(key,out var value))return "";if(value.ValueKind!=JsonValueKind.String)throw new Exception("Invalid "+key);return value.GetString()??"";}
}
}
