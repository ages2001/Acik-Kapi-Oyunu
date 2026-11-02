/* Test guest UI helper: dismiss only the saved crash-report notification.
 * Never send a report, approve a driver dialog, or terminate a process. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static unsigned closed;
static BOOL CALLBACK child(HWND w, LPARAM unused)
{
    char text[80]; (void)unused;
    if (GetWindowTextA(w,text,sizeof(text)) &&
        (!lstrcmpA(text,"Don't Send") || !lstrcmpA(text,"&Don't Send"))) {
        SendMessageA(w,BM_CLICK,0,0); closed++; return FALSE;
    }
    return TRUE;
}
static BOOL CALLBACK top(HWND w, LPARAM unused)
{
    char title[80],type[32]; (void)unused;
    if (GetWindowTextA(w,title,sizeof(title)) && !lstrcmpA(title,"Microsoft Windows") &&
        GetClassNameA(w,type,sizeof(type)) && !lstrcmpA(type,"#32770"))
        EnumChildWindows(w,child,0);
    return TRUE;
}
void Entry(void)
{
    DWORD n; EnumWindows(top,0);
    if(closed)WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),"Dismissed saved crash report; nothing sent.\r\n",44,&n,0);
    ExitProcess(closed?0:1);
}
