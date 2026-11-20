#include "wdm_remove_lock.h"
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>

static pthread_mutex_t spin = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t eventMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static atomic_uint waitEntered, beforeWait, pauseWait, failAllocation;
static atomic_uint semCreates, semDestroys, yields;

void KeAcquireSpinLock(KSPIN_LOCK *unused, KIRQL *level)
{
    (void)unused;
    *level = 0;
    assert(!pthread_mutex_lock(&spin));
}

void KeReleaseSpinLock(KSPIN_LOCK *unused, KIRQL level)
{
    (void)unused;
    assert(level == 0);
    assert(!pthread_mutex_unlock(&spin));
}

/* Freeze the remover precisely before it registers for notification. */
static void enterWait(void)
{
    atomic_store(&beforeWait, 1);
    while (atomic_load(&pauseWait)) sched_yield();
}

#ifdef WK_VMM
static pthread_mutex_t irq = PTHREAD_MUTEX_INITIALIZER;
static unsigned tokens;
KIRQL WkMaskInterrupts(void)
{
    assert(!pthread_mutex_lock(&irq));
    return 0;
}
void WkRestoreInterrupts(KIRQL old)
{
    assert(old == 0);
    assert(!pthread_mutex_unlock(&irq));
}
ULONG WkCreateSemaphore(void)
{
    enterWait();
    if (atomic_load(&failAllocation)) return 0;
    atomic_fetch_add(&semCreates, 1);
    assert(tokens == 0);
    return 1;
}
void WkWaitSemaphore(ULONG sem)
{
    assert(sem == 1);
    assert(!pthread_mutex_lock(&eventMutex));
    atomic_store(&waitEntered, 1);
    while (!tokens) assert(!pthread_cond_wait(&changed, &eventMutex));
    --tokens;
    assert(!pthread_mutex_unlock(&eventMutex));
}
void WkSignalSemaphore(ULONG sem)
{
    assert(sem == 1);
    assert(!pthread_mutex_lock(&eventMutex));
    ++tokens;
    assert(!pthread_cond_signal(&changed));
    assert(!pthread_mutex_unlock(&eventMutex));
}
void WkDestroySemaphore(ULONG sem)
{
    assert(sem == 1 && tokens == 0);
    atomic_fetch_add(&semDestroys, 1);
}
void WkYieldVM(void)
{
    atomic_store(&waitEntered, 1);
    atomic_fetch_add(&yields, 1);
    sched_yield();
}
#else
void KeInitializeEvent(PKEVENT event, int type, BOOLEAN set)
{
    assert(type == SynchronizationEvent);
    event->Header.SignalState = set;
}
void KeSetEvent(PKEVENT event, int boost, BOOLEAN wait)
{
    assert(boost == 0 && !wait);
    assert(!pthread_mutex_lock(&eventMutex));
    event->Header.SignalState = 1;
    assert(!pthread_cond_broadcast(&changed));
    assert(!pthread_mutex_unlock(&eventMutex));
}
void KeWaitForSingleObject(PKEVENT event, int reason, int mode, BOOLEAN alert, void *limit)
{
    assert(reason == Executive && mode == KernelMode && !alert && !limit);
    enterWait();
    assert(!pthread_mutex_lock(&eventMutex));
    atomic_store(&waitEntered, 1);
    while (!event->Header.SignalState) assert(!pthread_cond_wait(&changed, &eventMutex));
    event->Header.SignalState = 0;
    assert(!pthread_mutex_unlock(&eventMutex));
}
#endif

enum { workers = 6, rounds = 36 };
typedef struct {
    IO_REMOVE_LOCK lock;
    atomic_uint acquired, release, returned;
} FIXTURE;

static void *work(void *arg)
{
    FIXTURE *f = arg;
    unsigned i;
    for (i = 0; i < 32; ++i) {
        assert(IoAcquireRemoveLock(&f->lock, &i) == STATUS_SUCCESS);
        IoReleaseRemoveLock(&f->lock, &i);
    }
    assert(IoAcquireRemoveLock(&f->lock, &i) == STATUS_SUCCESS);
    atomic_fetch_add(&f->acquired, 1);
    while (!atomic_load(&f->release)) sched_yield();
    IoReleaseRemoveLock(&f->lock, &i);
    return NULL;
}
static void *removeDevice(void *arg)
{
    FIXTURE *f = arg;
    assert(IoAcquireRemoveLock(&f->lock, f) == STATUS_SUCCESS);
    IoReleaseRemoveLockAndWait(&f->lock, f);
    atomic_store(&f->returned, 1);
    return NULL;
}

int main(void)
{
    unsigned round, i;
    FIXTURE idle = {0};
    IoInitializeRemoveLock(&idle.lock, 0, 0, 0);
    removeDevice(&idle);
    assert(atomic_load(&idle.returned) && !atomic_load(&beforeWait));
    assert(IoAcquireRemoveLock(&idle.lock, &idle) == STATUS_DELETE_PENDING);
    for (round = 0; round < rounds; ++round) {
        pthread_t threads[workers], remover;
        FIXTURE f = {0};
        IO_REMOVE_LOCK independent;
        const unsigned presignal = round % 3 == 1;
        atomic_store(&beforeWait, 0);
        atomic_store(&waitEntered, 0);
        atomic_store(&pauseWait, presignal);
        atomic_store(&failAllocation, round % 3 == 2);
        IoInitializeRemoveLock(&f.lock, 0, 0, 0);
        IoInitializeRemoveLock(&independent, 0, 0, 0);
        assert(IoAcquireRemoveLock(&f.lock, &i) == STATUS_SUCCESS);
        for (i = 0; i < workers; ++i) assert(!pthread_create(&threads[i], NULL, work, &f));
        while (atomic_load(&f.acquired) != workers) sched_yield();
        assert(!pthread_create(&remover, NULL, removeDevice, &f));
        while (!(presignal ? atomic_load(&beforeWait) : atomic_load(&waitEntered))) sched_yield();
        assert(IoAcquireRemoveLock(&f.lock, &f) == STATUS_DELETE_PENDING);
        assert(IoAcquireRemoveLock(&independent, &i) == STATUS_SUCCESS);
        IoReleaseRemoveLockAndWait(&independent, &i);
        atomic_store(&f.release, 1);
        for (i = 0; i < workers; ++i) assert(!pthread_join(threads[i], NULL));
        assert(!atomic_load(&f.returned));
        IoReleaseRemoveLock(&f.lock, &i);
        atomic_store(&pauseWait, 0);
        assert(!pthread_join(remover, NULL));
        assert(atomic_load(&f.returned) && f.lock.Common.IoCount == 0);
        assert(IoAcquireRemoveLock(&f.lock, &f) == STATUS_DELETE_PENDING);
    }
    assert(atomic_load(&semCreates) == atomic_load(&semDestroys));
#ifdef WK_VMM
    assert(atomic_load(&semCreates) == 2 * rounds / 3 && atomic_load(&yields));
    puts("VMM remove locks: wait, pre-signal and semaphore allocation failure passed.");
#else
    assert(!atomic_load(&yields));
    puts("NT remove locks: wait, pre-signal and concurrent rundown passed.");
#endif
    printf("%u removal races, %u concurrent worker acquisitions.\n", rounds, rounds*workers*33);
    return 0;
}
