/* System-start qualification of the x86 NT 3.1 kernel compatibility calls.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <ntddk.h>
extern WCHAR NTAPI RtlAnsiCharToUnicodeChar(PUCHAR *);
extern NTSTATUS NTAPI IoEnumerateDeviceObjectList(PDRIVER_OBJECT, PDEVICE_OBJECT *, ULONG, PULONG);
extern LONG_PTR NTAPI native_release(PVOID) __asm__("_ObDereferenceObject@4");

static void msg(const char *s)
{
    while (*s) __asm__ volatile("outb %0,$0xe9" : : "a"(*s++));
}

static void number(ULONG n)
{
    char text[9];
    ULONG i;
    for (i = 8; i; --i) { text[i-1] = "0123456789abcdef"[n & 15]; n >>= 4; }
    text[8] = 0;
    msg(text);
}

static BOOLEAN check_version(void)
{
    RTL_OSVERSIONINFOEXW v = {0};
    UNICODE_STRING csd;
    ULONG major, minor, build;
    NTSTATUS status;
    PsGetVersion(&major, &minor, &build, &csd);
    v.dwOSVersionInfoSize = sizeof(v);
    status = RtlGetVersion((PRTL_OSVERSIONINFOW)&v);
    msg("Reported NT version "); number(major); msg("."); number(minor);
    msg(" build "); number(build); msg(" status "); number(status); msg("\n");
    if (status || major != 3 || minor != 10 || build != 528 ||
        v.dwMajorVersion != major || v.dwMinorVersion != minor || v.dwBuildNumber != build ||
        v.dwPlatformId != VER_PLATFORM_WIN32_NT || v.wProductType != VER_NT_WORKSTATION ||
        csd.Length >= sizeof(v.szCSDVersion)) return FALSE;
    v.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOW);
    if (!NT_SUCCESS(RtlGetVersion((PRTL_OSVERSIONINFOW)&v))) return FALSE;
    v.dwOSVersionInfoSize = 5;
    return RtlGetVersion((PRTL_OSVERSIONINFOW)&v) == STATUS_INVALID_PARAMETER;
}

static BOOLEAN apcs(void)
{
    volatile LONG *count = (volatile LONG *)((PUCHAR)KeGetCurrentThread() + 0x194);
    LONG before = *count;
    BOOLEAN ok;
    KeEnterCriticalRegion();
    KeEnterCriticalRegion();
    ok = *count == before - 2;
    KeLeaveCriticalRegion();
    ok = ok && *count == before - 1;
    KeLeaveCriticalRegion();
    return ok && *count == before;
}

static BOOLEAN device_objects(PDRIVER_OBJECT driver)
{
    PDEVICE_OBJECT p, bottom = NULL, top = NULL, list[2] = {NULL, NULL};
    ULONG count;
    BOOLEAN ok = FALSE;
    if (!NT_SUCCESS(IoCreateDevice(driver, 0, NULL, FILE_DEVICE_UNKNOWN, 0, FALSE, &bottom))) return FALSE;
    if (!NT_SUCCESS(IoCreateDevice(driver, 0, NULL, FILE_DEVICE_UNKNOWN, 0, FALSE, &top))) goto done;
    top->Flags &= ~DO_DEVICE_INITIALIZING;
    bottom->Flags &= ~DO_DEVICE_INITIALIZING;
    if (IoEnumerateDeviceObjectList(driver, NULL, 0, &count) != STATUS_BUFFER_TOO_SMALL || count != 2) goto done;
    if (IoEnumerateDeviceObjectList(driver, list, sizeof(*list), &count) != STATUS_BUFFER_TOO_SMALL ||
        count != 2 || !list[0] || list[1]) goto done;
    native_release(list[0]);
    if (IoEnumerateDeviceObjectList(driver, list, sizeof(list), &count) || count != 2) goto done;
    ok = (list[0] == top && list[1] == bottom) || (list[0] == bottom && list[1] == top);
    native_release(list[0]); native_release(list[1]);
    if (!ok) goto done;
    msg("Referenced device lists PASS\n");
    {
        NTSTATUS status = IoAttachDeviceByPointer(top, bottom);
        if (!NT_SUCCESS(status)) { msg("Attach status "); number(status); msg("\n"); ok = FALSE; goto done; }
    }
    p = IoGetAttachedDeviceReference(bottom);
    ok = p == top;
    if (p) native_release(p);
    IoDetachDevice(bottom);
    p = IoGetAttachedDeviceReference(bottom);
    ok = ok && p == bottom;
    if (p) native_release(p);
done:
    if (top) IoDeleteDevice(top);
    IoDeleteDevice(bottom);
    return ok;
}

NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING path)
{
    UCHAR source[] = { 'A', 0xe9, 0 };
    UCHAR *p = source;
    BOOLEAN ok = TRUE;
    UNREFERENCED_PARAMETER(path);
    msg("NT 3.1 extended kernel probe entered\n");
    if (check_version()) msg("Native version, product type and both OSVERSIONINFO sizes PASS\n");
    else { msg("Version FAIL\n"); ok = FALSE; }
    if (RtlAnsiCharToUnicodeChar(&p) == L'A' && p == source + 1 &&
        RtlAnsiCharToUnicodeChar(&p) == 0xe9 && p == source + 2 &&
        RtlAnsiCharToUnicodeChar(&p) == 0 && p == source + 3)
        msg("Installed ANSI code page and cursor advancement PASS\n");
    else { msg("Code page FAIL\n"); ok = FALSE; }
    DbgPrintEx(0, 3, "WINXPDM NT %s %lu\n", "build", 528UL);
    if (KeQueryActiveProcessors() == 1 && apcs()) msg("Processor mask and nested APC disable/enable PASS\n");
    else { msg("Processor or APC FAIL\n"); ok = FALSE; }
    if (device_objects(driver)) msg("Referenced full and partial device lists, attach and detach PASS\n");
    else { msg("Device objects FAIL\n"); ok = FALSE; }
    msg(ok ? "TEST COMPLETE PASS\n" : "TEST COMPLETE FAIL\n");
    return ok ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}
