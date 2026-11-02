/* Disposable NT timer-resolution fixture. No production installation changes. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef LONG (WINAPI *QUERY_RES)(ULONG *,ULONG *,ULONG *);
typedef LONG (WINAPI *SET_RES)(ULONG,BOOLEAN,ULONG *);
static SET_RES SetRes;
static BOOL held;
static const char cls[]="RltkTimerFixture";
static void zero(void*p,unsigned n){volatile BYTE*b=p;while(n--)*b++=0;}
static void out(const char*s){DWORD n;HANDLE f=CreateFileA("C:\\RLTTEST\\TIMER.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,OPEN_ALWAYS,0,0);WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,lstrlenA(s),&n,0);if(f!=INVALID_HANDLE_VALUE){SetFilePointer(f,0,0,FILE_END);WriteFile(f,s,lstrlenA(s),&n,0);CloseHandle(f);}}
static void number(ULONG n){char b[16];unsigned i=15;b[i]=0;do{b[--i]=(char)('0'+n%10);n/=10;}while(n);out(b+i);}
static void query(const char*tag){ULONG lo=0,hi=0,cur=0;QUERY_RES fn=(QUERY_RES)GetProcAddress(GetModuleHandleA("ntdll.dll"),"NtQueryTimerResolution");out(tag);if(!fn||fn(&lo,&hi,&cur)){out(" QUERY FAILED\r\n");ExitProcess(2);}out(" MIN100NS ");number(lo);out(" MAX100NS ");number(hi);out(" CURRENT100NS ");number(cur);out("\r\n");}
static LRESULT CALLBACK wnd(HWND w,UINT m,WPARAM a,LPARAM b){ULONG cur;if(m==WM_CLOSE){if(held){SetRes(10000,FALSE,&cur);held=FALSE;}DestroyWindow(w);return 0;}if(m==WM_DESTROY){PostQuitMessage(0);return 0;}return DefWindowProcA(w,m,a,b);}
static BOOL has(const char*s,const char*word){unsigned i,j;for(i=0;s[i];i++){for(j=0;word[j]&&s[i+j]==word[j];j++);if(!word[j])return TRUE;}return FALSE;}
static void stop(const char*name,UINT message){unsigned i;HWND w;for(i=0;i<100&&(w=FindWindowA(name,0))!=NULL;i++){PostMessageA(w,message,0,0);Sleep(20);}if(FindWindowA(name,0)){out("STOP FAILED\r\n");ExitProcess(3);}}
void Entry(void){const char*cmd=GetCommandLineA();ULONG cur;unsigned i;HWND w;
 SetRes=(SET_RES)GetProcAddress(GetModuleHandleA("ntdll.dll"),"NtSetTimerResolution");
 if(has(cmd,"/server")){WNDCLASSA wc;MSG msg;zero(&wc,sizeof(wc));wc.lpfnWndProc=wnd;wc.hInstance=GetModuleHandleA(0);wc.lpszClassName=cls;
  if(!SetRes||SetRes(10000,TRUE,&cur)){out("SET FAILED\r\n");ExitProcess(4);}held=TRUE;
  if(!RegisterClassA(&wc)||(w=CreateWindowA(cls,"Timer fixture",0,0,0,0,0,0,0,wc.hInstance,0))==NULL){SetRes(10000,FALSE,&cur);ExitProcess(5);}
  query("HELD");while(GetMessageA(&msg,0,0,0)>0){TranslateMessage(&msg);DispatchMessageA(&msg);}ExitProcess(0);
 }
 if(has(cmd,"/coarse")){stop("UsbNtMonitor",WM_USER+12);stop(cls,WM_CLOSE);Sleep(200);query("COARSE");}
 else if(has(cmd,"/fast")){STARTUPINFOA si;PROCESS_INFORMATION pi;char launch[]="C:\\RLTTEST\\TIMECTL.EXE /server";zero(&si,sizeof(si));si.cb=sizeof(si);zero(&pi,sizeof(pi));
  if(!FindWindowA(cls,0)){if(!CreateProcessA(0,launch,0,0,FALSE,DETACHED_PROCESS,0,0,&si,&pi)){out("SPAWN FAILED\r\n");ExitProcess(6);}CloseHandle(pi.hThread);CloseHandle(pi.hProcess);}
  for(i=0;i<100&&!FindWindowA(cls,0);i++)Sleep(20);if(!FindWindowA(cls,0))ExitProcess(7);query("FAST");
 }else if(has(cmd,"/stop")){stop(cls,WM_CLOSE);query("STOPPED");}else query("QUERY");
 ExitProcess(0);
}
