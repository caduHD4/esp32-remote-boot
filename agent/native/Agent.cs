using System;
using System.Diagnostics;
using System.Collections.Generic;
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
    readonly HashSet<string> consumed=new HashSet<string>(StringComparer.Ordinal);
    string lastId,pendingId="",action="",target="";long received;
    public CommandGate(IHost host,Config config,string ackPath,string session) {
        this.host=host;this.config=config;this.ackPath=ackPath;this.session=session;
        lastId=File.Exists(ackPath)?File.ReadAllText(ackPath):"";if(lastId!="")consumed.Add(lastId);
    }
    public bool Pending=>pendingId.Length>0;
    public string Accept(JsonElement d) {
        string id=Json.Text(d,"id"),next=Json.Text(d,"action"),boot=Json.Text(d,"boot_id");
        if(Pending||!AgentIdentity.ValidHex(id,32)||consumed.Contains(id)||!AgentIdentity.Matches(d,config,session))return "";
        if(next=="shutdown") { if(!config.AllowShutdown)return ""; }
        else if(next=="reboot") { if(!config.AllowReboot)return "";host.ValidateTarget(boot); }
        else return "";
        // Persist before ACK; no retries of a consumed ID after a lost response.
        using(var file=new FileStream(ackPath,FileMode.Create,FileAccess.Write,FileShare.None)) {
            file.Write(Encoding.ASCII.GetBytes(id));file.Flush(true);
        }
        lastId=id;consumed.Add(id);pendingId=id;action=next;target=boot;received=Stopwatch.GetTimestamp();return id;
    }
    public string Commit(JsonElement d) {
        if(!Pending||Json.Text(d,"id")!=pendingId||!AgentIdentity.Matches(d,config,session))return "";
        string id=pendingId;pendingId="";
        if(!Json.Bool(d,"accepted")||Stopwatch.GetElapsedTime(received).TotalSeconds>=15)return "";
        host.Execute(action,target);return id;
    }
}
public sealed class Agent {
    readonly Config config;readonly IHost host;readonly string ackPath;
    public Agent(Config config,IHost host,string ackPath){this.config=config;this.host=host;this.ackPath=ackPath;}
    public static async Task<JsonDocument> Receive(WebSocket socket,CancellationToken token) {
        var bytes=new byte[12000];int count=0;
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
    async Task Sync(string session,CancellationToken token) {
        var catalog=host.Catalog();
        var data=Json.Build(w=>{w.WriteStartArray("systems");foreach(var e in catalog){w.WriteStartObject();w.WriteString("id",e.id);w.WriteString("name",e.name);w.WriteBoolean("hidden",e.hidden);w.WriteBoolean("blocked",e.blocked);w.WriteEndObject();}w.WriteEndArray();});
        if(data.Length>12000)throw new Exception("Catalog too large");
        using var handler=new HttpClientHandler{UseProxy=false,AllowAutoRedirect=false};
        using var http=new HttpClient(handler){Timeout=TimeSpan.FromSeconds(8)};
        using var request=new HttpRequestMessage(HttpMethod.Post,new Uri(config.Url,"/api/v2/agent/systems/sync"));
        request.Headers.Authorization=new AuthenticationHeaderValue("Bearer",config.Token);
        request.Headers.Add("X-Agent-Session",session);
        request.Content=new ByteArrayContent(data);request.Content.Headers.ContentType=new MediaTypeHeaderValue("application/json");
        using var response=await http.SendAsync(request,token);response.EnsureSuccessStatusCode();
    }
    public async Task ConnectOnce(CancellationToken token,bool checkOnly=false) {
        using var socket=new ClientWebSocket();
        using var wsHandler=new HttpClientHandler{UseProxy=false,AllowAutoRedirect=false};
        using var wsHttp=new HttpMessageInvoker(wsHandler,false);
        socket.Options.Proxy=null;socket.Options.AddSubProtocol("arduino");
        socket.Options.KeepAliveInterval=TimeSpan.FromSeconds(60);
#if NET9_0_OR_GREATER
        socket.Options.KeepAliveTimeout=TimeSpan.FromSeconds(15);
#endif
        using(var timeout=CancellationTokenSource.CreateLinkedTokenSource(token)) {
            timeout.CancelAfter(TimeSpan.FromSeconds(10));
            await socket.ConnectAsync(new UriBuilder("ws",config.Url.Host,config.WsPort,"/agent/v2").Uri,wsHttp,timeout.Token);
        }
        string session=Guid.NewGuid().ToString("N");var gate=new CommandGate(host,config,ackPath,session);
        using(var timeout=CancellationTokenSource.CreateLinkedTokenSource(token)) {
            timeout.CancelAfter(TimeSpan.FromSeconds(10));
            await Send(socket,Json.Build(w=>{w.WriteString("type","hello");w.WriteNumber("protocol",2);w.WriteString("agent_id",config.AgentId);w.WriteString("token",config.Token);w.WriteString("session_id",session);
                w.WriteString("hostname",Environment.MachineName);w.WriteString("os",OperatingSystem.IsWindows()?"Windows":"Linux");w.WriteString("boot_id",host.CurrentBoot());
                w.WriteStartObject("permissions");w.WriteBoolean("shutdown",config.AllowShutdown);w.WriteBoolean("reboot",config.AllowReboot);w.WriteEndObject();}),timeout.Token);
            using var ready=await Receive(socket,timeout.Token);
            if(Json.Text(ready.RootElement,"type")!="ready"||!AgentIdentity.Matches(ready.RootElement,config,session))throw new Exception("Authentication rejected");
        }
        await Sync(session,token);if(checkOnly)return;Console.WriteLine("Agent connected via WebSocket");
        while(!token.IsCancellationRequested) {
            using var timeout=CancellationTokenSource.CreateLinkedTokenSource(token);
            if(gate.Pending)timeout.CancelAfter(TimeSpan.FromSeconds(15));
            using var doc=await Receive(socket,timeout.Token);var d=doc.RootElement;
            string type=Json.Text(d,"type");
            if(type=="discover") { if(!gate.Pending&&AgentIdentity.Matches(d,config,session))await Sync(session,token); }
            else if(type=="command") {
                string id=gate.Accept(d);
                if(id!="")await Send(socket,Json.Build(w=>{w.WriteString("type","ack");w.WriteString("id",id);AgentIdentity.Write(w,config,session);}),token);
            } else if(type=="ack") {
                try {
                    string id=gate.Commit(d);
                    if(id!="")await Send(socket,Json.Build(w=>{w.WriteString("type","result");w.WriteString("id",id);w.WriteBoolean("requested",true);AgentIdentity.Write(w,config,session);}),token);
                } catch(Exception ex) {
                    Console.Error.WriteLine("Power operation refused: "+ex.GetType().Name);
                    await Send(socket,Json.Build(w=>{w.WriteString("type","result");w.WriteString("id",Json.Text(d,"id"));w.WriteBoolean("requested",false);AgentIdentity.Write(w,config,session);}),token);
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
            bool pair=false,check=false,allowReboot=false,allowShutdown=false;string configPath="",url="";
            for(int i=0;i<args.Length;i++) {
                switch(args[i]) {
                    case "--pair":pair=true;break;
                    case "--check":check=true;break;
                    case "--allow-reboot":allowReboot=true;break;
                    case "--allow-shutdown":allowShutdown=true;break;
                    case "--config":if(++i==args.Length)throw new Exception("Missing configuration path");configPath=args[i];break;
                    case "--url":if(++i==args.Length)throw new Exception("Missing URL");url=args[i];break;
                    default:throw new Exception("Unknown option");
                }
            }
            if(configPath==""||pair&&url==""||pair&&check||!pair&&url!="") {
                Console.Error.WriteLine("Usage: remote-boot-agent --pair --url http://IP --config PATH [--allow-reboot] [--allow-shutdown] | --config PATH [--check] | --self-test");return 2;
            }
            string path=Path.GetFullPath(configPath);var directory=Path.GetDirectoryName(path);
            if(!Directory.Exists(directory))throw new Exception("Create a protected configuration directory first");
            using var instance=new FileStream(Path.Combine(directory,"agent.lock"),FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None);
            using var stop=new CancellationTokenSource();Console.CancelKeyPress+=(s,e)=>{e.Cancel=true;stop.Cancel();};
            if(pair) {
                using var pairing=new PairingClient();
                await pairing.Pair(new Uri(url),path,allowReboot,allowShutdown,async (paired,token)=>{
                    var verificationHost=new NativeHost();
                    await new Agent(paired,verificationHost,Path.Combine(directory,"ack.txt")).ConnectOnce(token,true);
                },stop.Token);return 0;
            }
            var config=Config.Read(path);
            var host=new NativeHost();var agent=new Agent(config,host,Path.Combine(directory,"ack.txt"));
            if(check){using var timeout=new CancellationTokenSource(TimeSpan.FromSeconds(20));await agent.ConnectOnce(timeout.Token,true);Console.WriteLine("Agent identity verified");}
            else await agent.Run(stop.Token);return 0;
        } catch(OperationCanceledException){Console.Error.WriteLine("Agent canceled or timed out");return 1;}
        catch(Exception ex){Console.Error.WriteLine(args.Length==1&&args[0]=="--self-test"?ex.ToString():"Agent stopped: "+ex.GetType().Name);return 1;}
    }
}
}
