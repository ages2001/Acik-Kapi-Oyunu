/* Read installed NT3.1 FP10 registration and exercise its SWF association.
   Test destination: C:\F10TEST. Does not alter registration or save files. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddeml.h>
#include <shellapi.h>
static HANDLE logh;static DWORD checks,failures;static char group[8192];
static void text(const char*s){DWORD w;WriteFile(logh,s,lstrlenA(s),&w,0);}
static void check(BOOL ok,const char*s){checks++;if(!ok)failures++;text(s);text(ok?": PASS\r\n":": FAIL\r\n");}
static BOOL contains(const char*s,const char*n){DWORD i;for(;*s;s++){for(i=0;n[i]&&s[i]==n[i];i++){}if(!n[i])return TRUE;}return FALSE;}
static BOOL registry(const char*key,const char*expected){HKEY h;char b[1024];DWORD size=sizeof(b),type;LONG r=RegOpenKeyExA(HKEY_CLASSES_ROOT,key,0,KEY_READ,&h);if(r)return FALSE;r=RegQueryValueExA(h,0,0,&type,(BYTE*)b,&size);RegCloseKey(h);return !r&&type==REG_SZ&&size==lstrlenA(expected)+1&&!lstrcmpA(b,expected);}
static HDDEDATA CALLBACK callback(UINT t,UINT f,HCONV c,HSZ a,HSZ b,HDDEDATA d,ULONG_PTR x,ULONG_PTR y){return 0;}
void WINAPI entry(void){DWORD id=0,size,w,entries=0,i;HSZ service=0,item=0;HCONV conv=0;HDDEDATA data;char summary[80];HINSTANCE opened;
 logh=CreateFileA("C:\\FP32\\INSTCHK.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(logh==INVALID_HANDLE_VALUE)ExitProcess(1);
 check(registry(".swf","Ages2001.FP10.SWF"),"SWF class");
 check(registry("Ages2001.FP10.SWF\\shell\\open\\command","\"C:\\F10TEST\\FP10.EXE\" %1"),"SWF open command");
 check(registry("Ages2001.FP10.SWF\\DefaultIcon","C:\\F10TEST\\FP10.EXE,0"),"SWF icon");
 check(DdeInitializeA(&id,callback,APPCMD_CLIENTONLY,0)==DMLERR_NO_ERROR,"DDE initialized");
 if(id){service=DdeCreateStringHandleA(id,"PROGMAN",CP_WINANSI);conv=DdeConnect(id,service,service,0);}
 check(conv!=0,"Program Manager connected");
 if(conv){
  item=DdeCreateStringHandleA(id,"Flash Player 10 - ages2001",CP_WINANSI);
  data=DdeClientTransaction(0,0,conv,item,CF_TEXT,XTYP_REQUEST,10000,0);
  check(data!=0,"Installed group read");
  if(data){size=DdeGetData(data,(BYTE*)group,sizeof(group)-1,0);DdeFreeDataHandle(data);group[size]=0;text(group);text("\r\n");
   /* Group data has one header followed by quoted item records. */
   for(i=0;i<size;i++)if(group[i]=='\n'&&group[i+1]=='"')entries++;
   check(entries==2,"Exactly two shortcuts");
   check(contains(group,"C:\\F10TEST\\FP10.EXE")&&contains(group,"\"Flash Player 10\""),"Player shortcut path");
   check(contains(group,"C:\\F10TEST\\UNINST.EXE /uninstall")&&contains(group,"\"Uninstall Flash Player 10\""),"Uninstall shortcut path");
  }
  {const char*s="[ShowGroup(\"Flash Player 10 - ages2001\",1)]";check(DdeClientTransaction((BYTE*)s,lstrlenA(s)+1,conv,0,0,XTYP_EXECUTE,10000,0)!=0,"Show installed shortcuts");}
  DdeDisconnect(conv);
 }
 if(item)DdeFreeStringHandle(id,item);if(service)DdeFreeStringHandle(id,service);if(id)DdeUninitialize(id);
 opened=ShellExecuteA(0,"open","C:\\FP32\\ACTION.SWF",0,0,SW_SHOWNORMAL);check((UINT_PTR)opened>32,"Native SWF association launch");
 wsprintfA(summary,"Checks %lu\r\nFailures %lu\r\n",checks,failures);text(summary);CloseHandle(logh);ExitProcess(failures?2:0);
}
