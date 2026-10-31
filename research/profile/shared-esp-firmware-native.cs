// Windows firmware variable API bridge. No reads/writes during type initialization.
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
namespace SharedEspFirmware {
 public sealed class Value { public byte[] Data; public uint Attributes; }
 public static class Native {
  const string Global="{8be4df61-93ca-11d2-aa0d-00e098032b8c}";
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern uint GetFirmwareEnvironmentVariableExW(string name,string guid,[Out] byte[] data,uint size,out uint attributes);
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] [return:MarshalAs(UnmanagedType.Bool)] static extern bool SetFirmwareEnvironmentVariableExW(string name,string guid,byte[] data,uint size,uint attributes);
  [DllImport("kernel32.dll")] static extern IntPtr GetCurrentProcess();
  [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
  [DllImport("kernel32.dll")] static extern void SetLastError(uint error);
  [DllImport("advapi32.dll",SetLastError=true)] static extern bool OpenProcessToken(IntPtr process,uint access,out IntPtr token);
  [StructLayout(LayoutKind.Sequential)] struct Luid { public uint Low; public int High; }
  [StructLayout(LayoutKind.Sequential)] struct Privileges { public uint Count; public Luid Luid; public uint Attributes; }
  [DllImport("advapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool LookupPrivilegeValue(string system,string name,out Luid luid);
  [DllImport("advapi32.dll",SetLastError=true)] static extern bool AdjustTokenPrivileges(IntPtr token,bool disable,ref Privileges privileges,uint size,out Privileges previous,out uint returned);
  sealed class Scope:IDisposable {
   IntPtr token; Privileges previous;
   public Scope() {
    if(!OpenProcessToken(GetCurrentProcess(),0x28,out token))throw new Win32Exception(Marshal.GetLastWin32Error());
    try {
     Luid luid;if(!LookupPrivilegeValue(null,"SeSystemEnvironmentPrivilege",out luid))throw new Win32Exception(Marshal.GetLastWin32Error());
     var enabled=new Privileges{Count=1,Luid=luid,Attributes=2};uint returned;
     SetLastError(0);
     if(!AdjustTokenPrivileges(token,false,ref enabled,(uint)Marshal.SizeOf(typeof(Privileges)),out previous,out returned))throw new Win32Exception(Marshal.GetLastWin32Error());
     int error=Marshal.GetLastWin32Error();if(error!=0)throw new Win32Exception(error,"SeSystemEnvironmentPrivilege is unavailable; run elevated. No firmware writes attempted.");
    }catch{CloseHandle(token);token=IntPtr.Zero;throw;}
   }
   public void Dispose(){if(token!=IntPtr.Zero){Privileges ignored;uint size;SetLastError(0);bool restored=AdjustTokenPrivileges(token,false,ref previous,(uint)Marshal.SizeOf(typeof(Privileges)),out ignored,out size);int error=Marshal.GetLastWin32Error();CloseHandle(token);token=IntPtr.Zero;if(!restored || error!=0)throw new Win32Exception(error,"Cannot restore the original process privilege state");}}
  }
  public static IDisposable EnablePrivilege(){return new Scope();}
  public static Value Read(string name){
   byte[] buffer=new byte[131072];uint attributes;
   uint count=GetFirmwareEnvironmentVariableExW(name,Global,buffer,(uint)buffer.Length,out attributes);
   if(count==0){int error=Marshal.GetLastWin32Error();if(error==203)return null;throw new Win32Exception(error,"Cannot read firmware variable "+name);}
   byte[] data=new byte[count];Array.Copy(buffer,data,count);return new Value{Data=data,Attributes=attributes};
  }
  public static void Write(string name,byte[] data,uint attributes){
   if(!SetFirmwareEnvironmentVariableExW(name,Global,data,data==null?0:(uint)data.Length,attributes)){
    int error=Marshal.GetLastWin32Error();
    throw new Win32Exception(error,"Cannot write firmware variable "+name+" (Windows error "+error+"): "+new Win32Exception(error).Message);
   }
  }
 }
}
