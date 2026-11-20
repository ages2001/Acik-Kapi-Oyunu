/* Exercise the service ABI inside the NT 3.1 kernel.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <ntddk.h>
#undef KeClearEvent
#undef KeQueryInterruptTime
#undef KeQueryTickCount
#undef IoCompleteRequest
__declspec(dllimport) VOID NTAPI KeClearEvent(PRKEVENT);
__declspec(dllimport) ULONGLONG NTAPI KeQueryInterruptTime(void);
extern VOID NTAPI IoCompleteRequest(PIRP, CCHAR);
extern KIRQL NTAPI VideoPortGetCurrentIrql(VOID);
extern ULONG NTAPI VideoPortCreateEvent(PVOID, ULONG, PVOID, PVOID *);
extern ULONG NTAPI VideoPortWaitForSingleObject(PVOID, PVOID, PLARGE_INTEGER);
extern ULONG NTAPI VideoPortDeleteEvent(PVOID, PVOID);

static void message(const char *s)
{
    while (*s) __asm__ volatile("outb %0,$0xe9" : : "a"((UCHAR)*s++));
}

static BOOLEAN events(void)
{
    KEVENT native;
    LARGE_INTEGER poll;
    ULONG flag;
    poll.QuadPart = 0;
    KeInitializeEvent(&native, NotificationEvent, TRUE);
    KeClearEvent(&native);
    if (KeWaitForSingleObject(&native, Executive, KernelMode, FALSE, &poll) != STATUS_TIMEOUT)
        return FALSE;
    for (flag = 0; flag < 4; ++flag) {
        PVOID event = NULL;
        ULONG first, second;
        if (VideoPortCreateEvent(NULL, flag, NULL, &event) || !event) return FALSE;
        first = VideoPortWaitForSingleObject(NULL, event, &poll);
        second = VideoPortWaitForSingleObject(NULL, event, &poll);
        if (VideoPortDeleteEvent(NULL, event) || first != (flag & 2 ? 0UL : 258UL) ||
            second != (flag == 3 ? 0UL : 258UL)) return FALSE;
    }
    return VideoPortDeleteEvent(NULL, NULL) == 87 &&
           VideoPortWaitForSingleObject(NULL, NULL, &poll) == 87;
}

/* Restore ESP even on failure so a regression reports a diagnostic. */
extern int irql_stack_intact(void);
__asm__(".text\n.globl _irql_stack_intact\n_irql_stack_intact:\n"
        " pushl %esi\n movl %esp,%esi\n call _VideoPortGetCurrentIrql@0\n"
        " xorl %eax,%eax\n cmpl %esi,%esp\n sete %al\n"
        " movl %esi,%esp\n popl %esi\n ret\n");

static BOOLEAN time_and_mode(void)
{
    LARGE_INTEGER interval;
    ULONGLONG before, elapsed;
    KIRQL old, raised;
    if (!irql_stack_intact() || VideoPortGetCurrentIrql() != PASSIVE_LEVEL || ExGetPreviousMode() != KernelMode)
        return FALSE;
    old = KfRaiseIrql(DISPATCH_LEVEL);
    raised = VideoPortGetCurrentIrql();
    KfLowerIrql(old);
    before = KeQueryInterruptTime();
    interval.QuadPart = -1000000;
    KeDelayExecutionThread(KernelMode, FALSE, &interval);
    elapsed = KeQueryInterruptTime() - before;
    return raised == DISPATCH_LEVEL && elapsed + KeQueryTimeIncrement() >= 1000000 &&
           elapsed < 50000000;
}

typedef struct {
    PIO_WORKITEM item;
    PDEVICE_OBJECT device;
    ERESOURCE resource;
    ULONG guard;
    KEVENT done;
    BOOLEAN worker_ok;
} WORK_CONTEXT;

static VOID NTAPI worker(PDEVICE_OBJECT device, PVOID opaque)
{
    WORK_CONTEXT *c = opaque;
    BOOLEAN shared = ExAcquireResourceSharedLite(&c->resource, FALSE);
    if (shared) ExReleaseResourceLite(&c->resource);
    c->worker_ok = !shared && device == c->device && KeGetCurrentIrql() == PASSIVE_LEVEL;
    IoFreeWorkItem(c->item);
    KeSetEvent(&c->done, IO_NO_INCREMENT, FALSE);
}

static BOOLEAN work_and_lock(PDEVICE_OBJECT device)
{
    ULONG tag = 0x33505857;
    WORK_CONTEXT *c = ExAllocatePoolWithTag(NonPagedPool, sizeof(*c), tag);
    BOOLEAN ok = FALSE, locked;
    if (!c) return FALSE;
    c->device = device;
    c->worker_ok = FALSE;
    c->guard = 0x7ceffdad;
    KeInitializeEvent(&c->done, NotificationEvent, FALSE);
    if (!NT_SUCCESS(ExInitializeResourceLite(&c->resource))) goto free_context;
    /* No other thread has this resource during the initial acquisition. */
    locked = ExAcquireResourceExclusiveLite(&c->resource, FALSE);
    if (!locked) goto delete_resource;
    c->item = IoAllocateWorkItem(device);
    if (c->item) {
        IoQueueWorkItem(c->item, worker, DelayedWorkQueue, c);
        KeWaitForSingleObject(&c->done, Executive, KernelMode, FALSE, NULL);
        ok = c->worker_ok;
    }
    ExReleaseResourceLite(&c->resource);
    if (ExAcquireResourceSharedLite(&c->resource, TRUE)) ExReleaseResourceLite(&c->resource);
    else ok = FALSE;
delete_resource:
    if (!NT_SUCCESS(ExDeleteResourceLite(&c->resource)) || c->guard != 0x7ceffdad) ok = FALSE;
free_context:
    ExFreePoolWithTag(c, tag);
    return ok;
}

static NTSTATUS NTAPI dispatch(PDEVICE_OBJECT device, PIRP irp)
{
    UNREFERENCED_PARAMETER(device);
    irp->IoStatus.Status = STATUS_SUCCESS;
    irp->IoStatus.Information = 0;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

static BOOLEAN submit(PDEVICE_OBJECT device)
{
    KEVENT signal;
    IO_STATUS_BLOCK io;
    PIRP irp;
    io.Status = STATUS_UNSUCCESSFUL;
    KeInitializeEvent(&signal, NotificationEvent, FALSE);
    irp = IoBuildDeviceIoControlRequest(CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS),
                                       device, NULL, 0, NULL, 0, FALSE, &signal, &io);
    if (!irp || !NT_SUCCESS(IofCallDriver(device, irp))) return FALSE;
    return KeWaitForSingleObject(&signal, Executive, KernelMode, FALSE, NULL) == STATUS_SUCCESS &&
           io.Status == STATUS_SUCCESS;
}

NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING key)
{
    PDEVICE_OBJECT device;
    NTSTATUS status;
    UNREFERENCED_PARAMETER(key);
    message("NT 3.1 Forceware service companion loaded\n");
    if (!events()) { message("Video and kernel events FAIL\n"); return STATUS_UNSUCCESSFUL; }
    message("Four video event modes, poll, native clear and invalid handles PASS\n");
    if (!time_and_mode()) { message("IRQL, previous mode and timer FAIL\n"); return STATUS_UNSUCCESSFUL; }
    message("Zero-argument stack balance, raised and passive IRQL, previous mode and interrupt clock PASS\n");
    status = IoCreateDevice(driver, 0, NULL, FILE_DEVICE_UNKNOWN, 0, FALSE, &device);
    if (!NT_SUCCESS(status)) return status;
    driver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = dispatch;
    device->Flags &= ~DO_DEVICE_INITIALIZING;
    status = ObReferenceObjectByPointer(device, 0, NULL, KernelMode);
    if (!NT_SUCCESS(status)) { IoDeleteDevice(device); return status; }
    ObfDereferenceObject(device);
    message("XP fastcall object dereference on a referenced native device PASS\n");
    if (!work_and_lock(device)) { message("Worker, pool and resource FAIL\n"); status = STATUS_UNSUCCESSFUL; }
    else {
        message("Tagged pool, cross-thread resource exclusion and self-freeing work item PASS\n");
        if (!submit(device)) { message("I/O submission FAIL\n"); status = STATUS_UNSUCCESSFUL; }
        else message("XP fastcall dispatch and completed native IRP PASS\n");
    }
    IoDeleteDevice(device);
    return status;
}
