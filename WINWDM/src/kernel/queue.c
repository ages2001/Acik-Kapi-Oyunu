#include "wdm_target.h"
#include "queue.h"
#include "wdm_csq.h"
#include "wdm_remove_lock.h"
#include "wdm_irp.h"
#include "protocol.h"

#define WK_INTERNAL_HOLD (WK_IOCTL + 8)

typedef struct {
    PIRP irp;
    ULONG reply;
    PIO_REMOVE_LOCK lifetime;
    BOOLEAN expectCanceled;
    BOOLEAN passed;
} QUEUE_PROBE;

static IO_CSQ requests;
static LIST_ENTRY list;
static KSPIN_LOCK lock;
static BOOLEAN stopping;
static QUEUE_PROBE held, delayed, removing;

static VOID NTAPI acquire(PIO_CSQ q, PKIRQL irql)
{
    (void)q;
    KeAcquireSpinLock(&lock, irql);
}

static VOID NTAPI release(PIO_CSQ q, KIRQL irql)
{
    (void)q;
    KeReleaseSpinLock(&lock, irql);
}

static NTSTATUS NTAPI insert(PIO_CSQ q, PIRP irp, PVOID context)
{
    (void)q;
    (void)context;
    if (stopping)
        return STATUS_DELETE_PENDING;
    InsertTailList(&list, &irp->Tail.Overlay.ListEntry);
    return STATUS_SUCCESS;
}

static VOID NTAPI removeIrp(PIO_CSQ q, PIRP irp)
{
    (void)q;
    RemoveEntryList(&irp->Tail.Overlay.ListEntry);
}

/* The list address selects runnable work. A file object selects cleanup for
 * one open handle. NULL includes the deliberately held internal cancel probe. */
static PIRP NTAPI peek(PIO_CSQ q, PIRP previous, PVOID selector)
{
    PLIST_ENTRY item = previous ? previous->Tail.Overlay.ListEntry.Flink : list.Flink;
    (void)q;
    for (; item != &list; item = item->Flink) {
        PIRP irp = CONTAINING_RECORD(item, IRP, Tail.Overlay.ListEntry);
        PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
        if (!selector || (selector == &list
                ? stack->Parameters.DeviceIoControl.IoControlCode != WK_INTERNAL_HOLD
                : (PVOID)stack->FileObject == selector))
            return irp;
    }
    return NULL;
}

static NTSTATUS finish(PIRP irp, NTSTATUS status, ULONG bytes)
{
    irp->IoStatus.Status = status;
    irp->IoStatus.Information = bytes;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}

static VOID NTAPI cancelIrp(PIO_CSQ q, PIRP irp)
{
    (void)q;
    finish(irp, STATUS_CANCELLED, 0);
}

VOID WkDrainRequests(NTSTATUS status)
{
    PIRP irp;
    while ((irp = IoCsqRemoveNextIrp(&requests, &list)) != NULL) {
        if (NT_SUCCESS(status)) {
            *(PULONG)irp->AssociatedIrp.SystemBuffer = WK_SIGNATURE;
            finish(irp, status, sizeof(ULONG));
        } else {
            finish(irp, status, 0);
        }
    }
}

NTSTATUS WkQueueRequest(PIRP irp)
{
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
    ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;
    BOOLEAN hold = code == WK_INTERNAL_HOLD && stack->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL;
    NTSTATUS status;
    if (!hold && code != WK_IOCTL_DEFERRED)
        return finish(irp, STATUS_INVALID_DEVICE_REQUEST, 0);
    if (stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(ULONG) ||
        !irp->AssociatedIrp.SystemBuffer)
        return finish(irp, STATUS_BUFFER_TOO_SMALL, 0);
    status = IoCsqInsertIrpEx(&requests, irp, NULL, NULL);
    if (!NT_SUCCESS(status))
        return finish(irp, status, 0);
    /* A concurrent cancellation may already have completed the IRP. */
    if (!hold && !WkWakeWorker())
        WkDrainRequests(STATUS_INSUFFICIENT_RESOURCES);
    return STATUS_PENDING;
}

VOID WkCleanupRequests(PFILE_OBJECT file)
{
    PIRP irp;
    if (file)
        while ((irp = IoCsqRemoveNextIrp(&requests, file)) != NULL)
            cancelIrp(&requests, irp);
}

static NTSTATUS NTAPI probeComplete(PDEVICE_OBJECT device, PIRP irp, PVOID context)
{
    QUEUE_PROBE *p = context;
    PIO_REMOVE_LOCK lifetime = p->lifetime;
    KIRQL irql;
    BOOLEAN ok = p->expectCanceled
        ? irp->IoStatus.Status == STATUS_CANCELLED && irp->IoStatus.Information == 0
        : irp->IoStatus.Status == STATUS_SUCCESS && irp->IoStatus.Information == sizeof(ULONG)
          && p->reply == WK_SIGNATURE;
    (void)device;
    /* The driver allocated this IRP. Returning MORE_PROCESSING_REQUIRED
     * transfers the final disposal to this completion routine. */
    IoFreeIrp(irp);
    acquire(&requests, &irql);
    p->irp = NULL;
    p->passed = ok;
    release(&requests, irql);
    if (lifetime)
        IoReleaseRemoveLock(lifetime, p);
    return STATUS_MORE_PROCESSING_REQUIRED;
}

static NTSTATUS issue(PDEVICE_OBJECT device, QUEUE_PROBE *p, BOOLEAN hold, PIO_REMOVE_LOCK lifetime)
{
    PIO_STACK_LOCATION stack;
    PIRP irp;
    CCHAR depth = device->StackSize;
    UCHAR allocationFlags;
    USHORT size;
    if (device->StackSize < 1 || device->StackSize == 127)
        return STATUS_INVALID_DEVICE_STATE;
    if (WkTargetVersion() == WK_FULL_TARGET && depth < 12) depth = 12;
    irp = IoAllocateIrp(depth, FALSE);
    if (!irp)
        return STATUS_INSUFFICIENT_RESOURCES;
    size = irp->Size;
    allocationFlags = irp->AllocationFlags;
    stack = IoGetNextIrpStackLocation(irp);
    stack->Parameters.DeviceIoControl.OutputBufferLength = 123;
    ((PIO_STACK_LOCATION)(irp+1))->Control = SL_PENDING_RETURNED;
    irp->UserBuffer = p;
    irp->PendingReturned = TRUE;
    IoReuseIrp(irp, STATUS_NOT_SUPPORTED);
    if (irp->Size != size || irp->StackCount != depth || irp->CurrentLocation != depth+1 ||
        irp->AllocationFlags != allocationFlags || irp->IoStatus.Status != STATUS_NOT_SUPPORTED ||
        irp->UserBuffer || irp->PendingReturned || IoGetNextIrpStackLocation(irp) != stack ||
        stack->Parameters.DeviceIoControl.OutputBufferLength || ((PIO_STACK_LOCATION)(irp+1))->Control) {
        IoFreeIrp(irp);
        return STATUS_DEVICE_DATA_ERROR;
    }
    if (lifetime) {
        NTSTATUS status = IoAcquireRemoveLock(lifetime, p);
        if (!NT_SUCCESS(status)) {
            IoFreeIrp(irp);
            return status;
        }
    }
    p->irp = irp;
    p->lifetime = lifetime;
    p->expectCanceled = hold;
    p->passed = FALSE;
    p->reply = 0;
    irp->AssociatedIrp.SystemBuffer = &p->reply;
    irp->Flags = IRP_BUFFERED_IO;
    stack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
    stack->Parameters.DeviceIoControl.IoControlCode = hold ? WK_INTERNAL_HOLD : WK_IOCTL_DEFERRED;
    stack->Parameters.DeviceIoControl.OutputBufferLength = sizeof(p->reply);
    IoSetCompletionRoutine(irp, probeComplete, p, TRUE, TRUE, TRUE);
    return IoCallDriver(device, irp);
}

NTSTATUS WkQueueRemovalProbe(PDEVICE_OBJECT device, PIO_REMOVE_LOCK lifetime)
{
    return issue(device, &removing, FALSE, lifetime);
}

BOOLEAN WkRemovalProbePassed(void)
{
    KIRQL irql;
    BOOLEAN passed;
    acquire(&requests, &irql);
    passed = removing.passed;
    release(&requests, irql);
    return passed;
}

BOOLEAN WkQueueReady(void)
{
    KIRQL irql;
    BOOLEAN ready;
    acquire(&requests, &irql);
    ready = held.passed && delayed.passed;
    release(&requests, irql);
    return ready;
}

NTSTATUS WkStartQueue(PDEVICE_OBJECT device)
{
    NTSTATUS status;
    InitializeListHead(&list);
    KeInitializeSpinLock(&lock);
    stopping = FALSE;
    status = IoCsqInitializeEx(&requests, insert, removeIrp, peek, acquire, release, cancelIrp);
    if (!NT_SUCCESS(status))
        return status;
    status = WkStartWorker();
    if (!NT_SUCCESS(status))
        return status;
    if (issue(device, &held, TRUE, NULL) == STATUS_PENDING && IoCancelIrp(held.irp) &&
        issue(device, &delayed, FALSE, NULL) == STATUS_PENDING)
        return STATUS_SUCCESS;
    WkStopQueue();
    return STATUS_UNSUCCESSFUL;
}

VOID WkStopQueue(void)
{
    KIRQL irql;
    PIRP irp;
    acquire(&requests, &irql);
    stopping = TRUE;
    release(&requests, irql);
    WkStopWorker();
    while ((irp = IoCsqRemoveNextIrp(&requests, NULL)) != NULL)
        cancelIrp(&requests, irp);
}
