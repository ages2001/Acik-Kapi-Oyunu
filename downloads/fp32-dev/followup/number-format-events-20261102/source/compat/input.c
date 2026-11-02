/* Guest UI test helper. Never shipped with the player. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static HANDLE logh;
static HWND target;
static void text(const char*s){DWORD n=0,w;while(s[n])n++;WriteFile(logh,s,n,&w,0);FlushFileBuffers(logh);}
static void value(const char*s,DWORD v){char b[11];int i;const char*h="0123456789ABCDEF";text(s);for(i=0;i<8;i++)b[i]=h[(v>>(28-4*i))&15];b[8]='\r';b[9]='\n';b[10]=0;text(b);}
static BOOL CALLBACK inspect(HWND w,LPARAM param){char c[256],t[256];DWORD pid;GetClassNameA(w,c,256);GetWindowTextA(w,t,256);if(!IsWindowVisible(w))return TRUE;text(c);text(" | ");text(t);text("\r\n");if(c[0]=='S'&&c[1]=='h'&&c[2]=='o'&&c[3]=='c'&&c[4]=='k'){target=w;GetWindowThreadProcessId(w,&pid);value("Player PID ",pid);if(param)PostMessageA(w,WM_CLOSE,0,0);return FALSE;}return TRUE;}
void WINAPI entry(void){
 LPSTR s=GetCommandLineA();int x=0,y=0,close=0,relative=0,posted=0;POINT p,q;
 logh=CreateFileA("C:\\FP10\\WINDOWS.LOG",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,0,CREATE_ALWAYS,0,0);text(s);text("\r\n");
 if(*s=='"'){s++;while(*s&&*s!='"')s++;if(*s)s++;}else while(*s&&*s!=' ')s++;while(*s==' ')s++;
 if(*s=='p'){posted=1;relative=1;s++;while(*s==' ')s++;}
 if(*s=='r'){relative=1;s++;while(*s==' ')s++;}
 if(*s=='c')close=1;
 EnumWindows(inspect,close);if(close||!target){CloseHandle(logh);ExitProcess(target?0:1);}
 while(*s>='0'&&*s<='9')x=x*10+*s++-'0';while(*s==' ')s++;while(*s>='0'&&*s<='9')y=y*10+*s++-'0';
 value("Foreground result ",SetForegroundWindow(target));Sleep(300);p.x=x;p.y=y;if(relative)ClientToScreen(target,&p);
 mouse_event(MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE,(DWORD)p.x*65535/(GetSystemMetrics(SM_CXSCREEN)-1),(DWORD)p.y*65535/(GetSystemMetrics(SM_CYSCREEN)-1),0,0);Sleep(300);GetCursorPos(&q);value("Cursor x ",q.x);value("Cursor y ",q.y);Sleep(300);
 if(posted){PostMessageA(target,WM_MOUSEMOVE,0,MAKELPARAM(x,y));PostMessageA(target,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x,y));}
 else mouse_event(MOUSEEVENTF_LEFTDOWN,0,0,0,0);
 value("Down tick ",GetTickCount());Sleep(500);
 if(posted)PostMessageA(target,WM_LBUTTONUP,0,MAKELPARAM(x,y));else mouse_event(MOUSEEVENTF_LEFTUP,0,0,0,0);
 value("Up tick ",GetTickCount());Sleep(300);value("Async button state ",(DWORD)GetAsyncKeyState(VK_LBUTTON));text("Done\r\n");CloseHandle(logh);ExitProcess(0);
}
