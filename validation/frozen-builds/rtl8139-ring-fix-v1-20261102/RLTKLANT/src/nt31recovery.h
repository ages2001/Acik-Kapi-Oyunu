/* NT3.1 full-MAC passive recovery. Included after the MAC/filter helpers.
 * The referenced system thread outlives every pending reset and is joined
 * before adapter storage or the executable resident image can be reclaimed.
 */
NTSTATUS STDC ObReferenceObjectByHandle(PVOID,ULONG,PVOID,int,PVOID*,PVOID);
VOID STDC ObDereferenceObject(PVOID);
NTSTATUS STDC NtWaitForSingleObject(PVOID,BOOLEAN,PLARGE_INT);

static VOID n31_binding_release(PN31_BINDING binding)
{
    PN31_ADAPTER a=binding->Adapter;BOOLEAN closing;KIRQL old;
    n31_filter_lock(a);binding->ResetReferences--;
    closing=binding->Closing&&!binding->ResetReferences;
    n31_filter_unlock(a);
    if(closing){
        NDIS_HANDLE ndis=binding->NdisBinding;
        NDIS_STATUS status=n31_close(binding);
        if(status!=NDIS_STATUS_PENDING){
            KeRaiseIrql(DISPATCH_LEVEL,&old);
            NdisCompleteCloseAdapter(ndis,status);KeLowerIrql(old);
        }
    }
}

static VOID n31_notify_reset(PN31_ADAPTER a,NDIS_STATUS status)
{
    PN31_BINDING binding,next;KIRQL old;
    n31_filter_lock(a);
    for(binding=a->Bindings;binding&&binding->Closing;binding=binding->Next);
    if(binding)binding->ResetReferences++;
    n31_filter_unlock(a);
    while(binding){
        /* Never call a protocol with a driver lock held. Its status callback
           may close either this open or another open in the list. */
        KeRaiseIrql(DISPATCH_LEVEL,&old);
        N31IndicateResetStatus(binding->NdisBinding,status);
        KeLowerIrql(old);
        n31_filter_lock(a);
        for(next=binding->Next;next&&next->Closing;next=next->Next);
        if(next)next->ResetReferences++;
        n31_filter_unlock(a);
        n31_binding_release(binding);binding=next;
    }
}

static VOID n31_reset_complete(PN31_BINDING binding,NDIS_STATUS status)
{
    KIRQL old;KeRaiseIrql(DISPATCH_LEVEL,&old);
    N31CompleteReset(binding->NdisBinding,status);
    KeLowerIrql(old);
}

static VOID STDC n31_recovery_thread(PVOID context)
{
    PN31_ADAPTER a=context;
    for(;;){
        PN31_BINDING binding;BOOLEAN stop;KIRQL old;
        NDIS_STATUS status=NDIS_STATUS_FAILURE;
        KeWaitForSingleObject(a->RecoveryWake,0,0,FALSE,NULL);
        KeAcquireSpinLock(&a->TxLock,&old);
        stop=a->RecoveryStop;binding=a->ResetBinding;
        KeReleaseSpinLock(&a->TxLock,old);
        if(binding){
            /* Resetting was published under TxLock before the wake. Taking
               that lock joined the last sender; now join receive users. */
            n31_poll_stop(a);
            n31_notify_reset(a,(NDIS_STATUS)0x40010004UL); /* RESET_START */
            if(!stop&&hw_init(a))status=NDIS_STATUS_SUCCESS;
            n31_filter_lock(a);
            KeAcquireSpinLock(&a->TxLock,&old);
            if(a->RecoveryStop)status=NDIS_STATUS_FAILURE;
            if(status==NDIS_STATUS_SUCCESS)hw_setfilter(a);
            a->ResetFailed=status!=NDIS_STATUS_SUCCESS;
            /* A failed reset leaves the adapter stopped until a successful
               retry. Filters may change while stopped but cannot touch I/O. */
            KeReleaseSpinLock(&a->TxLock,old);
            if(status==NDIS_STATUS_SUCCESS){
                KeAcquireSpinLock(&a->PollLock,&old);a->Running=TRUE;
                KeReleaseSpinLock(&a->PollLock,old);
            }
            n31_filter_unlock(a);
            n31_notify_reset(a,(NDIS_STATUS)0x40010005UL); /* RESET_END */
            n31_filter_lock(a);
            KeAcquireSpinLock(&a->TxLock,&old);
            if(a->RecoveryStop){status=NDIS_STATUS_FAILURE;a->ResetFailed=TRUE;}
            a->ResetBinding=NULL;a->Resetting=FALSE;
            KeReleaseSpinLock(&a->TxLock,old);
            n31_filter_unlock(a);
            n31_reset_complete(binding,status);
            /* Completion may synchronously queue another reset or close.
               Keep this binding alive through that callback and honor a
               deferred close only after its final reset reference leaves. */
            n31_binding_release(binding);
        }
        KeAcquireSpinLock(&a->TxLock,&old);stop=a->RecoveryStop;
        KeReleaseSpinLock(&a->TxLock,old);
        if(stop)break;
    }
    PsTerminateSystemThread(0);
}

static BOOLEAN n31_start_recovery(PN31_ADAPTER a)
{
    PVOID handle=NULL;NTSTATUS status;
    if(a->UsbContext)return TRUE;
    KeInitializeEvent(a->RecoveryWake,1,FALSE);
    /* AddAdapter executes in the original image. The worker must execute
       the relocated, globally mapped resident copy in the System process. */
    status=PsCreateSystemThread(&handle,0x00100000,NULL,NULL,NULL,
             (PKSTART_ROUTINE)cp(n31_recovery_thread),a);
    if(status<0)return FALSE;
    status=ObReferenceObjectByHandle(handle,0x00100000,NULL,0,&a->RecoveryThread,NULL);
    if(status<0){
        KIRQL old;KeAcquireSpinLock(&a->TxLock,&old);a->RecoveryStop=TRUE;
        KeReleaseSpinLock(&a->TxLock,old);KeSetEvent(a->RecoveryWake,0,FALSE);
        NtWaitForSingleObject(handle,FALSE,NULL);ZwClose(handle);
        a->RecoveryThread=NULL;return FALSE;
    }
    ZwClose(handle);return TRUE;
}

static VOID n31_stop_recovery(PN31_ADAPTER a)
{
    KIRQL old;
    KeAcquireSpinLock(&a->TxLock,&old);a->RecoveryStop=TRUE;
    KeReleaseSpinLock(&a->TxLock,old);
    if(!a->RecoveryThread)return;
    KeSetEvent(a->RecoveryWake,0,FALSE);
    KeWaitForSingleObject(a->RecoveryThread,0,0,FALSE,NULL);
    ObDereferenceObject(a->RecoveryThread);a->RecoveryThread=NULL;
}

static NDIS_STATUS n31_queue_reset(PN31_BINDING binding)
{
    PN31_ADAPTER a=binding->Adapter;KIRQL old;
    NDIS_STATUS status=NDIS_STATUS_FAILURE;
    n31_filter_lock(a);
    KeAcquireSpinLock(&a->TxLock,&old);
    if(!binding->Closing&&!a->RecoveryStop&&a->RecoveryThread&&!a->Resetting){
        binding->ResetReferences++;
        a->ResetBinding=binding;a->Resetting=TRUE;
        KeSetEvent(a->RecoveryWake,0,FALSE);status=NDIS_STATUS_PENDING;
    }
    KeReleaseSpinLock(&a->TxLock,old);
    n31_filter_unlock(a);return status;
}
