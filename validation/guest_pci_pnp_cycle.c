/* Test-only full PCI miniport teardown/reload. Three rounds disable all six
 * present RLTKLANT PCI adapters, then enable each and verify its devnode.
 * This is orderly PnP, not PCIe surprise removal or physical hot-plug. */
#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
static void zero(void*p,unsigned n){volatile BYTE*b=p;while(n--)*b++=0;}
static void out(const char*s){DWORD n;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,lstrlenA(s),&n,0);}
static void hex(DWORD v){char b[9];int i;for(i=0;i<8;i++)b[i]="0123456789ABCDEF"[(v>>(28-4*i))&15];b[8]=0;out(b);}
static int has(const char*s,const char*q){unsigned i,j;for(i=0;s[i];i++){for(j=0;q[j]&&s[i+j]==q[j];j++);if(!q[j])return 1;}return 0;}
static DWORD change(HDEVINFO set,SP_DEVINFO_DATA*d,int enable)
{
    SP_PROPCHANGE_PARAMS p;SP_DEVINSTALL_PARAMS_A install;ULONG status,problem;unsigned i;
    zero(&p,sizeof(p));p.ClassInstallHeader.cbSize=sizeof(SP_CLASSINSTALL_HEADER);p.ClassInstallHeader.InstallFunction=DIF_PROPERTYCHANGE;
    p.StateChange=enable?DICS_ENABLE:DICS_DISABLE;p.Scope=DICS_FLAG_GLOBAL;
    if(!SetupDiSetClassInstallParamsA(set,d,&p.ClassInstallHeader,sizeof(p))||!SetupDiCallClassInstaller(DIF_PROPERTYCHANGE,set,d))return GetLastError();
    zero(&install,sizeof(install));install.cbSize=sizeof(install);
    if(!SetupDiGetDeviceInstallParamsA(set,d,&install))return GetLastError();
    if(install.Flags&(DI_NEEDREBOOT|DI_NEEDRESTART))return ERROR_SUCCESS_REBOOT_REQUIRED;
    for(i=0;i<300;i++){
        if(CM_Get_DevNode_Status(&status,&problem,d->DevInst,0)==CR_SUCCESS&&
           (enable?(!problem&&(status&DN_STARTED)):(problem==CM_PROB_DISABLED&&!(status&DN_STARTED))))return 0;
        Sleep(100);
    }
    return ERROR_TIMEOUT;
}
void Entry(void)
{
    HDEVINFO set;SP_DEVINFO_DATA d,devices[16];unsigned index,count=0,round,phase,i,total=0;DWORD error=0;
    set=SetupDiGetClassDevsA(0,0,0,DIGCF_PRESENT|DIGCF_ALLCLASSES);if(set==INVALID_HANDLE_VALUE)ExitProcess(1);
    for(index=0;;index++){
        char id[512],service[64];zero(&d,sizeof(d));d.cbSize=sizeof(d);if(!SetupDiEnumDeviceInfo(set,index,&d))break;
        if(!SetupDiGetDeviceInstanceIdA(set,&d,id,sizeof(id),0)||!has(id,"PCI\\VEN_10EC"))continue;
        zero(service,sizeof(service));if(!SetupDiGetDeviceRegistryPropertyA(set,&d,SPDRP_SERVICE,0,(BYTE*)service,sizeof(service),0)||lstrcmpiA(service,"RLTKLANT"))continue;
        if(count==16)ExitProcess(2);devices[count++]=d;out("DEVICE ");out(id);out("\r\n");
    }
    if(count!=6){out("COUNT ");hex(count);out(" FAIL\r\n");SetupDiDestroyDeviceInfoList(set);ExitProcess(3);}
    for(round=0;round<3;round++)for(phase=0;phase<2;phase++)for(i=0;i<count;i++){
        error=change(set,&devices[i],phase);out("ROUND ");hex(round+1);out(phase?" ENABLE ":" DISABLE ");hex(i);out(" ERROR ");hex(error);out("\r\n");
        if(error)goto finish;total++;
    }
finish:
    /* A failed test still attempts to restore all six test adapters. */
    if(error)for(i=0;i<count;i++)change(set,&devices[i],1);
    SetupDiDestroyDeviceInfoList(set);out("TRANSITIONS ");hex(total);out(error?" FAIL\r\n":" PASS\r\n");ExitProcess(error?4:0);
}
