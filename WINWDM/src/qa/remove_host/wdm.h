/* Only the kernel primitives consumed by the remove-lock implementation.
 * The native and VMM objects in the kit use their actual x86 DDK types. */
#ifndef WK_REMOVE_HOST_H
#define WK_REMOVE_HOST_H
#include <stddef.h>
#include <stdint.h>

#define NTAPI
typedef void VOID, *PVOID;
typedef const char *PCSTR;
typedef uint32_t ULONG, KSPIN_LOCK;
typedef uintptr_t ULONG_PTR;
typedef int32_t NTSTATUS;
typedef unsigned char BOOLEAN, KIRQL;
typedef struct _LIST_ENTRY { struct _LIST_ENTRY *Flink, *Blink; } LIST_ENTRY, *PLIST_ENTRY;
typedef struct { struct { int32_t SignalState; LIST_ENTRY WaitListHead; } Header; } KEVENT, *PKEVENT;
typedef struct {
    struct { BOOLEAN Removed; int32_t IoCount; KEVENT RemoveEvent; } Common;
} IO_REMOVE_LOCK, *PIO_REMOVE_LOCK;

#define TRUE 1
#define FALSE 0
#define STATUS_SUCCESS 0
#define STATUS_DELETE_PENDING ((NTSTATUS)0xc0000056)
#define SynchronizationEvent 1
#define IO_NO_INCREMENT 0
#define Executive 0
#define KernelMode 0
void KeAcquireSpinLock(KSPIN_LOCK *, KIRQL *);
void KeReleaseSpinLock(KSPIN_LOCK *, KIRQL);
void KeInitializeEvent(PKEVENT, int, BOOLEAN);
void KeSetEvent(PKEVENT, int, BOOLEAN);
void KeWaitForSingleObject(PKEVENT, int, int, BOOLEAN, void *);

#define IoInitializeRemoveLock(l,t,m,w) IoInitializeRemoveLockEx(l,t,m,w,sizeof(*(l)))
#define IoAcquireRemoveLock(l,t) IoAcquireRemoveLockEx(l,t,__FILE__,__LINE__,sizeof(*(l)))
#define IoReleaseRemoveLock(l,t) IoReleaseRemoveLockEx(l,t,sizeof(*(l)))
#define IoReleaseRemoveLockAndWait(l,t) IoReleaseRemoveLockAndWaitEx(l,t,sizeof(*(l)))
#endif
