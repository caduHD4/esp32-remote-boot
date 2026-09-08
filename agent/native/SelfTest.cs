using System;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Net.WebSockets;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace RemoteBoot {
public static class SelfTest {
    sealed class FakeHost : IHost {
        public int Calls;
        public List<Firmware.Entry> Catalog()=>new List<Firmware.Entry>{new Firmware.Entry{id="0001",name="Test OS"}};
        public string CurrentBoot()=>"0001";
        public void ValidateTarget(string t){if(t!="0001")throw new Exception("Invalid target");}
        public void Execute(string action,string target){if(action=="reboot")ValidateTarget(target);Calls++;}
    }
    static void Assert(bool condition){if(!condition)throw new Exception("Self-test assertion failed");}
    static JsonDocument Message(string type,string id,string session,string action="shutdown",bool accepted=true)=>JsonDocument.Parse(Json.Build(w=>{
        w.WriteString("type",type);w.WriteString("id",id);w.WriteString("session_id",session);w.WriteString("action",action);w.WriteString("boot_id","0001");w.WriteBoolean("accepted",accepted);
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
        string header=await Header(stream,token);Assert(header.StartsWith("POST /api/v1/systems/sync ",StringComparison.Ordinal));
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
            var config=new Config{Url=new Uri("http://127.0.0.1:"+((IPEndPoint)httpListener.LocalEndpoint).Port),WsPort=((IPEndPoint)wsListener.LocalEndpoint).Port,Token=new string('x',24),AllowShutdown=true};
            var clientTask=new Agent(config,host,path).ConnectOnce(token);
            using var tcp=await wsListener.AcceptTcpClientAsync(token);var stream=tcp.GetStream();
            string header=await Header(stream,token),key="";
            foreach(string line in header.Split("\r\n"))if(line.StartsWith("Sec-WebSocket-Key:",StringComparison.OrdinalIgnoreCase))key=line.Substring(18).Trim();
            Assert(key!="");string accept=Convert.ToBase64String(SHA1.HashData(Encoding.ASCII.GetBytes(key+"258EAFA5-E914-47DA-95CA-C5AB0DC85B11")));
            await stream.WriteAsync(Encoding.ASCII.GetBytes("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: "+accept+"\r\nSec-WebSocket-Protocol: arduino\r\n\r\n"),token);
            using var socket=WebSocket.CreateFromStream(stream,true,"arduino",TimeSpan.Zero);
            using var hello=await Agent.Receive(socket,token);string session=Json.Text(hello.RootElement,"session_id");
            Assert(Json.Text(hello.RootElement,"token")==config.Token&&session.Length==32);
            await Send(socket,Json.Build(w=>{w.WriteString("type","ready");w.WriteString("session_id",session);}),token);
            await SyncResponse(httpListener,token);
            using var command=Message("command",new string('b',32),session);
            await Send(socket,Encoding.UTF8.GetBytes(command.RootElement.GetRawText()),token);
            using var ack=await Agent.Receive(socket,token);Assert(Json.Text(ack.RootElement,"type")=="ack");Assert(host.Calls==0);
            using var confirmation=Message("ack",new string('b',32),session);
            await Send(socket,Encoding.UTF8.GetBytes(confirmation.RootElement.GetRawText()),token);
            using var result=await Agent.Receive(socket,token);Assert(Json.Bool(result.RootElement,"requested"));Assert(host.Calls==1);
            await Send(socket,Json.Build(w=>w.WriteString("type","discover")),token);
            await SyncResponse(httpListener,token);
            await socket.CloseOutputAsync(WebSocketCloseStatus.NormalClosure,"done",token);
            try{await clientTask;}catch(Exception){ }
            Assert(host.Calls==1);Console.WriteLine("PASS: real WebSocket handshake, hello, event-driven discovery, command and ACK before simulated shutdown");
        } finally { wsListener.Stop();httpListener.Stop(); }
    }
    public static int Run() {
        string directory=Path.Combine(Path.GetTempPath(),"remote-boot-test-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(directory);
        try {
            string path=Path.Combine(directory,"ack");var host=new FakeHost();var config=new Config{AllowShutdown=true,AllowReboot=true};
            var gate=new CommandGate(host,config,path,"session-one");string id=new string('a',32);
            using var wrong=Message("command",id,"wrong");Assert(gate.Accept(wrong.RootElement)=="");
            config.AllowShutdown=false;using var command=Message("command",id,"session-one");Assert(gate.Accept(command.RootElement)=="");
            config.AllowShutdown=true;Assert(gate.Accept(command.RootElement)==id);Assert(host.Calls==0);Assert(gate.Accept(command.RootElement)=="");
            using var ack=Message("ack",id,"session-one");Assert(gate.Commit(ack.RootElement)==id);Assert(host.Calls==1);Assert(gate.Commit(ack.RootElement)=="");
            Assert(new CommandGate(host,config,path,"session-one").Accept(command.RootElement)=="");
            using var unknown=Message("command",new string('c',32),"session-one","shell");Assert(gate.Accept(unknown.RootElement)=="");
            using var rejected=Message("command",new string('d',32),"session-one");Assert(gate.Accept(rejected.RootElement)!="");
            using var no=Message("ack",new string('d',32),"session-one",accepted:false);Assert(gate.Commit(no.RootElement)=="");Assert(host.Calls==1);
            Protocol(Path.Combine(directory,"protocol-ack")).GetAwaiter().GetResult();
            Console.WriteLine("PASS: permission, session, duplicate, persistence, unsupported action, rejected ACK (no real power commands)");return 0;
        } finally { Directory.Delete(directory,true); }
    }
}
}
