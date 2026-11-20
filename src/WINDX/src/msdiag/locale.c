#define WIN32_LEAN_AND_MEAN
#include <windows.h>

BOOL WINAPI wx_version_info(OSVERSIONINFOW *info);

BOOL WINAPI wx_version_a(OSVERSIONINFOA *info)
{
    OSVERSIONINFOW wide = {sizeof(wide)};
    DWORD size;
    if (!info || (info->dwOSVersionInfoSize != sizeof(*info) &&
            info->dwOSVersionInfoSize != sizeof(OSVERSIONINFOEXA))) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }
    if (!wx_version_info(&wide)) return FALSE;
    size = info->dwOSVersionInfoSize;
    ZeroMemory(info, size);
    info->dwOSVersionInfoSize = size;
    info->dwMajorVersion = wide.dwMajorVersion;
    info->dwMinorVersion = wide.dwMinorVersion;
    info->dwBuildNumber = wide.dwBuildNumber;
    info->dwPlatformId = wide.dwPlatformId;
    return WideCharToMultiByte(CP_ACP, 0, wide.szCSDVersion, -1, info->szCSDVersion,
            sizeof(info->szCSDVersion), NULL, NULL) != 0;
}

static WCHAR *wide_string(UINT cp, const char *s, int bytes, int *count)
{
    WCHAR *wide;
    *count = MultiByteToWideChar(cp, 0, s, bytes, NULL, 0);
    if (*count <= 0) return NULL;
    wide = HeapAlloc(GetProcessHeap(), 0, (SIZE_T)*count * sizeof(*wide));
    if (wide && !MultiByteToWideChar(cp, 0, s, bytes, wide, *count)) {
        HeapFree(GetProcessHeap(), 0, wide);
        return NULL;
    }
    return wide;
}

static UINT locale_cp(LCID locale)
{
    WCHAR buf[8];
    UINT n = 0;
    if (GetLocaleInfoW(locale, LOCALE_IDEFAULTANSICODEPAGE, buf, 8))
        for (unsigned i = 0; buf[i] >= L'0' && buf[i] <= L'9'; ++i) n = n * 10 + buf[i] - L'0';
    return n ? n : CP_ACP;
}

char *WINAPI wx_env_a(void)
{
    const char *block = GetEnvironmentStrings();
    const char *p = block;
    char *copy;
    if (!p) return NULL;
    while (*p) p += lstrlenA(p) + 1;
    SIZE_T bytes = (SIZE_T)(p - block) + 1;
    copy = HeapAlloc(GetProcessHeap(), 0, bytes);
    if (copy) CopyMemory(copy, block, bytes);
    return copy;
}

WCHAR *WINAPI wx_env_w(void)
{
    const char *block = GetEnvironmentStrings();
    const char *p = block;
    int count;
    if (!p) return NULL;
    while (*p) p += lstrlenA(p) + 1;
    return wide_string(CP_ACP, block, (int)(p - block) + 1, &count);
}

BOOL WINAPI wx_free_env(void *block)
{
    return block && HeapFree(GetProcessHeap(), 0, block);
}

int WINAPI wx_compare_a(LCID locale, DWORD flags, const char *a, int na, const char *b, int nb)
{
    int wa, wb, result = 0;
    UINT cp = locale_cp(locale);
    WCHAR *left = wide_string(cp, a, na, &wa);
    WCHAR *right = wide_string(cp, b, nb, &wb);
    if (left && right) result = CompareStringW(locale, flags, left, na < 0 ? -1 : wa, right, nb < 0 ? -1 : wb);
    if (left) HeapFree(GetProcessHeap(), 0, left);
    if (right) HeapFree(GetProcessHeap(), 0, right);
    return result;
}

int WINAPI wx_map_a(LCID locale, DWORD flags, const char *src, int len, char *dst, int size)
{
    UINT cp = locale_cp(locale);
    int n, needed, result = 0;
    WCHAR *mapped, *wide = wide_string(cp, src, len, &n);
    if (!wide) return 0;
    if (flags & LCMAP_SORTKEY) {
        result = LCMapStringW(locale, flags, wide, n, (WCHAR *)dst, size);
    } else if ((needed = LCMapStringW(locale, flags, wide, n, NULL, 0)) > 0) {
        mapped = HeapAlloc(GetProcessHeap(), 0, needed * sizeof(*mapped));
        if (mapped) {
            if (LCMapStringW(locale, flags, wide, n, mapped, needed))
                result = WideCharToMultiByte(cp, 0, mapped, needed, dst, size, NULL, NULL);
            HeapFree(GetProcessHeap(), 0, mapped);
        }
    }
    HeapFree(GetProcessHeap(), 0, wide);
    return result;
}

BOOL WINAPI wx_string_type_a(LCID locale, DWORD kind, const char *src, int len, WORD *info)
{
    int n;
    WCHAR *wide = wide_string(locale_cp(locale), src, len, &n);
    if (!wide) return FALSE;
    BOOL ok = GetStringTypeW(kind, wide, n, info);
    HeapFree(GetProcessHeap(), 0, wide);
    return ok;
}

char *WINAPI wx_lstrcpyn_a(char *dst, const char *src, int count)
{
    if (count > 0) {
        int i = 0;
        while (i < count - 1 && src[i]) { dst[i] = src[i]; ++i; }
        dst[i] = 0;
    }
    return dst;
}
WCHAR *WINAPI wx_lstrcpyn_w(WCHAR *dst, const WCHAR *src, int count)
{
    if (count > 0) {
        int i = 0;
        while (i < count - 1 && src[i]) { dst[i] = src[i]; ++i; }
        dst[i] = 0;
    }
    return dst;
}

int WINAPI wx_locale_a(LCID locale, LCTYPE type, char *value, int size)
{
    if (size < 0 || (size && !value)) { SetLastError(ERROR_INVALID_PARAMETER); return 0; }
    if (type & LOCALE_RETURN_NUMBER) {
        DWORD number;
        if (!GetLocaleInfoW(locale, type, (WCHAR *)&number, sizeof(number) / sizeof(WCHAR))) return 0;
        if (size && size < (int)sizeof(number)) { SetLastError(ERROR_INSUFFICIENT_BUFFER); return 0; }
        if (size) CopyMemory(value, &number, sizeof(number));
        return sizeof(number);
    }
    int len = GetLocaleInfoW(locale, type, NULL, 0);
    if (!len) return 0;
    WCHAR *wide = HeapAlloc(GetProcessHeap(), 0, len * sizeof(*wide));
    if (!wide) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return 0; }
    int count = GetLocaleInfoW(locale, type, wide, len);
    if (count) count = WideCharToMultiByte(locale_cp(locale), 0, wide, count, value, size, NULL, NULL);
    HeapFree(GetProcessHeap(), 0, wide);
    return count;
}
