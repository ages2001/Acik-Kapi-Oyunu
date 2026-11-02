/* Native test input: screen-coordinate mouse clicks, never posted UI messages. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static DWORD number(char**s){DWORD n=0;while(**s==' ')(*s)++;while(**s>='0'&&**s<='9')n=n*10+*(*s)++-'0';return n;}
void WINAPI entry(void){char*s=GetCommandLineA();DWORD x,y,count,i;if(*s=='"'){s++;while(*s&&*s!='"')s++;if(*s)s++;}else while(*s&&*s!=' ')s++;x=number(&s);y=number(&s);count=number(&s);if(!count)count=1;if(count>2||x>=(DWORD)GetSystemMetrics(SM_CXSCREEN)||y>=(DWORD)GetSystemMetrics(SM_CYSCREEN))ExitProcess(1);
 mouse_event(MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE,x*65535/(GetSystemMetrics(SM_CXSCREEN)-1),y*65535/(GetSystemMetrics(SM_CYSCREEN)-1),0,0);Sleep(150);
 for(i=0;i<count;i++){mouse_event(MOUSEEVENTF_LEFTDOWN,0,0,0,0);Sleep(50);mouse_event(MOUSEEVENTF_LEFTUP,0,0,0,0);Sleep(50);}ExitProcess(0);
}
