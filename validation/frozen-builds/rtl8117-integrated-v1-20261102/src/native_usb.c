/* NDIS5 native USB adapter owner. Device identity/configuration come from its
   own lower device object, so each PnP instance has independent I/O lifetime.
   Common NDIS callbacks consume copied packet queues at DISPATCH_LEVEL. */
#include "backend_usb.h"
#include "native_urb.h"
#include "usbnames.h"
typedef struct {PVOID Transport;USBNIC Nic;} RTL_NATIVE_USB;
static VOID Service(PVOID context)
{
    RTL_NATIVE_USB*c=context;
    RtlUsbService(&c->Nic,1);
}
BOOLEAN RtlNativePrepare(PRLTK_ADAPTER a)
{
    RTL_NATIVE_USB*c;PVOID lower=NULL;USHORT vid=0,pid=0;UINT i;UCHAR any=0;
    USBNT_OSVER os={5,0};NDIS_STATUS status;const char*name;
    USBNIC_PLATFORM platform;
    NdisMGetDeviceProperty(a->MiniportHandle,NULL,NULL,&lower,NULL,NULL);
    if(!lower)return FALSE;
    c=RtlAlloc(sizeof(*c));if(!c)return FALSE;NdisZeroMemory(c,sizeof(*c));a->NativeUsb=c;
    c->Transport=RtlUrbOpen(lower,&vid,&pid);if(!c->Transport)return FALSE;
    platform.BusSpeed=RtlUrbBusSpeed(c->Transport);platform.WaitMs=RtlUrbWait;
    platform.Cancelled=RtlUrbCancelled;platform.Context=c->Transport;
    status=UsbNicAttachEx(&c->Nic,os,UsbHcUnknown,vid,pid,c->Transport,RtlUrbTransfer,&platform);
    if(status!=NDIS_STATUS_SUCCESS)return FALSE;
    for(i=0;i<6;i++)any|=c->Nic.Nic.Mac[i];
    if(!any||(c->Nic.Nic.Mac[0]&1))return FALSE;
    NdisMoveMemory(a->PermanentAddr,c->Nic.Nic.Mac,6);NdisMoveMemory(a->CurrentAddr,a->PermanentAddr,6);
    a->MaxSpeedMbit=c->Nic.Nic.MaxSpeedMbit;
    name=RtlUsbDetectedName(&c->Nic.Nic);
    for(i=0;i+1<sizeof(a->FriendlyName)&&name[i];i++)a->FriendlyName[i]=name[i];
    a->FriendlyName[i]=0;return TRUE;
}
BOOLEAN RtlNativeStart(PRLTK_ADAPTER a)
{
    RTL_NATIVE_USB*c=a->NativeUsb;return RtlUrbStart(c->Transport,Service,c)?TRUE:FALSE;
}
VOID RtlNativeStop(PRLTK_ADAPTER a)
{
    RTL_NATIVE_USB*c=a->NativeUsb;if(!c)return;
    RtlUrbClose(c->Transport);a->NativeUsb=NULL;RtlFree(c,sizeof(*c));
}

BOOLEAN RtlNativeSetCarrier(PRLTK_ADAPTER a,BOOLEAN up)
{
    RTL_NATIVE_USB*c=a->NativeUsb;return c&&RtlUrbSetCarrier(c->Transport,up)?TRUE:FALSE;
}

VOID RtlNativeResetRx(PRLTK_ADAPTER a)
{
    RTL_NATIVE_USB*c=a->NativeUsb;if(c)RtlUrbResetRx(c->Transport);
}

VOID RtlNativeObserveBusReset(PRLTK_ADAPTER a)
{
    RTL_NATIVE_USB*c=a->NativeUsb;if(c)RtlUrbObserveBusReset(c->Transport);
}
