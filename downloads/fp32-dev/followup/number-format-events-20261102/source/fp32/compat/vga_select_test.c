/* Test overlay only: inspect or select the existing NT 3.1 VGA driver. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static HANDLE log;
static void line(const char *s,DWORD n){char b[256];DWORD w;wsprintfA(b,"%s %lu\r\n",s,n);WriteFile(log,b,lstrlenA(b),&w,0);FlushFileBuffers(log);}
static LONG start(const char *name,BOOL set,DWORD *value){HKEY k;char p[160];DWORD n=4,t=0;LONG e;wsprintfA(p,"SYSTEM\\CurrentControlSet\\Services\\%s",name);e=RegOpenKeyExA(HKEY_LOCAL_MACHINE,p,0,set?KEY_SET_VALUE:KEY_QUERY_VALUE,&k);if(e)return e;e=set?RegSetValueExA(k,"Start",0,REG_DWORD,(BYTE*)value,4):RegQueryValueExA(k,"Start",0,&t,(BYTE*)value,&n);RegCloseKey(k);if(!set&&!e&&(n!=4||t!=REG_DWORD))return ERROR_INVALID_DATA;return e;}
void WINAPI entry(void){DWORD vga=0,vbe=0,a=1,b=4;LONG e;char *cmd=GetCommandLineA();BOOL apply=FALSE;char p[MAX_PATH];DWORD n;
 while(*cmd){if(*cmd=='/'&&cmd[1]=='a')apply=TRUE;cmd++;}
 log=CreateFileA(apply?"C:\\FP32\\VGASET.LOG":"C:\\FP32\\VGAREAD.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(log==INVALID_HANDLE_VALUE)ExitProcess(1);
 e=start("Vga",FALSE,&vga);line("Vga read error",e);line("Vga original Start",vga);if(e)goto end;
 e=start("winvbe",FALSE,&vbe);line("winvbe read error",e);line("winvbe original Start",vbe);if(e)goto end;
 n=GetSystemDirectoryA(p,sizeof(p));if(!n||n+17>=sizeof(p)){e=ERROR_BAD_PATHNAME;goto end;}lstrcatA(p,"\\drivers\\vga.sys");if(GetFileAttributesA(p)==INVALID_FILE_ATTRIBUTES){e=GetLastError();goto end;}
 if(apply){e=start("Vga",TRUE,&a);line("Vga enable error",e);if(e)goto end;e=start("winvbe",TRUE,&b);line("winvbe disable error",e);if(e){line("Vga rollback error",start("Vga",TRUE,&vga));goto end;}
 e=start("Vga",FALSE,&a);if(!e&&a!=1)e=ERROR_INVALID_DATA;if(!e)e=start("winvbe",FALSE,&b);if(!e&&b!=4)e=ERROR_INVALID_DATA;line("Readback error",e);}
end:line("Final error",e);CloseHandle(log);ExitProcess(e?1:0);
}
