/* Inspect/start existing networking services in the disposable component guest. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static HANDLE logh;static BYTE storage[4096];
static void line(const char*s){DWORD n;WriteFile(logh,s,lstrlenA(s),&n,0);WriteFile(logh,"\r\n",2,&n,0);}
static void number(const char*s,DWORD v){char b[200];wsprintfA(b,"%s %lu",s,v);line(b);}
void WINAPI entry(void){SC_HANDLE manager,service;SERVICE_STATUS status;DWORD i,needed;QUERY_SERVICE_CONFIGA*config=(void*)storage;char path[MAX_PATH];const char*names[]={"Afd","Tcpip","Streams","NetBT"};const char*files[]={"\\system32\\drivers\\afd.sys","\\system32\\drivers\\tcpip.sys","\\system32\\drivers\\streams.sys","\\system32\\wshtcpip.dll"};BOOL start=FALSE;char*c=GetCommandLineA();
 while(*c){if(c[0]=='/'&&c[1]=='s'&&c[2]=='t'&&c[3]=='a'&&c[4]=='r'&&c[5]=='t'){start=TRUE;break;}c++;}
 logh=CreateFileA("C:\\FP32\\SVCNET.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(logh==INVALID_HANDLE_VALUE)ExitProcess(1);
 manager=OpenSCManagerA(0,0,SC_MANAGER_CONNECT);number("SC manager",(DWORD)manager);
 for(i=0;i<4;i++){line(names[i]);service=manager?OpenServiceA(manager,names[i],SERVICE_QUERY_CONFIG|SERVICE_QUERY_STATUS|(start?SERVICE_START:0)):0;
  if(!service){number("Open service error",GetLastError());continue;}
  if(QueryServiceConfigA(service,config,sizeof(storage),&needed)){line(config->lpBinaryPathName);number("Start type",config->dwStartType);if(config->lpDependencies)line(config->lpDependencies);}else number("Query config error",GetLastError());
  if(QueryServiceStatus(service,&status)){number("Service state",status.dwCurrentState);number("Service exit code",status.dwWin32ExitCode);if(start&&status.dwCurrentState==SERVICE_STOPPED){number("Start requested",StartServiceA(service,0,0));number("Start error",GetLastError());}}
  CloseServiceHandle(service);
 }
 if(manager)CloseServiceHandle(manager);
 for(i=0;i<4;i++){GetWindowsDirectoryA(path,MAX_PATH);lstrcatA(path,files[i]);line(path);number("Attributes",GetFileAttributesA(path));number("File error",GetLastError());}
 CloseHandle(logh);ExitProcess(0);
}
