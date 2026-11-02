"""Production NT3.1 global timer drain: pending, cancelled, idle and live list."""
from pathlib import Path
import re,subprocess,tempfile
src=Path(__file__).resolve().parents[1]/'RLTKLANT/src'
code=(src/'drvnt31.c').read_text()
def fn(name):
    m=re.search(r'static \w+(?: STDC)? '+name+r'\([^;{]*\)\s*\{',code);assert m
    end=m.end();depth=1
    while depth:depth+=(code[end]=='{')-(code[end]=='}');end+=1
    return code[m.start():end]
prefix=r'''
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#define VOID void
#define BOOLEAN int
#define TRUE 1
#define FALSE 0
#define STDC
#define N31_MAX 8
typedef unsigned UINT;typedef unsigned char KIRQL;typedef void*PVOID;typedef void*NDIS_HANDLE;
typedef struct {pthread_mutex_t Lock;pthread_cond_t Cv;int Signalled;} Event;
typedef struct {void*FilterDb;} Adapter,*PN31_ADAPTER;
typedef struct {pthread_mutex_t Lock;UINT Timer;int Armed,Stopping;Event Drained[1];} N31_POLL_SET;
static N31_POLL_SET*g_poll,*expected_poll;static PN31_ADAPTER*g_ap;static UINT*g_apN;
static atomic_int waiting,freed,cancel_calls;static int cancel_result;
static void KeAcquireSpinLock(pthread_mutex_t*p,KIRQL*old){*old=0;assert(!pthread_mutex_lock(p));}
static void KeReleaseSpinLock(pthread_mutex_t*p,KIRQL old){assert(!old);assert(!pthread_mutex_unlock(p));}
static void KeSetEvent(Event*e,int priority,int wait){(void)priority;(void)wait;
 pthread_mutex_lock(&e->Lock);e->Signalled=1;pthread_cond_broadcast(&e->Cv);pthread_mutex_unlock(&e->Lock);
}
static void KeWaitForSingleObject(Event*e,int reason,int mode,int alert,void*timeout){
 (void)reason;(void)mode;(void)alert;(void)timeout;atomic_store(&waiting,1);
 pthread_mutex_lock(&e->Lock);while(!e->Signalled)pthread_cond_wait(&e->Cv,&e->Lock);pthread_mutex_unlock(&e->Lock);
}
static void n31_image_cancel(void){}
static int KeCancelTimer(UINT*t){assert(t==&g_poll->Timer&&g_poll->Stopping);atomic_fetch_add(&cancel_calls,1);return cancel_result;}
static void n31_image_get(void){}
static void NdisSetTimer(UINT*t,UINT ms){(void)t;(void)ms;assert(0);}
static int n31_poll_acquire(PN31_ADAPTER a){(void)a;assert(0);return 0;}
static void n31_poll_release(PN31_ADAPTER a){(void)a;assert(0);}
static void hw_poll(PN31_ADAPTER a){(void)a;assert(0);}
static void EthFilterIndicateReceiveComplete(void*p){(void)p;assert(0);}
static void MmFreeContiguousMemory(void*p){assert(p);
 if(p==expected_poll){assert(!g_poll);pthread_mutex_destroy(&expected_poll->Lock);
  pthread_mutex_destroy(&expected_poll->Drained->Lock);pthread_cond_destroy(&expected_poll->Drained->Cv);}
 free(p);atomic_fetch_add(&freed,1);
}
static void setup(void){g_poll=expected_poll=calloc(1,sizeof(*g_poll));g_ap=calloc(N31_MAX,sizeof(*g_ap));g_apN=calloc(1,sizeof(*g_apN));assert(g_poll&&g_ap&&g_apN);
 pthread_mutex_init(&g_poll->Lock,0);pthread_mutex_init(&g_poll->Drained->Lock,0);pthread_cond_init(&g_poll->Drained->Cv,0);
 atomic_store(&waiting,0);atomic_store(&freed,0);atomic_store(&cancel_calls,0);
}
'''
main=r'''
static void*unload(void*p){(void)p;n31_unload(0);return 0;}
int main(void){unsigned i,mode;
 for(i=0;i<200;i++)for(mode=0;mode<3;mode++){
  pthread_t thread;setup();g_poll->Armed=mode!=0;cancel_result=mode==1;
  if(mode==2){assert(!pthread_create(&thread,0,unload,0));while(!atomic_load(&waiting))sched_yield();
   assert(!atomic_load(&freed));n31_dpc(0,0,0,0);pthread_join(thread,0);
  }else n31_unload(0);
  assert(!g_poll&&!g_ap&&!g_apN&&atomic_load(&freed)==3&&atomic_load(&cancel_calls)==(mode!=0));
 }
 setup();*g_apN=1;n31_unload(0);assert(g_poll&&!g_poll->Stopping&&!atomic_load(&freed)&&!atomic_load(&cancel_calls));
 *g_apN=0;n31_unload(0);assert(atomic_load(&freed)==3);n31_unload(0);
 puts("PASS: 600 production MAC unload timer schedules; pending DPC signals and unlocks before global storage free; live-list guard and repeated unload");return 0;
}
'''
with tempfile.TemporaryDirectory() as td:
    p=Path(td)/'test.c';exe=Path(td)/'test'
    p.write_text(prefix+fn('n31_dpc')+fn('n31_unload')+main)
    subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-pthread',str(p),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
