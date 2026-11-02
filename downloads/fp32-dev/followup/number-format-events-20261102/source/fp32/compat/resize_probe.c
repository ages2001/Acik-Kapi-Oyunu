/* Diagnostic: change only the native player client size for a rendering A/B test. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static HWND player;
static BOOL CALLBACK find(HWND w,LPARAM unused){char c[64];(void)unused;
 GetClassNameA(w,c,sizeof(c));if(IsWindowVisible(w)&&!lstrcmpA(c,"ShockwaveFlash")){player=w;return FALSE;}return TRUE;}
void WINAPI entry(void){RECT r;char *p=GetCommandLineA();int small=0;
 while(*p){if(*p=='/' && p[1]=='s')small=1;p++;}
 EnumWindows(find,0);if(!player)ExitProcess(2);
 r.left=r.top=0;r.right=small?225:450;r.bottom=small?150:300;
 if(!AdjustWindowRectEx(&r,GetWindowLongA(player,GWL_STYLE),GetMenu(player)!=0,
                         GetWindowLongA(player,GWL_EXSTYLE)))ExitProcess(3);
 if(!SetWindowPos(player,0,0,0,r.right-r.left,r.bottom-r.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE))ExitProcess(4);
 ExitProcess(0);
}
