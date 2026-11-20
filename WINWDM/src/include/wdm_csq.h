#ifndef WK_WDM_CSQ_H
#define WK_WDM_CSQ_H

#include <wdm.h>

/* Use native IO_CSQ and IRP layouts while linking the six routines locally. */
#define IoCsqInitialize    WkIoCsqInitialize
#define IoCsqInitializeEx  WkIoCsqInitializeEx
#define IoCsqInsertIrp     WkIoCsqInsertIrp
#define IoCsqInsertIrpEx   WkIoCsqInsertIrpEx
#define IoCsqRemoveIrp     WkIoCsqRemoveIrp
#define IoCsqRemoveNextIrp WkIoCsqRemoveNextIrp

#ifdef __cplusplus
extern "C" {
#endif
NTSTATUS NTAPI IoCsqInitialize(PIO_CSQ, PIO_CSQ_INSERT_IRP, PIO_CSQ_REMOVE_IRP,
    PIO_CSQ_PEEK_NEXT_IRP, PIO_CSQ_ACQUIRE_LOCK, PIO_CSQ_RELEASE_LOCK,
    PIO_CSQ_COMPLETE_CANCELED_IRP);
NTSTATUS NTAPI IoCsqInitializeEx(PIO_CSQ, PIO_CSQ_INSERT_IRP_EX, PIO_CSQ_REMOVE_IRP,
    PIO_CSQ_PEEK_NEXT_IRP, PIO_CSQ_ACQUIRE_LOCK, PIO_CSQ_RELEASE_LOCK,
    PIO_CSQ_COMPLETE_CANCELED_IRP);
VOID NTAPI IoCsqInsertIrp(PIO_CSQ, PIRP, PIO_CSQ_IRP_CONTEXT);
NTSTATUS NTAPI IoCsqInsertIrpEx(PIO_CSQ, PIRP, PIO_CSQ_IRP_CONTEXT, PVOID);
PIRP NTAPI IoCsqRemoveIrp(PIO_CSQ, PIO_CSQ_IRP_CONTEXT);
PIRP NTAPI IoCsqRemoveNextIrp(PIO_CSQ, PVOID);
#ifdef __cplusplus
}
#endif
#endif
