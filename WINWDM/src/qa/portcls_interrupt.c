/* Separate PE consumer of the PortCls exports on a single-CPU QEMU EDU fixture. */
#include "../kernel/portcls_x86.h"

typedef struct {
    IResourceList Interface;
    CM_PARTIAL_RESOURCE_DESCRIPTOR Line;
} TEST_RESOURCES;

typedef struct {
    PULONG Registers;
    volatile ULONG Sequence;
    volatile ULONG EmptyPass;
    volatile BOOLEAN Handled;
    BOOLEAN Tracking, TrackingEmpty;
    KIRQL Level;
} TEST_INTERRUPT;

static PCM_PARTIAL_RESOURCE_DESCRIPTOR NTAPI findLine(IResourceList *iface, CM_RESOURCE_TYPE type, ULONG index)
{
    TEST_RESOURCES *resources = CONTAINING_RECORD(iface, TEST_RESOURCES, Interface);
    return type == CmResourceTypeInterrupt && !index ? &resources->Line : NULL;
}

static IResourceListVtbl resourceMethods = { .FindTranslatedEntry = findLine };
static const GUID interruptId = {0x22c6ac63, 0x851b, 0x11d0, {0x9a,0x7f,0,0xaa,0,0x38,0xac,0xfe}};

static NTSTATUS NTAPI measureLevel(IInterruptSync *iface, PVOID context)
{
    TEST_INTERRUPT *test = context;
    (void)iface;
    test->Level = KeGetCurrentIrql();
    return STATUS_DEVICE_BUSY;
}

static void record(TEST_INTERRUPT *test, unsigned id)
{
    if (test->Tracking) test->Sequence = (test->Sequence << 4) | id;
    if (test->TrackingEmpty) test->EmptyPass = (test->EmptyPass << 4) | id;
}

static NTSTATUS NTAPI stopRecording(IInterruptSync *iface, PVOID context)
{
    TEST_INTERRUPT *test = context;
    (void)iface;
    test->Tracking = FALSE;
    test->TrackingEmpty = FALSE;
    return STATUS_SUCCESS;
}

static NTSTATUS NTAPI first(IInterruptSync *iface, PVOID context)
{
    TEST_INTERRUPT *test = context;
    (void)iface;
    /* A shared line can trigger another unclaimed dispatch immediately after a
     * successful ISR. Keep its callbacks separate from the asserted EDU pass. */
    test->Tracking = (READ_REGISTER_ULONG(test->Registers + 0x24/4) & 1) != 0;
    test->TrackingEmpty = !test->Tracking && test->Handled && !test->EmptyPass;
    record(test, 1);
    return STATUS_UNSUCCESSFUL;
}

static NTSTATUS NTAPI acknowledge(IInterruptSync *iface, PVOID context)
{
    TEST_INTERRUPT *test = context;
    (void)iface;
    record(test, 2);
    if (!(READ_REGISTER_ULONG(test->Registers + 0x24/4) & 1)) return STATUS_UNSUCCESSFUL;
    WRITE_REGISTER_ULONG(test->Registers + 0x64/4, 1);
    test->Handled = TRUE;
    return STATUS_SUCCESS;
}

static NTSTATUS NTAPI last(IInterruptSync *iface, PVOID context)
{
    (void)iface;
    record(context, 3);
    return STATUS_UNSUCCESSFUL;
}

static NTSTATUS runMode(TEST_RESOURCES *resources, TEST_INTERRUPT *test, INTERRUPTSYNCMODE mode,
                        void (*report)(const char *, ULONG))
{
    PINTERRUPTSYNC sync = NULL, queried = NULL;
    LARGE_INTEGER delay;
    NTSTATUS status;
    unsigned attempt, expected = mode == InterruptSyncModeNormal ? 0x12 : 0x123;
    if (PcNewInterruptSync(&sync, NULL, &resources->Interface, 1, mode) != STATUS_INVALID_PARAMETER || sync)
        return STATUS_DEVICE_DATA_ERROR;
    status = PcNewInterruptSync(&sync, NULL, &resources->Interface, 0, mode);
    if (!NT_SUCCESS(status)) return status;
    status = sync->lpVtbl->QueryInterface(sync, &interruptId, (PVOID *)&queried);
    if (!NT_SUCCESS(status) || queried != sync || queried->lpVtbl->Release(queried) != 1 ||
        sync->lpVtbl->GetKInterrupt(sync)) goto fail;
    status = sync->lpVtbl->RegisterServiceRoutine(sync, acknowledge, test, FALSE);
    if (!NT_SUCCESS(status)) goto finish;
    status = sync->lpVtbl->RegisterServiceRoutine(sync, first, test, TRUE);
    if (!NT_SUCCESS(status)) goto finish;
    status = sync->lpVtbl->RegisterServiceRoutine(sync, last, test, FALSE);
    if (!NT_SUCCESS(status)) goto finish;
    if (sync->lpVtbl->CallSynchronizedRoutine(sync, measureLevel, test) != STATUS_DEVICE_BUSY ||
        test->Level != resources->Line.u.Interrupt.Level) goto fail;
    status = sync->lpVtbl->Connect(sync);
    if (!NT_SUCCESS(status)) goto finish;
    if (!sync->lpVtbl->GetKInterrupt(sync) ||
        sync->lpVtbl->CallSynchronizedRoutine(sync, measureLevel, test) != STATUS_DEVICE_BUSY) goto fail;
    test->Sequence = 0;
    test->EmptyPass = 0;
    test->Tracking = FALSE;
    test->TrackingEmpty = FALSE;
    test->Handled = FALSE;
    WRITE_REGISTER_ULONG(test->Registers + 0x60/4, 1);
    delay.QuadPart = -100000;
    for (attempt = 0; attempt < 200 && !test->Handled; ++attempt)
        KeDelayExecutionThread(KernelMode, FALSE, &delay);
    if (sync->lpVtbl->CallSynchronizedRoutine(sync, stopRecording, test) != STATUS_SUCCESS) goto fail;
    report("PortCls ISR order ", test->Sequence);
    report("PortCls first unclaimed pass ", test->EmptyPass);
    if (!test->Handled || test->Sequence != expected || READ_REGISTER_ULONG(test->Registers + 0x24/4)) goto fail;
    if (mode == InterruptSyncModeRepeat && test->EmptyPass != 0x123) goto fail;
    status = STATUS_SUCCESS;
    goto finish;
fail:
    status = STATUS_DEVICE_DATA_ERROR;
finish:
    WRITE_REGISTER_ULONG(test->Registers + 0x64/4, 1);
    sync->lpVtbl->Disconnect(sync);
    if (sync->lpVtbl->GetKInterrupt(sync)) status = STATUS_DEVICE_DATA_ERROR;
    if (sync->lpVtbl->Release(sync)) return STATUS_DEVICE_DATA_ERROR;
    return status;
}

NTSTATUS WkTestPortCls(PULONG registers, ULONG vector, KIRQL level, KAFFINITY affinity,
                       void (*report)(const char *, ULONG))
{
    TEST_RESOURCES resources = {0};
    TEST_INTERRUPT test = {0};
    ULONGLONG start, elapsed;
    LARGE_INTEGER delay;
    NTSTATUS status;
    INTERRUPTSYNCMODE mode;
    resources.Interface.lpVtbl = &resourceMethods;
    resources.Line.Type = CmResourceTypeInterrupt;
    resources.Line.ShareDisposition = CmResourceShareShared;
    resources.Line.u.Interrupt.Level = level;
    resources.Line.u.Interrupt.Vector = vector;
    resources.Line.u.Interrupt.Affinity = affinity;
    test.Registers = registers;
    start = PcGetTimeInterval(0);
    delay.QuadPart = -1000000;
    KeDelayExecutionThread(KernelMode, FALSE, &delay);
    elapsed = PcGetTimeInterval(start);
    report("PortCls time high ", (ULONG)(start >> 32));
    report("PortCls interval (100 ns) ", (ULONG)elapsed);
    if (!(start >> 32) || elapsed < 100000 || elapsed > 30000000) return STATUS_DEVICE_DATA_ERROR;
    for (mode = InterruptSyncModeNormal; mode <= InterruptSyncModeRepeat; ++mode) {
        status = runMode(&resources, &test, mode, report);
        if (!NT_SUCCESS(status)) return status;
    }
    return STATUS_SUCCESS;
}
