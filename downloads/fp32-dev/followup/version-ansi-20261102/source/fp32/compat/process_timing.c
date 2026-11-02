/* Read-only native process CPU-time sampling for the current player. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static DWORD pid;static HANDLE logh;
static void value(const char*s,DWORD v){char b[160],n[16];DWORD i=0,j=0,w;while(*s)b[i++]=*s++;do{n[j++]='0'+v%10;v/=10;}while(v);while(j)b[i++]=n[--j];b[i++]='\r';b[i++]='\n';WriteFile(logh,b,i,&w,0);}
static BOOL CALLBACK find(HWND w,LPARAM unused){char c[64];GetClassNameA(w,c,sizeof(c));if(IsWindowVisible(w)&&lstrcmpA(c,"ShockwaveFlash")==0){GetWindowThreadProcessId(w,&pid);return FALSE;}return TRUE;}
void WINAPI entry(void){HANDLE p;FILETIME c,e,k0,u0,k1,u1;DWORD t0,t1;LARGE_INTEGER q0,q1,f;
 logh=CreateFileA("C:\\FP32\\PERF10.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(logh==INVALID_HANDLE_VALUE)ExitProcess(1);
 EnumWindows(find,0);if(!pid){value("No player ",1);CloseHandle(logh);ExitProcess(2);}p=OpenProcess(PROCESS_QUERY_INFORMATION,FALSE,pid);if(!p){value("Open error ",GetLastError());CloseHandle(logh);ExitProcess(3);}
 if(!GetProcessTimes(p,&c,&e,&k0,&u0)||!QueryPerformanceFrequency(&f)||!QueryPerformanceCounter(&q0)){value("Initial sample error ",GetLastError());ExitProcess(4);}t0=GetTickCount();Sleep(10000);t1=GetTickCount();
 if(!GetProcessTimes(p,&c,&e,&k1,&u1)||!QueryPerformanceCounter(&q1)){value("Final sample error ",GetLastError());ExitProcess(5);}
 value("PID ",pid);value("Elapsed ms ",t1-t0);value("Kernel ms ",(k1.dwLowDateTime-k0.dwLowDateTime)/10000);value("User ms ",(u1.dwLowDateTime-u0.dwLowDateTime)/10000);value("QPC delta low ",q1.LowPart-q0.LowPart);value("QPC frequency low ",f.LowPart);value("QPC frequency high ",f.HighPart);CloseHandle(p);CloseHandle(logh);ExitProcess(0);
}
