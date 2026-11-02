/* Import bridge for the original NT3.51 multimedia provider. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static volatile LONG resolution_requested;
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,void*reserved){return TRUE;}
BOOL WINAPI fp_DisableThreadLibraryCalls(HMODULE module)
{
 typedef BOOL(WINAPI*Native)(HMODULE);Native f=(void*)GetProcAddress(GetModuleHandleA("KERNEL32.DLL"),"DisableThreadLibraryCalls");
 if(f)return f(module);SetLastError(ERROR_CALL_NOT_IMPLEMENTED);return FALSE;
}
static LONG resolution(ULONG *actual)
{
 typedef LONG(WINAPI*Query)(ULONG,void*,ULONG,ULONG*);DWORD basic[11],length=0;LONG status;
 Query f=(void*)GetProcAddress(GetModuleHandleA("NTDLL.DLL"),"NtQuerySystemInformation");
 if(!actual)return (LONG)0xc000000d;if(!f)return (LONG)0xc0000002;
 status=f(0,basic,sizeof(basic),&length);if(status<0)return status;
 if(length!=sizeof(basic)||!basic[1])return (LONG)0xc0000004;
 *actual=basic[1];return 0;
}
LONG WINAPI fp_NtQueryTimerResolution(ULONG *minimum,ULONG *maximum,ULONG *current)
{
 typedef LONG(WINAPI*Native)(ULONG*,ULONG*,ULONG*);Native f=(void*)GetProcAddress(GetModuleHandleA("NTDLL.DLL"),"NtQueryTimerResolution");ULONG value;LONG status;
 if(f)return f(minimum,maximum,current);
 if(!minimum||!maximum||!current)return (LONG)0xc000000d;
 status=resolution(&value);if(status<0)return status;
 /* NT3.1's original timeBeginPeriod accepts a range but cannot adjust the
    kernel clock. Report the real fixed clock as both resolution limits. */
 *minimum=*maximum=*current=value;return 0;
}
LONG WINAPI fp_NtSetTimerResolution(ULONG desired,BOOLEAN set,ULONG *actual)
{
 typedef LONG(WINAPI*Native)(ULONG,BOOLEAN,ULONG*);Native f=(void*)GetProcAddress(GetModuleHandleA("NTDLL.DLL"),"NtSetTimerResolution");LONG status;
 if(f)return f(desired,set,actual);
 status=resolution(actual);if(status<0)return status;
 if(set){InterlockedExchange(&resolution_requested,1);return 0;}
 return InterlockedExchange(&resolution_requested,0)?0:(LONG)0xc0000245;
}
LONG WINAPI fp_NtCreateTimer(HANDLE *timer,ULONG access,void *attributes)
{
 typedef LONG(WINAPI*Old)(HANDLE*,ULONG,void*);typedef LONG(WINAPI*New)(HANDLE*,ULONG,void*,ULONG);
 FARPROC f=GetProcAddress(GetModuleHandleA("NTDLL.DLL"),"NtCreateTimer");
 if((GetVersion()&255)>=4)return ((New)f)(timer,access,attributes,0);return ((Old)f)(timer,access,attributes);
}
LONG WINAPI fp_NtSetTimer(HANDLE timer,LARGE_INTEGER *due,void *apc,void *context,BOOLEAN resume,BOOLEAN *previous)
{
 typedef LONG(WINAPI*NT31)(HANDLE,LARGE_INTEGER*,void*,void*,BOOLEAN*);
 typedef LONG(WINAPI*NT35)(HANDLE,LARGE_INTEGER*,void*,void*,BOOLEAN,BOOLEAN*);
 typedef LONG(WINAPI*NT4)(HANDLE,LARGE_INTEGER*,void*,void*,BOOLEAN,LONG,BOOLEAN*);
 FARPROC f=GetProcAddress(GetModuleHandleA("NTDLL.DLL"),"NtSetTimer");DWORD version=GetVersion()&0xffff;
 if(version==0x0a03){
  /* NT3.51 argument five is Resume, not Period. Its original kernel
     rejects wake requests; NT3.1 has no such parameter or power support. */
  if(resume)return (LONG)0xc0000002;
  return ((NT31)f)(timer,due,apc,context,previous);
 }
 if((version&255)>=4)return ((NT4)f)(timer,due,apc,context,resume,0,previous);
 return ((NT35)f)(timer,due,apc,context,resume,previous);
}
