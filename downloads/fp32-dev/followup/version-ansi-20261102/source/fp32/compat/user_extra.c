#define WIN32_LEAN_AND_MEAN
#include <windows.h>
/* Later USER32 treats a NULL GWL_WNDPROC assignment as a no-op returning the
   old procedure. NT 3.1 installs NULL and crashes during DestroyWindow.
   FP32 performs this assignment in its helper-window teardown at 0x7487fa. */
static LONG null_window_proc(HWND window,BOOL wide)
{
    DWORD saved=GetLastError(),error;LONG old;
    SetLastError(0);
    old=wide?GetWindowLongW(window,GWL_WNDPROC):GetWindowLongA(window,GWL_WNDPROC);
    error=GetLastError();
    SetLastError(old||!error?saved:error);
    return old;
}
LONG WINAPI fp_SetWindowLongW(HWND window,int index,LONG value)
{
    if(index==GWL_WNDPROC&&!value)return null_window_proc(window,TRUE);
    return SetWindowLongW(window,index,value);
}
LONG WINAPI fp_SetWindowLongA(HWND window,int index,LONG value)
{
    if(index==GWL_WNDPROC&&!value)return null_window_proc(window,FALSE);
    return SetWindowLongA(window,index,value);
}
