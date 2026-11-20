#include "resident.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { pageSize = 4096, handles = 64 };
static void *systemPages[handles];
static pthread_mutex_t spin = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t provider = PTHREAD_MUTEX_INITIALIZER;
static _Thread_local int inInterrupt, masked;
static atomic_uint created, destroyed, deny;

KIRQL WkMaskInterrupts(void)
{
    assert(!masked);
    assert(!pthread_mutex_lock(&spin));
    masked = 1;
    return inInterrupt ? 2 : 0;
}
void WkRestoreInterrupts(KIRQL old)
{
    assert(masked && old == (inInterrupt ? 2 : 0));
    masked = 0;
    assert(!pthread_mutex_unlock(&spin));
}
void *WkGetSystemPages(ULONG pages, PULONG handle)
{
    unsigned i;
    void *ptr = NULL;
    assert(!inInterrupt && !masked);
    assert(pages > 1 && pages < 512);
    if (atomic_load(&deny)) return NULL;
    assert(!pthread_mutex_lock(&provider));
    for (i = 1; i < handles; ++i)
        if (!systemPages[i]) {
            ptr = aligned_alloc(pageSize, (size_t)pages * pageSize);
            assert(ptr);
            memset(ptr, 0, (size_t)pages * pageSize);
            systemPages[i] = ptr;
            *handle = i;
            atomic_fetch_add(&created, 1);
            break;
        }
    assert(!pthread_mutex_unlock(&provider));
    return ptr;
}
void WkReturnSystemPages(ULONG handle)
{
    assert(!inInterrupt && !masked);
    assert(!pthread_mutex_lock(&provider));
    assert(handle && handle < handles && systemPages[handle]);
    free(systemPages[handle]);
    systemPages[handle] = NULL;
    atomic_fetch_add(&destroyed, 1);
    assert(!pthread_mutex_unlock(&provider));
}

static void *stress(void *arg)
{
    unsigned char value = *(unsigned char *)arg;
    unsigned i;
    for (i = 0; i < 200; ++i) {
        size_t length = 17 + ((i * 703 + value * 179) % (pageSize * 4));
        unsigned char *p = WkResidentAllocate(length);
        assert(p && !((uintptr_t)p % pageSize));
        memset(p, value, length);
        assert(p[0] == value && p[length-1] == value);
        WkResidentFree(p);
    }
    return NULL;
}

int main(void)
{
    unsigned i;
    pthread_t threads[8];
    unsigned char pattern[8], *slots[16], *large, *pair;
    atomic_store(&deny, 1);
    assert(!WkStartResidentPool());
    assert(!WkResidentAllocate(1));
    atomic_store(&deny, 0);
    assert(WkStartResidentPool());
    assert(!WkResidentAllocate(0) && !WkResidentAllocate(0xfffff000UL));
    for (i = 0; i < 16; ++i) {
        slots[i] = WkResidentAllocate(7);
        assert(slots[i]);
        slots[i][0] = (unsigned char)i;
    }
    inInterrupt = 1;
    assert(!WkResidentAllocate(1));
    WkResidentFree(slots[1]+1);
    assert(!WkResidentAllocate(1));
    WkResidentFree(slots[1]);
    WkResidentFree(slots[3]);
    assert(!WkResidentAllocate(pageSize+1));
    WkResidentFree(slots[2]);
    pair = WkResidentAllocate(2*pageSize);
    assert(pair == slots[1]);
    pair[2*pageSize-1] = 42;
    WkResidentFree(pair+pageSize);
    assert(!WkResidentAllocate(2*pageSize));
    assert(slots[4][0] == 4 && pair[2*pageSize-1] == 42);
    WkResidentFree(pair);
    for (i = 0; i < 16; ++i)
        if (i < 1 || i > 3) WkResidentFree(slots[i]);
    inInterrupt = 0;
    atomic_store(&deny, 1);
    assert(!WkResidentAllocate(21*pageSize));
    atomic_store(&deny, 0);
    large = WkResidentAllocate(21*pageSize);
    assert(large);
    memset(large, 0xb6, 21*pageSize);
    inInterrupt = 1;
    WkResidentFree(large);
    inInterrupt = 0;
    assert(!atomic_load(&destroyed));
    large = WkResidentAllocate(21*pageSize);
    assert(large);
    WkResidentFree(large);
    assert(atomic_load(&destroyed) == 1);
    for (i = 0; i < 8; ++i) {
        pattern[i] = (unsigned char)(i+1);
        assert(!pthread_create(&threads[i], NULL, stress, &pattern[i]));
    }
    for (i = 0; i < 8; ++i) assert(!pthread_join(threads[i], NULL));
    WkStopResidentPool();
    assert(atomic_load(&created) == atomic_load(&destroyed));
    assert(!WkResidentAllocate(1));
    puts("VMM resident pages: 1600 concurrent allocations, interrupt exhaustion, fragmentation, low memory and full reclamation passed.");
    return 0;
}
