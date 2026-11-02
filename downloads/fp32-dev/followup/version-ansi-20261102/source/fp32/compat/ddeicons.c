#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddeml.h>
static HDDEDATA CALLBACK callback(UINT t,UINT f,HCONV c,HSZ a,HSZ b,HDDEDATA d,ULONG_PTR x,ULONG_PTR y){return 0;}
void WINAPI entry(void){DWORD id=0,i,n,w;HSZ s;HCONV c;HANDLE logh;char*arg=GetCommandLineA();BOOL clean=FALSE;const char*commands[]={
 "[ShowGroup(\"FP32 Icon Test\",1)]",
 "[AddItem(\"NOTEPAD.EXE\",\"Double quoted\",\"\"C:\\FP32 A\\UNINST.EXE\"\",1)]",
 "[AddItem(\"NOTEPAD.EXE\",\"Backslash quote\",\"\\\"C:\\FP32 A\\UNINST.EXE\\\"\",1)]",
 "[AddItem(\"NOTEPAD.EXE\",\"Relative icon\",UNINST.EXE,1,0,80,\"C:\\FP32 A\")]",
 "[AddItem(\"NOTEPAD.EXE\",\"Explicit folder\",\"C:\\FP32 A\\UNINST.EXE\",1,80,80,\"C:\\FP32 A\")]",
 "[AddItem(\"NOTEPAD.EXE\",\"Quoted relative\",\"UNINST.EXE\",1,160,80,\"C:\\FP32 A\")]"};
 while(*arg){if(arg[0]=='/'&&arg[1]=='c')clean=TRUE;arg++;}if(DdeInitializeA(&id,callback,APPCMD_CLIENTONLY,0))ExitProcess(1);s=DdeCreateStringHandleA(id,"PROGMAN",CP_WINANSI);c=DdeConnect(id,s,s,0);if(!c)ExitProcess(2);
 logh=CreateFileA("C:\\FP32\\DDEICONS.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
 for(i=0;i<(clean?1:6);i++){const char*cmd=clean?"[DeleteGroup(\"FP32 Icon Test\")]":commands[i];for(n=0;cmd[n];n++){}WriteFile(logh,cmd,n,&w,0);cmd=DdeClientTransaction((BYTE*)cmd,n+1,c,0,0,XTYP_EXECUTE,10000,0)?" OK\r\n":" FAIL\r\n";for(n=0;cmd[n];n++){}WriteFile(logh,cmd,n,&w,0);}
 CloseHandle(logh);DdeDisconnect(c);DdeFreeStringHandle(id,s);DdeUninitialize(id);ExitProcess(0);
}
