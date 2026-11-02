/* Read-only service diagnostics for the NT test guests. No CRT required. */
#include <windows.h>
static void out(const char *s) { DWORD n=0,w; while(s[n])n++; WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&w,0); }
static void hex(DWORD v) { char s[11];int i;s[0]='0';s[1]='x';s[10]=0;for(i=0;i<8;i++)s[2+i]="0123456789ABCDEF"[(v>>(28-4*i))&15];out(s); }
void Entry(void) {
 const char *names[]={"RLTKLANT","RLTKLANT1","usbnt","Tcpip","Dhcp","LanmanWorkstation",0};
 SC_HANDLE scm=OpenSCManagerA(0,0,SC_MANAGER_CONNECT);int i;
 if(!scm){out("OpenSCManager error ");hex(GetLastError());ExitProcess(1);}
 for(i=0;names[i];i++){
  SERVICE_STATUS status;SC_HANDLE s;
  out(names[i]);out(": ");s=OpenServiceA(scm,names[i],SERVICE_QUERY_STATUS);
  if(!s){out("OpenService error ");hex(GetLastError());}
  else {if(QueryServiceStatus(s,&status)){out("state ");hex(status.dwCurrentState);out(" exit ");hex(status.dwWin32ExitCode);}else{out("Query error ");hex(GetLastError());}CloseServiceHandle(s);}
  out("\r\n");
 }
 CloseServiceHandle(scm);ExitProcess(0);
}
