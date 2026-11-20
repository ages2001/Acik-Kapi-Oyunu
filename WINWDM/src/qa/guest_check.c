/* Run in a disposable x86 Explorer guest after Setup. The report is written
 * beside this executable. The test opens the installed driver and uses its
 * public IOCTL; no service or registry settings are changed. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "protocol.h"
#include "wdm_target.h"

#define WK_LABEL(id, code, label, file) label,
static const char *targets[] = { WK_WDM_TARGETS(WK_LABEL) };

static BOOL checkDeferred(FILE *report)
{
    OVERLAPPED work;
    HANDLE dev;
    DWORD bytes=0, reply=0, error;
    BOOL ok;
    memset(&work,0,sizeof(work));
    work.hEvent=CreateEventA(NULL,TRUE,FALSE,NULL);
    if (!work.hEvent) return FALSE;
    dev=CreateFileA("\\\\.\\WdmKit",0,0,NULL,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,NULL);
    if (dev==INVALID_HANDLE_VALUE) { CloseHandle(work.hEvent); return FALSE; }
    ok=DeviceIoControl(dev,WK_IOCTL_DEFERRED,NULL,0,&reply,sizeof(reply),&bytes,&work);
    error=ok ? 0 : GetLastError();
    if (!ok && error==ERROR_IO_PENDING)
        ok=GetOverlappedResult(dev,&work,&bytes,TRUE);
    fprintf(report,"Overlapped request start %lu, completion %lu, bytes %lu, reply 0x%lX\n",
            error,ok ? 0UL : GetLastError(),bytes,reply);
    CloseHandle(dev);
    CloseHandle(work.hEvent);
    return ok && bytes==sizeof(reply) && reply==WK_SIGNATURE;
}

int PASCAL WinMain(HINSTANCE h,HINSTANCE prev,LPSTR args,int show)
{
    const char *arp="Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\WdmKit";
    char file[MAX_PATH],setting[MAX_PATH],*leaf;
    HKEY key;
    DWORD size, type, version=GetVersion(), bytes=0, reply=0, error;
    BOOL nt=!(version&0x80000000UL), ioctl=FALSE;
    BOOL exact=!strncmp(args,"/pci=",5);
    BOOL requirePci=exact || !lstrcmpiA(args,"/pci"), registered=FALSE;
    int selected=-1, index, matches=0;
    LONG result;
    HANDLE dev;
    FILE *report;
    (void)prev;(void)show;
    if (!GetModuleFileName(h,file,sizeof(file))) return 1;
    leaf=strrchr(file,'\\');
    if (!leaf || leaf-file>MAX_PATH-14) return 1;
    strcpy(leaf+1,"KITCHK.TXT");
    report=fopen(file,"w");
    if (!report) return 1;
    fprintf(report,"GetVersion 0x%08lX\n",version);
    result=RegOpenKeyExA(HKEY_LOCAL_MACHINE,arp,0,KEY_QUERY_VALUE,&key);
    fprintf(report,"Uninstall registry open %ld\n",result);
    if (!result) {
        size=sizeof(setting);
        if (!RegQueryValueExA(key,"DisplayName",0,&type,(LPBYTE)setting,&size)) {
            fprintf(report,"DisplayName %s\n",setting);
            registered=!lstrcmpiA(setting,"WINWDM");
        }
        size=sizeof(setting);
        if (!RegQueryValueExA(key,"UninstallString",0,&type,(LPBYTE)setting,&size)) {
            fprintf(report,"UninstallString %s\n",setting);
            registered=registered && *setting;
        } else registered=FALSE;
        RegCloseKey(key);
    }
    dev=CreateFileA("\\\\.\\WDMKIT",0,0,NULL,OPEN_EXISTING,0,NULL);
    if (dev==INVALID_HANDLE_VALUE) {
        error=GetLastError();
        fprintf(report,"Open device error %lu\n",error);
    } else {
        for (index=0;index<WK_TARGET_COUNT;++index) {
            if (DeviceIoControl(dev,WK_IOCTL_TARGET_BASE+4UL*index,NULL,0,NULL,0,&bytes,NULL)) {
                selected=index;
                ++matches;
            }
        }
        fprintf(report,"Matching WDM targets %d, active %s\n",matches,selected>=0 ? targets[selected] : "unknown");
        if (exact) fprintf(report,"Requested WDM target %s\n",args+5);
        ioctl=DeviceIoControl(dev,WK_IOCTL,NULL,0,nt ? &reply : NULL,
                              nt ? sizeof(reply) : 0,&bytes,NULL);
        error=ioctl ? 0 : GetLastError();
        fprintf(report,"DeviceIoControl error %lu, bytes %lu, reply 0x%lX\n",error,bytes,reply);
        ioctl=ioctl && matches==1 && (!exact || !lstrcmpiA(args+5,targets[selected])) &&
              (!nt || (bytes==sizeof(reply) && reply==WK_SIGNATURE));
        if (ioctl)
            fprintf(report,"%s\n",selected==WK_INDEX_600 ?
                    "Seven-device dispatch, 5000-byte extension and 12-location IRP reuse passed" :
                    "Two-device startup check and stack-sized IRP reuse passed");
        bytes=reply=0;
        if (ioctl) {
            ioctl=DeviceIoControl(dev,WK_IOCTL_STACK,NULL,0,nt ? &reply : NULL,
                                  nt ? sizeof(reply) : 0,&bytes,NULL);
            fprintf(report,"Attached filter request error %lu, bytes %lu, reply 0x%lX\n",
                    ioctl ? 0UL : GetLastError(),bytes,reply);
            ioctl=ioctl && (!nt || (bytes==sizeof(reply) && reply==(WK_STACK_BASE|WK_STACK_FILTER)));
        }
        if (ioctl && !nt) {
            ioctl=DeviceIoControl(dev,WK_IOCTL_DEFERRED,NULL,0,NULL,0,&bytes,NULL);
            fprintf(report,"VMM deferred request error %lu\n",ioctl ? 0UL : GetLastError());
        }
        if (ioctl) {
            BOOL pci;
            reply=bytes=0;
            pci=DeviceIoControl(dev,WK_IOCTL_PCI,NULL,0,nt ? &reply : NULL,
                                nt ? sizeof(reply) : 0,&bytes,NULL);
            fprintf(report,"PCI PnP, power and device counter: error %lu, bytes %lu, reply %lu\n",
                    pci ? 0UL : GetLastError(),bytes,reply);
            if (pci && (!nt || (bytes==sizeof(reply) && reply==2)))
                fprintf(report,"Pending I/O completed before device removal\n");
            if (requirePci)
                ioctl=pci && (!nt || (bytes==sizeof(reply) && reply==2));
        }
        CloseHandle(dev);
    }
    if (ioctl && nt) ioctl=checkDeferred(report);
    if (requirePci && LOBYTE(LOWORD(version))>=4 && !registered) {
        fprintf(report,"Required Add/Remove Programs entry is missing or incomplete\n");
        ioctl=FALSE;
    }
    fprintf(report,"Kernel check %s\n",ioctl ? "PASS" : "FAIL");
    fclose(report);
    return ioctl ? 0 : 1;
}
