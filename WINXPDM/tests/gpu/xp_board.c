/* Exercise a vendor display INF on an emulated PCI board in a disposable NT 5 guest.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef BOOL (WINAPI *UpdateDriver)(HWND, LPCSTR, LPCSTR, DWORD, PBOOL);

static void address_range(FILE *log, const char *list, const char *type, ULONGLONG start, ULONGLONG end)
{
    fprintf(log, "%s%s=%08lx%08lx-%08lx%08lx\n", list, type,
            (DWORD)(start >> 32), (DWORD)start, (DWORD)(end >> 32), (DWORD)end);
}

static void resources(FILE *log, DEVINST node, ULONG kind, const char *label)
{
    LOG_CONF config;
    RES_DES current, next;
    RESOURCEID type;
    CONFIGRET result = CM_Get_First_Log_Conf(&config, node, kind);
    fprintf(log, "%sLogConf=%08lx\n", label, result);
    if (result != CR_SUCCESS) return;
    current = (RES_DES)config;
    while ((result = CM_Get_Next_Res_Des(&next, current, ResType_All, &type, 0)) == CR_SUCCESS) {
        ULONG size;
        void *data = NULL;
        if (current != (RES_DES)config) CM_Free_Res_Des_Handle(current);
        current = next;
        if (CM_Get_Res_Des_Data_Size(&size, current, 0) != CR_SUCCESS || !size) continue;
        data = malloc(size);
        if (!data) break;
        if (CM_Get_Res_Des_Data(current, data, size, 0) == CR_SUCCESS) {
            if (type == ResType_Mem && size >= sizeof(MEM_DES)) {
                const MEM_DES *mem = data;
                address_range(log, label, "Memory", mem->MD_Alloc_Base, mem->MD_Alloc_End);
            } else if (type == ResType_IO && size >= sizeof(IO_DES)) {
                const IO_DES *io = data;
                address_range(log, label, "IO", io->IOD_Alloc_Base, io->IOD_Alloc_End);
            } else if (type == ResType_IRQ && size >= sizeof(IRQ_DES)) {
                const IRQ_DES *irq = data;
                fprintf(log, "%sIRQ=%lu\n", label, irq->IRQD_Alloc_Num);
            }
        }
        free(data);
    }
    if (current != (RES_DES)config) CM_Free_Res_Des_Handle(current);
    CM_Free_Log_Conf_Handle(config);
}

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
        resources(log, dev.DevInst, BOOT_LOG_CONF, "Boot");
        resources(log, dev.DevInst, ALLOC_LOG_CONF, "Allocated");
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
