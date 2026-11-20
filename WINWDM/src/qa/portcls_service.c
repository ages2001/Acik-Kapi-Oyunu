/* Run the two fixed test services from the native NT drivers directory. */
#include <windows.h>
#include <winsvc.h>
#include <stdio.h>
#include <string.h>
#include <winioctl.h>
#include "ks_pin_test.h"

int PASCAL WinMain(HINSTANCE app, HINSTANCE previous, LPSTR args, int show)
{
    const char *names[] = {"WinWdmPcClassQa", "WinWdmPcIntQa"};
    const char *files[] = {"PORTCLS.SYS", "WKPCINT.SYS"};
    SC_HANDLE manager, services[2] = {NULL, NULL};
    BOOL started[2] = {FALSE, FALSE}, passed = TRUE, keepProvider = FALSE;
    char path[MAX_PATH];
    const char *reportPath = "C:\\DMAQA\\PORTCLS.TXT";
    SERVICE_STATUS state;
    FILE *report;
    UINT i, length;
    (void)app; (void)previous; (void)show;
    if (!lstrcmpiA(args, "/ks")) {
        names[0] = "WinWdmKsClassQa";
        names[1] = "WinWdmKsQueryQa";
        files[0] = "KS.SYS";
        files[1] = "WKKS.SYS";
        reportPath = "C:\\DMAQA\\KSQUERY.TXT";
    } else if (!lstrcmpiA(args, "/hid")) {
        names[0] = "WinWdmHidParserQa";
        names[1] = "WinWdmHidInputQa";
        files[0] = "HIDPARSE.SYS";
        files[1] = "WKHID.SYS";
        reportPath = "C:\\DMAQA\\HIDINPUT.TXT";
    } else if (!lstrcmpiA(args, "/ich")) {
        names[1] = "WinWdmPcIchQa";
        files[1] = "ICHAUD.SYS";
        reportPath = "C:\\DMAQA\\PCICH.TXT";
    } else if (*args) return 2;
    report = fopen(reportPath, "w");
    if (!report) return 2;
    fprintf(report, "GetVersion 0x%08lX\n", GetVersion());
    manager = OpenSCManagerA(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!manager) { fclose(report); return 2; }
    length = GetSystemDirectoryA(path, sizeof(path));
    if (!length || length + 24 >= sizeof(path)) passed = FALSE;
    for (i = 0; passed && i < 2; ++i) {
        strcpy(path + length, "\\drivers\\");
        strcat(path, files[i]);
        services[i] = CreateServiceA(manager, names[i], names[i], SERVICE_START | SERVICE_STOP | DELETE,
            SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE, path, NULL, NULL, NULL, NULL, NULL);
        if (services[i]) started[i] = StartServiceA(services[i], 0, NULL);
        fprintf(report, "%s start error %lu\n", files[i], started[i] ? 0UL : GetLastError());
        fflush(report);
        passed = started[i];
    }
    if (passed && !lstrcmpiA(args, "/ks")) {
        DWORD bytes;
        HANDLE filter = CreateFileA("\\\\.\\WkKsPinQa", GENERIC_READ | GENERIC_WRITE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        passed = filter != INVALID_HANDLE_VALUE &&
                 DeviceIoControl(filter, WK_KS_ROUNDTRIP, NULL, 0, NULL, 0, &bytes, NULL);
        fprintf(report, "Native filter pin round trip error %lu\n", passed ? 0UL : GetLastError());
        if (filter != INVALID_HANDLE_VALUE) CloseHandle(filter);
        fflush(report);
    }
    for (i = 2; i > 0; --i) {
        SC_HANDLE service = services[i-1];
        if (service) {
            if (i == 1 && keepProvider) {
                fputs("Provider retained while the client is active.\n", report);
            } else if (started[i-1] && !ControlService(service, SERVICE_CONTROL_STOP, &state)) {
                fprintf(report, "%s stop error %lu\n", files[i-1], GetLastError());
                passed = FALSE;
                keepProvider = TRUE;
            } else if (!DeleteService(service)) {
                fprintf(report, "%s removal error %lu\n", files[i-1], GetLastError());
                passed = FALSE;
            }
            CloseServiceHandle(service);
        }
    }
    fprintf(report, "%s\n", passed ? "PASS" : "FAIL");
    CloseServiceHandle(manager);
    return fclose(report) || !passed;
}
