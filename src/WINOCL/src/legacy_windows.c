#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "icd.h"

/* Windows NT 3.x and 9x have no INIT_ONCE or DXGK adapter registry.
 * Legacy Windows ICDs register named DWORD values in the Khronos vendor key. */
static CRITICAL_SECTION discovery_lock;
static BOOL discovery_started;

static void enumerate_registry(void)
{
    HKEY key;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Khronos\\OpenCL\\Vendors",
            0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return;
    for (DWORD index = 0; ; ++index) {
        char name[MAX_PATH];
        DWORD length = sizeof(name), type = 0, enabled = 1, bytes = sizeof(enabled);
        LONG result = RegEnumValueA(key, index, name, &length, NULL, &type, (BYTE *)&enabled, &bytes);
        if (result == ERROR_NO_MORE_ITEMS) break;
        if (result == ERROR_MORE_DATA) continue;
        if (result != ERROR_SUCCESS) break;
        if (length && length < sizeof(name) && type == REG_DWORD && bytes == sizeof(enabled) && !enabled) {
            name[length] = 0;
            khrIcdVendorAdd(name);
        }
    }
    RegCloseKey(key);
}

void khrIcdOsVendorsEnumerateOnce(void)
{
    EnterCriticalSection(&discovery_lock);
    if (!discovery_started) {
        discovery_started = TRUE;
        khrIcdInitializeEnvOptions();
        khrIcdVendorsEnumerateEnv();
        enumerate_registry();
    }
    LeaveCriticalSection(&discovery_lock);
}

BOOL WINAPI winocl_entry(HINSTANCE instance, DWORD reason, void *reserved)
{
    (void)instance;
    if (reason == DLL_PROCESS_ATTACH)
        InitializeCriticalSection(&discovery_lock);
    if (reason == DLL_PROCESS_DETACH) {
        khrIcdDeinitialize(reserved == NULL);
        DeleteCriticalSection(&discovery_lock);
    }
    return TRUE;
}

#include <stdarg.h>
#include <stdio.h>

void winocl_trace(const char *format, ...)
{
    char text[1024];
    DWORD written;
    va_list args;
    va_start(args, format);
    _vsnprintf(text, sizeof(text) - 1, format, args);
    va_end(args);
    text[sizeof(text) - 1] = 0;
    WriteFile(GetStdHandle(STD_ERROR_HANDLE), text, lstrlenA(text), &written, NULL);
}
