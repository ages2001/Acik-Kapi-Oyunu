/* NT3.1 MAC upper edge for the common USBNT frame queues.
   The MAC's resident poll callback supplies NDIS serialization and rundown.
   No miniport registration, interrupt object or miniport imports are used. */
#include "rltklant.h"
#include "backend_usb.h"
#include "nt31usb.h"
VOID STDC KeAcquireSpinLock(PVOID,PUCHAR);
VOID STDC KeReleaseSpinLock(PVOID,UCHAR);
VOID STDC NdisAllocateSpinLock(PNDIS_SPIN_LOCK l){l->SpinLock=0;l->OldIrql=0;}
VOID STDC NdisFreeSpinLock(PNDIS_SPIN_LOCK l){(void)l;}
VOID STDC NdisAcquireSpinLock(PNDIS_SPIN_LOCK l){UCHAR old;KeAcquireSpinLock(&l->SpinLock,&old);l->OldIrql=old;}
VOID STDC NdisReleaseSpinLock(PNDIS_SPIN_LOCK l){KeReleaseSpinLock(&l->SpinLock,l->OldIrql);}
/* MAC Send returns the resource error to the protocol directly; unlike a
   miniport, it does not ask the wrapper to retain a caller-owned packet. */
VOID STDC NdisMSendResourcesAvailable(NDIS_HANDLE owner){(void)owner;}
VOID RtlPublishName(PRLTK_ADAPTER a){(void)a;}
VOID RtlIndicateFrame(PRLTK_ADAPTER a,PUCHAR frame,UINT len){N31UsbReceive(a->MiniportHandle,frame,len);}
VOID RtlIndicateComplete(PRLTK_ADAPTER a){N31UsbReceiveComplete(a->MiniportHandle);}
VOID N31UsbInitialize(VOID){RtlUsbAdaptersInit();}
int N31UsbCreate(PVOID config,PVOID owner,PVOID *context,PUCHAR mac)
{
    PRLTK_ADAPTER a=RtlAlloc(sizeof(*a));int selected;
    *context=NULL;if(!a)return -1;
    NdisZeroMemory(a,sizeof(*a));a->WrapperConfig=config;a->MiniportHandle=owner;
    selected=RtlUsbReadConfig(a);
    if(selected!=1){RtlFree(a,sizeof(*a));return selected;}
    NdisAllocateSpinLock(&a->Lock);a->Ops=&RtlOpsUsb;
    if(!a->Ops->Init(a)){NdisFreeSpinLock(&a->Lock);RtlFree(a,sizeof(*a));return -1;}
    NdisMoveMemory(mac,a->PermanentAddr,6);*context=a;return 1;
}
VOID N31UsbDestroy(PVOID context)
{
    PRLTK_ADAPTER a=context;if(!a)return;
    a->Ops->Halt(a);NdisFreeSpinLock(&a->Lock);RtlFree(a,sizeof(*a));
}
VOID N31UsbReset(PVOID context)
{
    extern VOID RtlUsbResetQueues(PRLTK_ADAPTER);
    RtlUsbResetQueues(context);
}
VOID N31UsbFilter(PVOID context){PRLTK_ADAPTER a=context;a->Ops->SetRxMode(a);}
VOID N31UsbPoll(PVOID context){PRLTK_ADAPTER a=context;a->Ops->HandleInterrupt(a);}
int N31UsbSend(PVOID context,PUCHAR frame,UINT length)
{
    PRLTK_ADAPTER a=context;return a->Ops->Transmit(a,frame,length)?1:0;
}
int N31UsbLink(PVOID context,PULONG speed)
{
    PRLTK_ADAPTER a=context;BOOLEAN duplex;ULONG mbit=0;
    BOOLEAN up=a->Ops->LinkState(a,&mbit,&duplex);*speed=mbit*10000UL;return up?1:0;
}
VOID N31UsbName(PVOID context,char *name,UINT capacity)
{
    PRLTK_ADAPTER a=context;UINT i;if(!capacity)return;
    NdisAcquireSpinLock(&a->Lock);
    for(i=0;i+1<capacity&&a->FriendlyName[i];i++)name[i]=a->FriendlyName[i];
    name[i]=0;NdisReleaseSpinLock(&a->Lock);
}
