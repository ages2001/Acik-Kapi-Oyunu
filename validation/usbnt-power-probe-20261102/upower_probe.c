/* Explicit test-only reset invocation on the one present Realtek USB NIC. */
#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <setupapi.h>
#define RESET_TEST CTL_CODE(FILE_DEVICE_UNKNOWN,0x800,METHOD_BUFFERED,FILE_WRITE_ACCESS)
static void out(const char*s){DWORD n;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,lstrlenA(s),&n,0);}
static void zero(void*p,unsigned n){volatile BYTE*b=p;while(n--)*b++=0;}
static void hex(DWORD v){char b[9];int i;for(i=0;i<8;i++)b[i]="0123456789ABCDEF"[(v>>(28-4*i))&15];b[8]=0;out(b);}
static int match(const char*s){int i,j;const char*p="VID_0BDA";for(i=0;s[i];i++){for(j=0;p[j]&&s[i+j]==p[j];j++);if(!p[j])return 1;}return 0;}
void Entry(void){
    HDEVINFO set;SP_DEVINFO_DATA d;SC_HANDLE scm,svc;HANDLE h;DWORD i,n,error=0,count=0;WCHAR path[256];char id[512];
    scm=OpenSCManagerA(0,0,SC_MANAGER_CREATE_SERVICE);if(!scm){out("SCM ");hex(GetLastError());ExitProcess(1);}
    svc=CreateServiceA(scm,"RltkPowerTest","RltkPowerTest",SERVICE_START|SERVICE_QUERY_STATUS,SERVICE_KERNEL_DRIVER,SERVICE_DEMAND_START,SERVICE_ERROR_NORMAL,"C:\\rlttest\\upower.sys",0,0,0,0,0);
    if(!svc&&GetLastError()==ERROR_SERVICE_EXISTS)svc=OpenServiceA(scm,"RltkPowerTest",SERVICE_START|SERVICE_QUERY_STATUS);
    if(!svc){out("SERVICE ");hex(GetLastError());ExitProcess(2);}
    if(!StartServiceA(svc,0,0)&&GetLastError()!=ERROR_SERVICE_ALREADY_RUNNING){out("START ");hex(GetLastError());ExitProcess(3);}
    CloseServiceHandle(svc);CloseServiceHandle(scm);
    set=SetupDiGetClassDevsA(0,0,0,DIGCF_PRESENT|DIGCF_ALLCLASSES);if(set==INVALID_HANDLE_VALUE)ExitProcess(4);
    for(i=0;;i++){
        zero(&d,sizeof(d));d.cbSize=sizeof(d);if(!SetupDiEnumDeviceInfo(set,i,&d))break;
        if(!SetupDiGetDeviceInstanceIdA(set,&d,id,sizeof(id),0)||!match(id))continue;
        out("DEVICE ");out(id);out("\r\n");
        if(!SetupDiGetDeviceRegistryPropertyW(set,&d,SPDRP_PHYSICAL_DEVICE_OBJECT_NAME,0,(BYTE*)path,sizeof(path),&n)){error=GetLastError();break;}count++;
    }
    SetupDiDestroyDeviceInfoList(set);if(error||count!=1){out("ENUM ");hex(error);out(" COUNT ");hex(count);ExitProcess(5);}
    h=CreateFileA("\\\\.\\RltkPowerTest",GENERIC_WRITE,0,0,OPEN_EXISTING,0,0);if(h==INVALID_HANDLE_VALUE){out("OPEN ");hex(GetLastError());ExitProcess(6);}
    if(!DeviceIoControl(h,RESET_TEST,path,n,0,0,&n,0))error=GetLastError();CloseHandle(h);
    out("FUNCTION_POWER error=");hex(error);out("\r\n");ExitProcess(error?7:0);
}
