/* Enumerate or invoke native Flash menu commands in the test VM. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static HANDLE f;static HWND player;
static void text(const char*s){DWORD n=0,w;while(s[n])n++;WriteFile(f,s,n,&w,0);}
static void value(DWORD n){char b[16],o[16];int i=0,k=0;do{b[i++]='0'+n%10;n/=10;}while(n);while(i)o[k++]=b[--i];o[k]=0;text(o);}
static BOOL CALLBACK find(HWND w,LPARAM p){char s[64];GetClassNameA(w,s,sizeof(s));if(IsWindowVisible(w)&&s[0]=='S'&&s[1]=='h'&&s[2]=='o'&&s[3]=='c'&&s[4]=='k'){player=w;return FALSE;}return TRUE;}
static void menu(HMENU m,int depth){int i,j,count=GetMenuItemCount(m);for(i=0;i<count;i++){char name[256];HMENU sub=GetSubMenu(m,i);for(j=0;j<depth;j++)text("  ");GetMenuStringA(m,i,name,sizeof(name),MF_BYPOSITION);text(name);text(" | id=");value(GetMenuItemID(m,i));text(" state=");value(GetMenuState(m,i,MF_BYPOSITION));text("\r\n");if(sub)menu(sub,depth+1);}}
void WINAPI entry(void){char*s=GetCommandLineA();DWORD id=0;f=CreateFileA("C:\\FP10\\MENUS.LOG",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,0,CREATE_ALWAYS,0,0);EnumWindows(find,0);if(!player){text("No player\r\n");CloseHandle(f);ExitProcess(1);}if(*s=='"'){s++;while(*s&&*s!='"')s++;if(*s)s++;}else while(*s&&*s!=' ')s++;while(*s==' ')s++;while(*s>='0'&&*s<='9')id=id*10+*s++-'0';menu(GetMenu(player),0);if(id){text("Invoking command ");value(id);text("\r\n");SetForegroundWindow(player);PostMessageA(player,WM_COMMAND,id,0);}CloseHandle(f);ExitProcess(0);}
