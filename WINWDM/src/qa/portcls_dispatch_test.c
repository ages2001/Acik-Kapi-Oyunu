#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct list { struct list *Flink; } LIST_ENTRY;
typedef enum { InterruptSyncModeNormal = 1, InterruptSyncModeAll, InterruptSyncModeRepeat } INTERRUPTSYNCMODE;
typedef struct { char order[32]; unsigned used; } IInterruptSync;
typedef void *PVOID;
typedef int BOOLEAN, (*PINTERRUPTSYNCROUTINE)(IInterruptSync *, PVOID);
#define FALSE 0
#define TRUE 1
#define STATUS_SUCCESS 0
#define CONTAINING_RECORD(ptr,type,field) ((type *)((char *)(ptr) - offsetof(type,field)))
#include "../kernel/portcls_dispatch.h"

typedef struct { char id; unsigned claims; } MEMBER;
static int callback(IInterruptSync *sync, PVOID context)
{
    MEMBER *member = context;
    assert(sync->used + 1 < sizeof(sync->order));
    sync->order[sync->used++] = member->id;
    if (!member->claims) return 1; /* A positive informational status is not an interrupt claim. */
    --member->claims;
    return STATUS_SUCCESS;
}

static void check(INTERRUPTSYNCMODE mode, unsigned claims, const char *order)
{
    IInterruptSync mock = {{0}, 0};
    LIST_ENTRY head;
    MEMBER members[] = {{'1', 0}, {'2', claims}, {'3', 0}};
    SERVICE_ROUTINE services[3];
    unsigned i;
    head.Flink = &head;
    assert(!serviceInterrupt(&mock, &head, mode));
    for (i = 0; i < 3; ++i) {
        services[i].Link.Flink = i < 2 ? &services[i + 1].Link : &head;
        services[i].Callback = callback;
        services[i].Context = &members[i];
    }
    head.Flink = &services[0].Link;
    assert(serviceInterrupt(&mock, &head, mode) == (claims != 0));
    assert(!strcmp(mock.order, order));
}

int main(void)
{
    check(InterruptSyncModeNormal, 1, "12");
    check(InterruptSyncModeAll, 1, "123");
    check(InterruptSyncModeRepeat, 2, "123123123");
    check(InterruptSyncModeNormal, 0, "123");
    check(InterruptSyncModeAll, 0, "123");
    check(InterruptSyncModeRepeat, 0, "123");
    puts("PortCls ISR dispatch: first claim, all members, repeat until idle and unclaimed lines passed.");
    return 0;
}
