/* Native Windows 2000/XP USB lower edge. Keep DDK ABI structures in this
   translation unit instead of duplicating IRP/URB layouts in ndiskit.h.
   Independent passive RX and control/TX owners keep receive requests pending.
   Stop cancels both actual IRPs and joins both owners before releasing storage. */
#define _WIN32_WINNT 0x0500
#define NTDDI_VERSION 0x05000000
/* mingw-w64's WDM header references this four-byte union unconditionally,
   but declares it only for Vista. No Vista kernel iface is used here. */
typedef union {unsigned long ContextAsUlong;} SYSTEM_POWER_STATE_CONTEXT;
#if defined(_WIN64)
/* This header intrinsic has no XMM operands. GCC hides its builtin with
   -mno-sse, which remains required to avoid kernel SIMD state corruption. */
#define __builtin_ia32_sfence() __asm__ __volatile__("sfence" ::: "memory")
#endif
#include <ntddk.h>
#include <usb.h>
#include <usbioctl.h>
#include "native_urb.h"
#define RTL_URB_TAG 0x75544c52UL
#ifdef RTL_USB_TRACE
void RtlOutByte(ULONG_PTR,UCHAR);
static void TraceRx(NTSTATUS status,ULONG length,const UCHAR*data)
{
    const char*label="URB-RX status/length/fnv ";ULONG values[3],i,j;
    values[0]=(ULONG)status;values[1]=length;values[2]=2166136261UL;
    if(NT_SUCCESS(status)&&length<=2048)for(i=0;i<length;i++)values[2]=(values[2]^data[i])*16777619UL;
    while(*label)RtlOutByte(0xe9,*label++);
    for(i=0;i<3;i++){for(j=0;j<8;j++)RtlOutByte(0xe9,"0123456789ABCDEF"[(values[i]>>(28-4*j))&15]);RtlOutByte(0xe9,' ');}
    RtlOutByte(0xe9,'\n');
}
static void TraceRxFailure(NTSTATUS status,ULONG usbStatus)
{
    const char *label="URB-RX failure NT/USBD ";ULONG values[2],i,j;
    values[0]=(ULONG)status;values[1]=usbStatus;
    while(*label)RtlOutByte(0xe9,*label++);
    for(i=0;i<2;i++){for(j=0;j<8;j++)RtlOutByte(0xe9,"0123456789ABCDEF"[(values[i]>>(28-4*j))&15]);RtlOutByte(0xe9,' ');}
    RtlOutByte(0xe9,'\n');
}
#endif

NTKERNELAPI NTSTATUS NTAPI ZwWaitForSingleObject(HANDLE,BOOLEAN,PLARGE_INTEGER);

#define RTL_RX_DEPTH 16
#define RTL_RX_BYTES 2048
typedef struct RTL_URB_TARGET {
    PDEVICE_OBJECT Lower;
    KSPIN_LOCK Lock;
    PIRP Active;
    BOOLEAN Submitted,Stopping,Configured;
    unsigned BusSpeed;
    USBD_PIPE_HANDLE In,Out;
    USBD_CONFIGURATION_HANDLE Configuration;
    PVOID Thread,Owner;
    void (*Service)(void *);
    KEVENT Wake;
    struct RTL_URB_TARGET *RxLane;
    PVOID RxThread;
    PUCHAR RxBuffers;
    ULONG RxLength[RTL_RX_DEPTH],RxHead,RxCount,RxEpoch;
    BOOLEAN RxAccept,RxFault;
    BOOLEAN Reconfigure,ResetObserved;
    PUSB_CONFIGURATION_DESCRIPTOR ConfigurationData;
    ULONG InterfaceOffset;
    USB_DEVICE_DESCRIPTOR Identity;
} RTL_URB_TARGET;

typedef struct {KEVENT Event;NTSTATUS Status;} RTL_URB_WAIT;
static NTSTATUS NTAPI Completed(PDEVICE_OBJECT device,PIRP irp,PVOID context)
{
    RTL_URB_WAIT *wait=context;(void)device;
    wait->Status=irp->IoStatus.Status;
    KeSetEvent(&wait->Event,IO_NO_INCREMENT,FALSE);
    return STATUS_MORE_PROCESSING_REQUIRED;
}
static NTSTATUS Submit(RTL_URB_TARGET*t,PURB urb,ULONG milliseconds,BOOLEAN closing)
{
    PIRP irp;PIO_STACK_LOCATION stack;RTL_URB_WAIT wait;LARGE_INTEGER timeout;
    NTSTATUS status;KIRQL old;
    irp=IoAllocateIrp(t->Lower->StackSize,FALSE);if(!irp)return STATUS_INSUFFICIENT_RESOURCES;
    KeInitializeEvent(&wait.Event,NotificationEvent,FALSE);wait.Status=STATUS_UNSUCCESSFUL;
    stack=IoGetNextIrpStackLocation(irp);
    stack->MajorFunction=IRP_MJ_INTERNAL_DEVICE_CONTROL;
    stack->Parameters.DeviceIoControl.IoControlCode=IOCTL_INTERNAL_USB_SUBMIT_URB;
    stack->Parameters.Others.Argument1=urb;
    IoSetCompletionRoutine(irp,Completed,&wait,TRUE,TRUE,TRUE);
    irp->IoStatus.Status=STATUS_NOT_SUPPORTED;
    KeAcquireSpinLock(&t->Lock,&old);
    if((t->Stopping&&!closing)||t->Active){
        KeReleaseSpinLock(&t->Lock,old);IoFreeIrp(irp);return STATUS_DELETE_PENDING;
    }
    t->Active=irp;t->Submitted=FALSE;KeReleaseSpinLock(&t->Lock,old);
    IoCallDriver(t->Lower,irp);
    /* Stop during IoCallDriver is remembered. Do not cancel an IRP before
       the lower driver has received it, or race its final free. */
    KeAcquireSpinLock(&t->Lock,&old);t->Submitted=TRUE;
    if(t->Stopping&&!closing)IoCancelIrp(irp);
    KeReleaseSpinLock(&t->Lock,old);
    timeout.QuadPart=-(LONGLONG)milliseconds*10000;
    status=KeWaitForSingleObject(&wait.Event,Executive,KernelMode,FALSE,milliseconds?&timeout:NULL);
    if(status==STATUS_TIMEOUT){
        KeAcquireSpinLock(&t->Lock,&old);IoCancelIrp(irp);KeReleaseSpinLock(&t->Lock,old);
        KeWaitForSingleObject(&wait.Event,Executive,KernelMode,FALSE,NULL);
    }
    KeAcquireSpinLock(&t->Lock,&old);t->Active=NULL;t->Submitted=FALSE;KeReleaseSpinLock(&t->Lock,old);
    if(status!=STATUS_TIMEOUT)status=wait.Status;
    IoFreeIrp(irp);
    if(status==STATUS_TIMEOUT)return STATUS_IO_TIMEOUT;
    if(NT_SUCCESS(status)&&!USBD_SUCCESS(urb->UrbHeader.Status))return STATUS_UNSUCCESSFUL;
    return status;
}
static NTSTATUS Descriptor(RTL_URB_TARGET*t,UCHAR kind,void*buffer,ULONG length)
{
    struct _URB_CONTROL_DESCRIPTOR_REQUEST urb;NTSTATUS status;
    RtlZeroMemory(&urb,sizeof(urb));urb.Hdr.Length=sizeof(urb);urb.Hdr.Function=URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE;
    urb.DescriptorType=kind;urb.TransferBuffer=buffer;urb.TransferBufferLength=length;
    status=Submit(t,(PURB)&urb,5000,FALSE);
    if(NT_SUCCESS(status)&&urb.TransferBufferLength<length)return STATUS_DEVICE_DATA_ERROR;
    return status;
}
static NTSTATUS ResetPipe(RTL_URB_TARGET *lane,USBD_PIPE_HANDLE pipe)
{
    struct _URB_PIPE_REQUEST urb;
    RtlZeroMemory(&urb,sizeof(urb));urb.Hdr.Length=sizeof(urb);
    urb.Hdr.Function=URB_FUNCTION_SYNC_RESET_PIPE_AND_CLEAR_STALL;
    urb.PipeHandle=pipe;
    return Submit(lane,(PURB)&urb,5000,FALSE);
}
/* Construct the documented SELECT_CONFIGURATION ABI locally. Importing the
   USBD.SYS allocation helper makes the PCI driver unloadable on an NT5 system
   with no installed USB stack. The USB PDO still owns actual configuration. */
static PURB SelectConfiguration(PUSB_CONFIGURATION_DESCRIPTOR config,
                               PUSB_INTERFACE_DESCRIPTOR iface)
{
    PURB urb;PUSBD_INTERFACE_INFORMATION info;ULONG bytes,i;
    if(!iface||!iface->bNumEndpoints||iface->bNumEndpoints>16)return NULL;
    bytes=FIELD_OFFSET(struct _URB_SELECT_CONFIGURATION,Interface)+
          FIELD_OFFSET(USBD_INTERFACE_INFORMATION,Pipes)+
          iface->bNumEndpoints*sizeof(USBD_PIPE_INFORMATION);
    urb=ExAllocatePoolWithTag(NonPagedPool,bytes,RTL_URB_TAG);if(!urb)return NULL;
    RtlZeroMemory(urb,bytes);
    urb->UrbHeader.Length=(USHORT)bytes;
    urb->UrbHeader.Function=URB_FUNCTION_SELECT_CONFIGURATION;
    urb->UrbSelectConfiguration.ConfigurationDescriptor=config;
    info=&urb->UrbSelectConfiguration.Interface;
    info->Length=(USHORT)(bytes-FIELD_OFFSET(struct _URB_SELECT_CONFIGURATION,Interface));
    info->InterfaceNumber=iface->bInterfaceNumber;
    info->AlternateSetting=iface->bAlternateSetting;
    info->Class=iface->bInterfaceClass;info->SubClass=iface->bInterfaceSubClass;
    info->Protocol=iface->bInterfaceProtocol;
    info->NumberOfPipes=iface->bNumEndpoints;
    for(i=0;i<info->NumberOfPipes;i++)info->Pipes[i].MaximumTransferSize=4096;
    return urb;
}
void *RtlUrbOpen(void *lower,unsigned short *vendor,unsigned short *product)
{
    RTL_URB_TARGET*t;USB_DEVICE_DESCRIPTOR device;USB_CONFIGURATION_DESCRIPTOR head;
    PUSB_CONFIGURATION_DESCRIPTOR config=NULL;PUSB_INTERFACE_DESCRIPTOR iface=NULL;
    PUSBD_INTERFACE_INFORMATION info;PURB select=NULL;ULONG at,i,endpoints=0;BOOLEAN in=FALSE,out=FALSE;
    USHORT inPacket=0,outPacket=0;
    if(!lower)return NULL;
    t=ExAllocatePoolWithTag(NonPagedPool,sizeof(*t),RTL_URB_TAG);if(!t)return NULL;
    RtlZeroMemory(t,sizeof(*t));t->Lower=lower;KeInitializeSpinLock(&t->Lock);ObReferenceObject(t->Lower);
    KeInitializeEvent(&t->Wake,NotificationEvent,FALSE);
    if(!NT_SUCCESS(Descriptor(t,USB_DEVICE_DESCRIPTOR_TYPE,&device,sizeof(device)))||
       device.bLength!=sizeof(device)||!device.bNumConfigurations)goto fail;
    *vendor=device.idVendor;*product=device.idProduct;
    if(!NT_SUCCESS(Descriptor(t,USB_CONFIGURATION_DESCRIPTOR_TYPE,&head,sizeof(head)))||
       head.bLength!=sizeof(head)||head.wTotalLength<sizeof(head)||head.wTotalLength>4096)goto fail;
    config=ExAllocatePoolWithTag(NonPagedPool,head.wTotalLength,RTL_URB_TAG);if(!config)goto fail;
    if(!NT_SUCCESS(Descriptor(t,USB_CONFIGURATION_DESCRIPTOR_TYPE,config,head.wTotalLength))||
       config->wTotalLength!=head.wTotalLength)goto fail;
    for(at=0;at<head.wTotalLength;){
        PUCHAR p=(PUCHAR)config+at;ULONG len;
        if(head.wTotalLength-at<2)goto fail;
        len=p[0];
        if(len<2||len>head.wTotalLength-at)goto fail;
        if(p[1]==USB_INTERFACE_DESCRIPTOR_TYPE){
            if(iface&&in&&out)break;
            if(len<sizeof(USB_INTERFACE_DESCRIPTOR))goto fail;
            iface=(PUSB_INTERFACE_DESCRIPTOR)p;in=out=FALSE;endpoints=0;inPacket=outPacket=0;
            if(iface->bAlternateSetting!=0)iface=NULL;
        }else if(p[1]==USB_ENDPOINT_DESCRIPTOR_TYPE&&iface){
            PUSB_ENDPOINT_DESCRIPTOR e=(PUSB_ENDPOINT_DESCRIPTOR)p;
            if(len<sizeof(*e))goto fail;
            endpoints++;
            if((e->bmAttributes&3)==USB_ENDPOINT_TYPE_BULK){
                if(e->bEndpointAddress==0x81){in=TRUE;inPacket=e->wMaxPacketSize;}
                if(e->bEndpointAddress==0x02){out=TRUE;outPacket=e->wMaxPacketSize;}
            }
        }
        at+=len;
    }
    if(!iface||!in||!out||iface->bNumEndpoints!=endpoints)goto fail;
    /* The active bulk descriptor distinguishes FS/HS/SuperSpeed categories.
       USB3 link width/rate is not inferred from bcdUSB. The vendor init uses
       the same category for its 8157/8159 SuperSpeed and SuperSpeedPlus paths. */
    if(inPacket!=outPacket)goto fail;
    if(inPacket==64)t->BusSpeed=2;
    else if(inPacket==512)t->BusSpeed=3;
    else if(inPacket==1024)t->BusSpeed=5;
    else goto fail;
    select=SelectConfiguration(config,iface);if(!select)goto fail;
    info=&select->UrbSelectConfiguration.Interface;
    if(!NT_SUCCESS(Submit(t,select,5000,FALSE)))goto fail;
    t->Configured=TRUE;t->Configuration=select->UrbSelectConfiguration.ConfigurationHandle;
    if(info->NumberOfPipes!=endpoints||info->Length!=
       FIELD_OFFSET(USBD_INTERFACE_INFORMATION,Pipes)+endpoints*sizeof(USBD_PIPE_INFORMATION))goto fail;
    for(i=0;i<info->NumberOfPipes;i++){
        PUSBD_PIPE_INFORMATION pipe=&info->Pipes[i];
        if(pipe->PipeType!=UsbdPipeTypeBulk)continue;
        if(pipe->EndpointAddress==0x81)t->In=pipe->PipeHandle;
        if(pipe->EndpointAddress==0x02)t->Out=pipe->PipeHandle;
    }
    if(!t->In||!t->Out)goto fail;
    t->Identity=device;t->ConfigurationData=config;
    t->InterfaceOffset=(ULONG)((PUCHAR)iface-(PUCHAR)config);
    ExFreePool(select);return t;
fail:
    if(select)ExFreePool(select);
    if(config)ExFreePool(config);
    RtlUrbClose(t);return NULL;
}
unsigned RtlUrbBusSpeed(void*context)
{RTL_URB_TARGET*t=context;return t?t->BusSpeed:0;}
int RtlUrbCancelled(void*context)
{
    RTL_URB_TARGET*t=context;KIRQL old;BOOLEAN stopped;
    if(!t)return 1;
    KeAcquireSpinLock(&t->Lock,&old);stopped=t->Stopping;KeReleaseSpinLock(&t->Lock,old);
    return stopped;
}
int RtlUrbWait(void*context,unsigned ms)
{
    LARGE_INTEGER delay;
    if(RtlUrbCancelled(context))return 0;
    delay.QuadPart=-(LONGLONG)ms*10000;
    return KeDelayExecutionThread(KernelMode,FALSE,&delay)==STATUS_SUCCESS&&!RtlUrbCancelled(context);
}
static NTSTATUS TakeRx(RTL_URB_TARGET*t,void*buffer,ULONG length,PULONG done)
{
    KIRQL old;ULONG size;
    *done=0;
    KeAcquireSpinLock(&t->Lock,&old);
    if(t->Stopping||!t->RxCount){KeReleaseSpinLock(&t->Lock,old);return STATUS_DEVICE_NOT_READY;}
    size=t->RxLength[t->RxHead];
    if(length<size){KeReleaseSpinLock(&t->Lock,old);return STATUS_BUFFER_TOO_SMALL;}
    RtlCopyMemory(buffer,t->RxBuffers+t->RxHead*RTL_RX_BYTES,size);
    t->RxHead=(t->RxHead+1)%RTL_RX_DEPTH;t->RxCount--;
    KeReleaseSpinLock(&t->Lock,old);*done=size;return 0;
}
int RtlUrbTransfer(void *context,unsigned char kind,unsigned char endpoint,
                  unsigned char *setup,void *buffer,unsigned long length,unsigned long *done)
{
    RTL_URB_TARGET*t=context;NTSTATUS status;*done=0;
    if(kind==0){
        struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST urb;
        if(!setup||(setup[0]&0x7f)!=0x40||length!=(ULONG)(setup[6]|setup[7]<<8))return STATUS_INVALID_PARAMETER;
        RtlZeroMemory(&urb,sizeof(urb));urb.Hdr.Length=sizeof(urb);urb.Hdr.Function=URB_FUNCTION_VENDOR_DEVICE;
        urb.TransferFlags=(setup[0]&0x80)?USBD_TRANSFER_DIRECTION_IN:USBD_TRANSFER_DIRECTION_OUT;
        urb.TransferBuffer=buffer;urb.TransferBufferLength=length;urb.Request=setup[1];
        urb.Value=(USHORT)(setup[2]|setup[3]<<8);urb.Index=(USHORT)(setup[4]|setup[5]<<8);
        status=Submit(t,(PURB)&urb,5000,FALSE);if(NT_SUCCESS(status))*done=urb.TransferBufferLength;
    }else if(kind==1&&endpoint==0x81){
        return TakeRx(t,buffer,length,done);
    }else if(kind==1&&endpoint==0x02){
        struct _URB_BULK_OR_INTERRUPT_TRANSFER urb;
        RtlZeroMemory(&urb,sizeof(urb));urb.Hdr.Length=sizeof(urb);urb.Hdr.Function=URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER;
        urb.PipeHandle=t->Out;urb.TransferFlags=USBD_TRANSFER_DIRECTION_OUT;
        urb.TransferBuffer=buffer;urb.TransferBufferLength=length;
        status=Submit(t,(PURB)&urb,5000,FALSE);
        if(NT_SUCCESS(status))*done=urb.TransferBufferLength;
        else if(urb.Hdr.Status==USBD_STATUS_STALL_PID){
            /* Clear the endpoint for subsequent frames. Never resend this
               frame automatically: a failed completion may be ambiguous. */
            ResetPipe(t,t->Out);
        }
    }else return STATUS_INVALID_PARAMETER;
    return NT_SUCCESS(status)?0:status;
}
void RtlUrbCancel(void *context)
{
    RTL_URB_TARGET*t=context;KIRQL old;if(!t)return;
    KeAcquireSpinLock(&t->Lock,&old);t->Stopping=TRUE;
    if(t->Active&&t->Submitted)IoCancelIrp(t->Active);
    KeReleaseSpinLock(&t->Lock,old);
    KeSetEvent(&t->Wake,IO_NO_INCREMENT,FALSE);
}
/* Only the receive owner requests this, after its old URB has completed.
   It then submits nothing until the service owner publishes fresh handles.
   The service owner serializes this operation with all control/TX requests. */
static void RequestReconfiguration(RTL_URB_TARGET*t,ULONG usbStatus)
{
    KIRQL old;
    KeAcquireSpinLock(&t->Lock,&old);
    /* XP reports DEVICE_GONE during reset, then INVALID_PARAMETER on the
       old bulk request. A generic parameter error alone is not reset proof. */
    if(!t->Stopping&&usbStatus==USBD_STATUS_DEVICE_GONE)t->ResetObserved=TRUE;
    if(!t->Stopping&&(usbStatus==USBD_STATUS_INVALID_PIPE_HANDLE||
       (t->ResetObserved&&usbStatus==USBD_STATUS_INVALID_PARAMETER))){
        t->Reconfigure=TRUE;t->RxFault=TRUE;t->RxAccept=FALSE;
        t->RxEpoch++;t->RxHead=t->RxCount=0;
    }
    KeReleaseSpinLock(&t->Lock,old);
}
void RtlUrbObserveBusReset(void *context)
{
    RTL_URB_TARGET*t=context;KIRQL old;
    /* A full receive queue may leave no URB pending during a port reset.
       Lost MAC enable state is independent reset evidence in that case. */
    KeAcquireSpinLock(&t->Lock,&old);
    if(!t->Stopping)t->ResetObserved=TRUE;
    KeReleaseSpinLock(&t->Lock,old);
}
static int ReconfigurePipes(RTL_URB_TARGET*t)
{
    USB_DEVICE_DESCRIPTOR identity;PURB select;
    PUSB_INTERFACE_DESCRIPTOR iface;PUSBD_INTERFACE_INFORMATION info;
    USBD_PIPE_HANDLE in=NULL,out=NULL;ULONG i;KIRQL old;int success=0;
    if(!t->ConfigurationData||RtlUrbCancelled(t))return 0;
    if(!NT_SUCCESS(Descriptor(t,USB_DEVICE_DESCRIPTOR_TYPE,&identity,sizeof(identity)))||
       RtlCompareMemory(&identity,&t->Identity,sizeof(identity))!=sizeof(identity))return 0;
    iface=(PUSB_INTERFACE_DESCRIPTOR)((PUCHAR)t->ConfigurationData+t->InterfaceOffset);
    select=SelectConfiguration(t->ConfigurationData,iface);if(!select)return 0;
    info=&select->UrbSelectConfiguration.Interface;
    if(!NT_SUCCESS(Submit(t,select,5000,FALSE)))goto done;
    if(info->NumberOfPipes!=iface->bNumEndpoints||info->Length!=
       FIELD_OFFSET(USBD_INTERFACE_INFORMATION,Pipes)+iface->bNumEndpoints*sizeof(USBD_PIPE_INFORMATION))goto done;
    for(i=0;i<info->NumberOfPipes;i++){
        PUSBD_PIPE_INFORMATION pipe=&info->Pipes[i];
        if(pipe->PipeType!=UsbdPipeTypeBulk)continue;
        if(pipe->EndpointAddress==0x81)in=pipe->PipeHandle;
        if(pipe->EndpointAddress==0x02)out=pipe->PipeHandle;
    }
    if(!in||!out)goto done;
    KeAcquireSpinLock(&t->Lock,&old);
    if(!t->Stopping){
        t->In=in;t->Out=out;
        t->Configuration=select->UrbSelectConfiguration.ConfigurationHandle;
        t->RxEpoch++;t->RxHead=t->RxCount=0;
        t->RxFault=FALSE;t->Reconfigure=FALSE;t->ResetObserved=FALSE;success=1;
    }
    KeReleaseSpinLock(&t->Lock,old);
done:
    ExFreePool(select);return success;
}
static VOID NTAPI Worker(PVOID context)
{
    RTL_URB_TARGET*t=context;KIRQL old;BOOLEAN stop,reconfigure;LARGE_INTEGER delay,retry;
    delay.QuadPart=-100000;
    retry.QuadPart=-1000000;
    for(;;){
        KeAcquireSpinLock(&t->Lock,&old);stop=t->Stopping;reconfigure=t->Reconfigure;KeReleaseSpinLock(&t->Lock,old);
        if(stop)break;
        if(reconfigure&&!ReconfigurePipes(t)){
            KeWaitForSingleObject(&t->Wake,Executive,KernelMode,FALSE,&retry);continue;
        }
        t->Service(t->Owner);
        KeWaitForSingleObject(&t->Wake,Executive,KernelMode,FALSE,&delay);
    }
    PsTerminateSystemThread(STATUS_SUCCESS);
}
/* Carrier changes invalidate copied data without cancelling a partially
   received USB transfer. Its old epoch is discarded only after completion. */
int RtlUrbSetCarrier(void *context,int up)
{
    RTL_URB_TARGET*t=context;KIRQL old;
    KeAcquireSpinLock(&t->Lock,&old);
    up=up&&!t->RxFault&&!t->Stopping&&!t->Reconfigure;
    if(!up&&t->RxAccept){t->RxEpoch++;t->RxHead=t->RxCount=0;}
    t->RxAccept=!!up;KeReleaseSpinLock(&t->Lock,old);
    return up;
}
static void ReceiveFault(RTL_URB_TARGET*t,int fault)
{
    KIRQL old;KeAcquireSpinLock(&t->Lock,&old);t->RxFault=!!fault;
    if(fault){t->RxEpoch++;t->RxHead=t->RxCount=0;t->RxAccept=FALSE;}
    KeReleaseSpinLock(&t->Lock,old);
}
static void RecoverReceive(RTL_URB_TARGET*t,ULONG usbStatus)
{
    /* An acknowledged reset recovers a fresh pipe boundary after either a
       stall or port-reset cancellation; no packet is needed to unlock link. */
    if((usbStatus==USBD_STATUS_STALL_PID||usbStatus==USBD_STATUS_CANCELED)&&
       !RtlUrbCancelled(t)&&NT_SUCCESS(ResetPipe(t->RxLane,t->In)))
        ReceiveFault(t,FALSE);
}
void RtlUrbResetRx(void *context)
{
    RTL_URB_TARGET*t=context;KIRQL old;
    KeAcquireSpinLock(&t->Lock,&old);
    t->RxEpoch++;t->RxHead=t->RxCount=0;t->RxAccept=FALSE;
    KeReleaseSpinLock(&t->Lock,old);
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
    BOOLEAN stop,full;ULONG epoch;LARGE_INTEGER delay;delay.QuadPart=-100000;
#ifdef RTL_USB_TRACE
    NTSTATUS lastFailure=STATUS_SUCCESS;ULONG lastUsbFailure=0;
#endif
    for(;;){
        KeAcquireSpinLock(&t->Lock,&old);stop=t->Stopping;epoch=t->RxEpoch;full=t->RxCount==RTL_RX_DEPTH||t->Reconfigure;KeReleaseSpinLock(&t->Lock,old);
        if(stop)break;
        if(full){KeWaitForSingleObject(&t->Wake,Executive,KernelMode,FALSE,&delay);continue;}
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
        if(!NT_SUCCESS(status)&&(status!=lastFailure||urb.Hdr.Status!=lastUsbFailure)){
            TraceRxFailure(status,urb.Hdr.Status);lastFailure=status;lastUsbFailure=urb.Hdr.Status;
        }
#endif
        if(NT_SUCCESS(status)){
            KeAcquireSpinLock(&t->Lock,&old);t->ResetObserved=FALSE;KeReleaseSpinLock(&t->Lock,old);
            ReceiveFault(t,FALSE);QueueRx(t,epoch,buffer,urb.TransferBufferLength);
        }else{
            ReceiveFault(t,TRUE);
            RequestReconfiguration(t,urb.Hdr.Status);
            /* A port reset cancels the old receive. Waiting for a successful
               packet to clear RxFault would keep carrier down and prevent
               the traffic needed to recover. An acknowledged pipe reset
               establishes a fresh receive boundary without replaying data. */
            RecoverReceive(t,urb.Hdr.Status);
            KeWaitForSingleObject(&t->Wake,Executive,KernelMode,FALSE,&delay);
        }
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
void RtlUrbClose(void *context)
{
    RTL_URB_TARGET*t=context;struct _URB_SELECT_CONFIGURATION urb;if(!t)return;
    RtlUrbCancel(t);RtlUrbCancel(t->RxLane);
    if(t->Thread){KeWaitForSingleObject(t->Thread,Executive,KernelMode,FALSE,NULL);ObDereferenceObject(t->Thread);}
    if(t->RxThread){KeWaitForSingleObject(t->RxThread,Executive,KernelMode,FALSE,NULL);ObDereferenceObject(t->RxThread);}
    if(t->RxBuffers)ExFreePool(t->RxBuffers);
    if(t->RxLane)ExFreePool(t->RxLane);
    if(t->Configured){
        RtlZeroMemory(&urb,sizeof(urb));urb.Hdr.Length=sizeof(urb);urb.Hdr.Function=URB_FUNCTION_SELECT_CONFIGURATION;
        Submit(t,(PURB)&urb,1000,TRUE);
    }
    if(t->ConfigurationData)ExFreePool(t->ConfigurationData);
    ObDereferenceObject(t->Lower);ExFreePool(t);
}
