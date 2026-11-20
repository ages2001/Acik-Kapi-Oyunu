#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef LONG (WINAPI *query_handle_fn)(HANDLE,ULONG,void *,ULONG,ULONG *);
typedef LONG (WINAPI *query_system_fn)(ULONG,void *,ULONG,ULONG *);
static FARPROC native(const char *name) { return GetProcAddress(GetModuleHandleA("kernel32.dll"), name); }
static FARPROC nt(const char *name) { return GetProcAddress(GetModuleHandleA("ntdll.dll"), name); }
void WINAPI wx_system_time(FILETIME *time)
{
    void (WINAPI *fn)(FILETIME *) = (void *)native("GetSystemTimeAsFileTime");
    SYSTEMTIME clock;
    if (fn) { fn(time); return; }
    GetSystemTime(&clock);
    SystemTimeToFileTime(&clock, time);
}

static BOOL nt_result(LONG status)
{
    if (status < 0) {
        ULONG (WINAPI *map)(LONG) = (void *)nt("RtlNtStatusToDosError");
        SetLastError(map ? map(status) : ERROR_NOT_SUPPORTED);
        return FALSE;
    }
    return TRUE;
}
BOOL WINAPI wx_disable_threads(HMODULE module)
{
    BOOL (WINAPI *fn)(HMODULE) = (void *)native("DisableThreadLibraryCalls");
    LONG (WINAPI *disable)(HMODULE) = (void *)nt("LdrDisableThreadCalloutsForDll");
    /* The linked Windows 95 CRT processes TLS callbacks through DllMain. */
    if ((GetVersion() & 0x8000ffffu) == 0x80000004u) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (fn) return fn(module);
    if (disable) return nt_result(disable(module));
    /* The loader continues sending the normal thread notifications. */
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}
BOOL WINAPI wx_handle_flags(HANDLE handle, DWORD *flags)
{
    BOOL (WINAPI *fn)(HANDLE,DWORD *) = (void *)native("GetHandleInformation");
    query_handle_fn query = (void *)nt("NtQueryObject");
    DWORD basic[16], length = 0;
    LONG status;
    if (fn) return fn(handle, flags);
    if (!query || !flags) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    status = query(handle, 0, basic, sizeof(basic), &length);
    if (!nt_result(status)) return FALSE;
    *flags = ((basic[0] & 2) ? HANDLE_FLAG_INHERIT : 0) | ((basic[0] & 1) ? HANDLE_FLAG_PROTECT_FROM_CLOSE : 0);
    return TRUE;
}
BOOL WINAPI wx_affinity(HANDLE process, DWORD_PTR *process_mask, DWORD_PTR *system_mask)
{
    BOOL (WINAPI *fn)(HANDLE,DWORD_PTR *,DWORD_PTR *) = (void *)native("GetProcessAffinityMask");
    query_handle_fn query = (void *)nt("NtQueryInformationProcess");
    struct { LONG status; void *peb; DWORD_PTR affinity; LONG priority; ULONG_PTR pid, parent; } basic;
    SYSTEM_INFO info;
    if (fn) return fn(process, process_mask, system_mask);
    if (!query || !process_mask || !system_mask) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    if (!nt_result(query(process, 0, &basic, sizeof(basic), NULL))) return FALSE;
    GetSystemInfo(&info);
    *system_mask = info.dwActiveProcessorMask;
    *process_mask = basic.affinity;
    return TRUE;
}
DWORD_PTR WINAPI wx_thread_affinity(HANDLE thread, DWORD_PTR mask)
{
    DWORD_PTR (WINAPI *fn)(HANDLE,DWORD_PTR) = (void *)native("SetThreadAffinityMask");
    query_handle_fn query = (void *)nt("NtQueryInformationThread");
    LONG (WINAPI *set)(HANDLE,ULONG,const void *,ULONG) = (void *)nt("NtSetInformationThread");
    struct { LONG status; void *teb; HANDLE process, thread; DWORD_PTR affinity; LONG priority, base_priority; } basic;
    if (fn) return fn(thread, mask);
    if (!query || !set || !mask) { SetLastError(ERROR_INVALID_PARAMETER); return 0; }
    if (!nt_result(query(thread, 0, &basic, sizeof(basic), NULL)) || !nt_result(set(thread, 4, &mask, sizeof(mask))))
        return 0;
    return basic.affinity;
}
BOOL WINAPI wx_time_adjustment(DWORD *adjust, DWORD *increment, BOOL *disabled)
{
    BOOL (WINAPI *fn)(DWORD *,DWORD *,BOOL *) = (void *)native("GetSystemTimeAdjustment");
    query_system_fn query = (void *)nt("NtQuerySystemInformation");
    struct { DWORD adjust, increment; BOOLEAN disabled; } value;
    if (fn) return fn(adjust, increment, disabled);
    if (!query || !adjust || !increment || !disabled) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    if (!nt_result(query(28, &value, sizeof(value), NULL))) return FALSE;
    *adjust = value.adjust; *increment = value.increment; *disabled = !!value.disabled;
    return TRUE;
}
BOOL WINAPI wx_version_info(OSVERSIONINFOW *info)
{
    BOOL (WINAPI *fn)(OSVERSIONINFOW *) = (void *)native("GetVersionExW");
    DWORD version, size;
    if (fn) return fn(info);
    if (!info || (info->dwOSVersionInfoSize != sizeof(*info) && info->dwOSVersionInfoSize != sizeof(OSVERSIONINFOEXW))) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER); return FALSE;
    }
    size = info->dwOSVersionInfoSize;
    for (DWORD i = 0; i < size; ++i) ((BYTE *)info)[i] = 0;
    version = GetVersion();
    info->dwOSVersionInfoSize = size;
    info->dwMajorVersion = LOBYTE(LOWORD(version)); info->dwMinorVersion = HIBYTE(LOWORD(version));
    info->dwBuildNumber = HIWORD(version) & 0x7fff;
    info->dwPlatformId = (version & 0x80000000u) ? VER_PLATFORM_WIN32_WINDOWS : VER_PLATFORM_WIN32_NT;
    return TRUE;
}
BOOL WINAPI wx_lead_byte(UINT codepage, BYTE byte)
{
    CPINFO info;
    if (!GetCPInfo(codepage, &info)) return FALSE;
    for (unsigned i = 0; i + 1 < sizeof(info.LeadByte) && info.LeadByte[i]; i += 2)
        if (byte >= info.LeadByte[i] && byte <= info.LeadByte[i+1]) return TRUE;
    return FALSE;
}
