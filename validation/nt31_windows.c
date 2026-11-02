/* Read-only NT3.1 window inventory for installer diagnostics. */
#include <windows.h>
static HANDLE log;
static void out(const char *s){DWORD n;WriteFile(log,s,lstrlenA(s),&n,0);}
static void hex(DWORD n){char b[9];unsigned i;for(i=0;i<8;i++)b[i]="0123456789abcdef"[(n>>(28-i*4))&15];b[8]=0;out(b);}
static void row(HWND w,const char *prefix){char title[512],type[128];out(prefix);hex((DWORD)w);out(IsWindowVisible(w)?" visible ":" hidden ");out(IsWindowEnabled(w)?"enabled ":"disabled ");GetClassNameA(w,type,sizeof(type));GetWindowTextA(w,title,sizeof(title));out(type);out(" | ");out(title);out("\r\n");}
static BOOL CALLBACK child(HWND w,LPARAM unused){(void)unused;row(w," child ");return TRUE;}
static BOOL CALLBACK top(HWND w,LPARAM unused){(void)unused;row(w,"TOP ");EnumChildWindows(w,child,0);return TRUE;}
void Entry(void){log=CreateFileA("C:\\RLTTEST\\WINDOWS.TXT",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);if(log==INVALID_HANDLE_VALUE)ExitProcess(1);EnumWindows(top,0);CloseHandle(log);ExitProcess(0);}
