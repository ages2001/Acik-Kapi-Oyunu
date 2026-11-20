/* Additional x86 NT 3.1 kernel services used by the Forceware miniports.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <ntddk.h>
#include <stdarg.h>

__declspec(dllimport) extern PUSHORT NlsLeadByteInfo;
__declspec(dllimport) extern BOOLEAN NlsMbCodePageTag;
extern int __cdecl _vsnprintf(char *, size_t, const char *, va_list);
extern VOID NTAPI NativeCopy(PVOID, const VOID *, SIZE_T) __asm__("_RtlMoveMemory@12");
extern LONG_PTR NTAPI NativeDereference(PVOID) __asm__("_ObDereferenceObject@4");
extern BOOLEAN wxp_kernel_version(PULONG, PULONG, PULONG, PBOOLEAN);

/* Layout of the fields used here in the x86 NT 3.1 executive thread. */
typedef struct {
    UCHAR before_stack[0x178];
    ULONG_PTR initial_stack;
    UCHAR before_apc[0x194 - 0x17c];
    volatile LONG kernel_apc_disable;
    UCHAR before_id[0x208 - 0x198];
    CLIENT_ID id;
} WXP_THREAD31;
C_ASSERT(FIELD_OFFSET(WXP_THREAD31, initial_stack) == 0x178);
C_ASSERT(FIELD_OFFSET(WXP_THREAD31, kernel_apc_disable) == 0x194);
C_ASSERT(FIELD_OFFSET(WXP_THREAD31, id) == 0x208);

/* Both native NT 3.1 x86 probe routines exclude this address and above. */
const ULONG WxpUserProbeAddress = 0x7fff0000;

VOID NTAPI WxpStackLimits(PULONG_PTR low, PULONG_PTR high)
{
    const WXP_THREAD31 *thread = (const WXP_THREAD31 *)KeGetCurrentThread();
    /* NT 3.1 uses two fixed x86 kernel stack pages, ending at InitialStack. */
    *high = thread->initial_stack;
    *low = *high - 2 * PAGE_SIZE;
}

HANDLE NTAPI WxpCurrentProcessId(void)
{
    const WXP_THREAD31 *thread = (const WXP_THREAD31 *)KeGetCurrentThread();
    return thread->id.UniqueProcess;
}

USHORT NTAPI WxpCaptureStack(ULONG skip, ULONG count, PVOID *trace, PULONG hash)
{
    ULONG_PTR low, high, frame = (ULONG_PTR)__builtin_frame_address(0);
    ULONG sum = 0;
    USHORT captured = 0;
    if (hash) *hash = 0;
    /* XP limits the total number of skipped and returned frames to 62. */
    if (!trace || !count || count >= 63 || skip >= 63 - count) return 0;
    WxpStackLimits(&low, &high);
    while (captured < count && !(frame & 3) && frame >= low &&
           frame <= high - 2 * sizeof(ULONG_PTR)) {
        const ULONG_PTR *saved = (const ULONG_PTR *)frame;
        ULONG_PTR previous = saved[0], address = saved[1];
        /* The x86 NT 3.1 address split is fixed at 2 GiB. */
        if (address < 0x80000000UL) break;
        if (skip) {
            --skip;
        } else {
            trace[captured++] = (PVOID)address;
            sum += (ULONG)address;
        }
        if (previous <= frame) break;
        frame = previous;
    }
    if (hash) *hash = sum;
    return captured;
}

ULONG __cdecl WxpPrint(ULONG component, ULONG level, PCSTR format, ...)
{
    char text[512];
    va_list args;
    UNREFERENCED_PARAMETER(component);
    UNREFERENCED_PARAMETER(level);
    va_start(args, format);
    _vsnprintf(text, sizeof(text) - 1, format, args);
    va_end(args);
    text[sizeof(text) - 1] = 0;
    return DbgPrint("%s", text);
}

WCHAR NTAPI WxpAnsiChar(PUCHAR *cursor)
{
    WCHAR text[2] = {0};
    UCHAR *start = *cursor;
    ANSI_STRING source;
    UNICODE_STRING result = {0, sizeof(text), text};
    source.Length = (NlsMbCodePageTag && NlsLeadByteInfo[*start]) ? 2 : 1;
    source.MaximumLength = source.Length;
    source.Buffer = (PCHAR)start;
    *cursor += source.Length;
    RtlAnsiStringToUnicodeString(&result, &source, FALSE);
    return text[0];
}

KAFFINITY NTAPI WxpActiveProcessors(void)
{
    unsigned count = (unsigned char)KeNumberProcessors;
    return count < sizeof(KAFFINITY) * 8 ? ((KAFFINITY)1 << count) - 1 : ~(KAFFINITY)0;
}

PDEVICE_OBJECT NTAPI WxpAttachedReference(PDEVICE_OBJECT device)
{
    KIRQL old = KfRaiseIrql(DISPATCH_LEVEL);
    /* On the supported UP kernel DISPATCH_LEVEL serializes the attachment chain. */
    while (device->AttachedDevice) device = device->AttachedDevice;
    if (!NT_SUCCESS(ObReferenceObjectByPointer(device, 0, NULL, KernelMode))) device = NULL;
    KfLowerIrql(old);
    return device;
}

NTSTATUS NTAPI WxpDeviceList(PDRIVER_OBJECT driver, PDEVICE_OBJECT *list, ULONG bytes, PULONG total)
{
    PDEVICE_OBJECT d;
    ULONG count = 0, slots = list ? bytes / sizeof(*list) : 0;
    KIRQL old;
    if (!driver || !total || (!list && bytes)) return STATUS_INVALID_PARAMETER;
    old = KfRaiseIrql(DISPATCH_LEVEL);
    for (d = driver->DeviceObject; d; d = d->NextDevice) {
        if (count < slots) {
            NTSTATUS status = ObReferenceObjectByPointer(d, 0, NULL, KernelMode);
            if (!NT_SUCCESS(status)) {
                KfLowerIrql(old);
                while (count) NativeDereference(list[--count]);
                *total = 0;
                return status;
            }
            list[count] = d;
        }
        ++count;
    }
    KfLowerIrql(old);
    *total = count;
    return count > slots ? STATUS_BUFFER_TOO_SMALL : STATUS_SUCCESS;
}

/* In the NT 3.1 x86 KTHREAD the signed kernel APC disable count is a LONG.
   The native KeLeaveCriticalRegion increments this field and delivers pending
   APCs when the outermost region exits. */
VOID NTAPI WxpEnterCritical(void)
{
    WXP_THREAD31 *thread = (WXP_THREAD31 *)KeGetCurrentThread();
    --thread->kernel_apc_disable;
}

typedef struct {
    ULONG major, minor, build;
    USHORT sp_major, sp_minor;
    UCHAR product;
    BOOLEAN checked;
    WCHAR csd[128];
} WXP_VERSION;
static WXP_VERSION version;
static BOOLEAN version_ready;

static HANDLE open_key(PCWSTR path)
{
    OBJECT_ATTRIBUTES attributes;
    UNICODE_STRING name;
    HANDLE key;
    RtlInitUnicodeString(&name, path);
    InitializeObjectAttributes(&attributes, &name, OBJ_CASE_INSENSITIVE, NULL, NULL);
    return NT_SUCCESS(ZwOpenKey(&key, KEY_QUERY_VALUE, &attributes)) ? key : NULL;
}

static BOOLEAN version_string(HANDLE key, PCWSTR name, PWCHAR text, ULONG chars)
{
    ULONG data[72], actual;
    PKEY_VALUE_PARTIAL_INFORMATION info = (PKEY_VALUE_PARTIAL_INFORMATION)data;
    UNICODE_STRING value;
    RtlInitUnicodeString(&value, name);
    if (!NT_SUCCESS(ZwQueryValueKey(key, &value, KeyValuePartialInformation, data, sizeof(data), &actual)) ||
        info->Type != REG_SZ || (info->DataLength & 1) || info->DataLength > chars * sizeof(WCHAR)) return FALSE;
    NativeCopy(text, info->Data, info->DataLength);
    text[min(chars - 1, info->DataLength / sizeof(WCHAR))] = 0;
    return TRUE;
}

static ULONG decimal(const WCHAR **p)
{
    ULONG n = 0;
    while (**p >= L'0' && **p <= L'9') n = n * 10 + *(*p)++ - L'0';
    return n;
}

static BOOLEAN version_info(void)
{
    WXP_VERSION v = {0};
    WCHAR buf[128];
    const WCHAR *p, *start;
    HANDLE key;
    KIRQL old;
    if (version_ready) return TRUE;
    if (KeGetCurrentIrql() != PASSIVE_LEVEL) return FALSE;
    key = open_key(L"\\Registry\\Machine\\Software\\Microsoft\\Windows NT\\CurrentVersion");
    if (key && version_string(key, L"CurrentVersion", buf, 128)) {
        p = buf;
        v.major = decimal(&p);
        if (*p == L'.') {
            start = ++p;
            v.minor = decimal(&p);
            if (p - start == 1) v.minor *= 10;
        }
    }
    p = buf;
    if (key && version_string(key, L"CurrentBuildNumber", buf, 128)) v.build = decimal(&p);
    else if (key && version_string(key, L"CurrentBuild", buf, 128)) {
        v.build = decimal(&p);
        if (*p == L'.') { ++p; v.build = decimal(&p); }
    }
    if (key && version_string(key, L"CurrentType", buf, 128))
        for (p = buf; *p; ++p) if (*p == L'C' || *p == L'c') { v.checked = TRUE; break; }
    if (key) {
        version_string(key, L"CSDVersion", v.csd, 128);
        ZwClose(key);
    }
    for (p = v.csd; *p && (*p < L'0' || *p > L'9'); ++p) {}
    v.sp_major = (USHORT)decimal(&p);
    if (*p == L'.') { ++p; v.sp_minor = (USHORT)decimal(&p); }
    if ((!v.major || !v.build) && !wxp_kernel_version(&v.major, &v.minor, &v.build, &v.checked)) return FALSE;
    v.product = VER_NT_WORKSTATION;
    key = open_key(L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\ProductOptions");
    if (key) {
        if (version_string(key, L"ProductType", buf, 128)) {
            if (buf[0] == L'S' || buf[0] == L's') v.product = VER_NT_SERVER;
            if (buf[0] == L'L' || buf[0] == L'l') v.product = VER_NT_DOMAIN_CONTROLLER;
        }
        ZwClose(key);
    }
    old = KfRaiseIrql(DISPATCH_LEVEL);
    if (!version_ready) {
        NativeCopy(&version, &v, sizeof(v));
        version_ready = TRUE;
    }
    KfLowerIrql(old);
    return TRUE;
}

BOOLEAN NTAPI WxpPsVersion(PULONG major, PULONG minor, PULONG build, PUNICODE_STRING csd)
{
    version_info();
    if (major) *major = version.major;
    if (minor) *minor = version.minor;
    if (build) *build = version.build;
    if (csd) RtlInitUnicodeString(csd, version.csd);
    return version.checked;
}

NTSTATUS NTAPI WxpRtlVersion(PRTL_OSVERSIONINFOW info)
{
    ULONG size;
    if (!info || ((size = info->dwOSVersionInfoSize) != sizeof(*info) && size != sizeof(RTL_OSVERSIONINFOEXW)))
        return STATUS_INVALID_PARAMETER;
    if (!version_info()) return STATUS_UNSUCCESSFUL;
    info->dwMajorVersion = version.major;
    info->dwMinorVersion = version.minor;
    info->dwBuildNumber = version.build;
    info->dwPlatformId = VER_PLATFORM_WIN32_NT;
    NativeCopy(info->szCSDVersion, version.csd, sizeof(version.csd));
    if (size == sizeof(RTL_OSVERSIONINFOEXW)) {
        PRTL_OSVERSIONINFOEXW ext = (PRTL_OSVERSIONINFOEXW)info;
        ext->wServicePackMajor = version.sp_major;
        ext->wServicePackMinor = version.sp_minor;
        ext->wSuiteMask = 0;
        ext->wProductType = version.product;
        ext->wReserved = 0;
    }
    return STATUS_SUCCESS;
}
