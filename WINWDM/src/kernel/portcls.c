/* Native x86 NT implementation of the PortCls line interrupt synchronization object. */
#define PC_NO_IMPORTS
#include "portcls_x86.h"
#include "portcls_dispatch.h"

/* Original stdcall entry points are present on NT 3.1 through NT 4.0. */
#undef KeRaiseIrql
#undef KeLowerIrql
#undef KeAcquireSpinLockAtDpcLevel
#undef KeReleaseSpinLockFromDpcLevel
#undef KeQuerySystemTime
NTHALAPI VOID NTAPI KeRaiseIrql(KIRQL newLevel, PKIRQL previous);
NTHALAPI VOID NTAPI KeLowerIrql(KIRQL previous);
NTKERNELAPI VOID NTAPI KeAcquireSpinLockAtDpcLevel(PKSPIN_LOCK lock);
NTKERNELAPI VOID NTAPI KeReleaseSpinLockFromDpcLevel(PKSPIN_LOCK lock);
NTKERNELAPI VOID NTAPI KeQuerySystemTime(PLARGE_INTEGER now);

typedef struct {
    IInterruptSync Interface;
    ULONG References;
    KSPIN_LOCK ReferenceLock;
    LIST_ENTRY Routines;
    KSPIN_LOCK InterruptLock;
    KSEMAPHORE ConnectionLock;
    PKINTERRUPT Connected;
    KIRQL Level;
    ULONG Vector;
    KAFFINITY Affinity;
    BOOLEAN Share;
    KINTERRUPT_MODE Trigger;
    INTERRUPTSYNCMODE Mode;
} INTERRUPT_SYNC;

static const GUID unknownId = {0, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46}};
static const GUID interruptId = {0x22c6ac63, 0x851b, 0x11d0, {0x9a, 0x7f, 0, 0xaa, 0, 0x38, 0xac, 0xfe}};

static ULONG NTAPI addReference(IInterruptSync *iface)
{
    INTERRUPT_SYNC *object = CONTAINING_RECORD(iface, INTERRUPT_SYNC, Interface);
    return ExInterlockedAddUlong(&object->References, 1, &object->ReferenceLock) + 1;
}

static NTSTATUS NTAPI query(IInterruptSync *iface, REFIID id, PVOID *out)
{
    if (!out) return STATUS_INVALID_PARAMETER;
    *out = NULL;
    if (!id || (!IsEqualGUID(id, &unknownId) && !IsEqualGUID(id, &interruptId)))
        return STATUS_NOINTERFACE;
    addReference(iface);
    *out = iface;
    return STATUS_SUCCESS;
}

static KIRQL lockInterrupt(INTERRUPT_SYNC *object)
{
    KIRQL previous;
    KeRaiseIrql(object->Level, &previous);
    KeAcquireSpinLockAtDpcLevel(&object->InterruptLock);
    return previous;
}

static void unlockInterrupt(INTERRUPT_SYNC *object, KIRQL previous)
{
    KeReleaseSpinLockFromDpcLevel(&object->InterruptLock);
    KeLowerIrql(previous);
}

static NTSTATUS NTAPI callSynchronized(IInterruptSync *iface, PINTERRUPTSYNCROUTINE routine, PVOID context)
{
    INTERRUPT_SYNC *object = CONTAINING_RECORD(iface, INTERRUPT_SYNC, Interface);
    KIRQL previous;
    NTSTATUS status;
    if (!routine) return STATUS_INVALID_PARAMETER;
    previous = lockInterrupt(object);
    status = routine(iface, context);
    unlockInterrupt(object, previous);
    return status;
}

static BOOLEAN NTAPI dispatchInterrupt(PKINTERRUPT interrupt, PVOID context)
{
    INTERRUPT_SYNC *object = context;
    (void)interrupt;
    return serviceInterrupt(&object->Interface, &object->Routines, object->Mode);
}

static PKINTERRUPT NTAPI getInterrupt(IInterruptSync *iface)
{
    INTERRUPT_SYNC *object = CONTAINING_RECORD(iface, INTERRUPT_SYNC, Interface);
    return __atomic_load_n(&object->Connected, __ATOMIC_ACQUIRE);
}

static NTSTATUS NTAPI connect(IInterruptSync *iface)
{
    INTERRUPT_SYNC *object = CONTAINING_RECORD(iface, INTERRUPT_SYNC, Interface);
    PKINTERRUPT interrupt = NULL;
    NTSTATUS status;
    KeWaitForSingleObject(&object->ConnectionLock, Executive, KernelMode, FALSE, NULL);
    if (getInterrupt(iface)) status = STATUS_INVALID_DEVICE_STATE;
    else {
        status = IoConnectInterrupt(&interrupt, dispatchInterrupt, object, &object->InterruptLock,
            object->Vector, object->Level, object->Level, object->Trigger, object->Share, object->Affinity, FALSE);
        if (NT_SUCCESS(status)) __atomic_store_n(&object->Connected, interrupt, __ATOMIC_RELEASE);
    }
    KeReleaseSemaphore(&object->ConnectionLock, IO_NO_INCREMENT, 1, FALSE);
    return status;
}

static VOID NTAPI disconnect(IInterruptSync *iface)
{
    INTERRUPT_SYNC *object = CONTAINING_RECORD(iface, INTERRUPT_SYNC, Interface);
    PKINTERRUPT interrupt;
    KeWaitForSingleObject(&object->ConnectionLock, Executive, KernelMode, FALSE, NULL);
    interrupt = __atomic_exchange_n(&object->Connected, NULL, __ATOMIC_ACQ_REL);
    if (interrupt) IoDisconnectInterrupt(interrupt);
    KeReleaseSemaphore(&object->ConnectionLock, IO_NO_INCREMENT, 1, FALSE);
}

static NTSTATUS NTAPI registerRoutine(IInterruptSync *iface, PINTERRUPTSYNCROUTINE routine, PVOID context, BOOLEAN first)
{
    INTERRUPT_SYNC *object = CONTAINING_RECORD(iface, INTERRUPT_SYNC, Interface);
    SERVICE_ROUTINE *service;
    KIRQL previous;
    if (!routine) return STATUS_INVALID_PARAMETER;
    service = ExAllocatePool(NonPagedPool, sizeof(*service));
    if (!service) return STATUS_INSUFFICIENT_RESOURCES;
    service->Callback = routine;
    service->Context = context;
    previous = lockInterrupt(object);
    if (first) InsertHeadList(&object->Routines, &service->Link);
    else InsertTailList(&object->Routines, &service->Link);
    unlockInterrupt(object, previous);
    return STATUS_SUCCESS;
}

static ULONG NTAPI release(IInterruptSync *iface)
{
    INTERRUPT_SYNC *object = CONTAINING_RECORD(iface, INTERRUPT_SYNC, Interface);
    ULONG references = ExInterlockedAddUlong(&object->References, (ULONG)-1, &object->ReferenceLock) - 1;
    if (!references) {
        if (getInterrupt(iface)) disconnect(iface);
        while (!IsListEmpty(&object->Routines)) {
            LIST_ENTRY *entry = RemoveHeadList(&object->Routines);
            ExFreePool(CONTAINING_RECORD(entry, SERVICE_ROUTINE, Link));
        }
        ExFreePool(object);
    }
    return references;
}

static IInterruptSyncVtbl methods = {
    query, addReference, release, callSynchronized, getInterrupt, connect, disconnect, registerRoutine
};

NTSTATUS NTAPI PcNewInterruptSync(PINTERRUPTSYNC *out, PUNKNOWN outer, PRESOURCELIST resources,
                                  ULONG index, INTERRUPTSYNCMODE mode)
{
    PCM_PARTIAL_RESOURCE_DESCRIPTOR irq;
    INTERRUPT_SYNC *object;
    if (!out) return STATUS_INVALID_PARAMETER;
    *out = NULL;
    if (outer) return STATUS_NOT_SUPPORTED;
    if (!resources || mode < InterruptSyncModeNormal || mode > InterruptSyncModeRepeat)
        return STATUS_INVALID_PARAMETER;
    irq = resources->lpVtbl->FindTranslatedEntry(resources, CmResourceTypeInterrupt, index);
    if (!irq || irq->Type != CmResourceTypeInterrupt ||
        irq->u.Interrupt.Level <= DISPATCH_LEVEL || irq->u.Interrupt.Level > HIGH_LEVEL ||
        !irq->u.Interrupt.Affinity || !irq->u.Interrupt.Vector)
        return STATUS_INVALID_PARAMETER;
    if (irq->Flags & CM_RESOURCE_INTERRUPT_MESSAGE) return STATUS_NOT_SUPPORTED;
    object = ExAllocatePool(NonPagedPool, sizeof(*object));
    if (!object) return STATUS_INSUFFICIENT_RESOURCES;
    object->Interface.lpVtbl = &methods;
    object->References = 1;
    object->Connected = NULL;
    InitializeListHead(&object->Routines);
    KeInitializeSpinLock(&object->ReferenceLock);
    KeInitializeSpinLock(&object->InterruptLock);
    /* NT 3.1 enforces mutex level ordering even during driver initialization. */
    KeInitializeSemaphore(&object->ConnectionLock, 1, 1);
    object->Level = (KIRQL)irq->u.Interrupt.Level;
    object->Vector = irq->u.Interrupt.Vector;
    object->Affinity = irq->u.Interrupt.Affinity;
    object->Share = irq->ShareDisposition == CmResourceShareShared;
    object->Trigger = irq->Flags & CM_RESOURCE_INTERRUPT_LATCHED ? Latched : LevelSensitive;
    object->Mode = mode;
    *out = &object->Interface;
    return STATUS_SUCCESS;
}

ULONGLONG NTAPI PcGetTimeInterval(ULONGLONG since)
{
    LARGE_INTEGER now;
    KeQuerySystemTime(&now);
    return (ULONGLONG)now.QuadPart - since;
}

static VOID NTAPI unload(PDRIVER_OBJECT driver) { (void)driver; }

NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registryPath)
{
    (void)registryPath;
    driver->DriverUnload = unload;
    return STATUS_SUCCESS;
}
