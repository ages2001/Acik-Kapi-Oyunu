/* Test-only NT5 invoker: enumerate present RLTKLANT PCI bindings and request three
 * NDIS protocol resets on each binding, then unload the helper normally. */
#define _WIN32_WINNT 0x0400
#include <windows.h>
#include <setupapi.h>
#ifndef REQUEST_RESETS
#define REQUEST_RESETS 3
#endif
#define RESET_TEST CTL_CODE(FILE_DEVICE_UNKNOWN,0x801,METHOD_BUFFERED,FILE_WRITE_ACCESS)
typedef struct {DWORD Count;WCHAR Adapter[256];} REQUEST;
typedef struct {DWORD Open,Reset,Close,Completed,Pending,Vendor,ExpectedVendor;} RESULT;
static void zero(void*p,unsigned n){volatile BYTE*b=p;while(n--)*b++=0;}
static void out(const char*s){DWORD n;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,lstrlenA(s),&n,0);}
static void hex(DWORD v){char b[9];int i;for(i=0;i<8;i++)b[i]="0123456789ABCDEF"[(v>>(28-4*i))&15];b[8]=0;out(b);}
static int has(const char*s,const char*q){unsigned i,j;for(i=0;s[i];i++){for(j=0;q[j]&&s[i+j]==q[j];j++);if(!q[j])return 1;}return 0;}
static int bindings(WCHAR*exports,unsigned capacity)
{
    HDEVINFO set;SP_DEVINFO_DATA d;DWORD index;unsigned used=0,count=0;
    set=SetupDiGetClassDevsA(0,0,0,DIGCF_PRESENT|DIGCF_ALLCLASSES);if(set==INVALID_HANDLE_VALUE)return 0;
    for(index=0;;index++){
        char id[512],service[64],guid[128];HKEY key;DWORD size=sizeof(guid),type;unsigned i;
        zero(&d,sizeof(d));d.cbSize=sizeof(d);if(!SetupDiEnumDeviceInfo(set,index,&d))break;
        if(!SetupDiGetDeviceInstanceIdA(set,&d,id,sizeof(id),0)||!has(id,"PCI\\VEN_10EC"))continue;
        zero(service,sizeof(service));
        if(!SetupDiGetDeviceRegistryPropertyA(set,&d,SPDRP_SERVICE,0,(BYTE*)service,sizeof(service),0)||lstrcmpiA(service,"RLTKLANT"))continue;
        key=SetupDiOpenDevRegKey(set,&d,DICS_FLAG_GLOBAL,0,DIREG_DRV,KEY_QUERY_VALUE);if(key==INVALID_HANDLE_VALUE)break;
        zero(guid,sizeof(guid));type=0;
        if(RegQueryValueExA(key,"NetCfgInstanceId",0,&type,(BYTE*)guid,&size)!=ERROR_SUCCESS||type!=REG_SZ||size<2||size>sizeof(guid)||guid[size-1]){RegCloseKey(key);break;}
        RegCloseKey(key);if(used+8+lstrlenA(guid)+2>capacity)break;
        for(i=0;i<8;i++)exports[used++]=(WCHAR)"\\Device\\"[i];
        for(i=0;guid[i];i++)exports[used++]=(WCHAR)guid[i];exports[used++]=0;count++;
    }
    exports[used]=0;SetupDiDestroyDeviceInfoList(set);return count;
}
void Entry(void)
{
    SC_HANDLE scm,svc;SERVICE_STATUS service;HANDLE h;DWORD error=0,adapters=0,completed=0;
    WCHAR exports[2048],*p;char text[256];
    zero(exports,sizeof(exports));if(!bindings(exports,sizeof(exports)/sizeof(WCHAR))){out("NO_PCI_BINDINGS\r\n");ExitProcess(2);}
    scm=OpenSCManagerA(0,0,SC_MANAGER_CREATE_SERVICE);if(!scm)ExitProcess(3);
    svc=CreateServiceA(scm,"RltkPciResetTest","RltkPciResetTest",SERVICE_START|SERVICE_STOP|SERVICE_QUERY_STATUS,
        SERVICE_KERNEL_DRIVER,SERVICE_DEMAND_START,SERVICE_ERROR_NORMAL,"C:\\rlttest\\PCRT.SYS",0,0,0,0,0);
    if(!svc&&GetLastError()==ERROR_SERVICE_EXISTS)svc=OpenServiceA(scm,"RltkPciResetTest",SERVICE_START|SERVICE_STOP|SERVICE_QUERY_STATUS);
    if(!svc)ExitProcess(4);
    if(!StartServiceA(svc,0,0)){out("START_ERROR ");hex(GetLastError());out("\r\n");ExitProcess(5);}
    h=CreateFileA("\\\\.\\RltkPciResetTest",GENERIC_WRITE,0,0,OPEN_EXISTING,0,0);
    if(h==INVALID_HANDLE_VALUE){error=GetLastError();goto stop;}
    for(p=exports;*p;){
        REQUEST request;RESULT result;unsigned i=0;DWORD size=0;
        zero(&request,sizeof(request));zero(&result,sizeof(result));request.Count=REQUEST_RESETS;
        while(*p&&i<255){request.Adapter[i]=*p;text[i]=(char)*p;i++;p++;}
        if(*p){error=ERROR_INVALID_DATA;break;}p++;text[i]=0;adapters++;
        out("ADAPTER ");out(text);out("\r\n");
        if(!DeviceIoControl(h,RESET_TEST,&request,sizeof(request),&result,sizeof(result),&size,0)){error=GetLastError();break;}
        out("OPEN ");hex(result.Open);out(" RESET ");hex(result.Reset);out(" CLOSE ");hex(result.Close);
        out(" COMPLETED ");hex(result.Completed);out(" PENDING ");hex(result.Pending);out("\r\n");
        out("VENDOR ");hex(result.Vendor);out(" PERMANENT_PREFIX ");hex(result.ExpectedVendor);out("\r\n");
        if(size!=sizeof(result)||result.Open||result.Reset||result.Close||result.Completed!=REQUEST_RESETS||result.Vendor!=result.ExpectedVendor){error=ERROR_INVALID_DATA;break;}
        completed+=result.Completed;
    }
    CloseHandle(h);
stop:
    if(!ControlService(svc,SERVICE_CONTROL_STOP,&service)&&!error)error=GetLastError();
    CloseServiceHandle(svc);CloseServiceHandle(scm);
    out("ADAPTERS ");hex(adapters);out(" COMPLETED ");hex(completed);out(" ERROR ");hex(error);out("\r\n");
    if(!adapters||error){out("FAIL\r\n");ExitProcess(6);}out("PASS\r\n");ExitProcess(0);
}
