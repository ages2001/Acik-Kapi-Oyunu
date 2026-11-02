/* Test-guest maintenance utility: request an orderly reboot. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
void WINAPI entry(void){HANDLE token;TOKEN_PRIVILEGES p;DWORD error;
 if(!OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,&token))ExitProcess(GetLastError());
 if(!LookupPrivilegeValueA(0,"SeShutdownPrivilege",&p.Privileges[0].Luid))ExitProcess(GetLastError());
 p.PrivilegeCount=1;p.Privileges[0].Attributes=SE_PRIVILEGE_ENABLED;SetLastError(0);
 if(!AdjustTokenPrivileges(token,FALSE,&p,0,0,0))ExitProcess(GetLastError());error=GetLastError();CloseHandle(token);if(error)ExitProcess(error);
 if(!ExitWindowsEx(EWX_REBOOT,0))ExitProcess(GetLastError());ExitProcess(0);
}
