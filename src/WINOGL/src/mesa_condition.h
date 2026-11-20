/* Queue the waiter before releasing the associated mutex so a concurrent
 * signal cannot be lost. */
#ifndef WINDX_MESA_CONDITION_H
#define WINDX_MESA_CONDITION_H

#include <windows.h>

struct windx_waiter {
    struct windx_waiter *previous, *next;
    HANDLE event;
    BOOL notified;
};

typedef struct {
    CRITICAL_SECTION lock;
    struct windx_waiter *first, *last;
} windx_condition;

static inline void windx_condition_init(windx_condition *condition)
{
    InitializeCriticalSection(&condition->lock);
    condition->first = condition->last = NULL;
}

static inline void windx_condition_destroy(windx_condition *condition)
{
    DeleteCriticalSection(&condition->lock);
}

/* The list and the lifetime of its stack-allocated nodes are protected by lock. */
static inline void windx_unlink_waiter(windx_condition *condition, struct windx_waiter *waiter)
{
    if (waiter->previous)
        waiter->previous->next = waiter->next;
    else
        condition->first = waiter->next;
    if (waiter->next)
        waiter->next->previous = waiter->previous;
    else
        condition->last = waiter->previous;
}

static inline BOOL windx_condition_wake(windx_condition *condition, BOOL all)
{
    BOOL success = TRUE;
    struct windx_waiter *waiter, *next;
    EnterCriticalSection(&condition->lock);
    for (waiter = condition->first; waiter; waiter = next) {
        next = waiter->next;
        if (SetEvent(waiter->event)) {
            waiter->notified = TRUE;
            windx_unlink_waiter(condition, waiter);
        } else {
            success = FALSE;
        }
        if (!all)
            break;
    }
    LeaveCriticalSection(&condition->lock);
    return success;
}

static inline BOOL windx_condition_wait(windx_condition *condition, HANDLE mutex,
        DWORD timeout)
{
    DWORD result, error;
    struct windx_waiter waiter;
    waiter.event = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (!waiter.event)
        return FALSE;
    waiter.notified = FALSE;
    waiter.next = NULL;
    EnterCriticalSection(&condition->lock);
    waiter.previous = condition->last;
    if (condition->last)
        condition->last->next = &waiter;
    else
        condition->first = &waiter;
    condition->last = &waiter;
    if (!ReleaseMutex(mutex)) {
        error = GetLastError();
        windx_unlink_waiter(condition, &waiter);
        LeaveCriticalSection(&condition->lock);
        CloseHandle(waiter.event);
        SetLastError(error);
        return FALSE;
    }
    LeaveCriticalSection(&condition->lock);

    result = WaitForSingleObject(waiter.event, timeout);
    error = result == WAIT_FAILED ? GetLastError() : ERROR_TIMEOUT;
    EnterCriticalSection(&condition->lock);
    /* A signal can acquire lock after the wait times out but before we can
     * remove the node. In that case this waiter consumed that signal. */
    if (!waiter.notified)
        windx_unlink_waiter(condition, &waiter);
    LeaveCriticalSection(&condition->lock);
    CloseHandle(waiter.event);
    result = WaitForSingleObject(mutex, INFINITE);
    if (result != WAIT_OBJECT_0 && result != WAIT_ABANDONED)
        return FALSE;
    if (!waiter.notified)
        SetLastError(error);
    return waiter.notified;
}

#endif
