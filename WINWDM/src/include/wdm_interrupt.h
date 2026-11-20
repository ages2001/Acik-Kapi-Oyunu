#ifndef WINWDM_INTERRUPT_H
#define WINWDM_INTERRUPT_H

#include <wdm.h>

/* Native x86 NT with a translated hardware interrupt. Include after the DDK.
 * The caller owns the device and keeps its ISR and context resident. */
#undef IoConnectInterruptEx
#undef IoDisconnectInterruptEx
#define IoConnectInterruptEx WkIoConnectInterruptEx
#define IoDisconnectInterruptEx WkIoDisconnectInterruptEx

#ifdef __cplusplus
extern "C" {
#endif
NTSTATUS NTAPI IoConnectInterruptEx(PIO_CONNECT_INTERRUPT_PARAMETERS parameters);
VOID NTAPI IoDisconnectInterruptEx(PIO_DISCONNECT_INTERRUPT_PARAMETERS parameters);
#ifdef __cplusplus
}
#endif
#endif
