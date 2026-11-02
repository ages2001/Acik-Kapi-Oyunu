"""Production NT5 URB completion/cancellation ownership under ASan/UBSan.
The lower-driver fixture exercises inline completion, timeout, stop during
submission and competing submission. It does not certify the Windows USB stack.
"""
from pathlib import Path
import re,subprocess,tempfile
src=(Path(__file__).resolve().parents[1]/'RLTKLANT/src/native_urb.c').read_text()
def fn(name):
 m=re.search(r'(?:static )?(?:NTSTATUS|void)(?: NTAPI)? '+name+r'\([^;{]*\)\s*\{',src);assert m,name
 pos=m.end();depth=1
 while depth:depth+=(src[pos]=='{')-(src[pos]=='}');pos+=1
 return src[m.start():pos]
prefix=r'''
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#define NTAPI
#define TRUE 1
#define FALSE 0
#define NULLPTR ((void*)0)
#define STATUS_SUCCESS 0
#define STATUS_PENDING 0x103
#define STATUS_TIMEOUT 0x102
#define STATUS_UNSUCCESSFUL ((int)0xc0000001)
#define STATUS_INSUFFICIENT_RESOURCES ((int)0xc000009a)
#define STATUS_DELETE_PENDING ((int)0xc0000056)
#define STATUS_NOT_SUPPORTED ((int)0xc00000bb)
#define STATUS_MORE_PROCESSING_REQUIRED ((int)0xc0000016)
#define STATUS_IO_TIMEOUT ((int)0xc00000b5)
#define STATUS_CANCELLED ((int)0xc0000120)
#define NT_SUCCESS(x) ((int)(x)>=0)
#define USBD_SUCCESS(x) (!((x)&0x80000000u))
#define IO_NO_INCREMENT 0
#define Executive 0
#define KernelMode 0
#define NotificationEvent 0
#define IRP_MJ_INTERNAL_DEVICE_CONTROL 15
#define IOCTL_INTERNAL_USB_SUBMIT_URB 0x220003
#define IoGetNextIrpStackLocation(p) (&(p)->Stack)
typedef int NTSTATUS,BOOLEAN,KIRQL;typedef unsigned ULONG;typedef long long LONGLONG;typedef void*PVOID;
typedef struct {LONGLONG QuadPart;} LARGE_INTEGER;
typedef struct {pthread_mutex_t M;pthread_cond_t C;int Signaled;} KEVENT;
typedef pthread_mutex_t KSPIN_LOCK;
typedef struct {int StackSize;} DEVICE_OBJECT,*PDEVICE_OBJECT;
typedef struct {unsigned Status;} HDR;typedef struct {HDR UrbHeader;} URB,*PURB;
typedef struct {int MajorFunction;struct {struct {unsigned IoControlCode;} DeviceIoControl;struct {void*Argument1;} Others;} Parameters;} STACK,*PIO_STACK_LOCATION;
typedef struct IRP {struct {int Status;} IoStatus;STACK Stack;NTSTATUS(*Completion)(PDEVICE_OBJECT,struct IRP*,PVOID);void*Context;int Complete;} IRP,*PIRP;
typedef struct {KEVENT Event;NTSTATUS Status;} RTL_URB_WAIT;
typedef struct {PDEVICE_OBJECT Lower;KSPIN_LOCK Lock;PIRP Active;BOOLEAN Submitted,Stopping,Configured;KEVENT Wake;} RTL_URB_TARGET;
static pthread_mutex_t gate=PTHREAD_MUTEX_INITIALIZER;static pthread_cond_t condition=PTHREAD_COND_INITIALIZER;
static int entered,proceed,mode,allocfail,allocs,frees,cancels,completes;
static void KeInitializeEvent(KEVENT*e,int type,int signal){(void)type;pthread_mutex_init(&e->M,NULL);pthread_cond_init(&e->C,NULL);e->Signaled=signal;}
static void KeSetEvent(KEVENT*e,int increment,int wait){(void)increment;(void)wait;pthread_mutex_lock(&e->M);e->Signaled=1;pthread_cond_broadcast(&e->C);pthread_mutex_unlock(&e->M);}
static NTSTATUS KeWaitForSingleObject(KEVENT*e,int why,int kernel,int alert,LARGE_INTEGER*time){int result=0;struct timespec ts;(void)why;(void)kernel;(void)alert;clock_gettime(CLOCK_REALTIME,&ts);ts.tv_nsec+=1000000;if(ts.tv_nsec>=1000000000){ts.tv_sec++;ts.tv_nsec-=1000000000;}
 pthread_mutex_lock(&e->M);while(!e->Signaled){if(time){if(pthread_cond_timedwait(&e->C,&e->M,&ts)==ETIMEDOUT){result=STATUS_TIMEOUT;break;}}else pthread_cond_wait(&e->C,&e->M);}pthread_mutex_unlock(&e->M);return result;}
static void KeAcquireSpinLock(KSPIN_LOCK*l,KIRQL*old){pthread_mutex_lock(l);*old=0;}
static void KeReleaseSpinLock(KSPIN_LOCK*l,KIRQL old){assert(old==0);pthread_mutex_unlock(l);}
static PIRP IoAllocateIrp(int n,int charge){(void)n;(void)charge;if(allocfail)return NULL;allocs++;return calloc(1,sizeof(IRP));}
static void IoFreeIrp(PIRP p){RTL_URB_WAIT*w=p->Context;if(w){pthread_cond_destroy(&w->Event.C);pthread_mutex_destroy(&w->Event.M);}frees++;free(p);}
#define IoSetCompletionRoutine(p,f,c,a,b,d) ((p)->Completion=(f),(p)->Context=(c))
static int IoCancelIrp(PIRP p){cancels++;if(!p->Complete){p->Complete=1;p->IoStatus.Status=STATUS_CANCELLED;completes++;assert(p->Completion(NULL,p,p->Context)==STATUS_MORE_PROCESSING_REQUIRED);}return 1;}
static NTSTATUS IoCallDriver(PDEVICE_OBJECT d,PIRP p){(void)d;assert(p->Stack.MajorFunction==15&&p->Stack.Parameters.DeviceIoControl.IoControlCode==0x220003);
 if(mode==0){p->Complete=1;p->IoStatus.Status=0;completes++;assert(p->Completion(d,p,p->Context)==STATUS_MORE_PROCESSING_REQUIRED);return 0;}
 if(mode>=2){pthread_mutex_lock(&gate);entered=1;pthread_cond_signal(&condition);while(!proceed)pthread_cond_wait(&condition,&gate);pthread_mutex_unlock(&gate);}return STATUS_PENDING;}
'''
main=r'''
static RTL_URB_TARGET target;static URB urb;static NTSTATUS result;static ULONG request_timeout=20;
static void*run(void*p){(void)p;result=Submit(&target,&urb,request_timeout,FALSE);return NULL;}
int main(void){DEVICE_OBJECT lower={1};pthread_t thread;unsigned i;
 target.Lower=&lower;pthread_mutex_init(&target.Lock,NULL);KeInitializeEvent(&target.Wake,0,0);
 for(i=0;i<200;i++){
  target.Stopping=0;mode=0;urb.UrbHeader.Status=0;assert(Submit(&target,&urb,20,FALSE)==0&&!target.Active);
  urb.UrbHeader.Status=0x80000000u;assert(Submit(&target,&urb,20,FALSE)==STATUS_UNSUCCESSFUL);urb.UrbHeader.Status=0;
  mode=1;assert(Submit(&target,&urb,20,FALSE)==STATUS_IO_TIMEOUT&&!target.Active);
  mode=2+(i%2);entered=proceed=0;assert(!pthread_create(&thread,NULL,run,NULL));
  pthread_mutex_lock(&gate);while(!entered)pthread_cond_wait(&condition,&gate);pthread_mutex_unlock(&gate);
  if(mode==3)assert(Submit(&target,&urb,20,FALSE)==STATUS_DELETE_PENDING);
  RtlUrbCancel(&target);assert(target.Stopping&&!target.Submitted);
  pthread_mutex_lock(&gate);proceed=1;pthread_cond_signal(&condition);pthread_mutex_unlock(&gate);
  pthread_join(thread,NULL);assert(result==STATUS_CANCELLED&&!target.Active);
  assert(Submit(&target,&urb,20,FALSE)==STATUS_DELETE_PENDING);
 }
 allocfail=1;assert(Submit(&target,&urb,20,FALSE)==STATUS_INSUFFICIENT_RESOURCES);
 assert(allocs==frees&&completes==800&&cancels==400);

 /* RX remains pending indefinitely while an independent control/TX lane
    completes; only explicit teardown may cancel the receive. */
 {RTL_URB_TARGET control={0};control.Lower=&lower;pthread_mutex_init(&control.Lock,NULL);
 allocfail=0;request_timeout=0;
 for(i=0;i<100;i++){
  KIRQL old;int submitted;
  target.Stopping=0;mode=1;assert(!pthread_create(&thread,NULL,run,NULL));
  do{KeAcquireSpinLock(&target.Lock,&old);submitted=target.Submitted;KeReleaseSpinLock(&target.Lock,old);}while(!submitted);
  mode=0;assert(Submit(&control,&urb,5000,FALSE)==0);
  KeAcquireSpinLock(&target.Lock,&old);assert(target.Active&&!target.Active->Complete);KeReleaseSpinLock(&target.Lock,old);
  RtlUrbCancel(&target);pthread_join(thread,NULL);assert(result==STATUS_CANCELLED&&!target.Active);
 }
 pthread_mutex_destroy(&control.Lock);assert(allocs==frees&&completes==1000&&cancels==500);}
 pthread_cond_destroy(&target.Wake.C);pthread_mutex_destroy(&target.Wake.M);pthread_mutex_destroy(&target.Lock);
 puts("PASS: native URB 200 inline/error/timeout/stop schedules plus 100 independent indefinite RX/control lanes, submission barrier, competing request rejection and complete-before-free ownership");return 0;}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';exe=Path(d)/'test';p.write_text(prefix+fn('Completed')+fn('Submit')+fn('RtlUrbCancel')+main)
 subprocess.run(['gcc','-std=c99','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-pthread',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
