#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifdef TEST_VERSION_DLL
/* Exercise the linked production proxy, not an inlined copy of the adapter. */
static HMODULE proxy;
static BOOL WINAPI fp_GetVersionExA(LPOSVERSIONINFOA out) {
 typedef BOOL (WINAPI *Query)(LPOSVERSIONINFOA);
 Query query=(Query)GetProcAddress(proxy,"GetVersionExA");
 return query ? query(out) : FALSE;
}
static BOOL WINAPI fp_GetVersionExW(LPOSVERSIONINFOW out) {
 typedef BOOL (WINAPI *Query)(LPOSVERSIONINFOW);
 Query query=(Query)GetProcAddress(proxy,"GetVersionExW");
 return query ? query(out) : FALSE;
}
#elif defined(TEST_OLD_VERSION)
#include "../build/version-before.h"
#else
#include "../../compat/version_compat.h"
#endif
static DWORD checks,errors,used;static char report[4096];
static void check(BOOL ok,const char *name){checks++;if(!ok){errors++;used+=wsprintfA(report+used,"FAIL: %s (error %lu)\r\n",name,GetLastError());}}
static BOOL equalw(const WCHAR*a,const WCHAR*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
void WINAPI entry(void){
 typedef BOOL(WINAPI*NativeA)(LPOSVERSIONINFOA);
 NativeA native=(NativeA)GetProcAddress(GetModuleHandleA("KERNEL32.DLL"),"GetVersionExA");
 struct {OSVERSIONINFOA info;DWORD guard;} a;
 struct {OSVERSIONINFOW info;DWORD guard;} w;
 OSVERSIONINFOA control;OSVERSIONINFOEXA exa;OSVERSIONINFOEXW exw;WCHAR csd[128];
 DWORD raw=GetVersion(),n;BOOL have;HANDLE file;
#ifdef TEST_VERSION_DLL
 proxy=LoadLibraryA("C:\\FP32\\FVTEST.DLL");
 check(proxy!=0,"production proxy loads");
#endif
 control.dwOSVersionInfoSize=sizeof(control);have=native&&native(&control);
 if(!have){control.dwMajorVersion=LOBYTE(LOWORD(raw));control.dwMinorVersion=HIBYTE(LOWORD(raw));
  control.dwPlatformId=(raw&0x80000000u)?(control.dwMajorVersion<4?0:1):2;
  control.dwBuildNumber=control.dwPlatformId==2?HIWORD(raw):0;control.szCSDVersion[0]=0;}
 a.info.dwOSVersionInfoSize=sizeof(a.info);a.guard=0x12345678;
 w.info.dwOSVersionInfoSize=sizeof(w.info);w.guard=0x12345678;
 check(fp_GetVersionExA(&a.info),"ANSI query");check(fp_GetVersionExW(&w.info),"Unicode query");
 check(a.guard==0x12345678&&w.guard==0x12345678,"standard structure boundaries");
 check(a.info.dwPlatformId==control.dwPlatformId,"ANSI platform matches OS");
 check(w.info.dwPlatformId==control.dwPlatformId,"Unicode platform matches OS");
 check(a.info.dwBuildNumber==control.dwBuildNumber&&w.info.dwBuildNumber==control.dwBuildNumber,"build number matches OS");
 check(a.info.dwMajorVersion==control.dwMajorVersion&&w.info.dwMajorVersion==control.dwMajorVersion,"major version");
 check(a.info.dwMinorVersion==control.dwMinorVersion&&w.info.dwMinorVersion==control.dwMinorVersion,"minor version");
 check(!lstrcmpA(a.info.szCSDVersion,control.szCSDVersion),"ANSI CSD text preserved");
 MultiByteToWideChar(CP_ACP,0,control.szCSDVersion,-1,csd,128);
 check(equalw(w.info.szCSDVersion,csd),"Unicode CSD text preserved");
 exa.dwOSVersionInfoSize=sizeof(exa);exw.dwOSVersionInfoSize=sizeof(exw);
 check(fp_GetVersionExA((LPOSVERSIONINFOA)&exa),"extended ANSI query");
 check(fp_GetVersionExW((LPOSVERSIONINFOW)&exw),"extended Unicode query");
 check(exa.dwPlatformId==control.dwPlatformId&&exw.dwPlatformId==control.dwPlatformId,"extended platform");
 check(exa.dwBuildNumber==control.dwBuildNumber&&exw.dwBuildNumber==control.dwBuildNumber,"extended build");
 check(!fp_GetVersionExA(0)&&GetLastError()==ERROR_INSUFFICIENT_BUFFER,"NULL ANSI rejected");
 check(!fp_GetVersionExW(0)&&GetLastError()==ERROR_INSUFFICIENT_BUFFER,"NULL Unicode rejected");
 a.info.dwOSVersionInfoSize=1;w.info.dwOSVersionInfoSize=1;
 check(!fp_GetVersionExA(&a.info)&&GetLastError()==ERROR_INSUFFICIENT_BUFFER,"invalid ANSI size rejected");
 check(!fp_GetVersionExW(&w.info)&&GetLastError()==ERROR_INSUFFICIENT_BUFFER,"invalid Unicode size rejected");
 used+=wsprintfA(report+used,"Raw GetVersion: %08lX\r\nNative GetVersionExA available: %lu\r\nOS: %lu.%lu build %lu platform %lu\r\nAdapter: %lu.%lu build %lu platform %lu\r\nChecks: %lu\r\nErrors: %lu\r\n",raw,(DWORD)have,control.dwMajorVersion,control.dwMinorVersion,control.dwBuildNumber,control.dwPlatformId,exa.dwMajorVersion,exa.dwMinorVersion,exa.dwBuildNumber,exa.dwPlatformId,checks,errors);
 file=CreateFileA("C:\\FP32\\VERSION.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
 if(file!=INVALID_HANDLE_VALUE){WriteFile(file,report,used,&n,0);CloseHandle(file);}
 MessageBoxA(0,report,"Version adapter regression",MB_OK|(errors?MB_ICONSTOP:MB_ICONINFORMATION));ExitProcess(errors?1:0);
}
