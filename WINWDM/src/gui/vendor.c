/* Demand-start a user-selected SYS through the native NT service manager. */
#include "kit.h"
#include <commdlg.h>
#include <winsvc.h>
#include <stddef.h>
#include <string.h>

static const char prefix[] = "WINWDM.";
static const char displayPrefix[] = "WINWDM driver - ";

static DWORD waitForStop(SC_HANDLE service, SERVICE_STATUS *state)
{
    DWORD started = GetTickCount();
    while (state->dwCurrentState == SERVICE_STOP_PENDING) {
        if (GetTickCount() - started >= 10000UL) return ERROR_SERVICE_REQUEST_TIMEOUT;
        Sleep(100);
        if (!QueryServiceStatus(service, state)) return GetLastError();
    }
    return state->dwCurrentState == SERVICE_STOPPED ? ERROR_SUCCESS : ERROR_SERVICE_CANNOT_ACCEPT_CTRL;
}

/* The early SCM reports 1052 for some image-only drivers even after a reboot.
 * A stopped service may be removed only after NT confirms that its image has
 * left the loaded-module list. A matching basename anywhere keeps it intact. */
static BOOL imageAbsent(const char *path)
{
    typedef LONG (WINAPI *QUERY_SYSTEM)(ULONG, PVOID, ULONG, PULONG);
    typedef struct {
        PVOID section, mapped, base;
        ULONG size, flags;
        USHORT loadIndex, initIndex, loadCount, nameOffset;
        char path[256];
    } MODULE;
    typedef struct { ULONG count; MODULE entries[1]; } MODULES;
    MODULES *list;
    HMODULE ntdll = GetModuleHandleA("NTDLL.DLL");
    QUERY_SYSTEM query;
    const char *file = strrchr(path, '\\');
    ULONG i, size = 8192, needed, offset = offsetof(MODULES, entries);
    LONG status;
    BOOL absent = FALSE;
    if (!ntdll || !file) return FALSE;
    query = (QUERY_SYSTEM)GetProcAddress(ntdll, "NtQuerySystemInformation");
    if (!query) return FALSE;
    while (size <= 16UL * 1024 * 1024) {
        list = LocalAlloc(LPTR, size);
        if (!list) return FALSE;
        needed = 0;
        status = query(11, list, size, &needed); /* SystemModuleInformation */
        if (status != (LONG)0xc0000004UL) break;
        LocalFree(list);
        size = needed > size ? needed : size * 2;
    }
    if (size > 16UL * 1024 * 1024) return FALSE;
    if (needed && needed < size) size = needed;
    if (status >= 0 && size >= offset && list->count && list->count <= (size - offset) / sizeof(MODULE)) {
        for (i = 0; i < list->count; ++i) {
            const MODULE *m = &list->entries[i];
            if (m->nameOffset >= sizeof(m->path) || !m->path[m->nameOffset] ||
                !memchr(m->path + m->nameOffset, 0, sizeof(m->path) - m->nameOffset) ||
                !lstrcmpiA(file + 1, m->path + m->nameOffset)) break;
        }
        absent = i == list->count;
    }
    LocalFree(list);
    return absent;
}

static DWORD stopService(SC_HANDLE service, const char *path)
{
    SERVICE_STATUS state;
    DWORD error;
    BOOL stopped;
    if (!QueryServiceStatus(service, &state)) return GetLastError();
    stopped = state.dwCurrentState == SERVICE_STOPPED;
    /* Old SCM versions can report STOPPED for a resident class module with no
     * device objects. Let the native unload operation decide whether it can go. */
    if (state.dwCurrentState != SERVICE_STOP_PENDING &&
        !ControlService(service, SERVICE_CONTROL_STOP, &state)) {
        error = GetLastError();
        if (error != ERROR_SERVICE_NOT_ACTIVE &&
            !(stopped && error == ERROR_INVALID_SERVICE_CONTROL && imageAbsent(path))) return error;
        state.dwCurrentState = SERVICE_STOPPED;
    }
    error = waitForStop(service, &state);
    if (error) return error;
    if (DeleteService(service)) return ERROR_SUCCESS;
    error = GetLastError();
    return error == ERROR_SERVICE_MARKED_FOR_DELETE ? ERROR_SUCCESS : error;
}

/* Opening a service twice, or choosing a different file with the same name,
 * must never commandeer another driver's registration. */
static DWORD imageRegistration(const char *serviceName, const char *path, BOOL create)
{
    char keyName[384], saved[MAX_PATH];
    HKEY key;
    DWORD type, bytes = sizeof(saved);
    LONG result;
    wsprintfA(keyName, "SYSTEM\\CurrentControlSet\\Services\\%s", serviceName);
    result = RegOpenKeyExA(HKEY_LOCAL_MACHINE, keyName, 0, create ? KEY_SET_VALUE : KEY_QUERY_VALUE, &key);
    if (result != ERROR_SUCCESS) return result;
    if (create) {
        result = RegSetValueExA(key, "WINWDMImage", 0, REG_SZ, (const BYTE *)path, strlen(path) + 1);
    } else {
        result = RegQueryValueExA(key, "WINWDMImage", NULL, &type, (BYTE *)saved, &bytes);
        if (result == ERROR_FILE_NOT_FOUND || result == ERROR_MORE_DATA ||
            (result == ERROR_SUCCESS && (type != REG_SZ || bytes == 0 || saved[bytes - 1] ||
                                         lstrcmpiA(path, saved))))
            result = ERROR_SERVICE_EXISTS;
    }
    RegCloseKey(key);
    return result;
}

static DWORD checkConfig(SC_HANDLE service, const char *serviceName, const char *path, const char *display)
{
    QUERY_SERVICE_CONFIGA *config;
    const char *image;
    DWORD bytes = 0, error = ERROR_SERVICE_EXISTS;
    if (QueryServiceConfigA(service, NULL, 0, &bytes)) return ERROR_INVALID_DATA;
    error = GetLastError();
    if (error != ERROR_INSUFFICIENT_BUFFER || !bytes) return error;
    config = LocalAlloc(LPTR, bytes);
    if (!config) return ERROR_NOT_ENOUGH_MEMORY;
    if (!QueryServiceConfigA(service, config, bytes, &bytes)) error = GetLastError();
    else {
        image = config->lpBinaryPathName;
        if (image && !strncmp(image, "\\??\\", 4)) image += 4;
        else if (image && !strnicmp(image, "\\DosDevices\\", 12)) image += 12;
        error = ERROR_SERVICE_EXISTS;
        if (image && config->lpDisplayName && config->dwServiceType == SERVICE_KERNEL_DRIVER &&
            config->dwStartType == SERVICE_DEMAND_START && !lstrcmpiA(display, config->lpDisplayName) &&
            !lstrcmpiA(path, image))
            error = imageRegistration(serviceName, path, FALSE);
    }
    LocalFree(config);
    return error;
}

static DWORD vendorDriver(const char *input, BOOL load)
{
    char path[MAX_PATH], serviceName[256], display[256], *file, *dot;
    DWORD attrs, length, error = ERROR_SUCCESS, access = SERVICE_QUERY_CONFIG | SERVICE_QUERY_STATUS;
    BOOL created = FALSE;
    SC_HANDLE manager, service = NULL;
    if (!WkNtHost()) return ERROR_NOT_SUPPORTED;
    length = GetFullPathNameA(input, sizeof(path), path, &file);
    if (!length) return GetLastError();
    if (length >= sizeof(path) || !file) return ERROR_FILENAME_EXCED_RANGE;
    if (path[1] != ':' || path[2] != '\\' || !(dot = strrchr(file, '.')) ||
        dot == file || lstrcmpiA(dot, ".sys")) return ERROR_INVALID_NAME;
    length = (DWORD)(dot - file);
    if (length + sizeof(displayPrefix) > sizeof(display)) return ERROR_FILENAME_EXCED_RANGE;
    if (load) {
        attrs = GetFileAttributesA(path);
        if (attrs == INVALID_FILE_ATTRIBUTES) return GetLastError();
        if (attrs & FILE_ATTRIBUTE_DIRECTORY) return ERROR_INVALID_NAME;
    }
    memcpy(serviceName, prefix, sizeof(prefix) - 1);
    memcpy(serviceName + sizeof(prefix) - 1, file, length);
    serviceName[sizeof(prefix) - 1 + length] = 0;
    memcpy(display, displayPrefix, sizeof(displayPrefix) - 1);
    memcpy(display + sizeof(displayPrefix) - 1, file, length);
    display[sizeof(displayPrefix) - 1 + length] = 0;
    manager = OpenSCManagerA(NULL, NULL, load ? SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE : SC_MANAGER_CONNECT);
    if (!manager) return GetLastError();
    if (load) {
        access |= SERVICE_START | DELETE;
        service = CreateServiceA(manager, serviceName, display, access, SERVICE_KERNEL_DRIVER,
                    SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, path, NULL, NULL, NULL, NULL, NULL);
        created = service != NULL;
        if (!created && (error = GetLastError()) != ERROR_SERVICE_EXISTS) goto done;
    } else access |= SERVICE_STOP | DELETE;
    if (!created) {
        service = OpenServiceA(manager, serviceName, access);
        if (!service) {
            error = GetLastError();
            if (!load && error == ERROR_SERVICE_DOES_NOT_EXIST) error = ERROR_SUCCESS;
            goto done;
        }
        error = checkConfig(service, serviceName, path, display);
        if (error) goto done;
    } else {
        error = imageRegistration(serviceName, path, TRUE);
        if (error) {
            if (!DeleteService(service)) error = GetLastError();
            goto done;
        }
    }
    if (load) {
        error = ERROR_SUCCESS;
        if (!StartServiceA(service, 0, NULL)) {
            error = GetLastError();
            if (error == ERROR_SERVICE_ALREADY_RUNNING) error = ERROR_SUCCESS;
            else if (created && !DeleteService(service)) error = GetLastError();
        }
    } else error = stopService(service, path);
done:
    if (service) CloseServiceHandle(service);
    CloseServiceHandle(manager);
    return error;
}

DWORD WkVendorCommand(const char *command)
{
    char path[MAX_PATH];
    size_t n;
    BOOL load;
    if (!strnicmp(command, "/load ", 6)) { load = TRUE; command += 6; }
    else if (!strnicmp(command, "/unload ", 8)) { load = FALSE; command += 8; }
    else return ERROR_INVALID_PARAMETER;
    while (*command == ' ') ++command;
    n = strlen(command);
    while (n && command[n - 1] == ' ') --n;
    if (n >= 2 && *command == '"' && command[n - 1] == '"') { ++command; n -= 2; }
    if (!n || n >= sizeof(path)) return ERROR_INVALID_PARAMETER;
    memcpy(path, command, n); path[n] = 0;
    return vendorDriver(path, load);
}

void WkChooseVendor(HWND owner, BOOL load)
{
    char path[MAX_PATH] = "", message[512], reason[200] = "";
    OPENFILENAMEA dialog;
    DWORD error;
    memset(&dialog, 0, sizeof(dialog));
    /* NT 3.x accepts the original OPENFILENAME structure size. */
    dialog.lStructSize = offsetof(OPENFILENAMEA, lpTemplateName) + sizeof(dialog.lpTemplateName);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = "Kernel drivers (*.sys)\0*.sys\0\0";
    dialog.lpstrFile = path;
    dialog.nMaxFile = sizeof(path);
    dialog.lpstrTitle = load ? "Load a driver" : "Unload a driver";
    dialog.lpstrDefExt = "sys";
    dialog.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | (load ? OFN_FILEMUSTEXIST : 0);
    if (!GetOpenFileNameA(&dialog)) {
        error = CommDlgExtendedError();
        if (!error) return;
        wsprintfA(message, "Windows could not open the file dialog (error %lu).", error);
    } else {
        error = vendorDriver(path, load);
        if (!error) {
            MessageBoxA(owner, load ? "Windows accepted the driver start request." : "The driver registration has been removed.",
                        "WINWDM drivers", MB_OK | MB_ICONINFORMATION);
            return;
        }
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error, 0,
                       reason, sizeof(reason), NULL);
        wsprintfA(message, "Windows could not %s this driver (error %lu).\r\n%s",
                  load ? "load" : "unload", error, reason);
    }
    MessageBoxA(owner, message, "WINWDM drivers", MB_OK | MB_ICONEXCLAMATION);
}
