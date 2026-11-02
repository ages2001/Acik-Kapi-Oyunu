"""Execute the production passive reset worker with real concurrent threads.

Checks pending/reset/stop ownership, completion IRQL, reentrant resubmission,
and thread-handle failure unwind. Windows lifecycle/IRQL are mocked explicitly.
"""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
code=(root/'RLTKLANT/src/pcirecovery.c').read_text().replace('#include "rltklant.h"','')
prefix=r'''
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STDC
#define TRUE 1
#define FALSE 0
#define NDIS_STATUS_SUCCESS 0
#define NDIS_STATUS_FAILURE (-1)
#define NDIS_STATUS_PENDING 259
#define NT_SUCCESS(x) ((x)>=0)
typedef void VOID,*PVOID,*NDIS_HANDLE;typedef unsigned ULONG;typedef int LONG,BOOLEAN,NTSTATUS,NDIS_STATUS;
typedef unsigned char UCHAR;typedef void*PLARGE_INTEGER;
typedef struct {int kind;pthread_mutex_t m;pthread_cond_t c;int signal,type;} Event;
typedef struct {int kind;pthread_t id;unsigned refs;int joined;} Thread;
typedef struct Adapter Adapter,*PRLTK_ADAPTER;
typedef struct {void(*EnableInts)(Adapter*,int);} Ops;
struct Adapter {pthread_mutex_t Lock;Event RecoveryWake;PVOID RecoveryThread;
 int RecoveryStop,ResetPending,ResetFailed,Halting;NDIS_HANDLE MiniportHandle;Ops*Ops;
 unsigned completed,recovered,enabled,resumed;int last;};
static _Thread_local unsigned irql,locked;
static pthread_mutex_t gate=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t condition=PTHREAD_COND_INITIALIZER;
static int block_recovery,entered,proceed,recovery_failure,create_failure,reference_failure,reenter;
static unsigned created,joined,released,closed;
static void NdisAcquireSpinLock(pthread_mutex_t*l){assert(!locked);pthread_mutex_lock(l);locked=1;}
static void NdisReleaseSpinLock(pthread_mutex_t*l){assert(locked);locked=0;pthread_mutex_unlock(l);}
static void ObDereferenceObject(PVOID p){Thread*t=p;assert(t->kind==2&&t->refs==1&&t->joined);t->refs--;released++;free(t);}
static NDIS_STATUS RtlRecoverPci(Adapter*a){
 assert(!irql&&!locked);pthread_mutex_lock(&gate);a->recovered++;entered=1;pthread_cond_broadcast(&condition);
 while(block_recovery&&!proceed)pthread_cond_wait(&condition,&gate);
 pthread_mutex_unlock(&gate);return recovery_failure?NDIS_STATUS_FAILURE:NDIS_STATUS_SUCCESS;
}
static void RtlResumeLinkTimer(Adapter*a){assert(!irql&&!locked);a->resumed++;}
static void enable(Adapter*a,int on){assert(locked&&on&&!a->ResetPending&&!a->ResetFailed&&!a->Halting);a->enabled++;}
NDIS_STATUS RtlQueuePciReset(Adapter*);
'''
mock=r'''
VOID KeInitializeEvent(PVOID p,int type,BOOLEAN signal){Event*e=p;e->kind=1;e->type=type;e->signal=signal;assert(type==1);
 pthread_mutex_init(&e->m,NULL);pthread_cond_init(&e->c,NULL);}
LONG KeSetEvent(PVOID p,LONG increment,BOOLEAN wait){Event*e=p;assert(e->kind==1&&!increment&&!wait);
 pthread_mutex_lock(&e->m);e->signal=1;pthread_cond_signal(&e->c);pthread_mutex_unlock(&e->m);return 0;}
NTSTATUS KeWaitForSingleObject(PVOID p,int reason,int kernel,BOOLEAN alert,PLARGE_INTEGER timeout){
 assert(!irql&&!locked&&!reason&&!kernel&&!alert&&!timeout);
 if(*(int*)p==2){Thread*t=p;assert(!t->joined);pthread_join(t->id,NULL);t->joined=1;joined++;return 0;}
 Event*e=p;assert(e->kind==1);pthread_mutex_lock(&e->m);while(!e->signal)pthread_cond_wait(&e->c,&e->m);
 if(e->type)e->signal=0;pthread_mutex_unlock(&e->m);return 0;
}
typedef struct {void(*fn)(void*);void*arg;} Start;
static void*start(void*p){Start s=*(Start*)p;free(p);s.fn(s.arg);assert(0);return 0;}
NTSTATUS PsCreateSystemThread(PVOID*out,ULONG access,PVOID x,PVOID y,PVOID z,VOID(*fn)(PVOID),PVOID arg){
 assert(!irql&&!locked&&access==0x00100000&&!x&&!y&&!z);if(create_failure)return -1;
 Thread*t=calloc(1,sizeof(*t));Start*s=malloc(sizeof(*s));assert(t&&s);t->kind=2;t->refs=1;s->fn=fn;s->arg=arg;
 assert(!pthread_create(&t->id,NULL,start,s));*out=t;created++;return 0;
}
VOID PsTerminateSystemThread(NTSTATUS status){assert(!status&&!irql&&!locked);pthread_exit(NULL);}
NTSTATUS ObReferenceObjectByHandle(PVOID p,ULONG access,PVOID type,int mode,PVOID*out,PVOID info){Thread*t=p;
 assert(access==0x00100000&&!type&&!mode&&!info&&t->refs==1);if(reference_failure)return -1;t->refs++;*out=t;return 0;}
NTSTATUS ZwWaitForSingleObject(PVOID p,BOOLEAN alert,PLARGE_INTEGER timeout){return KeWaitForSingleObject(p,0,0,alert,timeout);}
NTSTATUS ZwClose(PVOID p){Thread*t=p;assert(t->refs);closed++;if(!--t->refs){assert(t->joined);released++;free(t);}return 0;}
VOID KeRaiseIrql(UCHAR level,UCHAR*old){assert(!locked&&!irql&&level==2);*old=irql;irql=level;}
VOID KeLowerIrql(UCHAR old){assert(!locked&&irql==2&&!old);irql=old;}
VOID NdisMResetComplete(NDIS_HANDLE h,NDIS_STATUS status,BOOLEAN addressing){Adapter*a=h;assert(irql==2&&!locked&&addressing);
 if(reenter){reenter=0;assert(RtlQueuePciReset(a)==NDIS_STATUS_PENDING);}
 pthread_mutex_lock(&gate);a->last=status;a->completed++;pthread_cond_broadcast(&condition);pthread_mutex_unlock(&gate);
}
static void wait_completion(Adapter*a,unsigned target){pthread_mutex_lock(&gate);while(a->completed<target)pthread_cond_wait(&condition,&gate);pthread_mutex_unlock(&gate);}
static void initialize(Adapter*a,Ops*ops){memset(a,0,sizeof(*a));pthread_mutex_init(&a->Lock,NULL);a->MiniportHandle=a;a->Ops=ops;}
static void destroy(Adapter*a){assert(!a->RecoveryThread&&!a->ResetPending);pthread_mutex_destroy(&a->Lock);pthread_mutex_destroy(&a->RecoveryWake.m);pthread_cond_destroy(&a->RecoveryWake.c);}
static void*stop(void*p){Adapter*a=p;NdisAcquireSpinLock(&a->Lock);a->Halting=1;NdisReleaseSpinLock(&a->Lock);RtlStopRecovery(a);return 0;}
int main(void){Ops ops={enable};Adapter a;pthread_t stopper;unsigned i;
 for(i=0;i<200;i++){
  initialize(&a,&ops);assert(RtlStartRecovery(&a));
  assert(RtlQueuePciReset(&a)==NDIS_STATUS_PENDING);wait_completion(&a,1);
  assert(a.last==0);recovery_failure=1;
  assert(RtlQueuePciReset(&a)==NDIS_STATUS_PENDING);wait_completion(&a,2);assert(a.last==-1);
  recovery_failure=0;reenter=1;
  assert(RtlQueuePciReset(&a)==NDIS_STATUS_PENDING);wait_completion(&a,4);
  assert(a.last==0);RtlStopRecovery(&a);assert(a.recovered==4&&a.enabled==3&&a.resumed==3);destroy(&a);

  initialize(&a,&ops);assert(RtlStartRecovery(&a));
  pthread_mutex_lock(&gate);block_recovery=1;entered=proceed=0;pthread_mutex_unlock(&gate);
  assert(RtlQueuePciReset(&a)==NDIS_STATUS_PENDING);
  pthread_mutex_lock(&gate);while(!entered)pthread_cond_wait(&condition,&gate);pthread_mutex_unlock(&gate);
  assert(RtlQueuePciReset(&a)==NDIS_STATUS_FAILURE);
  NdisAcquireSpinLock(&a.Lock);a.Halting=1;NdisReleaseSpinLock(&a.Lock);
  assert(!pthread_create(&stopper,NULL,stop,&a));
  pthread_mutex_lock(&gate);proceed=1;pthread_cond_broadcast(&condition);pthread_mutex_unlock(&gate);
  pthread_join(stopper,NULL);assert(a.completed==1&&a.last==-1&&!a.enabled&&!a.resumed);
  assert(RtlQueuePciReset(&a)==NDIS_STATUS_FAILURE);destroy(&a);block_recovery=0;
 }
 initialize(&a,&ops);create_failure=1;assert(!RtlStartRecovery(&a));RtlStopRecovery(&a);destroy(&a);create_failure=0;
 initialize(&a,&ops);reference_failure=1;assert(!RtlStartRecovery(&a));RtlStopRecovery(&a);destroy(&a);
 assert(created==401&&created==joined&&created==released&&created==closed);
 printf("PASS: 1000 reset completions across 400 worker lifetimes; success/failure, duplicate, reentrant reset, stop during recovery, handle failures; completion at DISPATCH with no lock; all 401 threads joined/released\n");
}
'''
with tempfile.TemporaryDirectory(prefix='rltk-pci-recovery-') as td:
 p=Path(td)/'test.c';p.write_text(prefix+code+mock);exe=Path(td)/'test'
 subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',
                 '-fsanitize=address,undefined','-fno-omit-frame-pointer','-pthread',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,timeout=40)
