#define WIN32_LEAN_AND_MEAN
#include <windows.h>

BOOL WINAPI wx_handle_flags(HANDLE handle, DWORD *flags);

static FARPROC native(const char *name)
{
    return GetProcAddress(GetModuleHandleA("ntdll.dll"), name);
}

BOOL WINAPI wocl_locale_supported(LCID id, DWORD flags)
{
    WCHAR language[8];
    if ((flags & ~(LCID_INSTALLED | LCID_SUPPORTED)) != 0) {
        SetLastError(ERROR_INVALID_FLAGS);
        return FALSE;
    }
    return GetLocaleInfoW(id, LOCALE_ILANGUAGE, language, 8) != 0;
}

BOOL WINAPI wocl_enum_locales(LOCALE_ENUMPROCA callback, DWORD flags)
{
    HKEY key;
    if (!callback || (flags != LCID_INSTALLED && flags != LCID_SUPPORTED && flags != 0)) {
        SetLastError(!callback ? ERROR_INVALID_PARAMETER : ERROR_INVALID_FLAGS);
        return FALSE;
    }
    /* Early NT stores the available language records as LCID-named values. */
    LONG error = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Nls\\Locale",
            0, KEY_QUERY_VALUE, &key);
    if (error) { SetLastError(error); return FALSE; }
    for (DWORD index = 0;; ++index) {
        char name[32];
        DWORD length = sizeof(name);
        error = RegEnumValueA(key, index, name, &length, NULL, NULL, NULL, NULL);
        if (error == ERROR_NO_MORE_ITEMS) { error = ERROR_SUCCESS; break; }
        if (error) break;
        if (length != 8) continue;
        LCID id = 0;
        for (DWORD i = 0; i < length; ++i) {
            int c = name[i];
            if (c >= '0' && c <= '9') c -= '0';
            else if (c >= 'A' && c <= 'F') c -= 'A' - 10;
            else if (c >= 'a' && c <= 'f') c -= 'a' - 10;
            else { id = 0; break; }
            id = (id << 4) | (unsigned)c;
        }
        if (id && wocl_locale_supported(id, flags) && !callback(name)) break;
    }
    RegCloseKey(key);
    if (error) SetLastError(error);
    return error == ERROR_SUCCESS;
}

SIZE_T WINAPI wocl_compact(HANDLE heap, DWORD flags)
{
    SIZE_T (WINAPI *compact)(HANDLE, DWORD) = (void *)native("RtlCompactHeap");
    if (compact) return compact(heap, flags);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}

BOOL WINAPI wocl_validate(HANDLE heap, DWORD flags, const void *block)
{
    BOOLEAN (WINAPI *validate)(HANDLE, DWORD, const void *) = (void *)native("RtlValidateHeap");
    if (validate) return validate(heap, flags, block);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

BOOL WINAPI wocl_heap_walk(HANDLE heap, LPPROCESS_HEAP_ENTRY entry)
{
    BOOL (WINAPI *walk)(HANDLE, LPPROCESS_HEAP_ENTRY) =
        (void *)GetProcAddress(GetModuleHandleA("kernel32.dll"), "HeapWalk");
    if (walk) return walk(heap, entry);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

BOOL WINAPI wocl_set_handle(HANDLE handle, DWORD mask, DWORD value)
{
    BOOL (WINAPI *set)(HANDLE, DWORD, DWORD) =
        (void *)GetProcAddress(GetModuleHandleA("kernel32.dll"), "SetHandleInformation");
    DWORD current;
    if (set) return set(handle, mask, value);
    if (!wx_handle_flags(handle, &current)) return FALSE;
    if (((value ^ current) & mask) == 0) return TRUE;
    /* NT 3.1 has NtQueryObject and does not export NtSetInformationObject. */
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}
