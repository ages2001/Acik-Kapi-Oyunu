#ifndef WINDX_NATIVE_IDENTITY_H
#define WINDX_NATIVE_IDENTITY_H

#ifndef WINDX_IDENTITY_HAS_WINAPI
#include <windows.h>
#endif
#if defined(__i386__) || defined(__x86_64__)
#include <cpuid.h>
#endif

struct wx_system_identity {
    char os[256], processor[384];
    char product_name[128], service_pack[128], brand[96], vendor[24];
    DWORD platform, major, minor, build;
    DWORD family, model, stepping, signature, logical_processors, mhz;
    BOOL has_signature;
};

struct wx_text {
    char *next;
    unsigned left;
};

static void wx_identity_text(struct wx_text *out, const char *s)
{
    while (*s && out->left > 1) {
        *out->next++ = *s++;
        --out->left;
    }
    *out->next = 0;
}

static void wx_identity_number(struct wx_text *out, DWORD value)
{
    char digits[12];
    unsigned n = sizeof(digits) - 1;
    digits[n] = 0;
    do { digits[--n] = '0' + value % 10; value /= 10; } while (value);
    wx_identity_text(out, digits + n);
}

static void wx_identity_simplify(char *text)
{
    const char *from = text;
    char *to = text;
    BOOL space = FALSE;
    for (; *from; ++from) {
        if ((unsigned char)*from <= ' ') { space = to != text; continue; }
        if (space) *to++ = ' ';
        space = FALSE;
        *to++ = *from;
    }
    *to = 0;
}

static BOOL wx_identity_reg(HKEY key, const char *name, DWORD wanted, void *data, DWORD capacity)
{
    DWORD type, bytes = capacity;
    if (RegQueryValueExA(key, name, NULL, &type, (BYTE *)data, &bytes) != ERROR_SUCCESS ||
            type != wanted || !bytes || bytes > capacity) return FALSE;
    if (wanted == REG_SZ) {
        ((char *)data)[capacity - 1] = 0;
        if (bytes < capacity) ((char *)data)[bytes] = 0;
        wx_identity_simplify(data);
        return *(const char *)data != 0;
    }
    return wanted == REG_DWORD && bytes == sizeof(DWORD);
}

static BOOL wx_identity_version(struct wx_system_identity *result)
{
    typedef BOOL (WINAPI *version_fn)(OSVERSIONINFOA *);
    typedef LONG (WINAPI *rtl_version_fn)(OSVERSIONINFOW *);
    HMODULE kernel = GetModuleHandleA("kernel32.dll"), nt = GetModuleHandleA("ntdll.dll");
    version_fn ansi = (version_fn)(void *)GetProcAddress(kernel, "GetVersionExA");
    rtl_version_fn rtl = nt ? (rtl_version_fn)(void *)GetProcAddress(nt, "RtlGetVersion") : NULL;
    OSVERSIONINFOA a = {0};
    OSVERSIONINFOW w = {0};
    a.dwOSVersionInfoSize = sizeof(a);
    w.dwOSVersionInfoSize = sizeof(w);
    if (rtl && rtl(&w) == 0) {
        result->platform = w.dwPlatformId;
        result->major = w.dwMajorVersion;
        result->minor = w.dwMinorVersion;
        result->build = w.dwBuildNumber;
        WideCharToMultiByte(CP_ACP, 0, w.szCSDVersion, -1, result->service_pack,
                            sizeof(result->service_pack), NULL, NULL);
    } else if (ansi && ansi(&a)) {
        struct wx_text pack = {result->service_pack, sizeof(result->service_pack)};
        result->platform = a.dwPlatformId;
        result->major = a.dwMajorVersion;
        result->minor = a.dwMinorVersion;
        result->build = a.dwBuildNumber;
        a.szCSDVersion[sizeof(a.szCSDVersion) - 1] = 0;
        wx_identity_text(&pack, a.szCSDVersion);
    } else {
        DWORD (WINAPI *native_version)(void) = (void *)GetProcAddress(kernel, "GetVersion");
        DWORD bits;
        if (!native_version) return FALSE;
        bits = native_version();
        result->major = LOBYTE(LOWORD(bits));
        result->minor = HIBYTE(LOWORD(bits));
        result->build = HIWORD(bits) & 0x7fff;
        result->platform = (bits & 0x80000000u) ? VER_PLATFORM_WIN32_WINDOWS : VER_PLATFORM_WIN32_NT;
    }
    if (result->platform == VER_PLATFORM_WIN32_WINDOWS) result->build = LOWORD(result->build);
    wx_identity_simplify(result->service_pack);
    return TRUE;
}

static const char *wx_identity_xp_edition(const struct wx_system_identity *result)
{
    typedef BOOL (WINAPI *version_fn)(OSVERSIONINFOEXA *);
    typedef int (WINAPI *metric_fn)(int);
    typedef void (WINAPI *native_info_fn)(SYSTEM_INFO *);
    HMODULE kernel, user;
    OSVERSIONINFOEXA info = {0};
    version_fn version;
    metric_fn metric;
    const char *edition = "";
    if (result->platform != VER_PLATFORM_WIN32_NT || result->major != 5 ||
            (result->minor != 1 && result->minor != 2)) return "";
    if (lstrcmpiA(result->product_name, "Microsoft Windows XP") &&
            lstrcmpiA(result->product_name, "Windows XP")) return "";
    kernel = GetModuleHandleA("kernel32.dll");
    version = (version_fn)(void *)GetProcAddress(kernel, "GetVersionExA");
    info.dwOSVersionInfoSize = sizeof(info);
    if (!version || !version(&info) || info.wProductType != VER_NT_WORKSTATION) return "";
    if (result->minor == 2) {
        SYSTEM_INFO machine;
        WORD architecture;
        native_info_fn native = (native_info_fn)(void *)GetProcAddress(kernel, "GetNativeSystemInfo");
        if (!native) return "";
        native(&machine);
        /* The architecture WORD is the first member of SYSTEM_INFO. Old Wine
         * headers give its surrounding union a different source-level name. */
        architecture = (WORD)(((const BYTE *)&machine)[0] | ((const BYTE *)&machine)[1] << 8);
        if (architecture == PROCESSOR_ARCHITECTURE_AMD64) return " Professional x64 Edition";
        if (architecture == PROCESSOR_ARCHITECTURE_IA64) return " 64-Bit Edition";
        return "";
    }
    user = LoadLibraryA("user32.dll");
    metric = user ? (metric_fn)(void *)GetProcAddress(user, "GetSystemMetrics") : NULL;
    if (metric) {
        if (metric(87)) edition = " Media Center Edition";
        else if (metric(86)) edition = " Tablet PC Edition";
        else if (metric(88)) edition = " Starter Edition";
        else if (info.wSuiteMask & VER_SUITE_PERSONAL) edition = " Home Edition";
        else edition = " Professional";
    }
    if (user) FreeLibrary(user);
    return edition;
}

static void wx_identity_os(struct wx_system_identity *result)
{
    HKEY key;
    const char *product;
    struct wx_text text = {result->os, sizeof(result->os)};
    const BOOL nt = result->platform == VER_PLATFORM_WIN32_NT;
    const char *path = nt ? "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion" :
                            "SOFTWARE\\Microsoft\\Windows\\CurrentVersion";
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, path, 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS) {
        wx_identity_reg(key, "ProductName", REG_SZ, result->product_name, sizeof(result->product_name));
        RegCloseKey(key);
    }
    product = result->product_name;
    if (!*product && nt) {
        char kind[24] = "";
        BOOL server = FALSE;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\ProductOptions",
                          0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS) {
            if (wx_identity_reg(key, "ProductType", REG_SZ, kind, sizeof(kind)))
                server = !lstrcmpiA(kind, "LanmanNT") || !lstrcmpiA(kind, "ServerNT");
            RegCloseKey(key);
        }
        product = server && result->major == 3 && result->minor == 10 ? "Windows NT Advanced Server 3.1" :
                  server ? "Windows NT Server" : "Windows NT";
    }
    if (!*product && result->major == 4) {
        if (result->minor == 0) product = "Microsoft Windows 95";
        else if (result->minor == 10) product = "Microsoft Windows 98";
        else if (result->minor == 90) product = "Microsoft Windows Millennium Edition";
    }
    if (!*product) product = "Windows";
    wx_identity_text(&text, product);
    wx_identity_text(&text, wx_identity_xp_edition(result));
    /* The original release was marketed as NT 3.1; the Win32 API and
     * WINVER report minor version 10. Preserve both release and kernel names. */
    if (nt && result->major == 3 && result->minor == 10 &&
            (lstrcmpiA(product,"Windows NT") == 0 || lstrcmpiA(product,"Microsoft Windows NT") == 0))
        wx_identity_text(&text, " 3.1");
    wx_identity_text(&text, " (");
    wx_identity_number(&text, result->major);
    wx_identity_text(&text, ".");
    wx_identity_number(&text, result->minor);
    wx_identity_text(&text, ", Build ");
    wx_identity_number(&text, result->build);
    wx_identity_text(&text, ")");
    if (result->service_pack[0]) {
        wx_identity_text(&text, " ");
        wx_identity_text(&text, result->service_pack);
    }
}

#if defined(__i386__) || defined(__x86_64__)
static void wx_identity_word(char *dst, unsigned word)
{
    for (unsigned i = 0; i < 4; ++i) dst[i] = (char)(word >> (8 * i));
}

static void wx_identity_cpuid(struct wx_system_identity *result)
{
    unsigned a, b, c, d, family, model;
    unsigned max = __get_cpuid_max(0, NULL);
    if (!max) return;
    __cpuid(0, a, b, c, d);
    wx_identity_word(result->vendor, b);
    wx_identity_word(result->vendor + 4, d);
    wx_identity_word(result->vendor + 8, c);
    result->vendor[12] = 0;
    __cpuid(1, a, b, c, d);
    result->signature = a;
    family = (a >> 8) & 15;
    model = (a >> 4) & 15;
    result->family = family == 15 ? family + ((a >> 20) & 255) : family;
    result->model = (family == 6 || family == 15) ? model | ((a >> 12) & 0xf0) : model;
    result->stepping = a & 15;
    result->has_signature = TRUE;
    if (__get_cpuid_max(0x80000000u, NULL) >= 0x80000004u) {
        for (unsigned i = 0; i < 3; ++i) {
            char *p = result->brand + i * 16;
            __cpuid(0x80000002u + i, a, b, c, d);
            wx_identity_word(p, a); wx_identity_word(p + 4, b);
            wx_identity_word(p + 8, c); wx_identity_word(p + 12, d);
        }
        result->brand[48] = 0;
        wx_identity_simplify(result->brand);
    }
    if (max >= 0x16) {
        __cpuid(0x16, a, b, c, d);
        result->mhz = a & 0xffff;
    }
}
#endif

static void wx_identity_processor(struct wx_system_identity *result)
{
    SYSTEM_INFO info;
    HKEY key;
    char identifier[96] = {0};
    struct wx_text text = {result->processor, sizeof(result->processor)};
    GetSystemInfo(&info);
    result->logical_processors = info.dwNumberOfProcessors;
#if defined(__i386__) || defined(__x86_64__)
    wx_identity_cpuid(result);
#endif
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                      0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS) {
        if (!result->brand[0]) wx_identity_reg(key, "ProcessorNameString", REG_SZ, result->brand, sizeof(result->brand));
        if (!result->vendor[0]) wx_identity_reg(key, "VendorIdentifier", REG_SZ, result->vendor, sizeof(result->vendor));
        wx_identity_reg(key, "Identifier", REG_SZ, identifier, sizeof(identifier));
        if (!result->mhz) wx_identity_reg(key, "~MHz", REG_DWORD, &result->mhz, sizeof(result->mhz));
        RegCloseKey(key);
    }

    if (result->brand[0]) wx_identity_text(&text, result->brand);
    else if (result->has_signature) {
        /* Intel family 6/model 5 is shared by Pentium II, Celeron and Xeon.
         * The signature identifies the microarchitecture, not a retail SKU. */
        if (!lstrcmpA(result->vendor, "GenuineIntel") && result->family == 6 &&
                (result->model == 3 || result->model == 5 || result->model == 6))
            wx_identity_text(&text, "Intel Pentium II-class");
        else if (!lstrcmpA(result->vendor, "GenuineIntel") && result->family == 6 &&
                (result->model == 7 || result->model == 8 || result->model == 10 || result->model == 11))
            wx_identity_text(&text, "Intel Pentium III-class");
        else wx_identity_text(&text, "x86 processor");
    } else if (identifier[0]) wx_identity_text(&text, identifier);
    else {
        wx_identity_text(&text, "Windows processor type ");
        wx_identity_number(&text, info.dwProcessorType);
    }
    if (result->has_signature) {
        wx_identity_text(&text, " (");
        wx_identity_text(&text, result->vendor);
        wx_identity_text(&text, ", Family "); wx_identity_number(&text, result->family);
        wx_identity_text(&text, " Model "); wx_identity_number(&text, result->model);
        wx_identity_text(&text, " Stepping "); wx_identity_number(&text, result->stepping);
        wx_identity_text(&text, ")");
    }
    wx_identity_text(&text, ", ");
    wx_identity_number(&text, result->logical_processors);
    wx_identity_text(&text, result->logical_processors == 1 ? " logical processor" : " logical processors");
    if (result->mhz) {
        wx_identity_text(&text, ", ~");
        wx_identity_number(&text, result->mhz);
        wx_identity_text(&text, " MHz");
    }
}

static BOOL wx_read_system_identity(struct wx_system_identity *result)
{
    if (!result) return FALSE;
    for (unsigned i = 0; i < sizeof(*result); ++i) ((BYTE *)result)[i] = 0;
    if (!wx_identity_version(result)) return FALSE;
    wx_identity_os(result);
    wx_identity_processor(result);
    return TRUE;
}

#endif
