/* Guest-only native UI command helper. Never part of the player payload. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static DWORD number(char**s){DWORD n=0;while(**s==' ')(*s)++;while(**s>='0'&&**s<='9')n=n*10+*(*s)++-'0';return n;}
void WINAPI entry(void){char*s=GetCommandLineA();DWORD window,msg,wp,lp;if(*s=='"'){s++;while(*s&&*s!='"')s++;if(*s)s++;}else while(*s&&*s!=' ')s++;window=number(&s);msg=number(&s);wp=number(&s);lp=number(&s);if(!IsWindow((HWND)window))ExitProcess(1);SetForegroundWindow((HWND)window);if(msg&&!PostMessageA((HWND)window,msg,wp,lp))ExitProcess(2);ExitProcess(0);}
