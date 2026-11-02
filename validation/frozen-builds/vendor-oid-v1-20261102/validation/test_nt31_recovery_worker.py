"""Run the NT3.1 production recovery worker with concurrent thread ownership.

Reuse the kernel-object mock from the miniport harness; test the full-MAC
binding completion, reentrant reset and deferred-close paths separately.
"""
from pathlib import Path
import ast,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
tree=ast.parse((root/'validation/test_pci_recovery_worker.py').read_text())
strings={n.targets[0].id:ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and isinstance(n.value,ast.Constant) and isinstance(n.value.value,str)}
mock=strings['mock'].split('VOID NdisMResetComplete(')[0].replace('ZwWaitForSingleObject','NtWaitForSingleObject')
decl=(root/'RLTKLANT/src/pcirecovery.c').read_text().split('VOID STDC KeInitializeEvent',1)[1].split('static VOID CompleteReset',1)[0]
decl='VOID STDC KeInitializeEvent'+decl.replace('ZwWaitForSingleObject','NtWaitForSingleObject')
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
#define DISPATCH_LEVEL 2
#define NDIS_STATUS_SUCCESS 0
#define NDIS_STATUS_FAILURE (-1)
#define NDIS_STATUS_PENDING 259
typedef void VOID,*PVOID,*NDIS_HANDLE;typedef unsigned ULONG;typedef int LONG,BOOLEAN,NTSTATUS,NDIS_STATUS;
typedef unsigned char UCHAR,KIRQL;typedef void*PLARGE_INTEGER;typedef void*PLARGE_INT;
typedef void(*PKSTART_ROUTINE)(void*);
typedef struct {int kind;pthread_mutex_t m;pthread_cond_t c;int signal,type;} Event;
typedef struct {int kind;pthread_t id;unsigned refs;int joined;} Thread;
typedef struct Adapter Adapter,*PN31_ADAPTER;
typedef struct Binding {Adapter*Adapter;void*NdisBinding;unsigned ResetReferences;int Closing;struct Binding*Next;} Binding,*PN31_BINDING;
struct Adapter {pthread_mutex_t TxLock,PollLock,FilterLock;unsigned char FilterIrql;
 Event RecoveryWake[1];void*RecoveryThread,*UsbContext;Binding*ResetBinding,*Bindings;
 int RecoveryStop,Resetting,ResetFailed,Running;unsigned recovered,filtered,completed,close_completed,binding_freed,starts,ends;int last;};
static _Thread_local unsigned irql,locked;
static pthread_mutex_t gate=PTHREAD_MUTEX_INITIALIZER;static pthread_cond_t condition=PTHREAD_COND_INITIALIZER;
static int block_recovery,entered,proceed,recovery_failure,create_failure,reference_failure,reenter;
static unsigned created,joined,released,closed;
static void KeAcquireSpinLock(pthread_mutex_t*m,KIRQL*old){pthread_mutex_lock(m);*old=irql;irql=2;locked++;}
static void KeReleaseSpinLock(pthread_mutex_t*m,KIRQL old){assert(locked);locked--;irql=old;pthread_mutex_unlock(m);}
static void n31_filter_lock(Adapter*a){KeAcquireSpinLock(&a->FilterLock,&a->FilterIrql);}
static void n31_filter_unlock(Adapter*a){KeReleaseSpinLock(&a->FilterLock,a->FilterIrql);}
static void n31_poll_stop(Adapter*a){assert(!locked&&!irql);a->Running=0;}
static int hw_init(Adapter*a){assert(!locked&&!irql&&!a->Running&&a->Resetting);
 pthread_mutex_lock(&gate);a->recovered++;entered=1;pthread_cond_broadcast(&condition);
 while(block_recovery&&!proceed)pthread_cond_wait(&condition,&gate);
 pthread_mutex_unlock(&gate);return !recovery_failure;
}
static void hw_setfilter(Adapter*a){assert(locked==2&&irql==2&&!a->RecoveryStop);a->filtered++;}
static void*cp(void*p){return p;}
static NDIS_STATUS n31_queue_reset(Binding*);
static NDIS_STATUS n31_close(Binding*);
static Binding *status_self,*status_other;
static int close_from_status;
static void N31IndicateResetStatus(void*h,NDIS_STATUS status){Adapter*a=h;assert(irql==2&&!locked);
 if(status==0x40010004){a->starts++;
  if(close_from_status){close_from_status=0;assert(n31_close(status_self)==259);assert(!n31_close(status_other));}
 }else {assert(status==0x40010005);a->ends++;}}
static void N31CompleteReset(NDIS_HANDLE h,NDIS_STATUS status){Adapter*a=h;Binding*b=a->UsbContext;assert(irql==2&&!locked);
 if(reenter){reenter=0;assert(n31_queue_reset(b)==NDIS_STATUS_PENDING);}
 pthread_mutex_lock(&gate);a->completed++;a->last=status;pthread_cond_broadcast(&condition);pthread_mutex_unlock(&gate);
}
static NDIS_STATUS n31_close(Binding*b){Adapter*a=b->Adapter;n31_filter_lock(a);
 if(b->ResetReferences){b->Closing=1;n31_filter_unlock(a);return NDIS_STATUS_PENDING;}
 Binding**link;for(link=&a->Bindings;*link&&*link!=b;link=&(*link)->Next);assert(*link==b);*link=b->Next;
 n31_filter_unlock(a);a->binding_freed++;free(b);return 0;
}
static void NdisCompleteCloseAdapter(void*h,NDIS_STATUS status){Adapter*a=h;assert(!locked&&irql==2&&!status);a->close_completed++;}
'''
# UsbContext is a real production exclusion, so keep the test binding in an
# independent map instead of overloading that field.
prefix=prefix.replace('static void N31CompleteReset(', 'static Binding*current_binding;\nstatic void N31CompleteReset(').replace('Binding*b=a->UsbContext;', 'Binding*b=current_binding;')
tests=r'''
VOID ObDereferenceObject(PVOID p){Thread*t=p;assert(t->kind==2&&t->refs==1&&t->joined);released++;free(t);}
static void wait_completion(Adapter*a,unsigned n){pthread_mutex_lock(&gate);while(a->completed<n)pthread_cond_wait(&condition,&gate);pthread_mutex_unlock(&gate);}
static Binding*initialize(Adapter*a){memset(a,0,sizeof(*a));pthread_mutex_init(&a->TxLock,0);pthread_mutex_init(&a->PollLock,0);pthread_mutex_init(&a->FilterLock,0);a->Running=1;
 Binding*b=calloc(1,sizeof(*b));assert(b);b->Adapter=a;b->NdisBinding=a;current_binding=b;a->Bindings=b;return b;}
static void destroy(Adapter*a,Binding*b){assert(!a->RecoveryThread&&!a->ResetBinding&&!a->Resetting);
 if(!a->binding_freed)free(b);pthread_mutex_destroy(&a->TxLock);pthread_mutex_destroy(&a->PollLock);pthread_mutex_destroy(&a->FilterLock);pthread_mutex_destroy(&a->RecoveryWake->m);pthread_cond_destroy(&a->RecoveryWake->c);}
static void*stopper(void*p){n31_stop_recovery(p);return 0;}
int main(void){unsigned i;Adapter a;Binding*b;pthread_t stop;
 for(i=0;i<100;i++){
  b=initialize(&a);assert(n31_start_recovery(&a));assert(n31_queue_reset(b)==259);wait_completion(&a,1);
  recovery_failure=1;assert(n31_queue_reset(b)==259);wait_completion(&a,2);assert(a.last==-1);
  recovery_failure=0;reenter=1;assert(n31_queue_reset(b)==259);wait_completion(&a,4);
  n31_stop_recovery(&a);assert(a.recovered==4&&a.filtered==3&&!b->ResetReferences&&a.starts==4&&a.ends==4);destroy(&a,b);

  b=initialize(&a);assert(n31_start_recovery(&a));block_recovery=1;entered=proceed=0;
  assert(n31_queue_reset(b)==259);pthread_mutex_lock(&gate);while(!entered)pthread_cond_wait(&condition,&gate);pthread_mutex_unlock(&gate);
  assert(n31_queue_reset(b)==-1);assert(n31_close(b)==259);assert(n31_queue_reset(b)==-1);
  pthread_mutex_lock(&gate);proceed=1;pthread_cond_broadcast(&condition);pthread_mutex_unlock(&gate);
  wait_completion(&a,1);n31_stop_recovery(&a);assert(a.binding_freed==1&&a.close_completed==1);destroy(&a,b);

  b=initialize(&a);assert(n31_start_recovery(&a));entered=proceed=0;
  assert(n31_queue_reset(b)==259);pthread_mutex_lock(&gate);while(!entered)pthread_cond_wait(&condition,&gate);pthread_mutex_unlock(&gate);
  KIRQL old;KeAcquireSpinLock(&a.TxLock,&old);a.RecoveryStop=1;KeReleaseSpinLock(&a.TxLock,old);
  assert(!pthread_create(&stop,0,stopper,&a));pthread_mutex_lock(&gate);proceed=1;pthread_cond_broadcast(&condition);pthread_mutex_unlock(&gate);
  pthread_join(stop,0);assert(a.completed==1&&a.last==-1&&!a.filtered&&!a.Running&&!b->ResetReferences);destroy(&a,b);block_recovery=0;

  /* A status callback closes itself and immediately frees the following
     binding. Iteration must still reach the surviving third protocol, and
     the initiating binding must survive until ResetComplete returns. */
  b=initialize(&a);Binding*other=calloc(1,sizeof(*other)),*survivor=calloc(1,sizeof(*survivor));assert(other&&survivor);
  other->Adapter=survivor->Adapter=&a;other->NdisBinding=survivor->NdisBinding=&a;
  b->Next=other;other->Next=survivor;status_self=b;status_other=other;close_from_status=1;
  assert(n31_start_recovery(&a));assert(n31_queue_reset(b)==259);wait_completion(&a,1);n31_stop_recovery(&a);
  assert(a.completed==1&&!a.last&&a.starts==2&&a.ends==1&&a.binding_freed==2&&a.close_completed==1);
  assert(a.Bindings==survivor&&!survivor->ResetReferences);assert(!n31_close(survivor));destroy(&a,b);
 }
 b=initialize(&a);create_failure=1;assert(!n31_start_recovery(&a));n31_stop_recovery(&a);destroy(&a,b);create_failure=0;
 b=initialize(&a);reference_failure=1;assert(!n31_start_recovery(&a));n31_stop_recovery(&a);destroy(&a,b);
 assert(created==401&&joined==created&&released==created&&closed==created);
 puts("PASS: 700 full-MAC reset completions, 100 reentrant reset pairs, 100 deferred binding closes, 100 stop-during-recovery schedules, 100 status callbacks closing current/next bindings while a third survives; completion at DISPATCH without locks; all 401 worker objects joined/released");
}
'''
with tempfile.TemporaryDirectory(prefix='rltk-nt31-worker-') as td:
    p=Path(td)/'test.c';p.write_text(prefix+decl+(root/'RLTKLANT/src/nt31recovery.h').read_text()+mock+tests);exe=Path(td)/'test'
    subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-omit-frame-pointer','-pthread',str(p),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,timeout=40)
