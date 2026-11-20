/* Exercise the shipping Info commands through CreateProcess and inspect the SCM. */
#include <windows.h>
#include <winsvc.h>
#include <stdio.h>
#include <string.h>

static FILE *logFile;
static SC_HANDLE manager;

static DWORD runInfo(const char *action, const char *path)
{
    char command[2 * MAX_PATH];
    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    DWORD code;
    memset(&startup, 0, sizeof(startup)); startup.cb = sizeof(startup);
    wsprintfA(command, "C:\\DMAQA\\INFO.EXE /%s \"%s\"", action, path);
    if (!CreateProcessA(NULL, command, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process))
        code = GetLastError();
    else {
        CloseHandle(process.hThread);
        if (WaitForSingleObject(process.hProcess, 45000) != WAIT_OBJECT_0) code = ERROR_TIMEOUT;
        else if (!GetExitCodeProcess(process.hProcess, &code)) code = GetLastError();
        CloseHandle(process.hProcess);
    }
    fprintf(logFile, "Info %s %s returned %lu\n", action, path, code);
    fflush(logFile);
    return code;
}

static DWORD state(const char *name)
{
    SC_HANDLE service = OpenServiceA(manager, name, SERVICE_QUERY_STATUS);
    SERVICE_STATUS status;
    DWORD result;
    if (!service) return GetLastError();
    result = QueryServiceStatus(service, &status) ? status.dwCurrentState : GetLastError();
    CloseServiceHandle(service);
    return result;
}

static BOOL lifecycle(void)
{
    const char *primary = "C:\\DMAQA\\CLASS QA\\KS.SYS";
    const char *other = "C:\\DMAQA\\KS.SYS";
    const char *declines = "C:\\DMAQA\\P000.SYS";
    SC_HANDLE collision;
    BOOL ok;
    /* NT 3.1's FAT driver exposes only the disk's 8.3 alias. */
    if (GetFileAttributesA(primary) == INVALID_FILE_ATTRIBUTES)
        primary = "C:\\DMAQA\\CLASSQ~1\\KS.SYS";
    if (runInfo("load", primary) || state("WINWDM.KS") != SERVICE_RUNNING || runInfo("load", primary) ||
        runInfo("load", other) != ERROR_SERVICE_EXISTS || runInfo("unload", other) != ERROR_SERVICE_EXISTS ||
        state("WINWDM.KS") != SERVICE_RUNNING || runInfo("unload", primary) ||
        state("WINWDM.KS") != ERROR_SERVICE_DOES_NOT_EXIST || runInfo("unload", primary)) return FALSE;
    if (runInfo("load", declines) != ERROR_BAD_UNIT ||
        state("WINWDM.P000") != ERROR_SERVICE_DOES_NOT_EXIST) return FALSE;
    collision = CreateServiceA(manager, "WINWDM.KS", "WINWDM driver - KS", DELETE, SERVICE_KERNEL_DRIVER,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, primary, NULL, NULL, NULL, NULL, NULL);
    if (!collision) return FALSE;
    ok = runInfo("load", primary) == ERROR_SERVICE_EXISTS &&
         runInfo("unload", primary) == ERROR_SERVICE_EXISTS && state("WINWDM.KS") == SERVICE_STOPPED;
    if (!DeleteService(collision)) ok = FALSE;
    CloseServiceHandle(collision);
    return ok;
}

int PASCAL WinMain(HINSTANCE app, HINSTANCE previous, LPSTR args, int show)
{
    char dir[MAX_PATH], provider[MAX_PATH], client[MAX_PATH];
    DWORD first, loaded, explicitLoad, noUnload;
    BOOL ok, after = !lstrcmpiA(args, "/after");
    UINT n;
    (void)app; (void)previous; (void)show;
    if (*args && !after) return 2;
    logFile = fopen(after ? "C:\\DMAQA\\VENDAFT.TXT" : "C:\\DMAQA\\VENDOR.TXT", "w");
    if (!logFile) return 2;
    manager = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE);
    n = GetSystemDirectoryA(dir, sizeof(dir));
    if (!manager || !n || n > MAX_PATH - 24) { fclose(logFile); return 2; }
    wsprintfA(provider, "%s\\DRIVERS\\HIDPARSE.SYS", dir);
    wsprintfA(client, "%s\\DRIVERS\\WKHID.SYS", dir);
    fprintf(logFile, "GetVersion 0x%08lX\n", GetVersion()); fflush(logFile);
    if (after) {
        fprintf(logFile, "Provider state before cleanup %lu\n", state("WINWDM.HIDPARSE"));
        ok = !runInfo("unload", provider) && state("WINWDM.HIDPARSE") == ERROR_SERVICE_DOES_NOT_EXIST;
        fprintf(logFile, "Cleanup after boot %s\n", ok ? "PASS" : "FAIL");
    } else {
        ok = lifecycle();
        fprintf(logFile, "Demand start, retry, conflicting paths, failure cleanup and service ownership %s\n", ok ? "PASS" : "FAIL");
        first = runInfo("load", client);
        if (!first && runInfo("unload", client)) ok = FALSE;
        fprintf(logFile, "Client-only load without starting a provider service %lu\n", first);
        explicitLoad = runInfo("load", provider);
        if (!explicitLoad) {
            loaded = runInfo("load", client);
            if (!loaded && (state("WINWDM.WKHID") != SERVICE_RUNNING || runInfo("unload", client))) ok = FALSE;
            fprintf(logFile, "Original parser and input client load %s\n", loaded ? "FAIL" : "PASS");
            if (loaded) ok = FALSE;
            noUnload = runInfo("unload", provider);
            fprintf(logFile, "Original parser stop %lu, service state %lu\n", noUnload, state("WINWDM.HIDPARSE"));
            if (noUnload != ERROR_INVALID_SERVICE_CONTROL ||
                state("WINWDM.HIDPARSE") == ERROR_SERVICE_DOES_NOT_EXIST) ok = FALSE;
        } else {
            fprintf(logFile, "Original parser native loader error %lu, remaining service %lu\n",
                    explicitLoad, state("WINWDM.HIDPARSE"));
            if (state("WINWDM.HIDPARSE") != ERROR_SERVICE_DOES_NOT_EXIST) ok = FALSE;
        }
    }
    fprintf(logFile, "Overall %s\n", ok ? "PASS" : "FAIL");
    CloseServiceHandle(manager);
    fclose(logFile);
    return ok ? 0 : 1;
}
