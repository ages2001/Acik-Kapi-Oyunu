/* List the display drivers offered by the running Windows XP SetupAPI.
 * Usage: place STOCK.INF and FORCE382.INF beside the program.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <windows.h>
#include <setupapi.h>
#include <stdio.h>
#include <string.h>

static const GUID display_class={0x4d36e968,0xe325,0x11ce,{0xbf,0xc1,0x08,0x00,0x2b,0xe1,0x03,0x18}};

static int enumerate(const char *label,const char *path,FILE *log)
{
    HDEVINFO set=SetupDiCreateDeviceInfoList(&display_class,NULL);
    SP_DEVINSTALL_PARAMS_A params;
    SP_DRVINFO_DATA_A driver;
    unsigned n=0;
    DWORD i,err;
    if (set==INVALID_HANDLE_VALUE) return -1;
    ZeroMemory(&params,sizeof(params));
    params.cbSize=sizeof(params);
    if (!SetupDiGetDeviceInstallParamsA(set,NULL,&params)) goto error;
    params.Flags|=DI_ENUMSINGLEINF;
    lstrcpynA(params.DriverPath,path,MAX_PATH);
    if (!SetupDiSetDeviceInstallParamsA(set,NULL,&params)) goto error;
    if (!SetupDiBuildDriverInfoList(set,NULL,SPDIT_CLASSDRIVER)) goto error;
    ZeroMemory(&driver,sizeof(driver));
    driver.cbSize=sizeof(driver);
    for (i=0;SetupDiEnumDriverInfoA(set,NULL,SPDIT_CLASSDRIVER,i,&driver);++i) {
        DWORD size=0;
        PSP_DRVINFO_DETAIL_DATA_A detail;
        SetupDiGetDriverInfoDetailA(set,NULL,&driver,NULL,0,&size);
        if (size<sizeof(*detail)) size=sizeof(*detail);
        detail=(PSP_DRVINFO_DETAIL_DATA_A)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,size);
        if (!detail) goto error;
        detail->cbSize=sizeof(*detail);
        if (!SetupDiGetDriverInfoDetailA(set,NULL,&driver,detail,size,NULL)) {
            err=GetLastError();HeapFree(GetProcessHeap(),0,detail);SetLastError(err);goto error;
        }
        fprintf(log,"%s\t%s\t%s\t%s\n",label,detail->HardwareID,detail->SectionName,driver.Description);
        HeapFree(GetProcessHeap(),0,detail);
        n++;
    }
    if (GetLastError()!=ERROR_NO_MORE_ITEMS) goto error;
    fprintf(log,"%s\trows\t%u\n",label,n);
    SetupDiDestroyDriverInfoList(set,NULL,SPDIT_CLASSDRIVER);
    SetupDiDestroyDeviceInfoList(set);
    return (int)n;
error:
    err=GetLastError();
    fprintf(log,"%s\terror\t0x%08lx\n",label,err);
    SetupDiDestroyDeviceInfoList(set);
    return -1;
}

int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,LPSTR args,int show)
{
    char path[MAX_PATH],*name;
    DWORD n=GetModuleFileNameA(NULL,path,sizeof(path));
    FILE *log;
    int stock,custom;
    (void)instance;(void)previous;(void)args;(void)show;
    if (!n || n>=MAX_PATH-15) return 2;
    for (name=path+n;name>path && name[-1]!='\\';--name) {}
    lstrcpyA(name,"VENDLIST.TSV");
    log=fopen(path,"w");
    if (!log) return 2;
    fprintf(log,"native_bits\t%u\n",(unsigned)(sizeof(void*)*8));
    lstrcpyA(name,"STOCK.INF");stock=enumerate("NVIDIA_368.81",path,log);
    lstrcpyA(name,"FORCE382.INF");custom=enumerate("Forceware_382.69",path,log);
    fclose(log);
    return stock>0 && custom>=0?0:1;
}
