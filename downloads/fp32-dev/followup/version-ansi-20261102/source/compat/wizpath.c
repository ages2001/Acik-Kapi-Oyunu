#define WIN32_LEAN_AND_MEAN
#include <windows.h>
void WINAPI entry(void){HWND w=FindWindowA("FP10SetupWizard",0),edit;char*s=GetCommandLineA(),actual[MAX_PATH];int i;if(!w)ExitProcess(1);if(*s=='"'){s++;while(*s&&*s!='"')s++;if(*s)s++;}else while(*s&&*s!=' ')s++;while(*s==' ')s++;edit=GetDlgItem(w,1003);if(!edit||!IsWindowVisible(edit))ExitProcess(2);
 /* SetWindowText cannot update another process's edit control. Marshal the
    standard control message, then verify the actual edit buffer. */
 if(!SendMessageA(edit,WM_SETTEXT,0,(LPARAM)s))ExitProcess(3);
 SendMessageA(edit,WM_GETTEXT,sizeof(actual),(LPARAM)actual);for(i=0;s[i]&&actual[i]==s[i];i++){}if(actual[i]!=s[i])ExitProcess(4);
 SetForegroundWindow(w);ExitProcess(0);}
