/* Demand-start the test miniport in a disposable Windows 2000 guest.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <windows.h>
#include <stdio.h>

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR args, int show)
{
    SERVICE_STATUS state;
    FILE *log = fopen("C:\\WXP2KNV.TXT", "w");
    (void)instance; (void)previous; (void)args; (void)show;
    if (!log) return 1;
    SC_HANDLE manager = OpenSCManagerA(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!manager) {
        fprintf(log, "OpenSCManager=%lu\n", GetLastError());
        fclose(log);
        return 1;
    }
    SC_HANDLE service = CreateServiceA(manager, "WxpNvLab", "WINXPDM Forceware lab",
                                      SERVICE_START | SERVICE_QUERY_STATUS | DELETE,
                                      SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
                                      "system32\\drivers\\wxpnv.sys", "Video", NULL, NULL, NULL, NULL);
    if (!service && GetLastError() == ERROR_SERVICE_EXISTS)
        service = OpenServiceA(manager, "WxpNvLab", SERVICE_START | SERVICE_QUERY_STATUS | DELETE);
    if (!service) {
        fprintf(log, "CreateService=%lu\n", GetLastError());
        fclose(log);
        CloseServiceHandle(manager);
        return 1;
    }
    fputs("Requesting miniport start in a VM with no NVIDIA PCI adapter\n", log);
    fflush(log);
    BOOL started = StartServiceA(service, 0, NULL);
    DWORD error = started ? ERROR_SUCCESS : GetLastError();
    fprintf(log, "StartService=%d Win32_error=%lu\n", (int)started, error);
    if (QueryServiceStatus(service, &state))
        fprintf(log, "service_state=%lu Win32_exit=%lu service_exit=%lu\n",
                state.dwCurrentState, state.dwWin32ExitCode, state.dwServiceSpecificExitCode);
    DeleteService(service);
    CloseServiceHandle(service);
    CloseServiceHandle(manager);
    fclose(log);
    return 0;
}
