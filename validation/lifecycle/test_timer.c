/* Exercise the production lifecycle.c with a controllable timer dispatcher. */
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#define RLTKLANT_H
#define STDC
#define VOID void
#define TRUE 1
#define FALSE 0
#define NDIS_STATUS_SUCCESS 0
#define NDIS_STATUS_FAILURE 1
#define NDIS_STATUS_MEDIA_CONNECT 1
#define NDIS_STATUS_MEDIA_DISCONNECT 2
#define RTL_SPEED_10000 10000
typedef uint32_t ULONG, *PULONG;
typedef int32_t LONG,NTSTATUS;
typedef void *PVOID,*NDIS_HANDLE,*PLARGE_INTEGER;
typedef int BOOLEAN,*PBOOLEAN,NDIS_STATUS;
typedef struct { pthread_mutex_t lock;pthread_cond_t cv;int signalled; } Event;
typedef struct { atomic_int queued; } Timer;
typedef struct Adapter Adapter,*PRLTK_ADAPTER;
typedef struct { int(*Init)(Adapter*);void(*Halt)(Adapter*);void(*SetRxMode)(Adapter*);void(*EnableInts)(Adapter*,int);void(*HandleInterrupt)(Adapter*);int(*LinkState)(Adapter*,PULONG,PBOOLEAN); } Ops;
struct Adapter { pthread_mutex_t Lock;int Halting,TimerSet,IntRegistered,MapRegs,TxStalled,LinkUp,FullDuplex;Timer LinkTimer;Event LinkTimerStopped;Ops *Ops;void *MiniportHandle,*IoBase,*IoPortVa,*UsbContext;ULONG MemLen,IoPortPhys,IoPortLen,LinkSpeedMbit;int Interrupt; };
typedef Adapter RLTK_ADAPTER;
static atomic_int freed,waiting,entered,release_link,parked,release_dispatch;
static int park_dispatch;
static void NdisAcquireSpinLock(pthread_mutex_t *m){assert(!pthread_mutex_lock(m));}
static void NdisReleaseSpinLock(pthread_mutex_t *m){assert(!pthread_mutex_unlock(m));}
static void NdisFreeSpinLock(pthread_mutex_t *m){assert(!pthread_mutex_destroy(m));}
static void NdisMCancelTimer(Timer *t,int *cancelled){*cancelled=atomic_exchange(&t->queued,0);}
static void NdisSetTimer(Timer *t,int ms){(void)ms;assert(!atomic_exchange(&t->queued,1));}
LONG KeSetEvent(PVOID p,LONG priority,BOOLEAN wait){Event *e=p;(void)priority;(void)wait;pthread_mutex_lock(&e->lock);e->signalled=1;pthread_cond_broadcast(&e->cv);pthread_mutex_unlock(&e->lock);return 0;}
NTSTATUS KeWaitForSingleObject(PVOID p,int a,int b,BOOLEAN c,PLARGE_INTEGER d){Event *e=p;(void)a;(void)b;(void)c;(void)d;atomic_store(&waiting,1);pthread_mutex_lock(&e->lock);while(!e->signalled)pthread_cond_wait(&e->cv,&e->lock);pthread_mutex_unlock(&e->lock);return 0;}
static void RtlReleaseSlot(Adapter*a){(void)a;}
static void RtlFree(void *p,unsigned n){Adapter*a=p;(void)n;pthread_mutex_destroy(&a->LinkTimerStopped.lock);pthread_cond_destroy(&a->LinkTimerStopped.cv);atomic_store(&freed,1);free(p);}
static void NdisMDeregisterInterrupt(void*p){(void)p;}
static void NdisMFreeMapRegisters(void*p){(void)p;}
static void NdisMUnmapIoSpace(void*a,void*b,ULONG c){(void)a;(void)b;(void)c;}
static void NdisMDeregisterIoPortRange(void*a,ULONG b,ULONG c,void*d){(void)a;(void)b;(void)c;(void)d;}
static void NdisMIndicateStatus(void*a,ULONG b,void*c,ULONG d){(void)a;(void)b;(void)c;(void)d;}
static void NdisMIndicateStatusComplete(void*a){(void)a;}
static ULONG RtlCapSpeed(Adapter*a,ULONG b){(void)a;return b>10000?10000:b;}
VOID RtlUsbResetQueues(Adapter*a){(void)a;assert(!"unexpected USB reset in PCI fixture");}
#include "../../RLTKLANT/src/lifecycle.c"
static void hardware_stop(Adapter*a){assert(a->Halting);assert(!atomic_load(&a->LinkTimer.queued));}
static void interrupts(Adapter*a,int on){(void)a;(void)on;}
static int link(Adapter*a,PULONG speed,PBOOLEAN duplex){(void)a;*speed=10000;*duplex=1;atomic_store(&entered,1);while(!atomic_load(&release_link))sched_yield();return 0;}
static Ops ops={.Halt=hardware_stop,.EnableInts=interrupts,.LinkState=link};
static void *callback(void*p){Adapter*a=p;assert(atomic_exchange(&a->LinkTimer.queued,0));if(park_dispatch){atomic_store(&parked,1);while(!atomic_load(&release_dispatch))sched_yield();}RtlLinkTimer(0,a,0,0);return 0;}
static void *halt(void*p){RtlHalt(p);return 0;}
static void wait_flag(atomic_int *p){while(!atomic_load(p))sched_yield();}
static int reset_resources,reset_halts,reset_fail,reset_enabled,reset_filter;
static int reset_init(Adapter*a){(void)a;assert(!reset_resources);reset_resources=3;return !reset_fail;}
static void reset_halt(Adapter*a){(void)a;reset_resources=0;reset_halts++;}
static void reset_ints(Adapter*a,int on){(void)a;reset_enabled=on;}
static void reset_mode(Adapter*a){(void)a;reset_filter++;}
static void check_failed_start(void){
 Ops reset_ops={.Init=reset_init,.Halt=reset_halt,.EnableInts=reset_ints,.SetRxMode=reset_mode};
 Adapter a={0};int addressing=0;a.Ops=&reset_ops;reset_fail=1;
 assert(!RtlStartBackend(&a)&&!reset_resources&&reset_halts==1);
 reset_resources=3;reset_halts=0;reset_enabled=1;
 assert(RtlReset(&addressing,&a)==NDIS_STATUS_FAILURE&&addressing);
 assert(reset_halts==2&&!reset_resources&&!reset_enabled&&!reset_filter);
 reset_fail=0;reset_halts=0;a.TxStalled=1;
 assert(RtlReset(&addressing,&a)==NDIS_STATUS_SUCCESS&&reset_resources==3);
 assert(reset_halts==1&&reset_enabled&&reset_filter==1&&!a.TxStalled);
 puts("PASS: partial backend init/reset failure releases resources and keeps interrupts disabled");
}
int main(void){int i,mode;check_failed_start();for(i=0;i<200;i++)for(mode=0;mode<4;mode++){
 Adapter*a=calloc(1,sizeof(*a));pthread_t cb,stop;
 pthread_mutex_init(&a->Lock,0);pthread_mutex_init(&a->LinkTimerStopped.lock,0);pthread_cond_init(&a->LinkTimerStopped.cv,0);a->Ops=&ops;a->TimerSet=1;atomic_init(&a->LinkTimer.queued,1);
 atomic_store(&freed,0);atomic_store(&waiting,0);atomic_store(&entered,0);atomic_store(&release_link,mode==3);atomic_store(&parked,0);atomic_store(&release_dispatch,0);park_dispatch=mode==2;
 if(mode==0){RtlHalt(a);assert(atomic_load(&freed));continue;}
 assert(!pthread_create(&cb,0,callback,a));
 if(mode==3){pthread_join(cb,0);assert(atomic_load(&a->LinkTimer.queued));RtlHalt(a);assert(atomic_load(&freed));continue;}
 wait_flag(mode==2?&parked:&entered);assert(!pthread_create(&stop,0,halt,a));wait_flag(&waiting);assert(!atomic_load(&freed));atomic_store(&release_link,1);atomic_store(&release_dispatch,1);pthread_join(cb,0);pthread_join(stop,0);assert(atomic_load(&freed));
 }puts("PASS: 800 timer stop schedules: queued, executing, dispatched, and rearmed; production lifecycle.c");return 0;}
