#include "backend_usb.h"
#include "usbnames.h"
#include "usbqueue.h"
#include "rtl8150.h"
#ifdef RTL_DBG
static VOID UsbInitTrace(const char *stage)
{
    const char *prefix="USB-INIT ";
    while(*prefix)RtlOutByte(0xe9,(UCHAR)*prefix++);
    while(*stage)RtlOutByte(0xe9,(UCHAR)*stage++);
    RtlOutByte(0xe9,'\n');
}
#else
#define UsbInitTrace(stage) ((void)0)
#endif
#define USB_ADAPTER_LIMIT 64
VOID STDC KeInitializeEvent(PVOID,int,BOOLEAN);
LONG STDC KeSetEvent(PVOID,LONG,BOOLEAN);
NTSTATUS STDC KeWaitForSingleObject(PVOID,int,int,BOOLEAN,PLARGE_INTEGER);
typedef struct {
    PRLTK_ADAPTER Adapter;
    NDIS_SPIN_LOCK Lock;
    RTL_USB_QUEUE Tx, Rx;
    ULONG Attachment, References, Speed, Epoch;
    BOOLEAN Up, Duplex, Removing, FilterDirty;
    union {ULONG_PTR Align;UCHAR Bytes[64];} Drained;
} RTL_USB_CONTEXT;
static RTL_USB_CONTEXT *adapters[USB_ADAPTER_LIMIT];
static NDIS_SPIN_LOCK adapterLock;

VOID RtlUsbAdaptersInit(VOID){NdisAllocateSpinLock(&adapterLock);}
static RTL_USB_CONTEXT *Acquire(const UCHAR *mac)
{
    RTL_USB_CONTEXT *c=NULL;UINT i;
    NdisAcquireSpinLock(&adapterLock);
    for(i=0;i<USB_ADAPTER_LIMIT;i++)if(adapters[i]&&!adapters[i]->Removing&&
        !memcmp(adapters[i]->Adapter->PermanentAddr,mac,6)){
        c=adapters[i];c->References++;break;
    }
    NdisReleaseSpinLock(&adapterLock);return c;
}
static VOID Release(RTL_USB_CONTEXT *c)
{
    /* Signal under the ownership lock: after the final decrement, Halt may
       otherwise free the event before this thread calls KeSetEvent. */
    NdisAcquireSpinLock(&adapterLock);
    c->References--;
    if(c->Removing&&!c->References)KeSetEvent(&c->Drained,0,FALSE);
    NdisReleaseSpinLock(&adapterLock);
}
static int Hex(WCHAR ch)
{
    if(ch>='0'&&ch<='9')return ch-'0';
    if(ch>='a'&&ch<='f')return ch-'a'+10;
    if(ch>='A'&&ch<='F')return ch-'A'+10;
    return -1;
}
int RtlUsbReadConfig(PRLTK_ADAPTER a)
{
    static WCHAR transport[]=L"UsbNtAdapter",mac[]=L"UsbPermanentAddress";
    NDIS_STATUS st;NDIS_HANDLE cfg;NDIS_STRING key;
    PNDIS_CONFIGURATION_PARAMETER p;UINT i;int hi,lo,found=0;UCHAR any=0;
    NdisOpenConfiguration(&st,&cfg,a->WrapperConfig);
    if(st!=NDIS_STATUS_SUCCESS)return 0;
#if defined(RTL_NDIS5)
    {
        static WCHAR native[]=L"UsbNativeAdapter";
        key.Buffer=native;key.Length=sizeof(native)-2;key.MaximumLength=sizeof(native);
        NdisReadConfiguration(&st,&p,cfg,&key,NdisParameterInteger);
        if(st==NDIS_STATUS_SUCCESS&&p->ParameterData.IntegerData){
            a->NativeUsbRequested=TRUE;a->Backend=RTL_BACKEND_USBNT;found=1;goto done;
        }
    }
#endif
    key.Buffer=transport;key.Length=sizeof(transport)-2;key.MaximumLength=sizeof(transport);
    NdisReadConfiguration(&st,&p,cfg,&key,NdisParameterInteger);
    if(st!=NDIS_STATUS_SUCCESS||!p->ParameterData.IntegerData)goto done;
    found=-1;
#if defined(RTL_NDIS5)
    goto done; /* NT5 uses its own PnP lower device; no competing global client. */
#endif
    key.Buffer=mac;key.Length=sizeof(mac)-2;key.MaximumLength=sizeof(mac);
    NdisReadConfiguration(&st,&p,cfg,&key,NdisParameterString);
    if(st!=NDIS_STATUS_SUCCESS||p->ParameterData.StringData.Length!=24)goto done;
    for(i=0;i<6;i++){
        hi=Hex(p->ParameterData.StringData.Buffer[i*2]);lo=Hex(p->ParameterData.StringData.Buffer[i*2+1]);
        if(hi<0||lo<0)goto done;
        a->PermanentAddr[i]=(UCHAR)((hi<<4)|lo);any|=a->PermanentAddr[i];
    }
    if(!any||(a->PermanentAddr[0]&1))goto done;
    NdisMoveMemory(a->CurrentAddr,a->PermanentAddr,6);
    NdisMoveMemory(a->FriendlyName,"Realtek USB Ethernet Adapter",29);
    a->Backend=RTL_BACKEND_USBNT;a->MaxSpeedMbit=10000;found=1;
done:
    NdisCloseConfiguration(cfg);return found;
}
static BOOLEAN UsbInit(PRLTK_ADAPTER a)
{
    RTL_USB_CONTEXT *c;UINT i,freeSlot=USB_ADAPTER_LIMIT;
#if defined(RTL_NDIS5)
    if(a->NativeUsbRequested&&!RtlNativePrepare(a))return FALSE;
#endif
    c=(RTL_USB_CONTEXT*)RtlAlloc(sizeof(*c));if(!c){UsbInitTrace("allocate");return FALSE;}
    NdisZeroMemory(c,sizeof(*c));c->Adapter=a;c->FilterDirty=TRUE;
    NdisAllocateSpinLock(&c->Lock);KeInitializeEvent(&c->Drained,0,FALSE);
    NdisAcquireSpinLock(&adapterLock);
    for(i=0;i<USB_ADAPTER_LIMIT;i++){
        if(!adapters[i]){if(freeSlot==USB_ADAPTER_LIMIT)freeSlot=i;}
        else if(!memcmp(adapters[i]->Adapter->PermanentAddr,a->PermanentAddr,6))break;
    }
    if(i==USB_ADAPTER_LIMIT&&freeSlot<USB_ADAPTER_LIMIT){adapters[freeSlot]=c;a->UsbContext=c;}
    NdisReleaseSpinLock(&adapterLock);
    if(!a->UsbContext){UsbInitTrace(i<USB_ADAPTER_LIMIT?"duplicate-mac":"adapter-limit");NdisFreeSpinLock(&c->Lock);RtlFree(c,sizeof(*c));return FALSE;}
#if defined(RTL_NDIS5)
    if(a->NativeUsbRequested&&!RtlNativeStart(a)){UsbInitTrace("threads");return FALSE;}
#endif
    return TRUE;
}
static VOID UsbHalt(PRLTK_ADAPTER a)
{
    RTL_USB_CONTEXT *c=(RTL_USB_CONTEXT*)a->UsbContext;UINT i;BOOLEAN busy;
#if defined(RTL_NDIS5)
    RtlNativeStop(a);
#endif
    if(!c)return;
    NdisAcquireSpinLock(&adapterLock);c->Removing=TRUE;
    for(i=0;i<USB_ADAPTER_LIMIT;i++)if(adapters[i]==c)adapters[i]=NULL;
    busy=c->References!=0;NdisReleaseSpinLock(&adapterLock);
    if(busy)KeWaitForSingleObject(&c->Drained,0,0,FALSE,NULL);
    NdisFreeSpinLock(&c->Lock);a->UsbContext=NULL;RtlFree(c,sizeof(*c));
}
static VOID UsbRxMode(PRLTK_ADAPTER a)
{
    RTL_USB_CONTEXT *c=a->UsbContext;
    NdisAcquireSpinLock(&c->Lock);c->FilterDirty=TRUE;NdisReleaseSpinLock(&c->Lock);
}
static BOOLEAN UsbTransmit(PRLTK_ADAPTER a,PUCHAR frame,UINT len)
{
    RTL_USB_CONTEXT *c=a->UsbContext;BOOLEAN result;
    NdisAcquireSpinLock(&c->Lock);
    result=c->Up&&RtlUsbQueuePut(&c->Tx,frame,len);
    NdisReleaseSpinLock(&c->Lock);return result;
}
static BOOLEAN UsbIsr(PRLTK_ADAPTER a){(void)a;return FALSE;}
static VOID UsbInts(PRLTK_ADAPTER a,BOOLEAN enable){(void)a;(void)enable;}
static BOOLEAN UsbLink(PRLTK_ADAPTER a,PULONG speed,PBOOLEAN duplex)
{
    RTL_USB_CONTEXT *c=a->UsbContext;BOOLEAN up;
    NdisAcquireSpinLock(&c->Lock);up=c->Up;*speed=c->Speed;*duplex=c->Duplex;NdisReleaseSpinLock(&c->Lock);
    return up;
}
static VOID UsbDrain(PRLTK_ADAPTER a)
{
    RTL_USB_CONTEXT *c=a->UsbContext;UINT len,i;UCHAR frame[RLTK_MAX_FRAME];
    for(i=0;i<RTL_USB_QUEUE_DEPTH;i++){
        NdisAcquireSpinLock(&c->Lock);len=RtlUsbQueueGet(&c->Rx,frame);NdisReleaseSpinLock(&c->Lock);
        if(!len)break;
        RtlIndicateFrame(a,frame,len);
    }
    if(i)RtlIndicateComplete(a);
    NdisAcquireSpinLock(&c->Lock);i=c->Up&&c->Tx.Count<RTL_USB_QUEUE_DEPTH;NdisReleaseSpinLock(&c->Lock);
    if(i&&a->TxStalled){a->TxStalled=FALSE;NdisMSendResourcesAvailable(a->MiniportHandle);}
}
VOID RtlUsbDisconnected(const UCHAR *mac)
{
    RTL_USB_CONTEXT *c=Acquire(mac);if(!c)return;
    NdisAcquireSpinLock(&c->Lock);c->Up=FALSE;c->Attachment=0;c->Epoch++;
    RtlUsbQueueClear(&c->Tx);RtlUsbQueueClear(&c->Rx);NdisReleaseSpinLock(&c->Lock);
    Release(c);
}
typedef struct {RTL_USB_CONTEXT *Context;ULONG Epoch,Attachment;} RTL_USB_RECEIVE;
static VOID Receive(PVOID context,PUCHAR frame,ULONG len)
{
    RTL_USB_RECEIVE *r=context;RTL_USB_CONTEXT *c=r->Context;
    NdisAcquireSpinLock(&c->Lock);
    if(c->Up&&c->Epoch==r->Epoch&&c->Attachment==r->Attachment&&
       !RtlUsbQueuePut(&c->Rx,frame,len))c->Adapter->RcvNoBuf++;
    NdisReleaseSpinLock(&c->Lock);
}
VOID RtlUsbService(PUSBNIC nic,ULONG attachment)
{
    RTL_USB_CONTEXT *c=Acquire(nic->Nic.Mac);PRLTK_ADAPTER a;
    UCHAR frame[RLTK_MAX_FRAME];UINT len,i;BOOLEAN up=FALSE,duplex=FALSE,newDevice,filter;
    ULONG speed=0,epoch;NDIS_STATUS status;BOOLEAN filterFailed=FALSE,reset=FALSE;RTL_USB_RECEIVE receive;
    if(!c)return;
    a=c->Adapter;
    NdisAcquireSpinLock(&c->Lock);newDevice=c->Attachment!=attachment;
    if(newDevice){c->Epoch++;c->Up=FALSE;c->Attachment=attachment;c->FilterDirty=TRUE;RtlUsbQueueClear(&c->Tx);RtlUsbQueueClear(&c->Rx);}
    epoch=c->Epoch;filter=c->FilterDirty;c->FilterDirty=FALSE;NdisReleaseSpinLock(&c->Lock);
    if(newDevice){
        const char *name=RtlUsbDetectedName(&nic->Nic);
        NdisAcquireSpinLock(&a->Lock);
        for(i=0;i<sizeof(a->FriendlyName)-1&&name[i];i++)a->FriendlyName[i]=name[i];
        a->FriendlyName[i]=0;
        a->MaxSpeedMbit=nic->Nic.MaxSpeedMbit?nic->Nic.MaxSpeedMbit:100;
        NdisReleaseSpinLock(&a->Lock);
        /* WrapperConfig is valid only during MiniportInitialize. Native PnP
           instances publish their name there, after descriptor discovery.
           A passive service callback must never reopen that expired context.
           Legacy persistent instances expose changes through the vendor OID. */
    }
    status=UsbNicCheckReset(nic,&reset);
    if(USB_OK(status)&&reset){
#if defined(RTL_NDIS5)
        if(a->NativeUsbRequested){
            RtlNativeSetCarrier(a,FALSE);
            RtlNativeObserveBusReset(a);
        }
#endif
        NdisAcquireSpinLock(&c->Lock);
        c->Epoch++;epoch=c->Epoch;c->Up=FALSE;
        RtlUsbQueueClear(&c->Tx);RtlUsbQueueClear(&c->Rx);
        c->FilterDirty=TRUE;filter=TRUE;
        NdisReleaseSpinLock(&c->Lock);
        status=UsbNicRecoverReset(nic);
    }
    if(!USB_OK(status))filterFailed=TRUE;
    if(filter&&!filterFailed){
        /* Accept all hardware classes; NDIS's Ethernet filter applies the
           per-binding unicast/multicast/promiscuous policy at indication. */
        if(nic->Nic.Chip->DeviceId==RTL8150_DEVICE_ID){
            UCHAR rcr=0x9f;
            status=nic->Nic.Pipe.Control(&nic->Nic.Pipe,RTLU_RT_WRITE,RTLU_REQ_SET_REGS,R8150_RCR,0,&rcr,1);
        }else status=RtlUsbSetRxMode(&nic->Nic,0x0f);
        if(!USB_OK(status)){filterFailed=TRUE;NdisAcquireSpinLock(&c->Lock);c->FilterDirty=TRUE;NdisReleaseSpinLock(&c->Lock);}
    }
    status=filterFailed?NDIS_STATUS_FAILURE:UsbNicReadMedia(nic,&up,&speed,&duplex);
    if(USB_OK(status)){
#if defined(RTL_NDIS5)
        if(a->NativeUsbRequested&&nic->HardwareReady&&
           (nic->HardwareActive!=up||(up&&(nic->HardwareSpeed!=speed||nic->HardwareDuplex!=duplex))))
            RtlNativeSetCarrier(a,FALSE);
#endif
        status=UsbNicApplyCarrier(nic,up,speed,duplex);
    }
    NdisAcquireSpinLock(&c->Lock);
    if(c->Epoch!=epoch||c->Attachment!=attachment){NdisReleaseSpinLock(&c->Lock);Release(c);return;}
    c->Up=USB_OK(status)&&up;c->Speed=speed;c->Duplex=duplex;
#if defined(RTL_NDIS5)
    /* Keep carrier publication ordered with the miniport epoch/reset. The
       native queue lock never nests this adapter lock in the opposite order. */
    if(a->NativeUsbRequested){extern BOOLEAN RtlNativeSetCarrier(PRLTK_ADAPTER,BOOLEAN);up=RtlNativeSetCarrier(a,c->Up);c->Up=up;}
#endif
    if(!c->Up){RtlUsbQueueClear(&c->Tx);RtlUsbQueueClear(&c->Rx);}
    NdisReleaseSpinLock(&c->Lock);
    if(USB_OK(status)&&up){
        for(i=0;i<RTL_USB_QUEUE_DEPTH;i++){
            NdisAcquireSpinLock(&c->Lock);
            len=(c->Epoch==epoch&&c->Attachment==attachment)?RtlUsbQueueGet(&c->Tx,frame):0;
            NdisReleaseSpinLock(&c->Lock);
            if(!len)break;
            if(!USB_OK(UsbNicSend(nic,frame,len)))a->XmitErr++;
        }
        receive.Context=c;receive.Epoch=epoch;receive.Attachment=attachment;
#if defined(RTL_NDIS5)
        if(a->NativeUsbRequested){
            /* Native RX owns already completed URBs: drain a bounded batch
               without blocking on the bus. Legacy synchronous poll stays one. */
            for(i=0;i<RTL_USB_QUEUE_DEPTH;i++)if(!USB_OK(UsbNicPoll(nic,Receive,&receive)))break;
        }else
#endif
        UsbNicPoll(nic,Receive,&receive);
    }
    Release(c);
}
RLTK_OPS RtlOpsUsb={"USBNT",UsbInit,UsbHalt,UsbRxMode,UsbTransmit,UsbIsr,UsbDrain,UsbInts,UsbLink};

VOID RtlUsbResetQueues(PRLTK_ADAPTER a)
{
    RTL_USB_CONTEXT *c=a->UsbContext;
    NdisAcquireSpinLock(&c->Lock);
    c->Epoch++;c->Up=FALSE;
    RtlUsbQueueClear(&c->Tx);RtlUsbQueueClear(&c->Rx);c->FilterDirty=TRUE;
#if defined(RTL_NDIS5)
    /* Also invalidate raw URBs queued before this reset. This is lock-only
       work and does not wait for USB I/O at the NDIS reset callback's IRQL.
       Hold c->Lock until both queues are invalidated: otherwise a new-epoch
       worker can copy an old raw URB into the already-reset frame queue. */
    if(a->NativeUsbRequested){extern VOID RtlNativeResetRx(PRLTK_ADAPTER);RtlNativeResetRx(a);}
#endif
    NdisReleaseSpinLock(&c->Lock);
}
