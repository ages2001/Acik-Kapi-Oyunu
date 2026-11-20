#define _XOPEN_SOURCE 700
#include "wdm_csq.h"
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>

typedef struct packet {
    IRP irp;
    IO_CSQ_IRP_CONTEXT context;
    struct packet *next, *prev;
    _Atomic unsigned completed, inserted;
    unsigned group;
} PACKET;

enum { COUNT=640, ROUNDS=12 };
static IO_CSQ queue;
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t cancelLock=PTHREAD_MUTEX_INITIALIZER;
static pthread_barrier_t start;
static PACKET records[COUNT], *first, *last;
static _Atomic unsigned finished;
static int reject, cancelDuringInsert;

static void take(PIO_CSQ q, PKIRQL irql)
{ (void)q; *irql=0; pthread_mutex_lock(&lock); }
static void drop(PIO_CSQ q, KIRQL irql)
{ (void)q; (void)irql; pthread_mutex_unlock(&lock); }
void IoReleaseCancelSpinLock(KIRQL irql)
{ (void)irql; pthread_mutex_unlock(&cancelLock); }

static void requestCancel(PIRP irp)
{
    PDRIVER_CANCEL routine;
    pthread_mutex_lock(&cancelLock);
    irp->Cancel=TRUE;
    routine=IoSetCancelRoutine(irp,NULL);
    if (routine) routine(NULL,irp);
    else IoReleaseCancelSpinLock(0);
}

static void append(PIO_CSQ q, PIRP irp)
{
    PACKET *p=(PACKET *)irp;
    (void)q;
    p->prev=last;
    p->next=NULL;
    if (last) last->next=p;
    else first=p;
    last=p;
}

static NTSTATUS appendEx(PIO_CSQ q, PIRP irp, PVOID condition)
{
    if (condition==&reject) return STATUS_INVALID_PARAMETER;
    append(q,irp);
    if (condition==&cancelDuringInsert) requestCancel(irp);
    return STATUS_SUCCESS;
}

static void unlinkIrp(PIO_CSQ q, PIRP irp)
{
    PACKET *p=(PACKET *)irp;
    (void)q;
    if (p->prev) p->prev->next=p->next;
    else first=p->next;
    if (p->next) p->next->prev=p->prev;
    else last=p->prev;
    p->next=p->prev=NULL;
}

static PIRP peek(PIO_CSQ q, PIRP previous, PVOID group)
{
    PACKET *p=previous ? ((PACKET *)previous)->next : first;
    (void)q;
    while (p && group && p->group!=*(unsigned *)group) p=p->next;
    return p ? &p->irp : NULL;
}

static void complete(PIRP irp)
{
    PACKET *p=(PACKET *)irp;
    assert(irp->pending==1);
    assert(atomic_fetch_add(&p->completed,1)==0);
    atomic_fetch_add(&finished,1);
}

static void canceled(PIO_CSQ q, PIRP irp)
{
    (void)q;
    assert(irp->Cancel);
    complete(irp);
}

static void *produce(void *unused)
{
    unsigned i;
    (void)unused;
    pthread_barrier_wait(&start);
    for (i=0;i<COUNT;++i) {
        PACKET *p=&records[i];
        IoCsqInsertIrp(&queue,&p->irp,(i&1) ? &p->context : NULL);
        p->inserted=1;
    }
    return NULL;
}

static void *cancelMany(void *unused)
{
    unsigned i;
    (void)unused;
    pthread_barrier_wait(&start);
    for (i=0;i<COUNT;i+=2) {
        requestCancel(&records[i].irp);
        if ((i&15)==0) sched_yield();
    }
    return NULL;
}

static void *claim(void *hint)
{
    unsigned i;
    PIRP irp;
    pthread_barrier_wait(&start);
    while (finished!=COUNT) {
        irp=IoCsqRemoveNextIrp(&queue,hint);
        if (irp) complete(irp);
        else sched_yield();
        if (hint) continue;
        for (i=1;i<COUNT;i+=34) {
            if (!records[i].inserted) continue;
            irp=IoCsqRemoveIrp(&queue,&records[i].context);
            if (irp) complete(irp);
        }
    }
    return NULL;
}

int main(void)
{
    unsigned i, n, odd=1;
    PACKET single={0};
    pthread_t threads[4];
    assert(NT_SUCCESS(IoCsqInitializeEx(&queue,appendEx,unlinkIrp,peek,take,drop,canceled)));
    assert(IoCsqInsertIrpEx(&queue,&single.irp,&single.context,&reject)==STATUS_INVALID_PARAMETER);
    assert(!single.irp.pending && !single.context.Irp && !first);
    assert(NT_SUCCESS(IoCsqInsertIrpEx(&queue,&single.irp,&single.context,&cancelDuringInsert)));
    assert(single.completed==1 && !single.context.Irp && !first);
    for (n=0;n<ROUNDS;++n) {
        memset(records,0,sizeof(records));
        first=last=NULL;
        finished=0;
        for (i=0;i<COUNT;++i) records[i].group=i%3;
        if (n&1)
            assert(NT_SUCCESS(IoCsqInitialize(&queue,append,unlinkIrp,peek,take,drop,canceled)));
        else
            assert(NT_SUCCESS(IoCsqInitializeEx(&queue,appendEx,unlinkIrp,peek,take,drop,canceled)));
        assert(!pthread_barrier_init(&start,NULL,4));
        assert(!pthread_create(&threads[0],NULL,produce,NULL));
        assert(!pthread_create(&threads[1],NULL,cancelMany,NULL));
        assert(!pthread_create(&threads[2],NULL,claim,NULL));
        assert(!pthread_create(&threads[3],NULL,claim,&odd));
        for (i=0;i<4;++i) assert(!pthread_join(threads[i],NULL));
        pthread_barrier_destroy(&start);
        assert(!first && !last);
        for (i=0;i<COUNT;++i)
            assert(records[i].completed==1 && !records[i].context.Irp && !records[i].irp.routine);
    }
    printf("PASS: %u concurrent IRPs each completed once; rejection, retry, filtering and cancellation passed.\n",
           ROUNDS*COUNT);
    return 0;
}
