#include "queue.h"

/* All supported VMM guests run on one processor. Preserve the caller's x86
 * interrupt-enable state across nested queue and cancel locks. */
extern KIRQL WkMaskInterrupts(void);
extern void WkRestoreInterrupts(KIRQL previous);
extern ULONG WkScheduleEvent(void);
extern void WkCancelEvent(ULONG event);
extern int WkVxdQuery(void);
static KSPIN_LOCK cancelLock;
static ULONG event;
static BOOLEAN stopping;

VOID NTAPI KeInitializeSpinLock(PKSPIN_LOCK lock)
{
    *lock = 0;
}

KIRQL FASTCALL KfAcquireSpinLock(PKSPIN_LOCK lock)
{
    KIRQL previous = WkMaskInterrupts();
    *lock = 1;
    return previous;
}

VOID FASTCALL KfReleaseSpinLock(PKSPIN_LOCK lock, KIRQL previous)
{
    *lock = 0;
    WkRestoreInterrupts(previous);
}

VOID NTAPI IoAcquireCancelSpinLock(PKIRQL previous)
{
    *previous = KfAcquireSpinLock(&cancelLock);
}

VOID NTAPI IoReleaseCancelSpinLock(KIRQL previous)
{
    KfReleaseSpinLock(&cancelLock, previous);
}

BOOLEAN NTAPI IoCancelIrp(PIRP irp)
{
    PDRIVER_CANCEL routine;
    KIRQL previous;
    IoAcquireCancelSpinLock(&previous);
    irp->Cancel = TRUE;
    routine = IoSetCancelRoutine(irp, NULL);
    if (routine) {
        PDEVICE_OBJECT device = irp->CurrentLocation <= irp->StackCount
            ? IoGetCurrentIrpStackLocation(irp)->DeviceObject : NULL;
        irp->CancelIrql = previous;
        routine(device, irp);
        return TRUE;
    }
    IoReleaseCancelSpinLock(previous);
    return FALSE;
}

NTSTATUS WkStartWorker(void)
{
    event = 0;
    stopping = FALSE;
    return STATUS_SUCCESS;
}

BOOLEAN WkWakeWorker(void)
{
    KIRQL previous = WkMaskInterrupts();
    BOOLEAN scheduled;
    if (!event && !stopping)
        event = WkScheduleEvent();
    scheduled = event != 0;
    WkRestoreInterrupts(previous);
    return scheduled;
}

int WkRunEvent(void)
{
    KIRQL previous = WkMaskInterrupts();
    event = 0;
    WkRestoreInterrupts(previous);
    WkDrainRequests(STATUS_SUCCESS);
    return WkVxdQuery();
}

VOID WkStopWorker(void)
{
    KIRQL previous = WkMaskInterrupts();
    stopping = TRUE;
    if (event) {
        WkCancelEvent(event);
        event = 0;
    }
    WkRestoreInterrupts(previous);
}
