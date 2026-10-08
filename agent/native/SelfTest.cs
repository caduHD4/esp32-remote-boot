using System;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Net.Sockets;
using System.Net.WebSockets;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace RemoteBoot {
public static class SelfTest {
    const string Pc="11111111111111111111111111111111",AgentId="22222222222222222222222222222222";
    sealed class FakeHost : IHost {
        public int Calls;
        public List<Firmware.Entry> Catalog()=>new List<Firmware.Entry>{new Firmware.Entry{id="0001",name="Test OS"}};
        public string CurrentBoot()=>"0001";
        public void ValidateTarget(string t){if(t!="0001")throw new Exception("Invalid target");}
        public void Execute(string action,string target){if(action=="reboot")ValidateTarget(target);Calls++;}
    }
    static void Assert(bool condition){if(!condition)throw new Exception("Self-test assertion failed");}
    static JsonDocument Message(string type,string id,string session,string action="shutdown",bool accepted=true)=>JsonDocument.Parse(Json.Build(w=>{
        w.WriteString("pc_id",Pc);w.WriteString("agent_id",AgentId);w.WriteString("type",type);w.WriteString("id",id);w.WriteString("session_id",session);w.WriteString("action",action);w.WriteString("boot_id","0001");w.WriteBoolean("accepted",accepted);
    }));
    static async Task<string> Header(Stream stream,CancellationToken token) {
        var text=new StringBuilder();var b=new byte[1];
        while(text.Length<16000) {
            if(await stream.ReadAsync(b,token)==0)throw new Exception("Unexpected EOF");
            text.Append((char)b[0]);if(text.ToString().EndsWith("\r\n\r\n",StringComparison.Ordinal))return text.ToString();
        }
        throw new Exception("Header too large");
    }
    static async Task SyncResponse(TcpListener listener,CancellationToken token) {
        using var client=await listener.AcceptTcpClientAsync(token);var stream=client.GetStream();
        string header=await Header(stream,token);Assert(header.StartsWith("POST /api/v2/agent/systems/sync ",StringComparison.Ordinal));
        Assert(header.Contains("X-Agent-Session:",StringComparison.OrdinalIgnoreCase)&&header.Contains("Authorization: Bearer ",StringComparison.OrdinalIgnoreCase));
        int length=0;foreach(string line in header.Split("\r\n"))if(line.StartsWith("Content-Length:",StringComparison.OrdinalIgnoreCase))length=int.Parse(line.Substring(15).Trim());
        var data=new byte[length];await stream.ReadExactlyAsync(data,token);
        using var body=JsonDocument.Parse(data);Assert(body.RootElement.GetProperty("systems").GetArrayLength()==1);
        await stream.WriteAsync(Encoding.ASCII.GetBytes("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\n{}"),token);
    }
    static Task Send(WebSocket socket,byte[] data,CancellationToken token)=>socket.SendAsync(new ArraySegment<byte>(data),WebSocketMessageType.Text,true,token);
    static async Task Protocol(string path) {
        using var stop=new CancellationTokenSource(TimeSpan.FromSeconds(15));var token=stop.Token;
        var wsListener=new TcpListener(IPAddress.Loopback,0);var httpListener=new TcpListener(IPAddress.Loopback,0);
        wsListener.Start();httpListener.Start();
        try {
            var host=new FakeHost();
            var config=new Config{Url=new Uri("http://127.0.0.1:"+((IPEndPoint)httpListener.LocalEndpoint).Port),WsPort=((IPEndPoint)wsListener.LocalEndpoint).Port,Token=new string('a',64),PcId=Pc,AgentId=AgentId,AllowShutdown=true};
            var clientTask=new Agent(config,host,path).Run(token);
            string previousSession="";
            for(int attempt=0;attempt<3;++attempt) {
            using var tcp=await wsListener.AcceptTcpClientAsync(token);var stream=tcp.GetStream();
            string header=await Header(stream,token),key="";
            foreach(string line in header.Split("\r\n"))if(line.StartsWith("Sec-WebSocket-Key:",StringComparison.OrdinalIgnoreCase))key=line.Substring(18).Trim();
            Assert(header.StartsWith("GET /agent/v2 ",StringComparison.Ordinal));Assert(key!="");string accept=Convert.ToBase64String(SHA1.HashData(Encoding.ASCII.GetBytes(key+"258EAFA5-E914-47DA-95CA-C5AB0DC85B11")));
            await stream.WriteAsync(Encoding.ASCII.GetBytes("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: "+accept+"\r\nSec-WebSocket-Protocol: arduino\r\n\r\n"),token);
            using var socket=WebSocket.CreateFromStream(stream,true,"arduino",TimeSpan.Zero);
            using var hello=await Agent.Receive(socket,token);string session=Json.Text(hello.RootElement,"session_id");
            Assert(hello.RootElement.GetProperty("protocol").GetInt32()==2&&Json.Text(hello.RootElement,"agent_id")==AgentId&&Json.Bool(hello.RootElement.GetProperty("permissions"),"shutdown"));
            Assert(Json.Text(hello.RootElement,"token")==config.Token&&session.Length==32&&session!=previousSession);
            if(attempt==0) {
                await Send(socket,Json.Build(w=>{w.WriteString("type","ready");w.WriteString("session_id",session);w.WriteString("pc_id",new string('f',32));w.WriteString("agent_id",AgentId);}),token);
                await Task.Delay(50,token);Assert(!httpListener.Pending()&&host.Calls==0);continue;
            }
            await Send(socket,Json.Build(w=>{AgentIdentity.Write(w,config,session);w.WriteString("type","ready");}),token);
            await SyncResponse(httpListener,token);
            string commandId=new string(attempt==1?'b':'c',32);
            if(attempt>1) {
                using var stale=Message("command",new string('e',32),previousSession);
                await Send(socket,Encoding.UTF8.GetBytes(stale.RootElement.GetRawText()),token);
            }
            using var wrongPc=JsonDocument.Parse(commandIdentity(new string('f',32),session,new string('3',32),AgentId));
            await Send(socket,Encoding.UTF8.GetBytes(wrongPc.RootElement.GetRawText()),token);
            using var wrongAgent=JsonDocument.Parse(commandIdentity(new string('f',32),session,Pc,new string('3',32)));
            await Send(socket,Encoding.UTF8.GetBytes(wrongAgent.RootElement.GetRawText()),token);
            using var command=Message("command",commandId,session);
            await Send(socket,Encoding.UTF8.GetBytes(command.RootElement.GetRawText()),token);
            using var ack=await Agent.Receive(socket,token);Assert(Json.Text(ack.RootElement,"type")=="ack"&&Json.Text(ack.RootElement,"id")==commandId);Assert(host.Calls==attempt-1);
            using var crossAck=JsonDocument.Parse(commandIdentity(commandId,session,new string('3',32),AgentId));
            await Send(socket,Encoding.UTF8.GetBytes(crossAck.RootElement.GetRawText().Replace("command","ack")),token);
            using var confirmation=Message("ack",commandId,session);
            await Send(socket,Encoding.UTF8.GetBytes(confirmation.RootElement.GetRawText()),token);
            using var result=await Agent.Receive(socket,token);Assert(Json.Bool(result.RootElement,"requested")&&AgentIdentity.Matches(result.RootElement,config,session));Assert(host.Calls==attempt);
            await Send(socket,Json.Build(w=>{w.WriteString("type","discover");AgentIdentity.Write(w,config,session);}),token);
            await SyncResponse(httpListener,token);
            await socket.CloseOutputAsync(WebSocketCloseStatus.NormalClosure,"done",token);
            previousSession=session;
            }
            stop.Cancel();try{await clientTask;}catch(OperationCanceledException){ }
            Assert(host.Calls==2);Console.WriteLine("PASS: real WebSocket handshake, discovery, reconnect/new session, wrong ready/PC/agent/session rejection and ACK before simulated shutdown");
        } finally { wsListener.Stop();httpListener.Stop(); }
    }
    static string commandIdentity(string id,string session,string pc,string agent)=>System.Text.Encoding.UTF8.GetString(Json.Build(w=>{
        w.WriteString("type","command");w.WriteString("id",id);w.WriteString("session_id",session);w.WriteString("pc_id",pc);w.WriteString("agent_id",agent);w.WriteString("action","shutdown");}));
    static async Task PairingTests(string directory) {
        foreach(string mode in new[]{"approved","canceled","expired","redirect","unauthorized","lost","lost-confirm","hello-failure","invalid"}) {
            using var stop=new CancellationTokenSource(TimeSpan.FromSeconds(10));var listener=new TcpListener(IPAddress.Loopback,0);listener.Start();
            string path=Path.Combine(directory,"pair-"+mode+".json");bool verified=false;int verificationCalls=0;
            var server=Task.Run(async()=>{
                int polls=0,confirms=0;
                while(!stop.IsCancellationRequested) {
                    using var tcp=await listener.AcceptTcpClientAsync(stop.Token);var stream=tcp.GetStream();string header=await Header(stream,stop.Token);
                    int length=0;foreach(string line in header.Split("\r\n"))if(line.StartsWith("Content-Length:",StringComparison.OrdinalIgnoreCase))length=int.Parse(line.Substring(15).Trim());
                    byte[] body=new byte[length];await stream.ReadExactlyAsync(body,stop.Token);using var request=JsonDocument.Parse(body);
                    string response,status="200 OK";
                    if(header.StartsWith("POST /api/v2/pairing/start ",StringComparison.Ordinal)) {
                        Assert(Json.Text(request.RootElement,"hostname")!=""&&Json.Text(request.RootElement,"os")!="");
                        response="{\"pairing_id\":\""+new string('a',32)+"\",\"device_secret\":\""+new string('b',64)+"\",\"user_code\":\"ABCD-EFGH\",\"expires_in\":300,\"poll_interval\":2}";
                        if(mode=="redirect")status="302 Found";
                    } else {
                        Assert(header.Contains("Authorization: Bearer "+new string('b',64),StringComparison.Ordinal));
                        Assert(request.RootElement.GetRawText()=="{}");
                        if(header.Contains("/confirm ",StringComparison.Ordinal)) {
                            Assert(verified&&verificationCalls==1&&Config.Read(path).Token==new string('c',64));if(mode=="lost-confirm"&&++confirms==1)continue;response="{\"saved\":true}";
                        } else {
                            polls++;
                            if(mode=="lost"&&polls==1)continue;
                            if(mode=="unauthorized"){status="401 Unauthorized";response="{}";}
                            else if(mode=="canceled"||mode=="expired")response="{\"status\":\""+mode+"\"}";
                            else if(polls==1)response="{\"status\":\"pending\"}";
                            else response="{\"status\":\"approved\",\"protocol\":2,\"pc_id\":\""+(mode=="invalid"?"bad":Pc)+"\",\"agent_id\":\""+AgentId+"\",\"token\":\""+new string('c',64)+"\"}";
                        }
                    }
                    byte[] data=Encoding.ASCII.GetBytes(response);
                    await stream.WriteAsync(Encoding.ASCII.GetBytes("HTTP/1.1 "+status+"\r\nContent-Length: "+data.Length+"\r\nLocation: http://127.0.0.1:1/\r\nConnection: close\r\n\r\n"),stop.Token);await stream.WriteAsync(data,stop.Token);
                }
            });
            bool succeeded=false;
            try {
                using var client=new PairingClient((delay,token)=>Task.CompletedTask,_=>{});
                var paired=await client.Pair(new Uri("http://127.0.0.1:"+((IPEndPoint)listener.LocalEndpoint).Port),path,false,false,(paired,token)=>{
                    verificationCalls++;Assert(Config.Read(path).Token==paired.Token);
                    if(mode=="hello-failure")throw new InvalidOperationException("Simulated hello failure");
                    verified=true;return Task.CompletedTask;
                },stop.Token);
                Assert(paired.PcId==Pc&&!paired.AllowReboot&&!paired.AllowShutdown);succeeded=true;
            } catch(InvalidOperationException){} catch(HttpRequestException){}
            finally {stop.Cancel();listener.Stop();try{await server;}catch(OperationCanceledException){}catch(SocketException){}catch(ObjectDisposedException){} }
            Assert(succeeded==(mode=="approved"||mode=="lost"||mode=="lost-confirm"));Assert(File.Exists(path)==(succeeded||mode=="hello-failure"));
            if(mode=="hello-failure")Assert(verificationCalls==1&&!verified);
            if(succeeded) {
                File.WriteAllText(path+".interrupted.tmp","partial");Assert(Config.Read(path).AgentId==AgentId);
                var invalid=new Config{Url=new Uri("http://127.0.0.1"),PcId="bad",AgentId=AgentId,Token=new string('c',64)};
                try{invalid.Save(path);throw new Exception("Invalid identity saved");}catch(Exception){Assert(Config.Read(path).PcId==Pc);}
            }
        }
        Console.WriteLine("PASS: loopback pairing approval, pending, canceled, expired, lost poll, lost confirm, save/hello/confirm ordering, rejected hello, redirect, secret header, denied secret, invalid identity, atomic configuration");
    }
    public static int Run() {
        Assert(!Config.ValidToken("senha 8!"));
        Assert(Config.ValidToken(new string('a',64)));Assert(!Config.ValidToken("!@#$%^&*"));
        Assert(!Config.ValidToken("curta7"));
        Assert(!Config.ValidToken("linha\nquebrada"));
        Assert(!Config.ValidToken(new string('x',129)));
        string directory=Path.Combine(Path.GetTempPath(),"remote-boot-test-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(directory);
        try {
            string path=Path.Combine(directory,"ack");var host=new FakeHost();var config=new Config{PcId=Pc,AgentId=AgentId,AllowShutdown=true,AllowReboot=true};
            var gate=new CommandGate(host,config,path,"session-one");string id=new string('a',32);
            using var wrong=Message("command",id,"wrong");Assert(gate.Accept(wrong.RootElement)=="");
            using var foreign=JsonDocument.Parse(commandIdentity(id,"session-one",new string('3',32),AgentId));Assert(gate.Accept(foreign.RootElement)=="");
            config.AllowShutdown=false;using var command=Message("command",id,"session-one");Assert(gate.Accept(command.RootElement)=="");
            config.AllowShutdown=true;Assert(gate.Accept(command.RootElement)==id);Assert(host.Calls==0);Assert(gate.Accept(command.RootElement)=="");
            using var ack=Message("ack",id,"session-one");Assert(gate.Commit(ack.RootElement)==id);Assert(host.Calls==1);Assert(gate.Commit(ack.RootElement)=="");
            Assert(new CommandGate(host,config,path,"session-one").Accept(command.RootElement)=="");
            using var unknown=Message("command",new string('c',32),"session-one","shell");Assert(gate.Accept(unknown.RootElement)=="");
            using var rejected=Message("command",new string('d',32),"session-one");Assert(gate.Accept(rejected.RootElement)!="");
            using var no=Message("ack",new string('d',32),"session-one",accepted:false);Assert(gate.Commit(no.RootElement)=="");Assert(host.Calls==1);Assert(gate.Accept(command.RootElement)=="");
            PairingTests(directory).GetAwaiter().GetResult();
            Protocol(Path.Combine(directory,"protocol-ack")).GetAwaiter().GetResult();
            Console.WriteLine("PASS: permission, session, duplicate, persistence, unsupported action, rejected ACK (no real power commands)");return 0;
        } finally { Directory.Delete(directory,true); }
    }
}
}
