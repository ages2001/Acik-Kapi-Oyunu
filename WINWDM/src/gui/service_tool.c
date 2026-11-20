#include "kit.h"
int PASCAL WinMain(HINSTANCE h,HINSTANCE prev,LPSTR cmd,int show)
{
    char path[PATH_CAP];
    int n=GetWindowsDirectory(path,sizeof(path));
    (void)h;(void)prev;(void)show;
    if (!n || n>PATH_CAP-20 || !WkNtHost()) return 1;
    lstrcat(path,"\\WINWDM\\WINWDM.SYS");
    if (!lstrcmpi(cmd,"install")) return WkService(path,TRUE)?0:1;
    if (!lstrcmpi(cmd,"remove")) return WkService(path,FALSE)?0:1;
    return 1;
}
