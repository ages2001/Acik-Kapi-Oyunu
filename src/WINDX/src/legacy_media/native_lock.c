#include <stdarg.h>
#include "windef.h"
#include "winbase.h"
#include "native_lock.h"

void wx_resource_init(RTL_RWLOCK *lock)
{
    ZeroMemory(lock, sizeof(*lock));
    InitializeCriticalSection(&lock->rtlCS);
    lock->hSharedReleaseSemaphore = CreateSemaphoreA(NULL, 0, 0x7fffffff, NULL);
    lock->hExclusiveReleaseSemaphore = CreateSemaphoreA(NULL, 0, 0x7fffffff, NULL);
    if (!lock->hSharedReleaseSemaphore || !lock->hExclusiveReleaseSemaphore) {
        if (lock->hSharedReleaseSemaphore) CloseHandle(lock->hSharedReleaseSemaphore);
        if (lock->hExclusiveReleaseSemaphore) CloseHandle(lock->hExclusiveReleaseSemaphore);
        lock->hSharedReleaseSemaphore = lock->hExclusiveReleaseSemaphore = NULL;
    }
}

void wx_resource_delete(RTL_RWLOCK *lock)
{
    if (lock->hSharedReleaseSemaphore) {
        CloseHandle(lock->hSharedReleaseSemaphore);
        CloseHandle(lock->hExclusiveReleaseSemaphore);
    }
    DeleteCriticalSection(&lock->rtlCS);
}

static BYTE acquire(RTL_RWLOCK *lock, BYTE wait, BOOL write)
{
    HANDLE self = (HANDLE)(ULONG_PTR)GetCurrentThreadId();
    if (!lock->hSharedReleaseSemaphore) {
        if (!wait) {
            BOOL (WINAPI *attempt)(CRITICAL_SECTION *) = (void *)GetProcAddress(
                GetModuleHandleA("kernel32.dll"), "TryEnterCriticalSection");
            return attempt && attempt(&lock->rtlCS);
        }
        EnterCriticalSection(&lock->rtlCS);
        return TRUE;
    }
    EnterCriticalSection(&lock->rtlCS);
    if (lock->iNumberActive < 0 && lock->hOwningThreadId == self) {
        --lock->iNumberActive;
    } else if (write && !lock->iNumberActive) {
        lock->iNumberActive = -1;
        lock->hOwningThreadId = self;
    } else if (!write && lock->iNumberActive >= 0) {
        ++lock->iNumberActive;
    } else {
        if (!wait) { LeaveCriticalSection(&lock->rtlCS); return FALSE; }
        HANDLE sem = write ? lock->hExclusiveReleaseSemaphore : lock->hSharedReleaseSemaphore;
        if (write) ++lock->uExclusiveWaiters; else ++lock->uSharedWaiters;
        LeaveCriticalSection(&lock->rtlCS);
        /* Reservations are installed before releasing the semaphore. Other
         * entrants cannot take a reservation intended for this waiter. */
        if (WaitForSingleObject(sem, INFINITE) != WAIT_OBJECT_0) return FALSE;
        if (!write) return TRUE;
        EnterCriticalSection(&lock->rtlCS);
        lock->hOwningThreadId = self;
    }
    LeaveCriticalSection(&lock->rtlCS);
    return TRUE;
}

BYTE wx_resource_write(RTL_RWLOCK *lock, BYTE wait) { return acquire(lock, wait, TRUE); }
BYTE wx_resource_read(RTL_RWLOCK *lock, BYTE wait) { return acquire(lock, wait, FALSE); }

void wx_resource_release(RTL_RWLOCK *lock)
{
    if (!lock->hSharedReleaseSemaphore) { LeaveCriticalSection(&lock->rtlCS); return; }
    EnterCriticalSection(&lock->rtlCS);
    if (lock->iNumberActive < 0) ++lock->iNumberActive;
    else if (lock->iNumberActive > 0) --lock->iNumberActive;
    if (!lock->iNumberActive) {
        lock->hOwningThreadId = NULL;
        if (lock->uExclusiveWaiters) {
            --lock->uExclusiveWaiters;
            lock->iNumberActive = -1;
            ReleaseSemaphore(lock->hExclusiveReleaseSemaphore, 1, NULL);
        } else if (lock->uSharedWaiters) {
            DWORD readers = lock->uSharedWaiters;
            lock->uSharedWaiters = 0;
            lock->iNumberActive = readers;
            ReleaseSemaphore(lock->hSharedReleaseSemaphore, readers, NULL);
        }
    }
    LeaveCriticalSection(&lock->rtlCS);
}
