/* Start the producer followed by the observer from the normal load= list.
   The producer deliberately exits with allocated private movable objects. */
#include <windows.h>
#include <conio.h>

static void text(const char *p) {
    while (*p) outp(0xe9, *p++);
}
static void number(DWORD n) {
    char digits[12];
    unsigned i=0;
    do { digits[i++]=(char)('0'+n%10); n/=10; } while(n);
    while(i) outp(0xe9,digits[--i]);
    text("\n");
}
static DWORD available(void) {
    DWORD result;
    _asm {
        mov ax,0d74fh
        mov bx,05856h
        int 2fh
        mov result,edx
    }
    return result;
}
int PASCAL WinMain(HINSTANCE h,HINSTANCE prev,LPSTR cmd,int show) {
    unsigned i;
#ifdef OBSERVE_EXIT
    MSG msg;
    UINT timer;
#endif
    (void)h; (void)prev; (void)cmd; (void)show;
#ifdef OBSERVE_EXIT
    timer = SetTimer(NULL,0,500,NULL);
    i=0;
    while(timer && i<20 && GetMessage(&msg,NULL,0,0)) {
        if(msg.message == WM_TIMER) ++i;
        else { TranslateMessage(&msg); DispatchMessage(&msg); }
    }
    if(timer) KillTimer(NULL,timer);
    text("After task exit and collection KiB="); number(available()); text("TASK EXIT END\n");
#else
    text("Before task KiB="); number(available());
    for(i=0;i<256;++i) {
        HGLOBAL obj=GlobalAlloc(GMEM_MOVEABLE,32768UL);
        BYTE FAR *p;
        if(!obj) break;
        p=GlobalLock(obj);
        if(!p) break;
        p[0]=(BYTE)i;
        GlobalUnlock(obj);
    }
    text("Abandoned objects="); number(i);
    text("While task holds objects KiB="); number(available());
#endif
    return 0;
}
