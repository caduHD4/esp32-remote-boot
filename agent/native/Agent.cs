using System;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Net.WebSockets;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace RemoteBoot {
public sealed class Config {
    public Uri Url;public string Token;public bool AllowShutdown,AllowReboot;public int WsPort=81;
    public static bool ValidToken(string token) {
        if(token==null||token.Length<8||token.Length>128)return false;
        foreach(char character in token)if(char.IsControl(character))return false;
        return true;
    }
    public static Config Read(string path) {
        if(new FileInfo(path).Length>16384)throw new Exception("Configuration too large");
        using var doc=JsonDocument.Parse(File.ReadAllText(path));var d=doc.RootElement;
        var c=new Config{Url=new Uri(d.GetProperty("url").GetString()),Token=d.GetProperty("token").GetString(),
            AllowShutdown=Json.Bool(d,"allow_shutdown"),AllowReboot=Json.Bool(d,"allow_reboot")};
        if(d.TryGetProperty("ws_port",out var port))c.WsPort=port.GetInt32();
        if(c.Url.Scheme!="http"||!IPAddress.TryParse(c.Url.Host,out var ip)||ip.AddressFamily!=System.Net.Sockets.AddressFamily.InterNetwork||
           c.Url.AbsolutePath!="/"||c.Url.Query!=""||c.Url.UserInfo!=""||c.WsPort<1||c.WsPort>65535||
           !ValidToken(c.Token))throw new Exception("Invalid configuration");
        return c;
    }
}
public static class Json {
    public static string Text(JsonElement d,string key)=>d.TryGetProperty(key,out var v)&&v.ValueKind==JsonValueKind.String?v.GetString():"";
    public static bool Bool(JsonElement d,string key)=>d.TryGetProperty(key,out var v)&&v.ValueKind==JsonValueKind.True;
    public static byte[] Build(Action<Utf8JsonWriter> build) {
        using var stream=new MemoryStream();using(var writer=new Utf8JsonWriter(stream)){writer.WriteStartObject();build(writer);writer.WriteEndObject();}
        return stream.ToArray();
    }
}
public sealed class CommandGate {
    readonly IHost host;readonly Config config;readonly string ackPath,session;
    string lastId,pendingId="",action="",target="";long received;
    public CommandGate(IHost host,Config config,string ackPath,string session) {
        this.host=host;this.config=config;this.ackPath=ackPath;this.session=session;
        lastId=File.Exists(ackPath)?File.ReadAllText(ackPath):"";
    }
    public bool Pending=>pendingId.Length>0;
    public string Accept(JsonElement d) {
        string id=Json.Text(d,"id"),next=Json.Text(d,"action"),boot=Json.Text(d,"boot_id");
        if(Pending||id.Length!=32||id==lastId||Json.Text(d,"session_id")!=session)return "";
        foreach(char ch in id)if(!Uri.IsHexDigit(ch))return "";
        if(next=="shutdown") { if(!config.AllowShutdown)return ""; }
        else if(next=="reboot") { if(!config.AllowReboot)return "";host.ValidateTarget(boot); }
        else return "";
        // Persist before ACK; no retries of a consumed ID after a lost response.
        using(var file=new FileStream(ackPath,FileMode.Create,FileAccess.Write,FileShare.None)) {
            file.Write(Encoding.ASCII.GetBytes(id));file.Flush(true);
        }
        lastId=id;pendingId=id;action=next;target=boot;received=Stopwatch.GetTimestamp();return id;
    }
    public string Commit(JsonElement d) {
        if(!Pending||Json.Text(d,"id")!=pendingId||Json.Text(d,"session_id")!=session)return "";
        string id=pendingId;pendingId="";
        if(!Json.Bool(d,"accepted")||Stopwatch.GetElapsedTime(received).TotalSeconds>=15)return "";
        host.Execute(action,target);return id;
    }
}
public sealed class Agent {
    readonly Config config;readonly IHost host;readonly string ackPath;
    public Agent(Config config,IHost host,string ackPath){this.config=config;this.host=host;this.ackPath=ackPath;}
    public static async Task<JsonDocument> Receive(WebSocket socket,CancellationToken token) {
        var bytes=new byte[12288];int count=0;
        while(true) {
            if(count==bytes.Length)throw new Exception("Frame exceeds protocol limit");
            var r=await socket.ReceiveAsync(new ArraySegment<byte>(bytes,count,bytes.Length-count),token);
            if(r.MessageType!=WebSocketMessageType.Text)throw new Exception("Connection closed or non-text message");
            count+=r.Count;if(r.EndOfMessage)break;
        }
        var doc=JsonDocument.Parse(bytes.AsMemory(0,count));
        if(doc.RootElement.ValueKind!=JsonValueKind.Object){doc.Dispose();throw new Exception("Invalid message");}
        return doc;
    }
    static Task Send(WebSocket socket,byte[] data,CancellationToken token)=>socket.SendAsync(new ArraySegment<byte>(data),WebSocketMessageType.Text,true,token);
    async Task Sync(CancellationToken token) {
        var catalog=host.Catalog();
        var data=Json.Build(w=>{w.WriteStartArray("systems");foreach(var e in catalog){w.WriteStartObject();w.WriteString("id",e.id);w.WriteString("name",e.name);w.WriteBoolean("hidden",e.hidden);w.WriteBoolean("blocked",e.blocked);w.WriteEndObject();}w.WriteEndArray();});
        if(data.Length>12000)throw new Exception("Catalog too large");
        using var handler=new HttpClientHandler{UseProxy=false,AllowAutoRedirect=false};
        using var http=new HttpClient(handler){Timeout=TimeSpan.FromSeconds(8)};
        using var request=new HttpRequestMessage(HttpMethod.Post,new Uri(config.Url,"/api/v1/systems/sync"));
        request.Headers.Authorization=new AuthenticationHeaderValue("Bearer",config.Token);
        request.Content=new ByteArrayContent(data);request.Content.Headers.ContentType=new MediaTypeHeaderValue("application/json");
        using var response=await http.SendAsync(request,token);response.EnsureSuccessStatusCode();
    }
    public async Task ConnectOnce(CancellationToken token) {
        using var socket=new ClientWebSocket();
        socket.Options.Proxy=null;socket.Options.AddSubProtocol("arduino");
        socket.Options.KeepAliveInterval=TimeSpan.FromSeconds(60);
#if NET9_0_OR_GREATER
        socket.Options.KeepAliveTimeout=TimeSpan.FromSeconds(15);
#endif
        using(var timeout=CancellationTokenSource.CreateLinkedTokenSource(token)) {
            timeout.CancelAfter(TimeSpan.FromSeconds(10));
            await socket.ConnectAsync(new UriBuilder("ws",config.Url.Host,config.WsPort,"/agent").Uri,timeout.Token);
        }
        string session=Guid.NewGuid().ToString("N");var gate=new CommandGate(host,config,ackPath,session);
        using(var timeout=CancellationTokenSource.CreateLinkedTokenSource(token)) {
            timeout.CancelAfter(TimeSpan.FromSeconds(10));
            await Send(socket,Json.Build(w=>{w.WriteString("type","hello");w.WriteString("token",config.Token);w.WriteString("session_id",session);
                w.WriteString("hostname",Environment.MachineName);w.WriteString("os",OperatingSystem.IsWindows()?"Windows":"Linux");w.WriteString("boot_id",host.CurrentBoot());
                w.WriteBoolean("shutdown_enabled",config.AllowShutdown);w.WriteBoolean("reboot_enabled",config.AllowReboot);}),timeout.Token);
            using var ready=await Receive(socket,timeout.Token);
            if(Json.Text(ready.RootElement,"type")!="ready"||Json.Text(ready.RootElement,"session_id")!=session)throw new Exception("Authentication rejected");
        }
        await Sync(token);Console.WriteLine("Agent connected via WebSocket");
        while(!token.IsCancellationRequested) {
            using var timeout=CancellationTokenSource.CreateLinkedTokenSource(token);
            if(gate.Pending)timeout.CancelAfter(TimeSpan.FromSeconds(15));
            using var doc=await Receive(socket,timeout.Token);var d=doc.RootElement;
            string type=Json.Text(d,"type");
            if(type=="discover") { if(!gate.Pending)await Sync(token); }
            else if(type=="command") {
                string id=gate.Accept(d);
                if(id!="")await Send(socket,Json.Build(w=>{w.WriteString("type","ack");w.WriteString("id",id);w.WriteString("session_id",session);}),token);
            } else if(type=="ack") {
                try {
                    string id=gate.Commit(d);
                    if(id!="")await Send(socket,Json.Build(w=>{w.WriteString("type","result");w.WriteString("id",id);w.WriteBoolean("requested",true);}),token);
                } catch(Exception ex) {
                    Console.Error.WriteLine("Power operation refused: "+ex.GetType().Name);
                    await Send(socket,Json.Build(w=>{w.WriteString("type","result");w.WriteString("id",Json.Text(d,"id"));w.WriteBoolean("requested",false);}),token);
                }
            }
        }
    }
    public async Task Run(CancellationToken token) {
        int delay=1;
        while(!token.IsCancellationRequested) {
            long started=Stopwatch.GetTimestamp();
            try { await ConnectOnce(token); } catch(OperationCanceledException) when(token.IsCancellationRequested){break;}
            catch(Exception ex){Console.Error.WriteLine("Connection ended: "+ex.GetType().Name);}
            if(Stopwatch.GetElapsedTime(started).TotalSeconds>60)delay=1;
            await Task.Delay(TimeSpan.FromSeconds(delay)+TimeSpan.FromMilliseconds(Random.Shared.Next(250)),token);
            delay=Math.Min(delay*2,60);
        }
    }
}
public static class Program {
    public static async Task<int> Main(string[] args) {
        try {
            if(args.Length==1&&args[0]=="--self-test")return SelfTest.Run();
            if(args.Length!=2||args[0]!="--config") { Console.Error.WriteLine("Usage: remote-boot-agent --config PATH | --self-test");return 2; }
            string path=Path.GetFullPath(args[1]);var config=Config.Read(path);var directory=Path.GetDirectoryName(path);
            using var instance=new FileStream(Path.Combine(directory,"agent.lock"),FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None);
            using var stop=new CancellationTokenSource();Console.CancelKeyPress+=(s,e)=>{e.Cancel=true;stop.Cancel();};
            var host=new NativeHost();await new Agent(config,host,Path.Combine(directory,"ack.txt")).Run(stop.Token);return 0;
        } catch(OperationCanceledException){return 0;}
        catch(Exception ex){Console.Error.WriteLine("Agent stopped: "+ex.GetType().Name);return 1;}
    }
}
}
