using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;

namespace RemoteBoot {
public interface IHost {
    List<Firmware.Entry> Catalog();
    string CurrentBoot();
    void ValidateTarget(string target);
    void Execute(string action,string target);
}
public sealed class NativeHost : IHost {
    const string Efi="/sys/firmware/efi/efivars/";
    const string GuidSuffix="-8be4df61-93ca-11d2-aa0d-00e098032b8c";
    [DllImport("advapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)]
    [return:MarshalAs(UnmanagedType.Bool)]
    static extern bool InitiateSystemShutdownExW(string machine,string message,uint timeout,
        [MarshalAs(UnmanagedType.Bool)] bool force,[MarshalAs(UnmanagedType.Bool)] bool reboot,uint reason);
    public NativeHost() {
        if(OperatingSystem.IsWindows()) Firmware.Enable();
        else if(!OperatingSystem.IsLinux()||!Directory.Exists(Efi)) throw new Exception("UEFI Windows/Linux is required");
    }
    byte[] Read(string name) {
        if(OperatingSystem.IsWindows()) return Firmware.Read(name);
        string path=Efi+name+GuidSuffix;
        if(!File.Exists(path)) return null;
        if(new FileInfo(path).Length>65540) throw new Exception("EFI variable exceeds size limit");
        byte[] bytes=File.ReadAllBytes(path);
        if(bytes.Length<4) throw new Exception("Truncated efivarfs attributes");
        return bytes.AsSpan(4).ToArray();
    }
    public List<Firmware.Entry> Catalog() {
        var order=Read("BootOrder");
        if(order==null||order.Length%2!=0) throw new Exception("Invalid BootOrder");
        var ids=new List<ushort>();
        for(int i=0;i<order.Length;i+=2) { var id=BitConverter.ToUInt16(order,i);if(!ids.Contains(id))ids.Add(id); }
        // Include unlisted Linux entries without polling the filesystem.
        if(OperatingSystem.IsLinux()) foreach(string path in Directory.EnumerateFiles(Efi,"Boot????"+GuidSuffix)) {
            string name=Path.GetFileName(path);
            if(ushort.TryParse(name.Substring(4,4),System.Globalization.NumberStyles.HexNumber,null,out ushort id)&&!ids.Contains(id)) ids.Add(id);
        }
        var result=new List<Firmware.Entry>();
        foreach(ushort id in ids) {
            var bytes=Read("Boot"+id.ToString("X4"));if(bytes==null)continue;
            try { result.Add(Firmware.Parse(id,bytes)); } catch(Exception) { Console.Error.WriteLine("Skipping malformed EFI entry"); }
        }
        if(result.Count>24) throw new Exception("Catalog exceeds ESP32 limit of 24 entries");
        return result;
    }
    public string CurrentBoot() {
        var bytes=Read("BootCurrent");
        return bytes!=null&&bytes.Length==2?BitConverter.ToUInt16(bytes,0).ToString("X4"):"";
    }
    public void ValidateTarget(string target) {
        if(target==null||target.Length!=4||!ushort.TryParse(target,System.Globalization.NumberStyles.HexNumber,null,out ushort id)) throw new Exception("Invalid Boot ID");
        if(Firmware.Parse(id,Read("Boot"+id.ToString("X4"))).blocked) throw new Exception("Blocked boot target");
    }
    static void Run(string tool,params string[] args) {
        string path=File.Exists("/usr/bin/"+tool)?"/usr/bin/"+tool:"/bin/"+tool;
        var info=new ProcessStartInfo(path){UseShellExecute=false,RedirectStandardError=true,RedirectStandardOutput=true};
        foreach(string arg in args)info.ArgumentList.Add(arg);
        using var process=Process.Start(info);
        var output=process.StandardOutput.ReadToEndAsync();var error=process.StandardError.ReadToEndAsync();
        if(!process.WaitForExit(10000)) { process.Kill(true);throw new Exception(tool+" timed out"); }
        if(process.ExitCode!=0)throw new Exception(tool+" refused operation");
        output.GetAwaiter().GetResult();error.GetAwaiter().GetResult();
    }
    void SetNext(byte[] bytes) {
        if(OperatingSystem.IsWindows())Firmware.Write("BootNext",bytes??Array.Empty<byte>());
        else if(bytes==null||bytes.Length==0)Run("efibootmgr","--delete-bootnext");
        else if(bytes.Length==2)Run("efibootmgr","--bootnext",BitConverter.ToUInt16(bytes,0).ToString("X4"));
        else throw new Exception("Invalid BootNext backup");
    }
    public void Execute(string action,string target) {
        bool reboot=action=="reboot";
        if(!reboot&&action!="shutdown")throw new Exception("Unsupported action");
        byte[] previous=null;bool wroteNext=false;
        try {
            if(reboot) {
                ValidateTarget(target);previous=Read("BootNext");
                if(previous!=null&&previous.Length!=2)throw new Exception("Invalid BootNext backup");
                SetNext(BitConverter.GetBytes(Convert.ToUInt16(target,16)));wroteNext=true;
            }
            if(OperatingSystem.IsWindows()) {
                Firmware.EnablePrivilege("SeShutdownPrivilege");
                if(!InitiateSystemShutdownExW(null,"Remote Boot",0,false,reboot,0x80040000))throw new Win32Exception(Marshal.GetLastWin32Error());
            } else Run("systemctl",reboot?"reboot":"poweroff");
        } catch {
            if(wroteNext)SetNext(previous);
            throw;
        }
    }
}
}
