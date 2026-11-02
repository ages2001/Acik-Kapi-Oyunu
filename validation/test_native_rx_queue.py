"""Production native receive queue: bounds, epochs and concurrent ownership."""
from pathlib import Path
import re,subprocess,tempfile
src=(Path(__file__).resolve().parents[1]/'RLTKLANT/src/native_urb.c').read_text()
backend=(Path(__file__).resolve().parents[1]/'RLTKLANT/src/backend_usb.c').read_text()
reset=backend[backend.index('VOID RtlUsbResetQueues('):]
def fn(name):
 m=re.search(r'(?:static )?(?:NTSTATUS|void|int) '+name+r'\([^;{]*\)\s*\{',src);assert m,name
 p=m.end();depth=1
 while depth:depth+=(src[p]=='{')-(src[p]=='}');p+=1
 return src[m.start():p]
prefix=r'''
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#define RTL_NDIS5 1
#define FALSE 0
#define TRUE 1
#define VOID void
#define RTL_RX_DEPTH 16
#define RTL_RX_BYTES 2048
#define STATUS_DEVICE_NOT_READY ((int)0xc00000a3)
#define STATUS_BUFFER_TOO_SMALL ((int)0xc0000023)
#define RtlCopyMemory memcpy
typedef uint32_t ULONG,*PULONG;typedef unsigned char UCHAR;typedef int NTSTATUS,KIRQL;
typedef struct {pthread_mutex_t Lock;unsigned Stopping,RxAccept,RxFault,Reconfigure;ULONG RxHead,RxCount,RxEpoch,RxLength[RTL_RX_DEPTH];UCHAR*RxBuffers;} RTL_URB_TARGET;
static void KeAcquireSpinLock(pthread_mutex_t*l,int*old){pthread_mutex_lock(l);*old=0;}
static void KeReleaseSpinLock(pthread_mutex_t*l,int old){assert(!old);pthread_mutex_unlock(l);}
'''
integration=r'''
typedef struct {unsigned Count;} RTL_USB_QUEUE;
typedef struct {pthread_mutex_t Lock;ULONG Epoch;int Up,FilterDirty;RTL_USB_QUEUE Tx,Rx;} RTL_USB_CONTEXT;
typedef struct {RTL_USB_CONTEXT*UsbContext;RTL_URB_TARGET*Native;int NativeUsbRequested;} RLTK_ADAPTER,*PRLTK_ADAPTER;
static void NdisAcquireSpinLock(pthread_mutex_t*l){pthread_mutex_lock(l);}
static void NdisReleaseSpinLock(pthread_mutex_t*l){pthread_mutex_unlock(l);}
static void RtlUsbQueueClear(RTL_USB_QUEUE*q){q->Count=0;}
void RtlNativeResetRx(PRLTK_ADAPTER a){assert(pthread_mutex_trylock(&a->UsbContext->Lock)==EBUSY);RtlUrbResetRx(a->Native);}
'''
main=r'''
static RTL_URB_TARGET target;static unsigned produced,consumed,finished;
static void fill(UCHAR*b,unsigned sequence){unsigned i;memcpy(b,&sequence,4);for(i=4;i<RTL_RX_BYTES;i++)b[i]=(UCHAR)(sequence+i*31);}
static void*producer(void*unused){unsigned i;UCHAR b[RTL_RX_BYTES];(void)unused;
 for(i=1;i<=20000;i++){ULONG epoch;fill(b,i);pthread_mutex_lock(&target.Lock);epoch=target.RxEpoch;pthread_mutex_unlock(&target.Lock);QueueRx(&target,epoch,b,sizeof(b));if(!(i%17))sched_yield();}
 pthread_mutex_lock(&target.Lock);finished=1;produced=i-1;pthread_mutex_unlock(&target.Lock);return NULL;}
static void*consumer(void*unused){UCHAR b[RTL_RX_BYTES];ULONG done;unsigned previous=0;(void)unused;
 for(;;){unsigned i,n,end;int status=TakeRx(&target,b,sizeof(b),&done);if(!status){assert(done==sizeof(b));memcpy(&n,b,4);assert(n>previous);previous=n;for(i=4;i<sizeof(b);i++)assert(b[i]==(UCHAR)(n+i*31));consumed++;}
 pthread_mutex_lock(&target.Lock);end=finished&&!target.RxCount;pthread_mutex_unlock(&target.Lock);if(end)break;sched_yield();}return NULL;}
int main(void){UCHAR b[RTL_RX_BYTES],out[RTL_RX_BYTES];ULONG done,epoch;unsigned i;pthread_t p,c;
 memset(&target,0,sizeof(target));pthread_mutex_init(&target.Lock,NULL);target.RxBuffers=calloc(RTL_RX_DEPTH,RTL_RX_BYTES);assert(target.RxBuffers);
 fill(b,1);QueueRx(&target,0,b,sizeof(b));assert(!target.RxCount);RtlUrbSetCarrier(&target,1);
 for(i=0;i<RTL_RX_DEPTH+5;i++)QueueRx(&target,0,b,sizeof(b));assert(target.RxCount==RTL_RX_DEPTH);
 assert(TakeRx(&target,out,100,&done)==STATUS_BUFFER_TOO_SMALL&&!done&&target.RxCount==RTL_RX_DEPTH);
 for(i=0;i<RTL_RX_DEPTH;i++){assert(!TakeRx(&target,out,sizeof(out),&done)&&done==sizeof(b)&&!memcmp(b,out,done));}
 assert(TakeRx(&target,out,sizeof(out),&done)==STATUS_DEVICE_NOT_READY&&!done);
 epoch=target.RxEpoch;QueueRx(&target,epoch,b,sizeof(b));RtlUrbSetCarrier(&target,0);assert(!target.RxCount&&target.RxEpoch==epoch+1);RtlUrbSetCarrier(&target,1);QueueRx(&target,epoch,b,sizeof(b));assert(!target.RxCount);
 {RTL_USB_CONTEXT context={0};RLTK_ADAPTER a={&context,&target,1};pthread_mutex_init(&context.Lock,NULL);
 epoch=target.RxEpoch;QueueRx(&target,epoch,b,sizeof(b));context.Tx.Count=context.Rx.Count=4;context.Up=1;
 RtlUsbResetQueues(&a);assert(context.Epoch==1&&!context.Up&&context.FilterDirty&&!context.Tx.Count&&!context.Rx.Count);assert(!target.RxCount&&target.RxEpoch==epoch+1&&!target.RxAccept);
 RtlUsbResetQueues(&a);assert(target.RxEpoch==epoch+2);RtlUrbSetCarrier(&target,1);QueueRx(&target,epoch,b,sizeof(b));assert(!target.RxCount);pthread_mutex_destroy(&context.Lock);}
 epoch=target.RxEpoch;QueueRx(&target,epoch,b,sizeof(b));ReceiveFault(&target,1);
 assert(target.RxFault&&!target.RxCount&&!target.RxAccept&&target.RxEpoch==epoch+1);
 assert(!RtlUrbSetCarrier(&target,1));QueueRx(&target,epoch,b,sizeof(b));assert(!target.RxCount);
 ReceiveFault(&target,0);assert(RtlUrbSetCarrier(&target,1));QueueRx(&target,epoch,b,sizeof(b));assert(!target.RxCount);
 target.Reconfigure=1;assert(!RtlUrbSetCarrier(&target,1));target.Reconfigure=0;assert(RtlUrbSetCarrier(&target,1));
 QueueRx(&target,target.RxEpoch,b,0);QueueRx(&target,target.RxEpoch,b,RTL_RX_BYTES+1);assert(!target.RxCount);
 assert(!pthread_create(&p,NULL,producer,NULL)&&!pthread_create(&c,NULL,consumer,NULL));
 for(i=0;i<1000;i++){RtlUrbSetCarrier(&target,0);RtlUrbSetCarrier(&target,1);sched_yield();}
 pthread_join(p,NULL);pthread_join(c,NULL);assert(produced==20000);
 target.Stopping=1;QueueRx(&target,target.RxEpoch,b,sizeof(b));assert(!target.RxCount);assert(TakeRx(&target,out,sizeof(out),&done)==STATUS_DEVICE_NOT_READY);
 free(target.RxBuffers);pthread_mutex_destroy(&target.Lock);printf("PASS: 20000 concurrent RX buffers, %u copied intact, 1000 carrier epochs, production miniport reset, bounds/full/stop checks\n",consumed);return 0;}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';exe=Path(d)/'test';p.write_text(prefix+fn('TakeRx')+fn('RtlUrbSetCarrier')+fn('ReceiveFault')+fn('RtlUrbResetRx')+fn('QueueRx')+integration+reset+main)
 subprocess.run(['gcc','-std=c99','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-pthread',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
