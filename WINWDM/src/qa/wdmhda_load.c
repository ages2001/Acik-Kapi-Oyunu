/* Native SCM loader checks for two fixed audio drivers on a disposable NT guest.
 * /test takes A:\\HDA026.SYS, /ich takes A:\\ICHAUD.SYS. */
#include <windows.h>
#include <winsvc.h>
#include <stdio.h>
#include <string.h>

int PASCAL WinMain(HINSTANCE app,HINSTANCE previous,LPSTR args,int show)
{
    const char *source, *path, *log, *name;
    FILE *report;
    SC_HANDLE manager,service;
    SERVICE_STATUS state;
    BOOL started=FALSE, stopped=TRUE, removed=TRUE;
    DWORD error;
    (void)app; (void)previous; (void)show;
    if (!lstrcmpiA(args,"/test")) {
        source="A:\\HDA026.SYS";
        path="C:\\WDMQA\\HDA026.SYS";
        log="C:\\WDMQA\\HDALOAD.TXT";
        name="WinWdmHdaQa";
    } else if (!lstrcmpiA(args,"/ich")) {
        source="A:\\ICHAUD.SYS";
        path="C:\\WDMQA\\ICHAUD.SYS";
        log="C:\\WDMQA\\ICHLOAD.TXT";
        name="WinWdmIchQa";
    } else return 2;
    CreateDirectoryA("C:\\WDMQA",NULL);
    if (!CopyFileA(source,path,TRUE)) return 2;
    report=fopen(log,"w");
    if (!report) { DeleteFileA(path); return 2; }
    fprintf(report,"GetVersion 0x%08lX\n",GetVersion());
    manager=OpenSCManagerA(NULL,NULL,SC_MANAGER_CREATE_SERVICE);
    if (!manager) { fclose(report); DeleteFileA(path); return 2; }
    service=CreateServiceA(manager,name,name,
        SERVICE_START|SERVICE_STOP|SERVICE_QUERY_STATUS|DELETE,SERVICE_KERNEL_DRIVER,
        SERVICE_DEMAND_START,SERVICE_ERROR_IGNORE,path,NULL,NULL,NULL,NULL,NULL);
    if (service) {
        started=StartServiceA(service,0,NULL);
        error=started ? 0 : GetLastError();
        fprintf(report,"Native StartService result %s, Windows error %lu\n",started ? "started" : "failed",error);
        if (started) stopped=ControlService(service,SERVICE_CONTROL_STOP,&state);
        if (stopped) {
            removed=DeleteService(service);
            fprintf(report,"Temporary service removal %s\n",removed ? "succeeded" : "failed");
        } else fputs("Service still running. Test image retained.\n",report);
        CloseServiceHandle(service);
    } else fprintf(report,"Temporary service creation failed, Windows error %lu\n",GetLastError());
    CloseServiceHandle(manager);
    if (stopped && removed && !DeleteFileA(path)) {
        fprintf(report,"Temporary file removal failed, Windows error %lu\n",GetLastError());
        removed=FALSE;
    }
    fclose(report);
    if (!stopped || !removed) return 2;
    return started ? 0 : 1;
}
