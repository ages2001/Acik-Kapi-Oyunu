#ifndef WINWDM_REMOVE_LOCK_H
#define WINWDM_REMOVE_LOCK_H
#include <wdm.h>

/* The DDK's Io*RemoveLock macros call these four size-aware entry points. */
#define IoInitializeRemoveLockEx WkIoInitializeRemoveLockEx
#define IoAcquireRemoveLockEx WkIoAcquireRemoveLockEx
#define IoReleaseRemoveLockEx WkIoReleaseRemoveLockEx
#define IoReleaseRemoveLockAndWaitEx WkIoReleaseRemoveLockAndWaitEx

#ifdef __cplusplus
extern "C" {
#endif
VOID NTAPI IoInitializeRemoveLockEx(PIO_REMOVE_LOCK lock, ULONG poolTag,
    ULONG maxMinutes, ULONG highWatermark, ULONG size);
NTSTATUS NTAPI IoAcquireRemoveLockEx(PIO_REMOVE_LOCK lock, PVOID tag,
    PCSTR file, ULONG line, ULONG size);
VOID NTAPI IoReleaseRemoveLockEx(PIO_REMOVE_LOCK lock, PVOID tag, ULONG size);
VOID NTAPI IoReleaseRemoveLockAndWaitEx(PIO_REMOVE_LOCK lock, PVOID tag, ULONG size);
#ifdef __cplusplus
}
#endif
#endif
