/* Retail WDM remove-lock semantics. The baseline reference is held from
 * initialization to removal. The remover also supplies its own acquired tag. */
#include "wdm_remove_lock.h"

/* A zero KSPIN_LOCK is unlocked on the supported x86 targets. This lock
 * serializes count and Removed as one operation without requiring CMPXCHG. */
static KSPIN_LOCK accounting;

#ifdef WK_VMM
#include "vmm_event.h"
#define initEvent WkInitVmmEvent
#define signalEvent WkSignalVmmEvent
#define waitEvent WkWaitVmmEvent
/* One thread, the remover, may wait on any one remove lock. The opaque KEVENT
 * storage records a one-shot semaphore and a signal bit. No VMM resource is
 * retained for a lock whose removal never needs to block. */
extern ULONG WkCreateSemaphore(void);
extern void WkWaitSemaphore(ULONG semaphore);
extern void WkSignalSemaphore(ULONG semaphore);
extern void WkDestroySemaphore(ULONG semaphore);
extern void WkYieldVM(void);
extern KIRQL WkMaskInterrupts(void);
extern void WkRestoreInterrupts(KIRQL previous);

void initEvent(PKEVENT event)
{
    event->Header.SignalState=0;
    event->Header.WaitListHead.Flink=NULL;
    event->Header.WaitListHead.Blink=NULL;
}

void signalEvent(PKEVENT event)
{
    KIRQL previous=WkMaskInterrupts();
    ULONG semaphore=(ULONG)(ULONG_PTR)event->Header.WaitListHead.Flink;
    __atomic_store_n(&event->Header.SignalState,1,__ATOMIC_RELEASE);
    WkRestoreInterrupts(previous);
    if (semaphore) WkSignalSemaphore(semaphore);
}

void waitEvent(PKEVENT event)
{
    ULONG semaphore=WkCreateSemaphore();
    KIRQL previous=WkMaskInterrupts();
    BOOLEAN waiting=!event->Header.SignalState;
    if (waiting && semaphore)
        event->Header.WaitListHead.Flink=(PLIST_ENTRY)(ULONG_PTR)semaphore;
    WkRestoreInterrupts(previous);
    if (waiting && semaphore) {
        WkWaitSemaphore(semaphore);
    } else if (waiting) {
        /* Removing a device cannot fail for lack of a semaphore. Give the
         * scheduler a chance to run the code that owns the last reference. */
        while (!__atomic_load_n(&event->Header.SignalState,__ATOMIC_ACQUIRE))
            WkYieldVM();
    }
    if (semaphore) WkDestroySemaphore(semaphore);
}
#else
static void initEvent(PKEVENT event)
{
    KeInitializeEvent(event,SynchronizationEvent,FALSE);
}

static void signalEvent(PKEVENT event)
{
    KeSetEvent(event,IO_NO_INCREMENT,FALSE);
}

static void waitEvent(PKEVENT event)
{
    KeWaitForSingleObject(event,Executive,KernelMode,FALSE,NULL);
}
#endif

VOID NTAPI IoInitializeRemoveLockEx(PIO_REMOVE_LOCK lock,ULONG poolTag,
    ULONG maxMinutes,ULONG highWatermark,ULONG size)
{
    (void)poolTag;
    (void)maxMinutes;
    (void)highWatermark;
    (void)size;
    lock->Common.Removed=FALSE;
    lock->Common.IoCount=1;
    initEvent(&lock->Common.RemoveEvent);
}

NTSTATUS NTAPI IoAcquireRemoveLockEx(PIO_REMOVE_LOCK lock,PVOID tag,
    PCSTR file,ULONG line,ULONG size)
{
    KIRQL irql;
    NTSTATUS status=STATUS_SUCCESS;
    (void)tag;
    (void)file;
    (void)line;
    (void)size;
    KeAcquireSpinLock(&accounting,&irql);
    if (lock->Common.Removed)
        status=STATUS_DELETE_PENDING;
    else
        ++lock->Common.IoCount;
    KeReleaseSpinLock(&accounting,irql);
    return status;
}

VOID NTAPI IoReleaseRemoveLockEx(PIO_REMOVE_LOCK lock,PVOID tag,ULONG size)
{
    KIRQL irql;
    BOOLEAN last;
    (void)tag;
    (void)size;
    KeAcquireSpinLock(&accounting,&irql);
    last=--lock->Common.IoCount==0;
    KeReleaseSpinLock(&accounting,irql);
    if (last) signalEvent(&lock->Common.RemoveEvent);
}

VOID NTAPI IoReleaseRemoveLockAndWaitEx(PIO_REMOVE_LOCK lock,PVOID tag,ULONG size)
{
    KIRQL irql;
    BOOLEAN wait;
    (void)tag;
    (void)size;
    KeAcquireSpinLock(&accounting,&irql);
    lock->Common.Removed=TRUE;
    lock->Common.IoCount-=2;
    wait=lock->Common.IoCount!=0;
    KeReleaseSpinLock(&accounting,irql);
    if (wait) waitEvent(&lock->Common.RemoveEvent);
}
