#include <ntifs.h>
#include "queue.h"

static KEVENT available;
static PVOID worker;
static volatile LONG stopping;

static VOID NTAPI threadMain(PVOID unused)
{
    (void)unused;
    for (;;) {
        KeWaitForSingleObject(&available, Executive, KernelMode, FALSE, NULL);
        if (__atomic_load_n(&stopping, __ATOMIC_ACQUIRE))
            break;
        WkDrainRequests(STATUS_SUCCESS);
    }
    PsTerminateSystemThread(STATUS_SUCCESS);
}

NTSTATUS WkStartWorker(void)
{
    HANDLE handle;
    NTSTATUS status;
    stopping = 0;
    KeInitializeEvent(&available, SynchronizationEvent, FALSE);
    /* DriverEntry runs in the system process. Keep an object reference so
     * unload can wait for thread exit regardless of its process context. */
    status = PsCreateSystemThread(&handle, SYNCHRONIZE, NULL, NULL, NULL, threadMain, NULL);
    if (!NT_SUCCESS(status))
        return status;
    status = ObReferenceObjectByHandle(handle, SYNCHRONIZE, NULL, KernelMode, &worker, NULL);
    if (!NT_SUCCESS(status)) {
        __atomic_store_n(&stopping, 1, __ATOMIC_RELEASE);
        KeSetEvent(&available, IO_NO_INCREMENT, FALSE);
        NtWaitForSingleObject(handle, FALSE, NULL);
    }
    ZwClose(handle);
    return status;
}

BOOLEAN WkWakeWorker(void)
{
    KeSetEvent(&available, IO_NO_INCREMENT, FALSE);
    return TRUE;
}

VOID WkStopWorker(void)
{
    __atomic_store_n(&stopping, 1, __ATOMIC_RELEASE);
    KeSetEvent(&available, IO_NO_INCREMENT, FALSE);
    KeWaitForSingleObject(worker, Executive, KernelMode, FALSE, NULL);
    ObDereferenceObject(worker);
    worker = NULL;
}
