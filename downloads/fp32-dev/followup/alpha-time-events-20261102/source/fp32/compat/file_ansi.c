#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* Win9x/Win32s file adapter. Reject lossy conversions instead of opening a
   different file after code-page substitution. No global scratch buffers. */
static char *path_to_ansi(LPCWSTR path) {
    int bytes, units, i;
    char *ansi;
    WCHAR *roundtrip;
    DWORD error;
    if (!path) { SetLastError(ERROR_INVALID_PARAMETER); return 0; }
    bytes = WideCharToMultiByte(CP_ACP, 0, path, -1, 0, 0, 0, 0);
    if (!bytes) return 0;
    ansi = LocalAlloc(LMEM_FIXED, bytes);
    if (!ansi) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return 0; }
    if (!WideCharToMultiByte(CP_ACP, 0, path, -1, ansi, bytes, 0, 0)) {
        error = GetLastError(); LocalFree(ansi); SetLastError(error); return 0;
    }
    units = MultiByteToWideChar(CP_ACP, 0, ansi, -1, 0, 0);
    if (!units) { error = GetLastError(); LocalFree(ansi); SetLastError(error); return 0; }
    roundtrip = LocalAlloc(LMEM_FIXED, units * sizeof(WCHAR));
    if (!roundtrip) { LocalFree(ansi); SetLastError(ERROR_NOT_ENOUGH_MEMORY); return 0; }
    if (!MultiByteToWideChar(CP_ACP, 0, ansi, -1, roundtrip, units)) {
        error = GetLastError(); LocalFree(roundtrip); LocalFree(ansi); SetLastError(error); return 0;
    }
    for (i = 0; i < units; i++) {
        if (path[i] != roundtrip[i]) {
            LocalFree(roundtrip); LocalFree(ansi);
            SetLastError(ERROR_NO_UNICODE_TRANSLATION); return 0;
        }
    }
    LocalFree(roundtrip);
    return ansi;
}

HANDLE WINAPI fp9_CreateFileW(LPCWSTR path, DWORD access, DWORD sharing,
    LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE template_file) {
    char *ansi = path_to_ansi(path);
    HANDLE result;
    DWORD error;
    if (!ansi) return INVALID_HANDLE_VALUE;
    result = CreateFileA(ansi, access, sharing, security, creation, flags, template_file);
    error = GetLastError(); LocalFree(ansi); SetLastError(error);
    return result;
}

DWORD WINAPI fp9_GetFileAttributesW(LPCWSTR path) {
    char *ansi = path_to_ansi(path);
    DWORD result, error;
    if (!ansi) return INVALID_FILE_ATTRIBUTES;
    result = GetFileAttributesA(ansi);
    error = GetLastError(); LocalFree(ansi); SetLastError(error);
    return result;
}

BOOL WINAPI fp9_DeleteFileW(LPCWSTR path) {
    char *ansi = path_to_ansi(path);
    BOOL result;
    DWORD error;
    if (!ansi) return FALSE;
    result = DeleteFileA(ansi);
    error = GetLastError(); LocalFree(ansi); SetLastError(error);
    return result;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) { return TRUE; }
