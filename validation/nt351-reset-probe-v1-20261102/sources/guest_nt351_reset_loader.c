/* Test fixture only: install/start the retained NT3.51 diagnostic once. */
#define _WIN32_WINNT 0x0400
#include <windows.h>
static void out(const char*s){DWORD n;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,lstrlenA(s),&n,0);}
void Entry(void){SC_HANDLE scm,svc;DWORD e;char b[9];int i;
 scm=OpenSCManagerA(0,0,SC_MANAGER_CREATE_SERVICE);if(!scm)ExitProcess(2);
 svc=CreateServiceA(scm,"RltkNt351ResetTest","RltkNt351ResetTest",SERVICE_START|SERVICE_QUERY_STATUS,
 SERVICE_KERNEL_DRIVER,SERVICE_DEMAND_START,SERVICE_ERROR_NORMAL,"C:\\N35TEST\\N35RESET.SYS",0,0,0,0,0);
 if(!svc){out("CREATE_FAILED\r\n");ExitProcess(3);}
 e=StartServiceA(svc,0,0)?0:GetLastError();for(i=0;i<8;i++)b[i]="0123456789ABCDEF"[(e>>(28-i*4))&15];b[8]=0;
 out("START_ERROR ");out(b);out("\r\nKernel diagnostic result is recorded on debug port E9.\r\nReboot before repeating; the test helper has no unload entry.\r\n");
 CloseServiceHandle(svc);CloseServiceHandle(scm);ExitProcess(e?4:0);
}
