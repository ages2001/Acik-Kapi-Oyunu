/* Persistent USBNT-backed miniport, matched by permanent station address.
 * Device I/O belongs to the passive-level USB worker. NDIS callbacks touch
 * copied-frame queues only. Native inbox USB/PnP is a separate lower edge.
 */
#ifndef RTL_BACKEND_USB_H
#define RTL_BACKEND_USB_H
#include "rltklant.h"
#include "usbnic.h"
#define RTL_BACKEND_USBNT 3
VOID RtlUsbAdaptersInit(VOID);
/* 0: PCI configuration, 1: USB configured, -1: invalid USB configuration. */
int RtlUsbReadConfig(PRLTK_ADAPTER a);
extern RLTK_OPS RtlOpsUsb;
VOID RtlUsbService(PUSBNIC nic, ULONG attachment);
VOID RtlUsbDisconnected(const UCHAR *mac);
#if defined(RTL_NDIS5)
BOOLEAN RtlNativePrepare(PRLTK_ADAPTER a);
BOOLEAN RtlNativeStart(PRLTK_ADAPTER a);
VOID RtlNativeStop(PRLTK_ADAPTER a);
BOOLEAN RtlNativeSetCarrier(PRLTK_ADAPTER a,BOOLEAN up);
VOID RtlNativeObserveBusReset(PRLTK_ADAPTER a);
#endif
#endif
