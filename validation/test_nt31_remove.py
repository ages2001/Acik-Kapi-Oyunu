"""Production NT3.1 list/reference/timer removal under concurrent polling."""
from pathlib import Path
import re,subprocess,tempfile
src=Path(__file__).resolve().parents[1]/'RLTKLANT/src'
code=(src/'drvnt31.c').read_text()
def fn(name):
    m=re.search(r'static \w+(?: STDC)? '+name+r'\([^;{]*\)\s*\{',code);assert m,name
    end=m.end();depth=1
    while depth:depth+=(code[end]=='{')-(code[end]=='}');end+=1
    return code[m.start():end]
prefix=r'''
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sched.h>
#define VOID void
#define BOOLEAN int
#define TRUE 1
#define FALSE 0
#define STDC
#define N31_MAX 8
typedef unsigned UINT;typedef unsigned char KIRQL;typedef void*PVOID;typedef void*NDIS_HANDLE;
typedef struct {pthread_mutex_t Lock;pthread_cond_t Cv;int Signalled;} Event;
typedef struct {pthread_mutex_t PollLock;UINT PollActive;int Running;Event PollDrained[1];
 void*UsbContext,*FilterDb,*NdisAdapterHandle,*Dma;UINT Id;
} Adapter,*PN31_ADAPTER;
typedef struct {pthread_mutex_t Lock;UINT Timer;int Armed,Stopping;Event Drained[1];} PollSet;
static PollSet set,*g_poll=&set;static PN31_ADAPTER adapters[N31_MAX],*g_ap=adapters;
static UINT count,*g_apN=&count;
static atomic_int entered,release_poll,waiting,removed,freed,park,polled;
static UINT armed,stops;static int dma_safe=1;static void*retained;
static void KeAcquireSpinLock(pthread_mutex_t*p,KIRQL*old){*old=0;assert(!pthread_mutex_lock(p));}
static void KeReleaseSpinLock(pthread_mutex_t*p,KIRQL old){assert(!old);assert(!pthread_mutex_unlock(p));}
static void KeSetEvent(Event*e,int priority,int wait){(void)priority;(void)wait;
 pthread_mutex_lock(&e->Lock);e->Signalled=1;pthread_cond_broadcast(&e->Cv);pthread_mutex_unlock(&e->Lock);
}
static void KeResetEvent(Event*e){pthread_mutex_lock(&e->Lock);e->Signalled=0;pthread_mutex_unlock(&e->Lock);}
static void KeWaitForSingleObject(Event*e,int reason,int mode,int alert,void*timeout){
 (void)reason;(void)mode;(void)alert;(void)timeout;atomic_store(&waiting,1);
 pthread_mutex_lock(&e->Lock);while(!e->Signalled)pthread_cond_wait(&e->Cv,&e->Lock);pthread_mutex_unlock(&e->Lock);
}
#include "nt31poll.h"
static void n31_image_get(void){}
static void NdisSetTimer(UINT*t,UINT ms){assert(t==&g_poll->Timer&&(ms==3||ms==100));armed++;}
static void hw_poll(PN31_ADAPTER a){assert(a->PollActive);atomic_fetch_add(&polled,1);
 if(!a->Id&&atomic_load(&park)){atomic_store(&entered,1);while(!atomic_load(&release_poll))sched_yield();}
}
static void EthFilterIndicateReceiveComplete(void*p){assert(((PN31_ADAPTER)p)->PollActive);}
static int n31_stop_hw(PN31_ADAPTER a){assert(!a->PollActive&&!a->Running);stops++;return dma_safe;}
static void n31_stop_recovery(PN31_ADAPTER a){(void)a;}
static void N31UsbDestroy(void*p){(void)p;assert(0);}
static void EthDeleteFilter(void*p){assert(!((PN31_ADAPTER)p)->PollActive);}
static void NdisDeregisterAdapter(void*p){assert(!((PN31_ADAPTER)p)->PollActive);}
static void n31_free_dma(PN31_ADAPTER a){assert(dma_safe&&a->Dma);free(a->Dma);a->Dma=NULL;}
static void n31_unmap(PN31_ADAPTER a){assert(!a->PollActive);}
static void MmFreeContiguousMemory(PN31_ADAPTER a){
 assert(!a->Running&&!a->PollActive);if(a->Dma){assert(!dma_safe);retained=a->Dma;}
 pthread_mutex_destroy(&a->PollLock);pthread_mutex_destroy(&a->PollDrained->Lock);
 pthread_cond_destroy(&a->PollDrained->Cv);free(a);atomic_fetch_add(&freed,1);
}
static void dbg(const char*s){(void)s;}
static PN31_ADAPTER make(UINT id){PN31_ADAPTER a=calloc(1,sizeof(*a));assert(a);
 pthread_mutex_init(&a->PollLock,0);pthread_mutex_init(&a->PollDrained->Lock,0);pthread_cond_init(&a->PollDrained->Cv,0);
 a->Id=id;a->Dma=malloc(128);assert(a->Dma);a->FilterDb=a->NdisAdapterHandle=a;return a;
}
'''
main=r'''
static void*dpc(void*p){(void)p;n31_dpc(0,0,0,0);return 0;}
static void*remove_thread(void*p){n31_remove(p);atomic_store(&removed,1);return 0;}
int main(void){unsigned i;pthread_mutex_init(&set.Lock,0);
 for(i=0;i<200;i++){
  PN31_ADAPTER a=make(0),b=make(1),c;pthread_t reader,remover;
  assert(!count&&!set.Armed);armed=stops=0;atomic_store(&freed,0);atomic_store(&polled,0);
  atomic_store(&entered,0);atomic_store(&release_poll,0);atomic_store(&waiting,0);atomic_store(&removed,0);atomic_store(&park,1);
  assert(n31_publish(a)&&n31_publish(b)&&count==2&&armed==1);
  assert(!pthread_create(&reader,0,dpc,0));while(!atomic_load(&entered))sched_yield();
  assert(!pthread_create(&remover,0,remove_thread,a));while(!atomic_load(&waiting))sched_yield();
  assert(!atomic_load(&freed)&&!atomic_load(&removed)&&count==1&&!g_ap[0]&&g_ap[1]==b);
  atomic_store(&release_poll,1);pthread_join(reader,0);pthread_join(remover,0);
  assert(atomic_load(&freed)==1&&atomic_load(&polled)==2&&armed==2&&set.Armed);
  c=make(2);assert(n31_publish(c)&&g_ap[0]==c&&count==2&&armed==2);
  atomic_store(&park,0);n31_dpc(0,0,0,0);assert(atomic_load(&polled)==4);
  n31_remove(b);n31_remove(c);assert(!count&&atomic_load(&freed)==3);
  n31_dpc(0,0,0,0);assert(!set.Armed);c=make(3);assert(n31_publish(c)&&set.Armed);
  n31_remove(c);n31_dpc(0,0,0,0);assert(!set.Armed&&!count&&atomic_load(&freed)==4);
 }
 {PN31_ADAPTER a=make(0);assert(n31_publish(a));dma_safe=0;n31_remove(a);assert(retained&&!count);
  n31_dpc(0,0,0,0);dma_safe=1;free(retained);retained=0;}
 pthread_mutex_destroy(&set.Lock);
 puts("PASS: 200 concurrent first-adapter removals, surviving adapter polling, hole reuse, last removal and timer restart; unacknowledged DMA retained");return 0;
}
'''
with tempfile.TemporaryDirectory() as td:
    p=Path(td)/'test.c';exe=Path(td)/'test'
    p.write_text(prefix+fn('n31_dpc')+fn('n31_publish')+fn('n31_remove')+main)
    subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-pthread','-I',str(src),str(p),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
