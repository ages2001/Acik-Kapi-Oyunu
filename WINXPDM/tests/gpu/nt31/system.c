/* System-start qualification of the x86 NT 3.1 kernel compatibility calls.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <ntddk.h>
extern WCHAR NTAPI RtlAnsiCharToUnicodeChar(PUCHAR *);
extern USHORT NTAPI RtlCaptureStackBackTrace(ULONG, ULONG, PVOID *, PULONG);
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

static BOOLEAN check_routines(void)
{
    extern PVOID bound_event __asm__("__imp__KeSetEvent@12");
    extern PVOID bound_clock __asm__("__imp__KeStallExecutionProcessor@4");
    extern PVOID bound_pci __asm__("__imp__HalGetBusDataByOffset@24");
    UNICODE_STRING s;
    RtlInitUnicodeString(&s, L"KeSetEvent#");
    s.Length -= sizeof(WCHAR);
    if (MmGetSystemRoutineAddress(&s) != bound_event) return FALSE;
    RtlInitUnicodeString(&s, L"KeStallExecutionProcessor");
    if (MmGetSystemRoutineAddress(&s) != bound_clock) return FALSE;
    RtlInitUnicodeString(&s, L"HalGetBusDataByOffset");
    if (MmGetSystemRoutineAddress(&s) != bound_pci) return FALSE;
    RtlInitUnicodeString(&s, L"KeQueryActiveProcessors");
    KAFFINITY (NTAPI *cpu_mask)(void) = MmGetSystemRoutineAddress(&s);
    if (!cpu_mask || cpu_mask() != KeQueryActiveProcessors()) return FALSE;
    RtlInitUnicodeString(&s, L"MmGetSystemRoutineAddress");
    PVOID (NTAPI *lookup)(PUNICODE_STRING) = MmGetSystemRoutineAddress(&s);
    if (!lookup || lookup(&s) != (PVOID)lookup) return FALSE;
    RtlInitUnicodeString(&s, L"kesetevent");
    if (lookup(&s)) return FALSE;
    RtlInitUnicodeString(&s, L"VideoPortInitialize");
    return !lookup(&s);
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

typedef struct {
    KEVENT done;
    ULONG_PTR low, high;
    HANDLE process;
    BOOLEAN valid;
} STACK_PROBE;

static BOOLEAN stack_bounds(PULONG_PTR low, PULONG_PTR high)
{
    ULONG_PTR here = (ULONG_PTR)&here, raised_low, raised_high;
    HANDLE pid = PsGetCurrentProcessId();
    KIRQL irql;
    IoGetStackLimits(low, high);
    irql = KfRaiseIrql(DISPATCH_LEVEL);
    IoGetStackLimits(&raised_low, &raised_high);
    BOOLEAN same = pid == PsGetCurrentProcessId();
    KfLowerIrql(irql);
    return same && *low == raised_low && *high == raised_high &&
           *high - *low == 2 * PAGE_SIZE && here >= *low && here < *high;
}

static VOID NTAPI check_worker(PVOID context)
{
    STACK_PROBE *probe = context;
    probe->valid = stack_bounds(&probe->low, &probe->high);
    probe->process = PsGetCurrentProcessId();
    KeSetEvent(&probe->done, IO_NO_INCREMENT, FALSE);
    PsTerminateSystemThread(STATUS_SUCCESS);
}

static BOOLEAN thread_services(void)
{
    ULONG_PTR low, high;
    STACK_PROBE probe;
    CLIENT_ID id;
    HANDLE thread;
    if (MmUserProbeAddress != 0x7fff0000 || !stack_bounds(&low, &high)) return FALSE;
    KeInitializeEvent(&probe.done, NotificationEvent, FALSE);
    if (!NT_SUCCESS(PsCreateSystemThread(&thread, THREAD_ALL_ACCESS, NULL, NULL,
                                         &id, check_worker, &probe))) return FALSE;
    KeWaitForSingleObject(&probe.done, Executive, KernelMode, FALSE, NULL);
    ZwClose(thread);
    return probe.valid && probe.process == id.UniqueProcess &&
           (high <= probe.low || probe.high <= low);
}

/* Supply frame pointers and a repeatable call site on the x86 kernel stack. */
__attribute__((noinline, noclone, optimize("no-omit-frame-pointer", "no-optimize-sibling-calls")))
static USHORT collect_trace(unsigned depth, ULONG skip, ULONG count, PVOID *trace, PULONG hash)
{
    if (depth) return collect_trace(depth - 1, skip, count, trace, hash);
    return RtlCaptureStackBackTrace(skip, count, trace, hash);
}

static BOOLEAN stack_trace(void)
{
    PVOID full[4] = {0}, tail[3] = {0};
    ULONG full_hash, tail_hash, empty_hash = 1;
    KIRQL old = KfRaiseIrql(DISPATCH_LEVEL);
    USHORT first = collect_trace(3, 0, 3, full, &full_hash);
    USHORT second = collect_trace(3, 1, 2, tail, &tail_hash);
    KfLowerIrql(old);
    if (first != 3 || second != 2 || !full[0] || full[0] == full[1] || full[3] || tail[2] ||
        full[1] != tail[0] || full[2] != tail[1] ||
        tail_hash != (ULONG)tail[0] + (ULONG)tail[1] ||
        full_hash != (ULONG)full[0] + tail_hash) return FALSE;
    return !collect_trace(0, 62, 1, tail + 2, &empty_hash) && !empty_hash &&
           !collect_trace(0, MAXULONG, 2, tail, NULL) &&
           !collect_trace(0, 0, 0, tail, NULL) && !tail[2];
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
    if (check_routines()) msg("Counted kernel and HAL routine names, companion calls and unknown names PASS\n");
    else { msg("System routine lookup FAIL\n"); ok = FALSE; }
    if (RtlAnsiCharToUnicodeChar(&p) == L'A' && p == source + 1 &&
        RtlAnsiCharToUnicodeChar(&p) == 0xe9 && p == source + 2 &&
        RtlAnsiCharToUnicodeChar(&p) == 0 && p == source + 3)
        msg("Installed ANSI code page and cursor advancement PASS\n");
    else { msg("Code page FAIL\n"); ok = FALSE; }
    DbgPrintEx(0, 3, "WINXPDM NT %s %lu\n", "build", 528UL);
    if (KeQueryActiveProcessors() == 1 && apcs()) msg("Processor mask and nested APC disable/enable PASS\n");
    else { msg("Processor or APC FAIL\n"); ok = FALSE; }
    if (thread_services()) msg("Two live stack ranges, original process ID at DISPATCH_LEVEL and user probe boundary PASS\n");
    else { msg("Thread or address limit FAIL\n"); ok = FALSE; }
    if (stack_trace()) msg("Kernel stack trace, skipped frames, hash and XP limits at DISPATCH_LEVEL PASS\n");
    else { msg("Kernel stack trace FAIL\n"); ok = FALSE; }
    if (device_objects(driver)) msg("Referenced full and partial device lists, attach and detach PASS\n");
    else { msg("Device objects FAIL\n"); ok = FALSE; }
    msg(ok ? "TEST COMPLETE PASS\n" : "TEST COMPLETE FAIL\n");
    return ok ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}
