"""Check production timer/interrupt/DMA ordering around passive PCI recovery."""
from pathlib import Path
import re,subprocess,tempfile
source=(Path(__file__).resolve().parents[1]/'RLTKLANT/src/lifecycle.c').read_text()
def fn(name):
 m=re.search(r'^(?:static )?(?:VOID|BOOLEAN|NDIS_STATUS)(?: STDC)? '+name+r'\([^;{]*\)\s*\{',source,re.M);assert m,name
 end=m.end();depth=1
 while depth:depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[m.start():end]
prefix=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef void VOID,*PVOID;typedef unsigned ULONG;typedef int BOOLEAN,NDIS_STATUS;
#define STDC
#define TRUE 1
#define FALSE 0
#define NULLPTR ((void*)0)
#define NDIS_STATUS_SUCCESS 0
#define NDIS_STATUS_FAILURE (-1)
#define NdisInterruptLevelSensitive 0
#define NDIS_STATUS_MEDIA_CONNECT 1
#define NDIS_STATUS_MEDIA_DISCONNECT 2
typedef struct Adapter Adapter,*PRLTK_ADAPTER;
typedef struct {int(*Init)(Adapter*);void(*Halt)(Adapter*);void(*EnableInts)(Adapter*,int);void(*SetRxMode)(Adapter*);void(*HandleInterrupt)(Adapter*);int(*LinkState)(Adapter*,unsigned*,int*);} Ops;
struct Adapter {Ops*Ops;int TimerSet,LinkTimer,LinkTimerStopped,Lock,Halting,ResetPending,ResetFailed,IntRegistered,Interrupt;
 int UsbContext,MiniportHandle,IrqVector,IrqLevel,TxStalled,LinkUp,FullDuplex;unsigned LinkSpeedMbit;};
static unsigned sequence,waited,halted,started,registered,filtered,masked,rearmed,cleared,linked;
static int cancel_result,init_fail,register_fail,halt_at_register,halt_in_link,callback_outstanding;
static void NdisAcquireSpinLock(int*l){assert(!*l);*l=1;}
static void NdisReleaseSpinLock(int*l){assert(*l);*l=0;}
static void NdisMCancelTimer(int*t,int*c){(void)t;assert(!halted);*c=cancel_result;sequence++;}
static void KeWaitForSingleObject(int*event,int why,int mode,int alert,void*timeout){assert(callback_outstanding&&!why&&!mode&&!alert&&!timeout&&!halted);*event=1;waited=++sequence;callback_outstanding=0;}
static void KeClearEvent(int*e){*e=0;cleared=++sequence;}
static void KeSetEvent(int*e,int increment,int wait){assert(!increment&&!wait);*e=1;}
static void NdisSetTimer(int*t,unsigned ms){(void)t;assert((ms==1000||ms==10));rearmed=++sequence;}
static Adapter*current;
static void NdisMDeregisterInterrupt(int*i){(void)i;assert(masked&&!halted&&!callback_outstanding);sequence++;current->IntRegistered=0;}
static int NdisMRegisterInterrupt(int*i,int handle,int vector,int level,int request,int shared,int mode){(void)i;(void)handle;
 assert(started&&halted&&!current->Lock&&!current->IntRegistered&&vector==17&&level==17&&request&&shared&&!mode);
 registered=++sequence;if(halt_at_register)current->Halting=1;return register_fail?-1:0;}
static void enable(Adapter*a,int on){assert(!on&&!a->Lock);masked=++sequence;}
static void halt(Adapter*a){assert(!a->IntRegistered&&!a->Lock&&!callback_outstanding);halted=++sequence;}
static int init(Adapter*a){assert(halted&&!a->IntRegistered&&!a->Lock);started=++sequence;return !init_fail;}
static void filter(Adapter*a){assert(a->Lock&&a->IntRegistered&&!a->Halting);filtered=++sequence;}
static void interrupt(Adapter*a){(void)a;assert(0);}
static int link(Adapter*a,unsigned*speed,int*duplex){assert(!a->Lock);linked++;*speed=10000;*duplex=1;if(halt_in_link)a->Halting=1;return 1;}
static unsigned RtlCapSpeed(Adapter*a,unsigned speed){(void)a;return speed;}
static void NdisMIndicateStatus(int h,int status,void*p,unsigned n){(void)h;assert(status==1&&!p&&!n);}
static void NdisMIndicateStatusComplete(int h){(void)h;}
'''
tests=r'''
int main(void){Ops ops={init,halt,enable,filter,interrupt,link};Adapter a;unsigned schedule;
 for(schedule=0;schedule<24;schedule++){
  memset(&a,0,sizeof(a));a.Ops=&ops;a.IrqVector=a.IrqLevel=17;a.TimerSet=1;a.IntRegistered=1;a.ResetPending=1;current=&a;
  sequence=waited=halted=started=registered=filtered=masked=rearmed=cleared=linked=0;
  cancel_result=schedule&1;callback_outstanding=!cancel_result;
  init_fail=(schedule/2)%3==1;register_fail=(schedule/2)%3==2;halt_at_register=(schedule/6)%2;a.Halting=schedule>=12;
  int result=RtlRecoverPci(&a);
  assert(!a.TimerSet&&!callback_outstanding&&!a.Lock&&!rearmed);
  assert((result==0)==(!a.Halting&&!init_fail&&!register_fail));
  if(!cancel_result)assert(waited<halted);
  if(a.Halting&&schedule>=12)assert(!started&&!registered);
  if(!result)assert(halted<started&&started<registered&&registered<filtered);
 }
 for(schedule=0;schedule<8;schedule++){
  memset(&a,0,sizeof(a));a.Ops=&ops;a.Halting=schedule&1;a.ResetPending=schedule&2;a.ResetFailed=schedule&4;
  cleared=rearmed=0;a.LinkTimerStopped=1;RtlResumeLinkTimer(&a);
  if(schedule)assert(!a.TimerSet&&!rearmed&&!cleared&&a.LinkTimerStopped);
  else assert(a.TimerSet&&cleared<rearmed&&!a.LinkTimerStopped);
 }
 for(schedule=0;schedule<4;schedule++){
  memset(&a,0,sizeof(a));a.Ops=&ops;a.TimerSet=1;a.ResetPending=schedule==1;a.Halting=schedule==2;halt_in_link=schedule==3;
  linked=rearmed=0;RtlLinkTimer(0,&a,0,0);
  if(schedule)assert(!rearmed&&a.LinkTimerStopped);else assert(rearmed&&linked==1&&!a.LinkTimerStopped);
  if(schedule==1||schedule==2)assert(!linked);
 }
 puts("PASS: 24 passive recovery schedules; timer cancellation/join before interrupt drain and DMA teardown; init/register/halt failures; clear-before-rearm; pending/halting callback rundown");
}
'''
with tempfile.TemporaryDirectory(prefix='rltk-recovery-order-') as td:
 p=Path(td)/'test.c';p.write_text(prefix+'\n'.join(fn(x) for x in ['StopLinkTimer','RtlResumeLinkTimer','RtlStartBackend','RtlRecoverPci','RtlLinkTimer'])+tests)
 exe=Path(td)/'test';subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
