#ifndef WK_QUEUE_H
#define WK_QUEUE_H
#include <wdm.h>

NTSTATUS WkStartQueue(PDEVICE_OBJECT device);
VOID WkStopQueue(void);
BOOLEAN WkQueueReady(void);
NTSTATUS WkQueueRemovalProbe(PDEVICE_OBJECT device, PIO_REMOVE_LOCK lifetime);
BOOLEAN WkRemovalProbePassed(void);
NTSTATUS WkQueueRequest(PIRP irp);
VOID WkCleanupRequests(PFILE_OBJECT file);

/* Backend storage stays valid until Stop has joined the worker or withdrawn
 * the VMM event. There is one worker per loaded kit. */
NTSTATUS WkStartWorker(void);
BOOLEAN WkWakeWorker(void);
VOID WkStopWorker(void);
VOID WkDrainRequests(NTSTATUS status);
#endif
