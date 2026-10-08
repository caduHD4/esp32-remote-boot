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
    public static bool ValidToken(string token)=>AgentIdentity.ValidHex(token,64);
    public static void ValidateUrl(Uri url) {
        if(url==null||url.Scheme!="http"||!IPAddress.TryParse(url.Host,out var ip)||ip.AddressFamily!=System.Net.Sockets.AddressFamily.InterNetwork||
            url.AbsolutePath!="/"||url.Query!=""||url.Fragment!=""||url.UserInfo!="")throw new Exception("Invalid server URL");
    }
    public void Validate() {
        ValidateUrl(Url);
        if(Protocol!=2||!AgentIdentity.ValidHex(PcId,32)||!AgentIdentity.ValidHex(AgentId,32)||!ValidToken(Token)||WsPort<1||WsPort>65535)
            throw new InvalidOperationException("Invalid protocol 2 agent configuration");
    }
    public static Config Read(string path) {
        if(new FileInfo(path).Length>16384)throw new Exception("Configuration too large");
        using var doc=JsonDocument.Parse(File.ReadAllText(path));var d=doc.RootElement;
        var c=new Config{Protocol=d.GetProperty("protocol").GetInt32(),Url=new Uri(d.GetProperty("url").GetString()),Token=Json.Text(d,"token"),
            PcId=Json.Text(d,"pc_id"),AgentId=Json.Text(d,"agent_id"),AllowShutdown=Json.Bool(d,"allow_shutdown"),AllowReboot=Json.Bool(d,"allow_reboot")};
        if(d.TryGetProperty("ws_port",out var port))c.WsPort=port.GetInt32();c.Validate();return c;
    }
    public void Save(string path) {
        Validate();string temporary=path+"."+Guid.NewGuid().ToString("N")+".tmp";
        try {
            var options=new FileStreamOptions{Mode=FileMode.CreateNew,Access=FileAccess.Write,Share=FileShare.None};
            if(!OperatingSystem.IsWindows())options.UnixCreateMode=UnixFileMode.UserRead|UnixFileMode.UserWrite;
            using(var file=new FileStream(temporary,options)) {
                var data=Json.Build(w=>{w.WriteNumber("protocol",2);w.WriteString("url",Url.AbsoluteUri);w.WriteString("pc_id",PcId);w.WriteString("agent_id",AgentId);
                    w.WriteString("token",Token);w.WriteBoolean("allow_reboot",AllowReboot);w.WriteBoolean("allow_shutdown",AllowShutdown);w.WriteNumber("ws_port",WsPort);});
                file.Write(data);file.Flush(true);
            }
            File.Move(temporary,path,true);
        } finally {if(File.Exists(temporary))File.Delete(temporary);}
    }
}
}
