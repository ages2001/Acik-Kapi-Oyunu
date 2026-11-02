/* Diagnostic helper for native NT 3.1 file association and icon behavior. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
static HANDLE logh;
static void value(const char*s,DWORD v){char b[300];DWORD n=0,i,w;while(*s)b[n++]=*s++;b[n++]=' ';for(i=0;i<8;i++)b[n++]="0123456789ABCDEF"[(v>>(28-i*4))&15];b[n++]='\r';b[n++]='\n';WriteFile(logh,b,n,&w,0);}
void WINAPI entry(void){const char*paths[]={"C:\\FP32\\SETUP32.EXE","C:\\FP32 A\\UNINST.EXE","C:\\FP32\\UNINST.ICO","C:\\winnt31\\system32\\NOTEPAD.EXE"};DWORD i,j,n,code;HICON h;HKEY k;char*args=GetCommandLineA();const char*command=0;BOOL uninstall=FALSE;
 logh=CreateFileA("C:\\FP32\\SHELLPRB.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
 for(i=0;i<4;i++){value(paths[i],(DWORD)ExtractIconA(GetModuleHandleA(0),paths[i],(UINT)-1));for(j=0;j<2;j++){h=ExtractIconA(GetModuleHandleA(0),paths[i],j);value("Icon",(DWORD)h);value("Last error",GetLastError());if((UINT_PTR)h>1)DestroyIcon(h);}}
 while(*args){if(args[0]=='/'&&args[1]=='u')uninstall=TRUE;if(args[0]=='/'&&args[1]=='f')command="\"C:\\FP32 A\\FP32.EXE\" %1";if(args[0]=='/'&&args[1]=='r')command="\"C:\\FP32 A\\FP32.EXE\" \"%1\"";args++;}
 if(command){code=RegOpenKeyExA(HKEY_CLASSES_ROOT,"Ages2001.FP32.SWF\\shell\\open\\command",0,KEY_SET_VALUE,&k);if(!code){for(n=0;command[n];n++){}code=RegSetValueExA(k,0,0,REG_SZ,(BYTE*)command,n+1);RegCloseKey(k);}value("Association update",code);}
 if(uninstall)value("Uninstall shell result",(DWORD)ShellExecuteA(0,"open","C:\\FP32 A\\UNINST.EXE","/uninstall",0,SW_SHOWNORMAL));
 CloseHandle(logh);ExitProcess(0);
}
