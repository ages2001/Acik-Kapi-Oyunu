/* Host definitions for the queue's public WDM contracts exercised by the
 * pthread test. The shipping objects are compiled with the x86 DDK. */
#ifndef WK_CSQ_HOST_WDM_H
#define WK_CSQ_HOST_WDM_H
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>
#define NTAPI
typedef void VOID, *PVOID, *PDEVICE_OBJECT;
typedef int32_t NTSTATUS;
typedef uint32_t ULONG, *PULONG;
typedef unsigned char BOOLEAN, KIRQL, *PKIRQL;
#define TRUE 1
#define FALSE 0
#define STATUS_SUCCESS 0
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xc000000d)
#define NT_SUCCESS(s) ((s)>=0)
#define IO_TYPE_CSQ_IRP_CONTEXT 1
#define IO_TYPE_CSQ 2
#define IO_TYPE_CSQ_EX 3
typedef struct _IRP IRP, *PIRP;
typedef void (*PDRIVER_CANCEL)(PDEVICE_OBJECT, PIRP);
struct _IRP {
    struct { struct { PVOID DriverContext[4]; } Overlay; } Tail;
    _Atomic(PDRIVER_CANCEL) routine;
    _Atomic BOOLEAN Cancel;
    KIRQL CancelIrql;
    _Atomic unsigned pending;
};
typedef struct _IO_CSQ IO_CSQ, *PIO_CSQ;
typedef struct { ULONG Type; PIRP Irp; PIO_CSQ Csq; } IO_CSQ_IRP_CONTEXT, *PIO_CSQ_IRP_CONTEXT;
typedef void (*PIO_CSQ_INSERT_IRP)(PIO_CSQ, PIRP);
typedef NTSTATUS (*PIO_CSQ_INSERT_IRP_EX)(PIO_CSQ, PIRP, PVOID);
typedef void (*PIO_CSQ_REMOVE_IRP)(PIO_CSQ, PIRP);
typedef PIRP (*PIO_CSQ_PEEK_NEXT_IRP)(PIO_CSQ, PIRP, PVOID);
typedef void (*PIO_CSQ_ACQUIRE_LOCK)(PIO_CSQ, PKIRQL);
typedef void (*PIO_CSQ_RELEASE_LOCK)(PIO_CSQ, KIRQL);
typedef void (*PIO_CSQ_COMPLETE_CANCELED_IRP)(PIO_CSQ, PIRP);
struct _IO_CSQ {
    ULONG Type;
    PIO_CSQ_INSERT_IRP CsqInsertIrp;
    PIO_CSQ_REMOVE_IRP CsqRemoveIrp;
    PIO_CSQ_PEEK_NEXT_IRP CsqPeekNextIrp;
    PIO_CSQ_ACQUIRE_LOCK CsqAcquireLock;
    PIO_CSQ_RELEASE_LOCK CsqReleaseLock;
    PIO_CSQ_COMPLETE_CANCELED_IRP CsqCompleteCanceledIrp;
    PVOID ReservePointer;
};
#define IoSetCancelRoutine(i, fn) atomic_exchange(&(i)->routine, (fn))
#define IoMarkIrpPending(i) ((void)atomic_fetch_add(&(i)->pending, 1))
void IoReleaseCancelSpinLock(KIRQL irql);
#endif
