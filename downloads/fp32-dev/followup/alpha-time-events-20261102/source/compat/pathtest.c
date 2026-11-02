#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static HANDLE logh;static int failures,total;
static void check(int ok,const char*s){DWORD n=0,w;total++;if(!ok)failures++;WriteFile(logh,ok?"PASS ":"FAIL ",5,&w,0);while(s[n])n++;WriteFile(logh,s,n,&w,0);WriteFile(logh,"\r\n",2,&w,0);}
void WINAPI entry(void){HMODULE m=LoadLibraryA("C:\\FP10\\TRACE\\FPK.DLL");HANDLE(WINAPI*f)(LPCWSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE)=(void*)GetProcAddress(m,"CreateFileW");HANDLE h;char b[4];DWORD n;logh=CreateFileA("C:\\FP10\\PATH.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(!f)ExitProcess(2);
 h=f(L"\\\\?\\C:\\FP10\\PATH.DAT",GENERIC_READ|GENERIC_WRITE,0,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);check(h!=INVALID_HANDLE_VALUE,"create with extended absolute DOS path");if(h!=INVALID_HANDLE_VALUE){WriteFile(h,"data",4,&n,0);CloseHandle(h);}
 h=CreateFileW(L"C:\\FP10\\PATH.DAT",GENERIC_READ,0,0,OPEN_EXISTING,0,0);check(h!=INVALID_HANDLE_VALUE,"native path reaches same file");if(h!=INVALID_HANDLE_VALUE){check(ReadFile(h,b,4,&n,0)&&n==4&&b[0]=='d'&&b[3]=='a',"written bytes preserved");CloseHandle(h);}
 h=f(L"\\\\?\\C:\\FP10\\ABSENT.XYZ",GENERIC_READ,0,0,OPEN_EXISTING,0,0);check(h==INVALID_HANDLE_VALUE&&GetLastError()==ERROR_FILE_NOT_FOUND,"missing file retains honest error");
 h=f(L"\\\\?\\C:\\FP10\\PATH.DAT.",GENERIC_READ,0,0,OPEN_EXISTING,0,0);check(h==INVALID_HANDLE_VALUE,"trailing dot never aliases ordinary file");if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);
 h=f(L"\\\\?\\C:\\FP10\\..\\FP10\\PATH.DAT",GENERIC_READ,0,0,OPEN_EXISTING,0,0);check(h==INVALID_HANDLE_VALUE,"dot segments not silently normalized");if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);
 h=f(L"C:\\FP10\\PATH.DAT",GENERIC_READ,0,0,OPEN_EXISTING,0,0);check(h!=INVALID_HANDLE_VALUE,"ordinary paths still open");if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);
 DeleteFileA("C:\\FP10\\PATH.DAT");CloseHandle(logh);MessageBoxA(0,failures?"Path checks FAILED: see PATH.LOG":"7 path checks PASSED","NT 3.1 file path regression",0);ExitProcess(failures?1:0);}
