#define WIN32_LEAN_AND_MEAN
#include <windows.h>
/* Native capabilities only; this program is not Flash Player. */
static char report[1800];static DWORD used;
static void line(const char*s,DWORD value){const char*hex="0123456789ABCDEF";int i;while(*s)report[used++]=*s++;report[used++]=' ';for(i=0;i<8;i++)report[used++]=hex[(value>>(28-4*i))&15];report[used++]='\r';report[used++]='\n';report[used]=0;}
static DWORD WINAPI thread(void*p){return 67;}
void WINAPI entry(void){HMODULE kernel=GetModuleHandleA("KERNEL32.DLL");HANDLE h,f;DWORD tid=0,exitcode=0,n,a=1,b,c,d;OSVERSIONINFOA v;BOOL(WINAPI*version)(LPOSVERSIONINFOA);HANDLE(WINAPI*filew)(LPCWSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE);DWORD i,error;
 {SYSTEM_INFO system;GetSystemInfo(&system);line("Logical processors",system.dwNumberOfProcessors);line("Active processor mask",system.dwActiveProcessorMask);}
 line("GetVersion",GetVersion());version=(void*)GetProcAddress(kernel,"GetVersionExA");for(i=0;i<sizeof(v);i++)((BYTE*)&v)[i]=0;v.dwOSVersionInfoSize=sizeof(v);if(version&&version(&v)){line("Platform ID",v.dwPlatformId);line("OS major",v.dwMajorVersion);line("OS minor",v.dwMinorVersion);}
 SetLastError(0);h=CreateThread(0,0,thread,0,0,&tid);error=GetLastError();line("CreateThread handle",(DWORD)h);line("CreateThread error",error);if(h){line("Thread wait",WaitForSingleObject(h,2000));GetExitCodeThread(h,&exitcode);line("Thread result",exitcode);CloseHandle(h);}
 filew=(void*)GetProcAddress(kernel,"CreateFileW");SetLastError(0);h=filew?filew(L"C:\\AUTOEXEC.BAT",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_EXISTING,0,0):INVALID_HANDLE_VALUE;error=GetLastError();line("CreateFileW export",(DWORD)filew);line("CreateFileW handle",(DWORD)h);line("CreateFileW error",error);if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);
 __asm__ volatile("cpuid":"+a"(a),"=b"(b),"=c"(c),"=d"(d));line("CPUID EDX",d);line("CPU SSE2 bit",(d>>26)&1);
 f=CreateFileA("C:\\F32CAPS.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(f!=INVALID_HANDLE_VALUE){WriteFile(f,report,used,&n,0);CloseHandle(f);}
 MessageBoxA(0,report,"FP32 platform capability probe - not the player",MB_OK);ExitProcess(0);
}
