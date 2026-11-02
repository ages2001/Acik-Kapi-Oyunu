#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static DWORD checks,failures,destroyed;
static HANDLE logh;
static LONG (WINAPI *seta)(HWND,int,LONG), (WINAPI *setw)(HWND,int,LONG);
static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM b){if(m==WM_DESTROY)destroyed++;if(m==WM_APP)return 0x1234;return DefWindowProcW(w,m,a,b);}
static LRESULT CALLBACK alternate(HWND w,UINT m,WPARAM a,LPARAM b){if(m==WM_APP)return 0x5678;return proc(w,m,a,b);}
static void value(const char*s,DWORD v){char b[128];DWORD i,n=0,w;while(*s)b[n++]=*s++;b[n++]=' ';for(i=0;i<8;i++)b[n++]="0123456789ABCDEF"[(v>>(28-i*4))&15];b[n++]='\r';b[n++]='\n';WriteFile(logh,b,n,&w,0);FlushFileBuffers(logh);}
static void check(BOOL ok,const char*name){checks++;if(!ok){failures++;value(name,GetLastError());}}
void WINAPI entry(void){WNDCLASSW c;HWND w;HMODULE m;LONG original,ansi,old;DWORD i;
 logh=CreateFileA("C:\\FP32\\WNDPROC.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
 m=LoadLibraryA("C:\\FP32\\F32U.DLL");seta=m?(void*)GetProcAddress(m,"SetWindowLongA"):0;setw=m?(void*)GetProcAddress(m,"SetWindowLongW"):0;if(!seta||!setw){value("Load error",GetLastError());ExitProcess(2);}
 for(i=0;i<sizeof(c);i++)((BYTE*)&c)[i]=0;c.hInstance=GetModuleHandleA(0);c.lpfnWndProc=proc;c.lpszClassName=L"FP32WndProcContract";if(!RegisterClassW(&c))ExitProcess(3);
 w=CreateWindowExW(0,c.lpszClassName,L"",WS_POPUP,0,0,8,8,0,0,c.hInstance,0);if(!w)ExitProcess(4);
 original=GetWindowLongW(w,GWL_WNDPROC);SetWindowLongW(w,GWL_WNDPROC,0);value("Native NULL procedure value",GetWindowLongW(w,GWL_WNDPROC));SetWindowLongW(w,GWL_WNDPROC,original);
 check(GetWindowLongW(w,GWL_WNDPROC)==original,"Restore native baseline");
 SetLastError(0xdeadbeef);old=setw(w,GWL_WNDPROC,0);check(old==original&&GetLastError()==0xdeadbeef,"W NULL returns old and preserves error");check(GetWindowLongW(w,GWL_WNDPROC)==original&&IsWindowUnicode(w),"W NULL retains procedure and encoding");
 ansi=GetWindowLongA(w,GWL_WNDPROC);SetLastError(0xdeadbeef);old=seta(w,GWL_WNDPROC,0);check(old==ansi&&GetLastError()==0xdeadbeef,"A NULL returns old and preserves error");check(GetWindowLongW(w,GWL_WNDPROC)==original&&IsWindowUnicode(w),"A NULL retains procedure and encoding");
 check(SendMessageW(w,WM_APP,0,0)==0x1234,"Original procedure invoked");
 setw(w,GWL_USERDATA,123);check(setw(w,GWL_USERDATA,0)==123&&GetWindowLongW(w,GWL_USERDATA)==0,"NULL user data remains writable");
 check(setw(w,GWL_WNDPROC,(LONG)alternate)==original&&SendMessageW(w,WM_APP,0,0)==0x5678,"Non-NULL subclass preserved");
 check(!setw(0,GWL_WNDPROC,0)&&GetLastError()==ERROR_INVALID_WINDOW_HANDLE,"Invalid window rejected");
 check(DestroyWindow(w)&&destroyed==1,"Teardown delivered exactly once");UnregisterClassW(c.lpszClassName,c.hInstance);
 value("Checks",checks);value("Failures",failures);CloseHandle(logh);FreeLibrary(m);ExitProcess(failures?1:0);
}
