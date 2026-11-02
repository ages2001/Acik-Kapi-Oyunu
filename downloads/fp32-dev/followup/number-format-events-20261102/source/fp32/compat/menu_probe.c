/* Read actual native menu state without sending application commands. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static HANDLE out;
static BOOL CALLBACK find(HWND w,LPARAM unused){char cls[64];GetClassNameA(w,cls,sizeof(cls));if(!lstrcmpA(cls,"ShockwaveFlash")&&IsWindowVisible(w)){*((HWND*)unused)=w;return FALSE;}return TRUE;}
static void walk(HMENU menu,DWORD depth){int i,count=GetMenuItemCount(menu);char name[256],line[384];DWORD n;for(i=0;i<count;i++){HMENU sub=GetSubMenu(menu,i);name[0]=0;GetMenuStringA(menu,i,name,sizeof(name),MF_BYPOSITION);n=wsprintfA(line,"depth=%lu pos=%d id=%u flags=%08lX text=%s\r\n",depth,i,GetMenuItemID(menu,i),(DWORD)GetMenuState(menu,i,MF_BYPOSITION),name);WriteFile(out,line,n,&n,0);if(sub)walk(sub,depth+1);}}
void WINAPI entry(void){HWND w=0;EnumWindows(find,(LPARAM)&w);if(!w)ExitProcess(1);out=CreateFileA("C:\\FP32\\MENUS.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(out==INVALID_HANDLE_VALUE)ExitProcess(2);walk(GetMenu(w),0);CloseHandle(out);ExitProcess(0);}
