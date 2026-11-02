#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
static HANDLE logh;
static void value(const char*s,DWORD v){char b[128];DWORD i,n=0,w;while(*s)b[n++]=*s++;b[n++]=' ';for(i=0;i<8;i++)b[n++]="0123456789ABCDEF"[(v>>(28-i*4))&15];b[n++]='\r';b[n++]='\n';WriteFile(logh,b,n,&w,0);FlushFileBuffers(logh);}
void WINAPI entry(void){HMODULE proxy,native;FARPROC ordinal,named,actual;WSADATA a,b;int x,y;DWORD failures=0;
 int (WINAPI *startup)(WORD,LPWSADATA);int (WINAPI *cleanup)(void);
 logh=CreateFileA("C:\\FP32\\ORDINAL.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
 proxy=LoadLibraryA("C:\\FP32\\F32W.DLL");native=LoadLibraryA("WSOCK32.DLL");
 if(!proxy||!native){value("Load error",GetLastError());ExitProcess(2);}
 ordinal=GetProcAddress(proxy,(LPCSTR)115);named=GetProcAddress(proxy,"WSAStartup");actual=GetProcAddress(native,"WSAStartup");
 value("Ordinal export",(DWORD)ordinal);value("Named export",(DWORD)named);value("Native export",(DWORD)actual);
 if(!ordinal||ordinal!=named||ordinal!=actual){value("Failures",1);ExitProcess(3);}
 startup=(void*)ordinal;x=startup(MAKEWORD(1,1),&a);startup=(void*)actual;y=startup(MAKEWORD(1,1),&b);
 value("Proxy startup result",x);value("Native startup result",y);
 if(x!=y)failures++;
 if(!x&&!y&&a.wVersion!=b.wVersion)failures++;
 cleanup=(void*)GetProcAddress(native,"WSACleanup");if(!x&&cleanup)cleanup();if(!y&&cleanup)cleanup();
 value("Failures",failures);CloseHandle(logh);FreeLibrary(proxy);FreeLibrary(native);ExitProcess(failures?1:0);
}
