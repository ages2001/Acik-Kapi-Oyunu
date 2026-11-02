from pathlib import Path
p=Path('/workspace/rltklant-work/RLTKLANT/src/native_urb.c');s=p.read_text()
s=s.replace('typedef struct {\n    PDEVICE_OBJECT Lower;', '#define RTL_RX_DEPTH 16\n#define RTL_RX_BYTES 2048\ntypedef struct RTL_URB_TARGET {\n    PDEVICE_OBJECT Lower;')
s=s.replace('    KEVENT Wake;\n} RTL_URB_TARGET;', '''    KEVENT Wake;
    struct RTL_URB_TARGET *RxLane;
    PVOID RxThread;
    PUCHAR RxBuffers;
    ULONG RxLength[RTL_RX_DEPTH],RxHead,RxCount,RxEpoch;
    BOOLEAN RxAccept;
} RTL_URB_TARGET;''')
s=s.replace('FALSE,&timeout);\n    if(status==STATUS_TIMEOUT)', 'FALSE,milliseconds?&timeout:NULL);\n    if(status==STATUS_TIMEOUT)')
start=s.index('    }else if(kind==1&&(endpoint==0x81||endpoint==0x02)){')
end=s.index('    }else return STATUS_INVALID_PARAMETER;',start)
s=s[:start]+'''    }else if(kind==1&&endpoint==0x81){
        KIRQL old;ULONG size;
        KeAcquireSpinLock(&t->Lock,&old);
        if(t->Stopping||!t->RxCount){KeReleaseSpinLock(&t->Lock,old);return STATUS_DEVICE_NOT_READY;}
        size=t->RxLength[t->RxHead];
        if(length<size){KeReleaseSpinLock(&t->Lock,old);return STATUS_BUFFER_TOO_SMALL;}
        RtlCopyMemory(buffer,t->RxBuffers+t->RxHead*RTL_RX_BYTES,size);
        t->RxHead=(t->RxHead+1)%RTL_RX_DEPTH;t->RxCount--;
        KeReleaseSpinLock(&t->Lock,old);*done=size;return 0;
    }else if(kind==1&&endpoint==0x02){
        struct _URB_BULK_OR_INTERRUPT_TRANSFER urb;
        RtlZeroMemory(&urb,sizeof(urb));urb.Hdr.Length=sizeof(urb);urb.Hdr.Function=URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER;
        urb.PipeHandle=t->Out;urb.TransferFlags=USBD_TRANSFER_DIRECTION_OUT;
        urb.TransferBuffer=buffer;urb.TransferBufferLength=length;
        status=Submit(t,(PURB)&urb,5000,FALSE);
        if(NT_SUCCESS(status))*done=urb.TransferBufferLength;
''' + s[end:]
start=s.index('int RtlUrbStart(');end=s.index('void RtlUrbClose(',start)
s=s[:start]+'''/* Carrier changes invalidate copied data without cancelling a partially
   received USB transfer. Its old epoch is discarded only after completion. */
void RtlUrbSetCarrier(void *context,int up)
{
    RTL_URB_TARGET*t=context;KIRQL old;
    KeAcquireSpinLock(&t->Lock,&old);
    if(!up&&t->RxAccept){t->RxEpoch++;t->RxHead=t->RxCount=0;}
    t->RxAccept=!!up;KeReleaseSpinLock(&t->Lock,old);
}
static void QueueRx(RTL_URB_TARGET*t,ULONG epoch,const UCHAR*data,ULONG size)
{
    KIRQL old;ULONG slot;
    if(!size||size>RTL_RX_BYTES)return;
    KeAcquireSpinLock(&t->Lock,&old);
    if(!t->Stopping&&t->RxAccept&&epoch==t->RxEpoch&&t->RxCount<RTL_RX_DEPTH){
        slot=(t->RxHead+t->RxCount)%RTL_RX_DEPTH;
        RtlCopyMemory(t->RxBuffers+slot*RTL_RX_BYTES,data,size);
        t->RxLength[slot]=size;t->RxCount++;
    }
    KeReleaseSpinLock(&t->Lock,old);
}
static VOID NTAPI ReceiveWorker(PVOID context)
{
    RTL_URB_TARGET*t=context;UCHAR buffer[RTL_RX_BYTES];KIRQL old;
    struct _URB_BULK_OR_INTERRUPT_TRANSFER urb;NTSTATUS status;
    BOOLEAN stop;ULONG epoch;LARGE_INTEGER delay;delay.QuadPart=-100000;
    for(;;){
        KeAcquireSpinLock(&t->Lock,&old);stop=t->Stopping;epoch=t->RxEpoch;KeReleaseSpinLock(&t->Lock,old);
        if(stop)break;
        RtlZeroMemory(&urb,sizeof(urb));urb.Hdr.Length=sizeof(urb);urb.Hdr.Function=URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER;
        urb.PipeHandle=t->In;urb.TransferFlags=USBD_TRANSFER_DIRECTION_IN|USBD_SHORT_TRANSFER_OK;
        urb.TransferBuffer=buffer;urb.TransferBufferLength=sizeof(buffer);
        /* A receive stays pending while the device is idle. Periodic RX
           cancellation can split a frame across URBs and lose its boundary.
           Control/TX have a separate ownership lane and remain responsive. */
        status=Submit(t->RxLane,(PURB)&urb,0,FALSE);
#ifdef RTL_USB_TRACE
        if(NT_SUCCESS(status)||(urb.TransferBufferLength&&urb.TransferBufferLength<sizeof(buffer)))
            TraceRx(status,urb.TransferBufferLength,buffer);
#endif
        if(NT_SUCCESS(status))QueueRx(t,epoch,buffer,urb.TransferBufferLength);
        else KeWaitForSingleObject(&t->Wake,Executive,KernelMode,FALSE,&delay);
    }
    PsTerminateSystemThread(STATUS_SUCCESS);
}
static int StartThread(RTL_URB_TARGET*t,PKSTART_ROUTINE worker,PVOID*thread)
{
    HANDLE handle;NTSTATUS status;
    status=PsCreateSystemThread(&handle,SYNCHRONIZE,NULL,NULL,NULL,worker,t);
    if(!NT_SUCCESS(status))return 0;
    status=ObReferenceObjectByHandle(handle,SYNCHRONIZE,NULL,KernelMode,thread,NULL);
    if(!NT_SUCCESS(status)){
        RtlUrbCancel(t);RtlUrbCancel(t->RxLane);
        ZwWaitForSingleObject(handle,FALSE,NULL);ZwClose(handle);return 0;
    }
    ZwClose(handle);return 1;
}
int RtlUrbStart(void *context,void (*service)(void *),void *owner)
{
    RTL_URB_TARGET*t=context;
    t->Service=service;t->Owner=owner;
    t->RxLane=ExAllocatePoolWithTag(NonPagedPool,sizeof(*t),RTL_URB_TAG);
    if(!t->RxLane)return 0;
    RtlZeroMemory(t->RxLane,sizeof(*t));t->RxLane->Lower=t->Lower;
    KeInitializeSpinLock(&t->RxLane->Lock);KeInitializeEvent(&t->RxLane->Wake,NotificationEvent,FALSE);
    t->RxBuffers=ExAllocatePoolWithTag(NonPagedPool,RTL_RX_DEPTH*RTL_RX_BYTES,RTL_URB_TAG);
    if(!t->RxBuffers)return 0;
    if(!StartThread(t,ReceiveWorker,&t->RxThread))return 0;
    return StartThread(t,Worker,&t->Thread);
}
''' + s[end:]
s=s.replace('    RtlUrbCancel(t);\n    if(t->Thread)', '    RtlUrbCancel(t);RtlUrbCancel(t->RxLane);\n    if(t->Thread)')
s=s.replace('    if(t->Configured){', '    if(t->RxThread){KeWaitForSingleObject(t->RxThread,Executive,KernelMode,FALSE,NULL);ObDereferenceObject(t->RxThread);}\n    if(t->RxBuffers)ExFreePool(t->RxBuffers);\n    if(t->RxLane)ExFreePool(t->RxLane);\n    if(t->Configured){')
p.write_text(s)
p=Path('/workspace/rltklant-work/RLTKLANT/src/native_urb.h');s=p.read_text().replace('void RtlUrbCancel(', 'void RtlUrbSetCarrier(void *context,int up);\nvoid RtlUrbCancel(');p.write_text(s)
p=Path('/workspace/rltklant-work/RLTKLANT/src/native_usb.c');s=p.read_text();s+='''\nVOID RtlNativeSetCarrier(PRLTK_ADAPTER a,BOOLEAN up)
{
    RTL_NATIVE_USB*c=a->NativeUsb;if(c)RtlUrbSetCarrier(c->Transport,up);
}
''';p.write_text(s)
p=Path('/workspace/rltklant-work/RLTKLANT/src/backend_usb.c');s=p.read_text();s=s.replace('    if(USB_OK(status)&&up){\n', '''#if defined(RTL_NDIS5)
    if(a->NativeUsbRequested){extern VOID RtlNativeSetCarrier(PRLTK_ADAPTER,BOOLEAN);RtlNativeSetCarrier(a,USB_OK(status)&&up);}
#endif
    if(USB_OK(status)&&up){
''');p.write_text(s)
