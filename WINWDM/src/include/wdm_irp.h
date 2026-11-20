#ifndef WINWDM_IRP_H
#define WINWDM_IRP_H
#include <wdm.h>
#define IoReuseIrp WkIoReuseIrp
#ifdef __cplusplus
extern "C" {
#endif
VOID NTAPI IoReuseIrp(PIRP irp, NTSTATUS status);
#ifdef __cplusplus
}
#endif
#endif
