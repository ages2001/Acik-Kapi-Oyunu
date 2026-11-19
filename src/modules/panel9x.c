#include <windows.h>
#include <conio.h>
#include <string.h>

static char line[200], text[160], general[160], performance[160];
static HFILE file;
static HWND dialog;
static void log_text(const char *s) {
    _lwrite(file, s, lstrlen(s));
    while (*s) outp(0xe9, *s++);
}
static void pump(DWORD ms) {
    MSG msg;
    DWORD start=GetTickCount();
    while (GetTickCount()-start<ms) {
        while (PeekMessage(&msg,0,0,0,PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        Yield();
    }
}
static BOOL FAR PASCAL child(HWND wnd,LPARAM phase) {
    if (!IsWindowVisible(wnd)) return TRUE;
    GetWindowText(wnd,text,sizeof(text));
    if (strstr(text,"RAM")) {
        wsprintf(line,"Visible control = %s\r\n",(LPSTR)text);
        log_text(line);
        lstrcpyn(phase?performance:general,text,sizeof(general));
    }
    return TRUE;
}
int PASCAL WinMain(HINSTANCE h,HINSTANCE prev,LPSTR cmd,int show) {
    unsigned i;DWORD bytes;FARPROC callback;
    (void)prev;(void)cmd;(void)show;
    file=_lcreat("C:\\PANEL.LOG",0);
    bytes=GetFreeSpace(0x1794);
    wsprintf(line,"KERNEL.169(0x1794) = %lu bytes\r\n",bytes);log_text(line);
    callback=MakeProcInstance((FARPROC)child,h);
    WinExec("CONTROL.EXE SYSDM.CPL",SW_SHOWNORMAL);
    for (i=0;i<40;i++) {
        pump(320);
        dialog=FindWindow(NULL,"System Properties");
        if(dialog){EnumChildWindows(dialog,(WNDENUMPROC)callback,0);if(general[0])break;}
    }
    if(dialog){
        pump(2200);
        SendMessage(dialog,WM_USER+101,3,0);
        for (i=0;i<18;i++) {
            pump(250);EnumChildWindows(dialog,(WNDENUMPROC)callback,1);
            if(performance[0] && strcmp(performance,general))break;
        }
        pump(1500);
    }
    wsprintf(line,"General = %s\r\n",(LPSTR)general);log_text(line);
    wsprintf(line,"Performance = %s\r\n",(LPSTR)performance);log_text(line);
    log_text("W31PROBE end\r\n");_lclose(file);
    FreeProcInstance(callback);
    return 0;
}
