using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;
using System.Collections.Generic;

public static class Firmware {
    const string Global="{8BE4DF61-93CA-11D2-AA0D-00E098032B8C}";
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern uint GetFirmwareEnvironmentVariableExW(string name,string guid,byte[] buffer,uint size,out uint attributes);
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool SetFirmwareEnvironmentVariableExW(string name,string guid,byte[] buffer,uint size,uint attributes);
    [DllImport("kernel32.dll")] static extern IntPtr GetCurrentProcess();
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool CloseHandle(IntPtr handle);
    [DllImport("advapi32.dll",SetLastError=true)] static extern bool OpenProcessToken(IntPtr process,uint access,out IntPtr token);
    [DllImport("advapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool LookupPrivilegeValue(string system,string name,out Luid luid);
    [DllImport("advapi32.dll",SetLastError=true)] static extern bool AdjustTokenPrivileges(IntPtr token,bool disable,ref TokenPrivileges state,uint size,IntPtr previous,IntPtr needed);
    [StructLayout(LayoutKind.Sequential)] struct Luid { public uint Low; public int High; }
    [StructLayout(LayoutKind.Sequential)] struct TokenPrivileges { public uint Count; public Luid Luid; public uint Attributes; }
    public static void Enable() {
        IntPtr token;
        if(!OpenProcessToken(GetCurrentProcess(),0x28,out token)) throw new Win32Exception();
        try {
            Luid luid; if(!LookupPrivilegeValue(null,"SeSystemEnvironmentPrivilege",out luid)) throw new Win32Exception();
            var state=new TokenPrivileges{Count=1,Luid=luid,Attributes=2};
            if(!AdjustTokenPrivileges(token,false,ref state,0,IntPtr.Zero,IntPtr.Zero)) throw new Win32Exception();
            int error=Marshal.GetLastWin32Error(); if(error!=0) throw new Win32Exception(error);
        } finally { CloseHandle(token); }
    }
    public static byte[] Read(string name) {
        var buffer=new byte[65536]; uint attributes;
        uint count=GetFirmwareEnvironmentVariableExW(name,Global,buffer,(uint)buffer.Length,out attributes);
        if(count==0) { int error=Marshal.GetLastWin32Error(); if(error==203) return null; throw new Win32Exception(error,"Reading "+name); }
        Array.Resize(ref buffer,(int)count); return buffer;
    }
    public static void Write(string name,byte[] value) {
        if(!SetFirmwareEnvironmentVariableExW(name,Global,value,(uint)value.Length,value.Length==0?0u:7u)) throw new Win32Exception(Marshal.GetLastWin32Error(),"Writing "+name);
    }
    public static ushort[] Order() {
        var bytes=Read("BootOrder"); if(bytes==null||bytes.Length%2!=0) throw new Exception("Missing or malformed BootOrder");
        var ids=new ushort[bytes.Length/2]; for(int i=0;i<ids.Length;i++) ids[i]=BitConverter.ToUInt16(bytes,2*i); return ids;
    }
    public sealed class Entry { public string id; public string name; public bool hidden; public bool blocked; }
    public static Entry Parse(ushort id,byte[] b) {
        if(b==null||b.Length<12) throw new Exception("Truncated EFI_LOAD_OPTION");
        int pathLength=BitConverter.ToUInt16(b,4),end=6;
        while(end+2<=b.Length && BitConverter.ToUInt16(b,end)!=0) end+=2;
        if(end+2>b.Length || pathLength<4 || pathLength>b.Length-end-2) throw new Exception("Invalid EFI_LOAD_OPTION offsets");
        string name=Encoding.Unicode.GetString(b,6,end-6); int start=end+2,finish=start+pathLength,p=start;
        string files="";bool ended=false;
        while(p+4<=finish) {
            int length=BitConverter.ToUInt16(b,p+2); if(length<4||length>finish-p) throw new Exception("Invalid device path length");
            if(b[p]==127) { if(b[p+1]!=255||length!=4||p+4!=finish) throw new Exception("Unsupported device path end"); ended=true; break; }
            if(b[p]==4&&b[p+1]==4) { if(length<6||length%2!=0||BitConverter.ToUInt16(b,p+length-2)!=0) throw new Exception("Invalid file path"); files+=Encoding.Unicode.GetString(b,p+4,length-4); }
            p+=length;
        }
        if(!ended) throw new Exception("Missing device path end");
        string text=(name+" "+files).ToLowerInvariant();
        bool blocked=text.Contains("remote boot")||text.Contains("ipxe")||(BitConverter.ToUInt32(b,0)&1)==0;
        bool hidden=blocked||text.Contains("network")||text.Contains("ipv4")||text.Contains("ipv6")||text.Contains("usb")||text.Contains("dvd")||text.Contains("cdrom");
        return new Entry{id=id.ToString("X4"),name=name.Length>63?name.Substring(0,63):name,hidden=hidden,blocked=blocked};
    }
    public static byte[] NewOption(uint partition,ulong start,ulong size,Guid signature) {
        byte[] description=Encoding.Unicode.GetBytes("Remote Boot iPXE\0"),file=Encoding.Unicode.GetBytes("\\EFI\\iPXE\\ipxe.efi\0");
        int fileLength=file.Length+4,pathLength=42+fileLength+4;
        var b=new byte[6+description.Length+pathLength];
        Buffer.BlockCopy(BitConverter.GetBytes(1u),0,b,0,4);Buffer.BlockCopy(BitConverter.GetBytes((ushort)pathLength),0,b,4,2);Buffer.BlockCopy(description,0,b,6,description.Length);
        int p=6+description.Length;b[p]=4;b[p+1]=1;b[p+2]=42;
        Buffer.BlockCopy(BitConverter.GetBytes(partition),0,b,p+4,4);Buffer.BlockCopy(BitConverter.GetBytes(start),0,b,p+8,8);Buffer.BlockCopy(BitConverter.GetBytes(size),0,b,p+16,8);Buffer.BlockCopy(signature.ToByteArray(),0,b,p+24,16);b[p+40]=2;b[p+41]=2;
        p+=42;b[p]=4;b[p+1]=4;Buffer.BlockCopy(BitConverter.GetBytes((ushort)fileLength),0,b,p+2,2);Buffer.BlockCopy(file,0,b,p+4,file.Length);p+=fileLength;b[p]=127;b[p+1]=255;b[p+2]=4;
        return b;
    }
}
