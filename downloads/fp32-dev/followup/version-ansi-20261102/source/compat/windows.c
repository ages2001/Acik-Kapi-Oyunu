#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static HANDLE logh;
static void text(const char*s){DWORD n=0,w;while(s[n])n++;WriteFile(logh,s,n,&w,0);}
static void value(const char*s,DWORD v){char b[11];DWORD i;const char*h="0123456789ABCDEF";text(s);for(i=0;i<8;i++)b[i]=h[(v>>(28-i*4))&15];b[8]='\r';b[9]='\n';b[10]=0;text(b);}
static BOOL CALLBACK show(HWND w,LPARAM p){char c[256],t[256];RECT r;DWORD pid;GetClassNameA(w,c,256);GetWindowTextA(w,t,256);GetWindowThreadProcessId(w,&pid);text(c);text(" | ");text(t);text("\r\n");value("HWND ",(DWORD)w);value("PID ",pid);value("Visible ",IsWindowVisible(w));value("Enabled ",IsWindowEnabled(w));value("Owner ",(DWORD)GetWindow(w,GW_OWNER));value("Popup ",(DWORD)GetLastActivePopup(w));GetWindowRect(w,&r);value("Left ",r.left);value("Top ",r.top);value("Right ",r.right);value("Bottom ",r.bottom);return TRUE;}
void WINAPI entry(void){logh=CreateFileA("C:\\FP10\\ALLWIN.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);EnumWindows(show,0);value("Foreground ",(DWORD)GetForegroundWindow());value("Capture ",(DWORD)GetCapture());FlushFileBuffers(logh);CloseHandle(logh);ExitProcess(0);}
