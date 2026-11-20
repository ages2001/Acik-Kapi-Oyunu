/* Exercise a vendor display INF on an emulated PCI board in a disposable NT 5 guest.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <stdio.h>
#include <string.h>

typedef BOOL (WINAPI *UpdateDriver)(HWND, LPCSTR, LPCSTR, DWORD, PBOOL);

static int describe(FILE *log, char hardware[MAX_PATH])
{
    HDEVINFO set = SetupDiGetClassDevsA(NULL,"PCI",NULL,DIGCF_PRESENT | DIGCF_ALLCLASSES);
    SP_DEVINFO_DATA dev = {sizeof(dev)};
    DWORD index, flags, problem;
    int found = 0;
    if (set == INVALID_HANDLE_VALUE) return 0;
    for (index=0;SetupDiEnumDeviceInfo(set,index,&dev);++index) {
        char ids[1024]={0}, service[256]="", name[256]="";
        if (!SetupDiGetDeviceRegistryPropertyA(set,&dev,SPDRP_HARDWAREID,NULL,(BYTE *)ids,sizeof(ids)-2,NULL) ||
            (strncmp(ids,"PCI\\VEN_10DE&DEV_",17) && strncmp(ids,"PCI\\VEN_1002&DEV_",17))) continue;
        if (strlen(ids)>=MAX_PATH) break;
        strcpy(hardware,ids);
        SetupDiGetDeviceRegistryPropertyA(set,&dev,SPDRP_SERVICE,NULL,(BYTE *)service,sizeof(service)-1,NULL);
        SetupDiGetDeviceRegistryPropertyA(set,&dev,SPDRP_DEVICEDESC,NULL,(BYTE *)name,sizeof(name)-1,NULL);
        fprintf(log,"HardwareID=%s\nDescription=%s\nService=%s\n",hardware,name,service);
        if (CM_Get_DevNode_Status(&flags,&problem,dev.DevInst,0)==CR_SUCCESS)
            fprintf(log,"NodeStatus=%08lx\nProblem=%lu\n",flags,problem);
        if (*service) {
            SC_HANDLE scm=OpenSCManagerA(NULL,NULL,SC_MANAGER_CONNECT);
            SC_HANDLE svc=scm?OpenServiceA(scm,service,SERVICE_QUERY_STATUS):NULL;
            SERVICE_STATUS state;
            if (svc && QueryServiceStatus(svc,&state))
                fprintf(log,"ServiceState=%lu\nServiceWin32Exit=%lu\n",state.dwCurrentState,state.dwWin32ExitCode);
            if (svc) CloseServiceHandle(svc);
            if (scm) CloseServiceHandle(scm);
        }
        found=1;
        break;
    }
    SetupDiDestroyDeviceInfoList(set);
    fflush(log);
    return found;
}

int main(int argc,char **argv)
{
    BOOL reboot=FALSE, ok;
    char inf[MAX_PATH], ini[MAX_PATH], relative[MAX_PATH], id[MAX_PATH], *slash;
    FILE *log;
    int status=argc==2 && !strcmp(argv[1],"status");
    if ((argc!=1 && !status) || !GetModuleFileNameA(NULL,inf,sizeof(inf)) || !(slash=strrchr(inf,'\\'))) return 2;
    slash[1]=0;
    if (strlen(inf)+sizeof("BOARD.INI")>sizeof(ini)) return 2;
    strcpy(ini,inf);
    strcat(ini,"BOARD.INI");
    DWORD length=GetPrivateProfileStringA("Driver","Inf","NV\\nv4_dispi.inf",relative,sizeof(relative),ini);
    if (!length || length>=sizeof(relative)-1 || strlen(inf)+length>=sizeof(inf)) return 2;
    strcat(inf,relative);
    SetErrorMode(SEM_FAILCRITICALERRORS);
    int floppy=GetFileAttributesA("A:\\BOARD.TAG")!=INVALID_FILE_ATTRIBUTES;
    log=floppy?fopen(status?"A:\\WXPNODE.TXT":"A:\\WXPBIND.TXT","w"):fopen(status?"C:\\WXPNODE.TXT":"C:\\WXPBIND.TXT","w");
    if (!log) return 2;
    fprintf(log,"NativeBits=%u\n",(unsigned)sizeof(void *)*8);
    if (!describe(log,id)) { fputs("Target PCI node absent\n",log); fclose(log); return 1; }
    if (status) { fclose(log); return 0; }
    HMODULE lib=LoadLibraryA("newdev.dll");
    UpdateDriver update=lib?(UpdateDriver)(void *)GetProcAddress(lib,"UpdateDriverForPlugAndPlayDevicesA"):NULL;
    if (!update) { fclose(log); return 2; }
    fputs("Requesting vendor INF binding\n",log);fflush(log);
    /* The guest can show its normal installation prompts. */
    ok=update(NULL,id,inf,1,&reboot);
    DWORD error=ok?ERROR_SUCCESS:GetLastError();
    fprintf(log,"UpdateResult=%u\nUpdateError=%08lx\nRestartRequested=%u\n",ok,error,reboot);
    describe(log,id);
    fclose(log);FreeLibrary(lib);
    return ok?0:1;
}
