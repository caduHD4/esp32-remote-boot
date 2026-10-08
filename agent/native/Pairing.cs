using System;

using System.IO;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace RemoteBoot {
public sealed class PairingClient : IDisposable {
    readonly HttpClient http;readonly Func<TimeSpan,CancellationToken,Task> delay;readonly Action<string> status;
    public PairingClient(Func<TimeSpan,CancellationToken,Task> delay=null,Action<string> status=null) {
        http=new HttpClient(new HttpClientHandler{UseProxy=false,AllowAutoRedirect=false}){Timeout=TimeSpan.FromSeconds(8)};
        this.delay=delay??Task.Delay;this.status=status??Console.WriteLine;
    }
    async Task<JsonDocument> Post(Uri url,string secret,byte[] data,CancellationToken token) {
        using var requestTimeout=CancellationTokenSource.CreateLinkedTokenSource(token);requestTimeout.CancelAfter(TimeSpan.FromSeconds(8));token=requestTimeout.Token;
        using var request=new HttpRequestMessage(HttpMethod.Post,url);
        if(secret!=null)request.Headers.Authorization=new AuthenticationHeaderValue("Bearer",secret);
        request.Content=new ByteArrayContent(data);request.Content.Headers.ContentType=new MediaTypeHeaderValue("application/json");
        using var response=await http.SendAsync(request,HttpCompletionOption.ResponseHeadersRead,token);
        if((int)response.StatusCode>=300&&(int)response.StatusCode<400)throw new InvalidOperationException("Pairing redirect refused");
        if((int)response.StatusCode==401||(int)response.StatusCode==403)throw new InvalidOperationException("Pairing authentication rejected");
        response.EnsureSuccessStatusCode();
        using var stream=await response.Content.ReadAsStreamAsync(token);using var bytes=new MemoryStream();var buffer=new byte[2048];
        int count;while((count=await stream.ReadAsync(buffer,token))>0){if(bytes.Length+count>12000)throw new InvalidOperationException("Pairing response too large");bytes.Write(buffer,0,count);}
        return JsonDocument.Parse(bytes.ToArray());
    }
    public async Task<Config> Pair(Uri url,string path,bool allowReboot,bool allowShutdown,Func<Config,CancellationToken,Task> verify,CancellationToken token) {
        if(verify==null)throw new ArgumentNullException(nameof(verify));
        Config.ValidateUrl(url);
        using var lifetime=CancellationTokenSource.CreateLinkedTokenSource(token);lifetime.CancelAfter(TimeSpan.FromSeconds(300));var bounded=lifetime.Token;
        using var start=await Post(new Uri(url,"/api/v2/pairing/start"),null,Json.Build(w=>{
            w.WriteString("hostname",Environment.MachineName);w.WriteString("os",OperatingSystem.IsWindows()?"Windows":"Linux");}),bounded);
        var d=start.RootElement;string id=Json.Text(d,"pairing_id"),secret=Json.Text(d,"device_secret"),code=Json.Text(d,"user_code");
        if(!AgentIdentity.ValidHex(id,32)||!AgentIdentity.ValidHex(secret,64)||code.Length!=9||code[4]!='-'||
            d.GetProperty("poll_interval").GetInt32()!=2||d.GetProperty("expires_in").GetInt32()!=300)throw new InvalidOperationException("Invalid pairing response");
        const string alphabet="0123456789ABCDEFGHJKMNPQRSTVWXYZ";
        for(int i=0;i<code.Length;i++)if(i!=4&&alphabet.IndexOf(code[i])<0)throw new InvalidOperationException("Invalid pairing code");
        status(code);status("Waiting for administrator approval");
        var pollUrl=new Uri(url,"/api/v2/pairing/"+id+"/poll");
        while(true) {
            await delay(TimeSpan.FromSeconds(2),bounded);bounded.ThrowIfCancellationRequested();
            JsonDocument poll;
            try{poll=await Post(pollUrl,secret,Json.Build(w=>{}),bounded);}
            catch(HttpRequestException){status("Connection interrupted; retrying");continue;}
            catch(OperationCanceledException) when(!bounded.IsCancellationRequested){status("Connection timed out; retrying");continue;}
            using(poll) {
                var result=poll.RootElement;string state=Json.Text(result,"status");
                if(state=="pending")continue;
                if(state=="canceled"||state=="expired"){status("Pairing "+state);throw new InvalidOperationException("Pairing "+state);}
                if(state!="approved")throw new InvalidOperationException("Invalid pairing state");
                var config=new Config{Url=url,Protocol=result.GetProperty("protocol").GetInt32(),PcId=Json.Text(result,"pc_id"),AgentId=Json.Text(result,"agent_id"),
                    Token=Json.Text(result,"token"),AllowReboot=allowReboot,AllowShutdown=allowShutdown};
                config.Save(path);status("Approved; credential saved");
                await verify(config,bounded);status("Agent identity verified");
                for(int attempt=0;attempt<3;attempt++) {
                    try{using var confirmed=await Post(new Uri(url,"/api/v2/pairing/"+id+"/confirm"),secret,Json.Build(w=>{}),bounded);status("Pairing confirmed");return config;}
                    catch(HttpRequestException) when(attempt<2){await delay(TimeSpan.FromSeconds(2),bounded);}
                    catch(OperationCanceledException) when(!bounded.IsCancellationRequested&&attempt<2){await delay(TimeSpan.FromSeconds(2),bounded);}
                }
                throw new InvalidOperationException("Confirmation failed");
            }
        }
    }
    public void Dispose()=>http.Dispose();
}
}
