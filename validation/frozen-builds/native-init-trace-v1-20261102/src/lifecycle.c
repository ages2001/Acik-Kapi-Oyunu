/* Adapter stop, reset and link-timer lifetime. */
#include "rltklant.h"
#if defined(RTL_NDIS5)
#include "native_pci.h"
#endif
LONG STDC KeSetEvent(PVOID, LONG, BOOLEAN);
NTSTATUS STDC KeWaitForSingleObject(PVOID, int, int, BOOLEAN, PLARGE_INTEGER);
VOID STDC KeClearEvent(PVOID);

static VOID StopLinkTimer(PRLTK_ADAPTER a)
{
    BOOLEAN cancelled;
    if (!a->TimerSet) return;
    NdisMCancelTimer(&a->LinkTimer,&cancelled);
    if (!cancelled) KeWaitForSingleObject(&a->LinkTimerStopped,0,0,FALSE,NULL);
    a->TimerSet=FALSE;
}

VOID RtlResumeLinkTimer(PRLTK_ADAPTER a)
{
    NdisAcquireSpinLock(&a->Lock);
    if (!a->Halting&&!a->ResetPending&&!a->ResetFailed&&!a->TimerSet) {
        KeClearEvent(&a->LinkTimerStopped);
        a->TimerSet=TRUE;
        NdisSetTimer(&a->LinkTimer,a->UsbContext?10:1000);
    }
    NdisReleaseSpinLock(&a->Lock);
}

VOID STDC RtlHalt(NDIS_HANDLE ctx)
{
    PRLTK_ADAPTER a = (PRLTK_ADAPTER)ctx;

    /* Serialize the stop flag with the callback's final rearm. A failed
     * cancellation means a callback can still own this adapter: wait for
     * its final access before freeing DMA, mappings, or the adapter. */
    NdisAcquireSpinLock(&a->Lock);
    a->Halting = TRUE;
    NdisReleaseSpinLock(&a->Lock);
    RtlStopRecovery(a);
    StopLinkTimer(a);
    a->Ops->EnableInts(a, FALSE);
    if (a->IntRegistered) NdisMDeregisterInterrupt(&a->Interrupt);
    a->Ops->Halt(a);
    if (a->MapRegs) NdisMFreeMapRegisters(a->MiniportHandle);
    if (a->IoBase) NdisMUnmapIoSpace(a->MiniportHandle, a->IoBase, a->MemLen);
    if (a->IoPortVa) NdisMDeregisterIoPortRange(a->MiniportHandle, a->IoPortPhys, a->IoPortLen, a->IoPortVa);
#if defined(RTL_NDIS5)
    RtlNativePciRelease(a->PciPdo);a->PciPdo=NULL;
#endif
    RtlReleaseSlot(a);
    NdisFreeSpinLock(&a->Lock);
    RtlFree(a, sizeof(RLTK_ADAPTER));
}

/* Backend Init can fail after allocating some DMA buffers or enabling DMA.
   Stop it and reclaim those buffers before the caller releases mappings. */
BOOLEAN RtlStartBackend(PRLTK_ADAPTER a)
{
    if (a->Ops->Init(a)) return TRUE;
    a->Ops->Halt(a);
    return FALSE;
}

NDIS_STATUS STDC RtlReset(PBOOLEAN AddressingReset, NDIS_HANDLE ctx)
{
    PRLTK_ADAPTER a = (PRLTK_ADAPTER)ctx;
    *AddressingReset = TRUE;
    if(a->UsbContext){
        /* USB transfers belong to the passive worker. The miniport reset
           callback cannot wait for that worker at DISPATCH_LEVEL. */
        extern VOID RtlUsbResetQueues(PRLTK_ADAPTER);
        RtlUsbResetQueues(a);a->TxStalled=FALSE;
        return NDIS_STATUS_SUCCESS;
    }
    return RtlQueuePciReset(a);
}

NDIS_STATUS RtlRecoverPci(PRLTK_ADAPTER a)
{
    NDIS_STATUS status;
    /* ResetPending prevents timer rearm. Deregistration joins the ISR/DPC
       before the passive owner touches PHY/MAC state or releases DMA. */
    StopLinkTimer(a);
    a->Ops->EnableInts(a,FALSE);
    if (a->IntRegistered) {
        NdisMDeregisterInterrupt(&a->Interrupt);a->IntRegistered=FALSE;
    }
    a->Ops->Halt(a);
    if (a->Halting||!RtlStartBackend(a)) return NDIS_STATUS_FAILURE;
    status=NdisMRegisterInterrupt(&a->Interrupt,a->MiniportHandle,
             a->IrqVector,a->IrqLevel,TRUE,TRUE,NdisInterruptLevelSensitive);
    if (status!=NDIS_STATUS_SUCCESS) {a->Ops->Halt(a);return status;}
    a->IntRegistered=TRUE;
    NdisAcquireSpinLock(&a->Lock);
    if (a->Halting) status=NDIS_STATUS_FAILURE;
    else {
        a->Ops->SetRxMode(a);
        a->TxStalled=FALSE;
        a->TxProgress=a->CpTxHead;a->TxNoProgressChecks=0;
    }
    NdisReleaseSpinLock(&a->Lock);
    return status;
}

BOOLEAN STDC RtlCheckForHang(NDIS_HANDLE ctx)
{
    PRLTK_ADAPTER a=ctx;BOOLEAN hung=FALSE;
    NdisAcquireSpinLock(&a->Lock);
    if (a->Backend==RTL_BACKEND_8139 && a->RxRecoveryNeeded &&
        !a->Halting && !a->ResetPending && !a->ResetFailed) {
        NdisReleaseSpinLock(&a->Lock);
#ifdef RTL_DBG
        {const char*s="RTL8139 RX watchdog reset\n";while(*s)RtlOutByte(0xe9,(UCHAR)*s++);}
#endif
        return TRUE;
    }
    /* NDIS serializes this callback with the miniport send/DPC paths.
       A queued descriptor with no completion across three checks indicates
       a stopped engine or lost interrupt. Link loss and idle queues are not
       hangs. USB has its own passive transfer/recovery owner. */
    if(a->Backend!=RTL_BACKEND_CPLUS||a->UsbContext||a->Halting||
       a->ResetPending||a->ResetFailed||!a->LinkUp||a->CpTxHead==a->CpTxTail||
       a->CpTxHead!=a->TxProgress){
        a->TxNoProgressChecks=0;
    }else if(++a->TxNoProgressChecks>=3){
        a->TxNoProgressChecks=0;hung=TRUE;
    }
    a->TxProgress=a->CpTxHead;
    NdisReleaseSpinLock(&a->Lock);
#ifdef RTL_DBG
    if(hung){const char*s="PCI TX watchdog reset\n";while(*s)RtlOutByte(0xe9,(UCHAR)*s++);}
#endif
    return hung;
}

VOID STDC RtlLinkTimer(PVOID s, PVOID ctx, PVOID a1, PVOID a2)
{
    PRLTK_ADAPTER a = (PRLTK_ADAPTER)ctx;
    ULONG spd = 0; BOOLEAN fd = FALSE, up;
    (void)s; (void)a1; (void)a2;
    NdisAcquireSpinLock(&a->Lock);
    if (a->Halting || a->ResetPending || a->ResetFailed) {
        NdisReleaseSpinLock(&a->Lock);
        KeSetEvent(&a->LinkTimerStopped, 0, FALSE);
        return;
    }
    NdisReleaseSpinLock(&a->Lock);

    if(a->UsbContext) a->Ops->HandleInterrupt(a);
    up = a->Ops->LinkState(a, &spd, &fd);
    if (up) {
        a->LinkSpeedMbit = RtlCapSpeed(a, spd);
        a->FullDuplex = fd;
    }
    if (up != a->LinkUp) {
        a->LinkUp = up;
        NdisMIndicateStatus(a->MiniportHandle,
            up ? NDIS_STATUS_MEDIA_CONNECT : NDIS_STATUS_MEDIA_DISCONNECT, NULL, 0);
        NdisMIndicateStatusComplete(a->MiniportHandle);
    }
    NdisAcquireSpinLock(&a->Lock);
    if (!a->Halting && !a->ResetPending && !a->ResetFailed) {
        NdisSetTimer(&a->LinkTimer, a->UsbContext ? 10 : 1000);
        NdisReleaseSpinLock(&a->Lock);
    } else {
        NdisReleaseSpinLock(&a->Lock);
        KeSetEvent(&a->LinkTimerStopped, 0, FALSE);
    }
}

VOID STDC RtlEnableInterrupt(NDIS_HANDLE ctx)
{
    PRLTK_ADAPTER a = (PRLTK_ADAPTER)ctx;
    a->Ops->EnableInts(a, TRUE);
}

VOID STDC RtlDisableInterrupt(NDIS_HANDLE ctx)
{
    PRLTK_ADAPTER a = (PRLTK_ADAPTER)ctx;
    a->Ops->EnableInts(a, FALSE);
}
