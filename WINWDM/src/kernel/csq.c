#include "wdm_csq.h"

/* IO_CSQ has a single slot for either insert callback in the public ABI. */
typedef union {
    PIO_CSQ_INSERT_IRP basic;
    PIO_CSQ_INSERT_IRP_EX extended;
} INSERT_CALLBACK;

NTSTATUS NTAPI IoCsqInitialize(PIO_CSQ q, PIO_CSQ_INSERT_IRP insert,
    PIO_CSQ_REMOVE_IRP remove, PIO_CSQ_PEEK_NEXT_IRP peek, PIO_CSQ_ACQUIRE_LOCK acquire,
    PIO_CSQ_RELEASE_LOCK release, PIO_CSQ_COMPLETE_CANCELED_IRP complete)
{
    if (!q || !insert || !remove || !peek || !acquire || !release || !complete)
        return STATUS_INVALID_PARAMETER;
    q->Type = IO_TYPE_CSQ;
    q->CsqInsertIrp = insert;
    q->CsqRemoveIrp = remove;
    q->CsqPeekNextIrp = peek;
    q->CsqAcquireLock = acquire;
    q->CsqReleaseLock = release;
    q->CsqCompleteCanceledIrp = complete;
    q->ReservePointer = NULL;
    return STATUS_SUCCESS;
}

NTSTATUS NTAPI IoCsqInitializeEx(PIO_CSQ q, PIO_CSQ_INSERT_IRP_EX insert,
    PIO_CSQ_REMOVE_IRP remove, PIO_CSQ_PEEK_NEXT_IRP peek, PIO_CSQ_ACQUIRE_LOCK acquire,
    PIO_CSQ_RELEASE_LOCK release, PIO_CSQ_COMPLETE_CANCELED_IRP complete)
{
    INSERT_CALLBACK callback;
    NTSTATUS status;
    callback.extended = insert;
    status = IoCsqInitialize(q, callback.basic, remove, peek, acquire, release, complete);
    if (NT_SUCCESS(status))
        q->Type = IO_TYPE_CSQ_EX;
    return status;
}

/* The queue lock protects membership and the lifetime of a live context. */
static void detach(PIO_CSQ q, PIRP irp)
{
    PVOID owner = irp->Tail.Overlay.DriverContext[3];
    q->CsqRemoveIrp(q, irp);
    if (owner != q)
        ((PIO_CSQ_IRP_CONTEXT)owner)->Irp = NULL;
    irp->Tail.Overlay.DriverContext[3] = NULL;
}

static VOID NTAPI cancel(PDEVICE_OBJECT device, PIRP irp)
{
    PVOID owner = irp->Tail.Overlay.DriverContext[3];
    PIO_CSQ q = *(PULONG)owner == IO_TYPE_CSQ_IRP_CONTEXT
             ? ((PIO_CSQ_IRP_CONTEXT)owner)->Csq : (PIO_CSQ)owner;
    KIRQL irql;
    (void)device;
    /* IoCancelIrp has already claimed the IRP. Drop the system cancel lock
     * before taking the driver's queue lock or invoking driver callbacks. */
    IoReleaseCancelSpinLock(irp->CancelIrql);
    q->CsqAcquireLock(q, &irql);
    detach(q, irp);
    q->CsqReleaseLock(q, irql);
    q->CsqCompleteCanceledIrp(q, irp);
}

NTSTATUS NTAPI IoCsqInsertIrpEx(PIO_CSQ q, PIRP irp, PIO_CSQ_IRP_CONTEXT context,
                                PVOID insertContext)
{
    KIRQL irql;
    BOOLEAN canceled;
    NTSTATUS status = STATUS_SUCCESS;
    q->CsqAcquireLock(q, &irql);
    if (context) {
        context->Type = IO_TYPE_CSQ_IRP_CONTEXT;
        context->Csq = q;
        context->Irp = NULL;
    }
    canceled = irp->Cancel;
    if (!canceled) {
        if (q->Type == IO_TYPE_CSQ_EX) {
            INSERT_CALLBACK callback;
            callback.basic = q->CsqInsertIrp;
            status = callback.extended(q, irp, insertContext);
        } else {
            q->CsqInsertIrp(q, irp);
        }
        if (!NT_SUCCESS(status)) {
            q->CsqReleaseLock(q, irql);
            return status;
        }
        if (context)
            context->Irp = irp;
        irp->Tail.Overlay.DriverContext[3] = context ? (PVOID)context : (PVOID)q;
        /* Publish before checking Cancel again. Only the thread that exchanges
         * a non-null routine is entitled to remove and complete this IRP. */
        (void)IoSetCancelRoutine(irp, cancel);
        canceled = irp->Cancel && IoSetCancelRoutine(irp, NULL) != NULL;
        if (canceled)
            detach(q, irp);
    }
    IoMarkIrpPending(irp);
    q->CsqReleaseLock(q, irql);
    if (canceled)
        q->CsqCompleteCanceledIrp(q, irp);
    return status;
}

VOID NTAPI IoCsqInsertIrp(PIO_CSQ q, PIRP irp, PIO_CSQ_IRP_CONTEXT context)
{
    (void)IoCsqInsertIrpEx(q, irp, context, NULL);
}

PIRP NTAPI IoCsqRemoveIrp(PIO_CSQ q, PIO_CSQ_IRP_CONTEXT context)
{
    KIRQL irql;
    PIRP irp;
    q->CsqAcquireLock(q, &irql);
    irp = context->Csq == q ? context->Irp : NULL;
    if (irp && IoSetCancelRoutine(irp, NULL))
        detach(q, irp);
    else
        irp = NULL;
    q->CsqReleaseLock(q, irql);
    return irp;
}

PIRP NTAPI IoCsqRemoveNextIrp(PIO_CSQ q, PVOID peekContext)
{
    KIRQL irql;
    PIRP irp = NULL;
    q->CsqAcquireLock(q, &irql);
    while ((irp = q->CsqPeekNextIrp(q, irp, peekContext)) != NULL) {
        if (IoSetCancelRoutine(irp, NULL)) {
            detach(q, irp);
            break;
        }
    }
    q->CsqReleaseLock(q, irql);
    return irp;
}
