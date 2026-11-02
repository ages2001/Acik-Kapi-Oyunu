/* Passive PCI reset ownership. The referenced system thread is joined before
 * any adapter/mapping is freed, including initialization failure unwind. */
#include "rltklant.h"

VOID STDC KeInitializeEvent(PVOID, int, BOOLEAN);
LONG STDC KeSetEvent(PVOID, LONG, BOOLEAN);
NTSTATUS STDC KeWaitForSingleObject(PVOID, int, int, BOOLEAN, PLARGE_INTEGER);
NTSTATUS STDC PsCreateSystemThread(PVOID *, ULONG, PVOID, PVOID, PVOID,
                                   VOID (STDC *)(PVOID), PVOID);
VOID STDC PsTerminateSystemThread(NTSTATUS);
NTSTATUS STDC ObReferenceObjectByHandle(PVOID, ULONG, PVOID, int, PVOID *, PVOID);
NTSTATUS STDC ZwWaitForSingleObject(PVOID, BOOLEAN, PLARGE_INTEGER);
NTSTATUS STDC ZwClose(PVOID);
VOID STDC NdisMResetComplete(NDIS_HANDLE, NDIS_STATUS, BOOLEAN);
#if defined(_WIN64)
UCHAR KeRaiseIrqlToDpcLevel(VOID);
#else
VOID STDC KeRaiseIrql(UCHAR, UCHAR *);
#endif
VOID STDC KeLowerIrql(UCHAR);

static VOID CompleteReset(PRLTK_ADAPTER a, NDIS_STATUS status)
{
    UCHAR old;
    /* NDIS 3/5 require DISPATCH_LEVEL and no held driver spin lock. */
#if defined(_WIN64)
    old=KeRaiseIrqlToDpcLevel();
#else
    KeRaiseIrql(2,&old);
#endif
    NdisMResetComplete(a->MiniportHandle,status,TRUE);
    KeLowerIrql(old);
}

static VOID STDC RecoveryThread(PVOID context)
{
    PRLTK_ADAPTER a=context;
    for (;;) {
        BOOLEAN pending,stop;NDIS_STATUS status=NDIS_STATUS_FAILURE;
        KeWaitForSingleObject(&a->RecoveryWake,0,0,FALSE,NULL);
        NdisAcquireSpinLock(&a->Lock);
        stop=a->RecoveryStop;pending=a->ResetPending;
        NdisReleaseSpinLock(&a->Lock);
        if (pending) {
            if (!stop) status=RtlRecoverPci(a);
            NdisAcquireSpinLock(&a->Lock);
            if (a->Halting) status=NDIS_STATUS_FAILURE;
            a->ResetFailed=status!=NDIS_STATUS_SUCCESS;
            a->ResetPending=FALSE;
            if (status==NDIS_STATUS_SUCCESS) a->Ops->EnableInts(a,TRUE);
            NdisReleaseSpinLock(&a->Lock);
            if (status==NDIS_STATUS_SUCCESS) RtlResumeLinkTimer(a);
            CompleteReset(a,status);
        }
        /* The final completion may queue another reset. The auto-reset
           wake retains that notification; never clear it after completion. */
        NdisAcquireSpinLock(&a->Lock);stop=a->RecoveryStop;
        NdisReleaseSpinLock(&a->Lock);
        if (stop) break;
    }
    PsTerminateSystemThread(0);
}

BOOLEAN RtlStartRecovery(PRLTK_ADAPTER a)
{
    PVOID handle=NULL;NTSTATUS status;
    KeInitializeEvent(&a->RecoveryWake,1,FALSE); /* synchronization event */
    a->RecoveryStop=FALSE;
    status=PsCreateSystemThread(&handle,0x00100000,NULL,NULL,NULL,RecoveryThread,a);
    if (!NT_SUCCESS(status)) return FALSE;
    status=ObReferenceObjectByHandle(handle,0x00100000,NULL,0,&a->RecoveryThread,NULL);
    if (!NT_SUCCESS(status)) {
        NdisAcquireSpinLock(&a->Lock);a->RecoveryStop=TRUE;
        NdisReleaseSpinLock(&a->Lock);
        KeSetEvent(&a->RecoveryWake,0,FALSE);
        ZwWaitForSingleObject(handle,FALSE,NULL);ZwClose(handle);
        a->RecoveryThread=NULL;return FALSE;
    }
    ZwClose(handle);return TRUE;
}

VOID RtlStopRecovery(PRLTK_ADAPTER a)
{
    if (!a->RecoveryThread) return;
    NdisAcquireSpinLock(&a->Lock);a->RecoveryStop=TRUE;
    NdisReleaseSpinLock(&a->Lock);
    KeSetEvent(&a->RecoveryWake,0,FALSE);
    KeWaitForSingleObject(a->RecoveryThread,0,0,FALSE,NULL);
    ObDereferenceObject(a->RecoveryThread);a->RecoveryThread=NULL;
}

NDIS_STATUS RtlQueuePciReset(PRLTK_ADAPTER a)
{
    NDIS_STATUS status=NDIS_STATUS_FAILURE;
    NdisAcquireSpinLock(&a->Lock);
    if (!a->Halting&&!a->RecoveryStop&&a->RecoveryThread&&!a->ResetPending) {
        a->ResetPending=TRUE;
        KeSetEvent(&a->RecoveryWake,0,FALSE);
        status=NDIS_STATUS_PENDING;
    }
    NdisReleaseSpinLock(&a->Lock);
    return status;
}
