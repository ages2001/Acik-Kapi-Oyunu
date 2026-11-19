#include <windows.h>
#include <conio.h>
#include <string.h>
static char data[220],title[160];static FARPROC child_cb,top_cb;static int found;
static void line(const char *s){while(*s)outp(0xe9,*s++);outp(0xe9,'\n');}
BOOL FAR PASCAL __export __loadds ver_child(HWND w,LPARAM unused){
    (void)unused;
    GetWindowText(w,data,sizeof(data));if(data[0] && IsWindowVisible(w))line(data);return TRUE;
}
BOOL FAR PASCAL __export __loadds ver_top(HWND w,LPARAM unused){
    (void)unused;GetWindowText(w,title,sizeof(title));
    if (IsWindowVisible(w) && (strstr(title,"About") || strstr(title,"Windows Version"))) {
        line(title);EnumChildWindows(w,(WNDENUMPROC)child_cb,0);found=1;
    }
    return TRUE;
}
int PASCAL WinMain(HINSTANCE h,HINSTANCE pr,LPSTR cmd,int show){
    MSG m;DWORD t;int i;(void)pr;(void)cmd;(void)show;
    child_cb=MakeProcInstance((FARPROC)ver_child,h);top_cb=MakeProcInstance((FARPROC)ver_top,h);
    WinExec("C:\\SHELLWIN.EXE",SW_SHOWNORMAL);
    for(i=0;i<40 && !found;i++){
        t=GetTickCount();while(GetTickCount()-t<350){
            while(PeekMessage(&m,0,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessage(&m);}Yield();
        }
        EnumWindows((WNDENUMPROC)top_cb,0);
    }
    if(found){t=GetTickCount();while(GetTickCount()-t<6000){while(PeekMessage(&m,0,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessage(&m);}Yield();}}
    line("W31PROBE end");FreeProcInstance(child_cb);FreeProcInstance(top_cb);return !found;
}
