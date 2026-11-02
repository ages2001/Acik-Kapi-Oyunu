/* Version reporting shared by FP10 and FP32. No advertised OS substitution. */
#ifndef FP_VERSION_COMPAT_H
#define FP_VERSION_COMPAT_H
#include <windows.h>
BOOL WINAPI fp_GetVersionExA(LPOSVERSIONINFOA out) {
    typedef BOOL (WINAPI *VersionA)(LPOSVERSIONINFOA);
    VersionA native;
    OSVERSIONINFOA info;
    DWORD raw, size, error, i;
    if (!out || (out->dwOSVersionInfoSize != sizeof(OSVERSIONINFOA) &&
                 out->dwOSVersionInfoSize != sizeof(OSVERSIONINFOEXA))) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER); return FALSE;
    }
    size = out->dwOSVersionInfoSize;
    native = (VersionA)GetProcAddress(GetModuleHandleA("KERNEL32.DLL"), "GetVersionExA");
    if (native && native(out)) return TRUE;
    if (native) {
        error = GetLastError();
        /* Win95 rejects EX structures without setting LastError. Always
           retry its standard structure before interpreting the error. */
        if (size != sizeof(OSVERSIONINFOEXA) &&
            error != ERROR_CALL_NOT_IMPLEMENTED && error != ERROR_INVALID_PARAMETER &&
            error != ERROR_INSUFFICIENT_BUFFER) return FALSE;
        info.dwOSVersionInfoSize = sizeof(info);
        if (native(&info)) {
            out->dwMajorVersion = info.dwMajorVersion;
            out->dwMinorVersion = info.dwMinorVersion;
            out->dwBuildNumber = info.dwBuildNumber;
            out->dwPlatformId = info.dwPlatformId;
            for (i = 0; i < 127 && info.szCSDVersion[i]; i++)
                out->szCSDVersion[i] = info.szCSDVersion[i];
            out->szCSDVersion[i] = 0;
            goto extended;
        }
        if (GetLastError() != ERROR_CALL_NOT_IMPLEMENTED) return FALSE;
    }
    /* NT 3.1 has no GetVersionEx. Retain a raw-version fallback for other
       legacy runtimes without it; Win32s 1.30c does supply a native API.
       The Win32s high word is not an NT build number. */
    raw = GetVersion();
    out->dwMajorVersion = LOBYTE(LOWORD(raw));
    out->dwMinorVersion = HIBYTE(LOWORD(raw));
    out->dwPlatformId = !(raw & 0x80000000u) ? VER_PLATFORM_WIN32_NT :
        (out->dwMajorVersion < 4 ? VER_PLATFORM_WIN32s : VER_PLATFORM_WIN32_WINDOWS);
    out->dwBuildNumber = out->dwPlatformId == VER_PLATFORM_WIN32_NT ? HIWORD(raw) : 0;
    out->szCSDVersion[0] = 0;
extended:
    out->dwOSVersionInfoSize = size;
    if (size == sizeof(OSVERSIONINFOEXA)) {
        OSVERSIONINFOEXA *ex = (OSVERSIONINFOEXA*)out;
        /* Retain the existing legacy defaults where the OS has no EX API.
           These fields do not establish server/suite detection. */
        ex->wServicePackMajor = ex->wServicePackMinor = ex->wSuiteMask = 0;
        ex->wProductType = VER_NT_WORKSTATION; ex->wReserved = 0;
    }
    return TRUE;
}
BOOL WINAPI fp_GetVersionExW(LPOSVERSIONINFOW out) {
    typedef BOOL (WINAPI *VersionW)(LPOSVERSIONINFOW);
    VersionW native;
    OSVERSIONINFOEXA info;
    DWORD size, error;
    if (!out || (out->dwOSVersionInfoSize != sizeof(OSVERSIONINFOW) &&
                 out->dwOSVersionInfoSize != sizeof(OSVERSIONINFOEXW))) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER); return FALSE;
    }
    size = out->dwOSVersionInfoSize;
    if (!(GetVersion() & 0x80000000u)) {
        native = (VersionW)GetProcAddress(GetModuleHandleA("KERNEL32.DLL"), "GetVersionExW");
        if (native && native(out)) return TRUE;
        if (native) {
            error = GetLastError();
            if (size != sizeof(OSVERSIONINFOEXW) &&
                error != ERROR_CALL_NOT_IMPLEMENTED && error != ERROR_INVALID_PARAMETER &&
                error != ERROR_INSUFFICIENT_BUFFER) return FALSE;
        }
    }
    info.dwOSVersionInfoSize = size == sizeof(OSVERSIONINFOEXW) ? sizeof(info) : sizeof(OSVERSIONINFOA);
    if (!fp_GetVersionExA((LPOSVERSIONINFOA)&info)) return FALSE;
    out->dwOSVersionInfoSize = size;
    out->dwMajorVersion = info.dwMajorVersion; out->dwMinorVersion = info.dwMinorVersion;
    out->dwBuildNumber = info.dwBuildNumber; out->dwPlatformId = info.dwPlatformId;
    if (!MultiByteToWideChar(CP_ACP, 0, info.szCSDVersion, -1, out->szCSDVersion, 128)) return FALSE;
    if (size == sizeof(OSVERSIONINFOEXW)) {
        OSVERSIONINFOEXW *ex = (OSVERSIONINFOEXW*)out;
        ex->wServicePackMajor = info.wServicePackMajor; ex->wServicePackMinor = info.wServicePackMinor;
        ex->wSuiteMask = info.wSuiteMask; ex->wProductType = info.wProductType; ex->wReserved = info.wReserved;
    }
    return TRUE;
}
#endif
