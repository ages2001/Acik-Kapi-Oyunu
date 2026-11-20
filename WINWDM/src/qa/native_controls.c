/* Fixed NT driver controls. Stage this EXE beside the indicated test images
 * in C:\PEQA or C:\DMAQA. The test creates and removes its own services. */
#include <windows.h>
#include <winsvc.h>
#include <stdio.h>

static BOOL runControl(SC_HANDLE manager, const char *name, const char *file, FILE *report)
{
    SC_HANDLE service;
    SERVICE_STATUS state;
    BOOL started, removed;
    DWORD error;
    service = CreateServiceA(manager, name, name, SERVICE_START | SERVICE_STOP | DELETE,
        SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE, file,
        NULL, NULL, NULL, NULL, NULL);
    if (!service) {
        fprintf(report, "%s create error %lu\n", name, GetLastError());
        return FALSE;
    }
    started = StartServiceA(service, 0, NULL);
    error = started ? 0 : GetLastError();
    fprintf(report, "%s StartService error %lu\n", name, error);
    if (started && !ControlService(service, SERVICE_CONTROL_STOP, &state)) {
        fprintf(report, "%s stop error %lu\n", name, GetLastError());
        CloseServiceHandle(service);
        return FALSE;
    }
    removed = DeleteService(service);
    if (!removed) fprintf(report, "%s removal error %lu\n", name, GetLastError());
    CloseServiceHandle(service);
    fflush(report);
    return removed;
}

int PASCAL WinMain(HINSTANCE app, HINSTANCE previous, LPSTR args, int show)
{
    SC_HANDLE manager;
    char name[16], file[64];
    FILE *report;
    int os, subsystem, alignment;
    BOOL success = TRUE, dma = !lstrcmpiA(args, "/edu"), irq = !lstrcmpiA(args, "/irq");
    (void)app; (void)previous; (void)show;
    if (!dma && !irq && lstrcmpiA(args, "/pe")) return 2;
    if (irq) report = fopen("C:\\DMAQA\\IRQ.TXT", "w");
    else report = fopen(dma ? "C:\\DMAQA\\RESULTS.TXT" : "C:\\PEQA\\RESULTS.TXT", "w");
    if (!report) return 2;
    fprintf(report, "GetVersion 0x%08lX\n", GetVersion());
    manager = OpenSCManagerA(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!manager) {
        fprintf(report, "OpenSCManager error %lu\n", GetLastError());
        fclose(report);
        return 2;
    }
    if (irq) {
        success = runControl(manager, "WdmEduIrq", "C:\\DMAQA\\WKEDUIRQ.SYS", report);
    } else if (dma) {
        success = runControl(manager, "WdmEduQa", "C:\\DMAQA\\WKEDUDMA.SYS", report);
    } else {
        for (os = 0; os < 2; ++os) for (subsystem = 0; subsystem < 2; ++subsystem) {
            for (alignment = 0; alignment < 2; ++alignment) {
                sprintf(name, "WdmPE%d%d%d", os, subsystem, alignment);
                sprintf(file, "C:\\PEQA\\P%d%d%d.SYS", os, subsystem, alignment);
                if (!runControl(manager, name, file, report)) success = FALSE;
            }
        }
    }
    CloseServiceHandle(manager);
    fputs(success ? "Finished\n" : "Incomplete\n", report);
    fclose(report);
    return success ? 0 : 2;
}
