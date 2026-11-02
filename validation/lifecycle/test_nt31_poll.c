/* Production NT3.1 poll ownership with concurrent removal and two readers. */
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#define VOID void
#define BOOLEAN int
#define TRUE 1
#define FALSE 0
typedef unsigned char KIRQL;
typedef struct { pthread_mutex_t lock; pthread_cond_t cv; int signalled; } Event;
typedef struct {
 pthread_mutex_t PollLock;
 unsigned PollActive;
 int Running;
 Event PollDrained[1];
} Adapter, *PN31_ADAPTER;
static atomic_int waiting,finished;
static void KeAcquireSpinLock(pthread_mutex_t *lock,KIRQL *old){*old=0;assert(!pthread_mutex_lock(lock));}
static void KeReleaseSpinLock(pthread_mutex_t *lock,KIRQL old){(void)old;assert(!pthread_mutex_unlock(lock));}
static void KeSetEvent(Event *event,int priority,int wait){(void)priority;(void)wait;pthread_mutex_lock(&event->lock);event->signalled=1;pthread_cond_broadcast(&event->cv);pthread_mutex_unlock(&event->lock);}
static void KeWaitForSingleObject(Event *event,int reason,int mode,int alert,void *timeout){(void)reason;(void)mode;(void)alert;(void)timeout;atomic_store(&waiting,1);pthread_mutex_lock(&event->lock);while(!event->signalled)pthread_cond_wait(&event->cv,&event->lock);pthread_mutex_unlock(&event->lock);}
#include "../../RLTKLANT/src/nt31poll.h"
static void *stop(void *p){n31_poll_stop(p);atomic_store(&finished,1);return NULL;}
int main(void){unsigned i,readers;
 for(i=0;i<200;i++)for(readers=0;readers<3;readers++){
  Adapter a={0};pthread_t remover;unsigned r;
  pthread_mutex_init(&a.PollLock,0);pthread_mutex_init(&a.PollDrained->lock,0);pthread_cond_init(&a.PollDrained->cv,0);a.Running=1;
  atomic_store(&waiting,0);atomic_store(&finished,0);
  for(r=0;r<readers;r++)assert(n31_poll_acquire(&a));
  assert(!pthread_create(&remover,0,stop,&a));
  if(readers){while(!atomic_load(&waiting))sched_yield();assert(!atomic_load(&finished));assert(!n31_poll_acquire(&a));
   for(r=0;r<readers;r++){assert(!atomic_load(&finished));n31_poll_release(&a);}
  }
  pthread_join(remover,0);assert(atomic_load(&finished));assert(!n31_poll_acquire(&a));assert(!a.PollActive);
  pthread_mutex_destroy(&a.PollLock);pthread_mutex_destroy(&a.PollDrained->lock);pthread_cond_destroy(&a.PollDrained->cv);
 }
 puts("PASS: NT3.1 production poll rundown, 600 schedules with zero/one/two active readers");return 0;
}
