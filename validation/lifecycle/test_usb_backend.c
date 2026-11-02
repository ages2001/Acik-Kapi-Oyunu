/* Production USB miniport: queue boundaries, reset/detach races and Halt rundown. */
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sched.h>
#include <wchar.h>
#define RTL_BACKEND_USB_H
#include "usbnic.h"
#define RTL_BACKEND_USBNT 3
#define RLTK_MAX_FRAME 1514
#define RLTK_HEADER_SIZE 14
typedef int32_t LONG,NTSTATUS;
typedef uintptr_t ULONG_PTR;
typedef wchar_t WCHAR;
typedef void *NDIS_HANDLE,*PLARGE_INTEGER;
typedef pthread_mutex_t NDIS_SPIN_LOCK;
typedef struct {WCHAR *Buffer;USHORT Length,MaximumLength;} NDIS_STRING;
typedef struct {union {ULONG IntegerData;NDIS_STRING StringData;} ParameterData;} NDIS_CONFIGURATION_PARAMETER,*PNDIS_CONFIGURATION_PARAMETER;
enum {NdisParameterInteger,NdisParameterString};
typedef struct Adapter {UCHAR PermanentAddr[6],CurrentAddr[6];char FriendlyName[80];ULONG MaxSpeedMbit,Backend,RcvNoBuf,XmitErr;BOOLEAN TxStalled,NativeUsbRequested;void *UsbContext,*WrapperConfig,*MiniportHandle;NDIS_SPIN_LOCK Lock;} RLTK_ADAPTER,*PRLTK_ADAPTER;
typedef struct {char *Name;BOOLEAN(*Init)(PRLTK_ADAPTER);VOID(*Halt)(PRLTK_ADAPTER);VOID(*SetRxMode)(PRLTK_ADAPTER);BOOLEAN(*Transmit)(PRLTK_ADAPTER,PUCHAR,UINT);BOOLEAN(*Isr)(PRLTK_ADAPTER);VOID(*HandleInterrupt)(PRLTK_ADAPTER);VOID(*EnableInts)(PRLTK_ADAPTER,BOOLEAN);BOOLEAN(*LinkState)(PRLTK_ADAPTER,PULONG,PBOOLEAN);} RLTK_OPS;
static atomic_int waiting,freed,park_poll,poll_entered,release_poll;
static int fail_filter,fail_media,reset_poll,detach_poll,reset_media,indicated,sent,polls,resources;
static PRLTK_ADAPTER active;
static int mac_reset,fail_recovery,recovered;
static void NdisAllocateSpinLock(NDIS_SPIN_LOCK*p){assert(!pthread_mutex_init(p,NULL));}
static void NdisAcquireSpinLock(NDIS_SPIN_LOCK*p){assert(!pthread_mutex_lock(p));}
static void NdisReleaseSpinLock(NDIS_SPIN_LOCK*p){assert(!pthread_mutex_unlock(p));}
static void NdisFreeSpinLock(NDIS_SPIN_LOCK*p){assert(!pthread_mutex_destroy(p));}
#define NdisMoveMemory memcpy
#define NdisZeroMemory(p,n) memset(p,0,n)
static void *RtlAlloc(size_t n){return calloc(1,n);}
static void RtlFree(void*p,size_t n){(void)n;free(p);atomic_fetch_add(&freed,1);}
VOID KeInitializeEvent(PVOID p,int type,BOOLEAN value){(void)type;atomic_init((atomic_int*)p,value);}
LONG KeSetEvent(PVOID p,LONG inc,BOOLEAN wait){(void)inc;(void)wait;atomic_store((atomic_int*)p,1);return 0;}
NTSTATUS KeWaitForSingleObject(PVOID p,int a,int b,BOOLEAN c,PLARGE_INTEGER d){(void)a;(void)b;(void)c;(void)d;atomic_store(&waiting,1);while(!atomic_load((atomic_int*)p))sched_yield();return 0;}
static void NdisOpenConfiguration(NDIS_STATUS*s,NDIS_HANDLE*h,NDIS_HANDLE w){(void)h;(void)w;*s=NDIS_STATUS_FAILURE;}
static void NdisReadConfiguration(NDIS_STATUS*s,PNDIS_CONFIGURATION_PARAMETER*p,NDIS_HANDLE c,NDIS_STRING*k,int type){(void)p;(void)c;(void)k;(void)type;*s=NDIS_STATUS_FAILURE;}
static void NdisCloseConfiguration(NDIS_HANDLE h){(void)h;}
static void RtlPublishName(PRLTK_ADAPTER a){(void)a;assert(!"passive worker reused the expired initialize configuration");}
static void RtlIndicateFrame(PRLTK_ADAPTER a,PUCHAR f,UINT n){(void)a;assert(n==64&&f[0]==0x5a);indicated++;}
static void RtlIndicateComplete(PRLTK_ADAPTER a){(void)a;}
static void NdisMSendResourcesAvailable(NDIS_HANDLE h){(void)h;resources++;}
VOID RtlUsbResetQueues(PRLTK_ADAPTER a);
VOID RtlUsbDisconnected(const UCHAR *mac);
NDIS_STATUS UsbNicCheckReset(PUSBNIC n,PBOOLEAN reset){(void)n;*reset=mac_reset;return NDIS_STATUS_SUCCESS;}
NDIS_STATUS UsbNicRecoverReset(PUSBNIC n){(void)n;recovered++;mac_reset=0;return fail_recovery?NDIS_STATUS_FAILURE:NDIS_STATUS_SUCCESS;}
NDIS_STATUS RtlUsbSetRxMode(PRTL_USB_NIC n,ULONG flags){(void)n;assert(flags==15);return fail_filter?NDIS_STATUS_FAILURE:NDIS_STATUS_SUCCESS;}
NDIS_STATUS UsbNicReadMedia(PUSBNIC n,PBOOLEAN up,PULONG speed,PBOOLEAN duplex){(void)n;if(reset_media)RtlUsbResetQueues(active);*up=TRUE;*speed=1000;*duplex=TRUE;return fail_media?NDIS_STATUS_FAILURE:NDIS_STATUS_SUCCESS;}
NDIS_STATUS UsbNicApplyCarrier(PUSBNIC n,BOOLEAN up,ULONG speed,BOOLEAN duplex){(void)n;(void)up;(void)speed;(void)duplex;return NDIS_STATUS_SUCCESS;}
NDIS_STATUS UsbNicSend(PUSBNIC n,PUCHAR f,ULONG len){(void)n;assert(len==64&&f[0]==0x5a);sent++;return NDIS_STATUS_SUCCESS;}
NDIS_STATUS UsbNicPoll(PUSBNIC n,PFN_USB_RX_SINK sink,PVOID ctx){UCHAR frame[64];polls++;memset(frame,0x5a,sizeof(frame));if(reset_poll)RtlUsbResetQueues(active);if(detach_poll)RtlUsbDisconnected(n->Nic.Mac);if(atomic_load(&park_poll)){atomic_store(&poll_entered,1);while(!atomic_load(&release_poll))sched_yield();}sink(ctx,frame,sizeof(frame));return NDIS_STATUS_SUCCESS;}
#if defined(RTL_NDIS5)
BOOLEAN RtlNativePrepare(PRLTK_ADAPTER a){(void)a;return TRUE;}
BOOLEAN RtlNativeStart(PRLTK_ADAPTER a){(void)a;return TRUE;}
VOID RtlNativeStop(PRLTK_ADAPTER a){(void)a;}
BOOLEAN RtlNativeSetCarrier(PRLTK_ADAPTER a,BOOLEAN up);
static unsigned native_bus_resets;
VOID RtlNativeObserveBusReset(PRLTK_ADAPTER a){(void)a;native_bus_resets++;}
#endif
#include "../../RLTKLANT/src/backend_usb.c"
#if defined(RTL_NDIS5)
static unsigned native_carriers,native_resets;
static int native_fault;
BOOLEAN RtlNativeSetCarrier(PRLTK_ADAPTER a,BOOLEAN up){
 RTL_USB_CONTEXT*c=a->UsbContext;(void)up;
 {int locked=pthread_mutex_trylock(&c->Lock);if(!locked){assert(!up);pthread_mutex_unlock(&c->Lock);}else assert(locked==EBUSY);}
 native_carriers++;return up&&!native_fault;
}
VOID RtlNativeResetRx(PRLTK_ADAPTER a){
 RTL_USB_CONTEXT*c=a->UsbContext;
 assert(pthread_mutex_trylock(&c->Lock)==EBUSY);native_resets++;
}
#endif
static void init(RLTK_ADAPTER*a,USBNIC*n){memset(a,0,sizeof(*a));memset(n,0,sizeof(*n));a->PermanentAddr[0]=2;a->PermanentAddr[5]=1;memcpy(n->Nic.Mac,a->PermanentAddr,6);n->Nic.Chip=&RtlUsbIds[3];n->Nic.MaxSpeedMbit=1000;NdisAllocateSpinLock(&a->Lock);assert(UsbInit(a));active=a;}
static void *service(void*p){RtlUsbService(p,1);return NULL;}
static void *halt(void*p){UsbHalt(p);return NULL;}
static void wait_flag(atomic_int*p){while(!atomic_load(p))sched_yield();}
int main(void){
 RLTK_ADAPTER a,b;USBNIC n;RTL_USB_CONTEXT*c;UCHAR frame[64];int i,j;pthread_t worker,stop;
 RtlUsbAdaptersInit();init(&a,&n);c=a.UsbContext;memset(frame,0x5a,sizeof(frame));
 assert(!UsbTransmit(&a,frame,64));RtlUsbService(&n,1);assert(c->Up&&c->Rx.Count==1);UsbDrain(&a);assert(indicated==1);
 b=a;b.UsbContext=NULL;assert(!UsbInit(&b));assert(!b.UsbContext);
 assert(!UsbTransmit(&a,frame,13));assert(!UsbTransmit(&a,frame,1515));
 for(i=0;i<16;i++)assert(UsbTransmit(&a,frame,64));assert(!UsbTransmit(&a,frame,64));frame[0]=0;RtlUsbService(&n,1);assert(sent==16&&!c->Tx.Count);frame[0]=0x5a;
 a.TxStalled=1;UsbDrain(&a);assert(resources==1&&!a.TxStalled);
 reset_poll=1;RtlUsbService(&n,1);assert(!c->Up&&!c->Rx.Count);reset_poll=0;RtlUsbService(&n,1);assert(c->Up&&c->Rx.Count==1);
 detach_poll=1;RtlUsbService(&n,1);assert(!c->Up&&!c->Attachment&&!c->Rx.Count);detach_poll=0;
 reset_media=1;RtlUsbService(&n,2);assert(!c->Up&&!c->Rx.Count);reset_media=0;
 fail_filter=1;c->FilterDirty=TRUE;j=polls;RtlUsbService(&n,2);assert(!c->Up&&c->FilterDirty&&polls==j);fail_filter=0;
 RtlUsbService(&n,2);assert(c->Up);fail_media=1;RtlUsbService(&n,2);assert(!c->Up&&!c->Rx.Count);fail_media=0;
 UsbHalt(&a);NdisFreeSpinLock(&a.Lock);
 for(i=0;i<200;i++){
  init(&a,&n);atomic_store(&freed,0);atomic_store(&waiting,0);atomic_store(&poll_entered,0);atomic_store(&release_poll,0);atomic_store(&park_poll,1);
  assert(!pthread_create(&worker,NULL,service,&n));wait_flag(&poll_entered);
  assert(!pthread_create(&stop,NULL,halt,&a));wait_flag(&waiting);assert(!atomic_load(&freed));assert(!Acquire(n.Nic.Mac));
  atomic_store(&release_poll,1);pthread_join(worker,NULL);pthread_join(stop,NULL);assert(atomic_load(&freed)==1&&!a.UsbContext);NdisFreeSpinLock(&a.Lock);
 }
#if defined(RTL_NDIS5)
 atomic_store(&park_poll,0);init(&a,&n);a.NativeUsbRequested=TRUE;c=a.UsbContext;
 RtlUsbService(&n,1);assert(native_carriers&&c->Up&&c->Rx.Count==16);
 {ULONG epoch=c->Epoch;int oldsent=sent;
  assert(UsbTransmit(&a,frame,64));mac_reset=1;RtlUsbService(&n,1);
  assert(native_bus_resets==1&&recovered==1&&c->Up&&c->Epoch==epoch+1&&!c->Tx.Count&&sent==oldsent);
  assert(UsbTransmit(&a,frame,64));mac_reset=fail_recovery=1;RtlUsbService(&n,1);
  assert(native_bus_resets==2&&recovered==2&&!c->Up&&!c->Tx.Count&&!c->Rx.Count&&sent==oldsent);
  fail_recovery=0;RtlUsbService(&n,1);assert(c->Up);
 }
 native_fault=1;assert(UsbTransmit(&a,frame,64));RtlUsbService(&n,1);
 assert(!c->Up&&!c->Tx.Count&&!c->Rx.Count);native_fault=0;RtlUsbService(&n,1);assert(c->Up);
 RtlUsbResetQueues(&a);assert(native_resets==1&&!c->Up&&!c->Rx.Count);
 reset_media=1;j=native_carriers;RtlUsbService(&n,1);
 assert(native_carriers==(unsigned)j&&native_resets==2&&!c->Up);reset_media=0;
 RtlUsbService(&n,1);assert(c->Up&&native_carriers==(unsigned)j+1);
 UsbHalt(&a);NdisFreeSpinLock(&a.Lock);
#endif
 NdisFreeSpinLock(&adapterLock);puts("PASS: USB miniport queue bounds/copies, filter/media failures, reset/detach stale RX, 200 in-flight Halt schedules");return 0;
}
