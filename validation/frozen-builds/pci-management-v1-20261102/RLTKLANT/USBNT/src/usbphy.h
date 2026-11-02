/* Revision-specific PHY register transport, separate from PHY configuration.
 * Call at passive level under the device owner's serialization. The wait
 * callback must wait at least the requested milliseconds without dispatching
 * another operation on this device. No callback is needed for legacy access
 * or an already-ready TGPHY command; a busy command without one fails closed.
 */
#ifndef USBPHY_H
#define USBPHY_H
#include "rtl815x.h"
typedef VOID (*PFN_USB_PHY_WAIT)(PVOID context, ULONG milliseconds);
NDIS_STATUS RtlUsbPhyRead(PRTL_USB_NIC n, USHORT address, PUSHORT value,
                         PFN_USB_PHY_WAIT wait, PVOID context);
NDIS_STATUS RtlUsbPhyWrite(PRTL_USB_NIC n, USHORT address, USHORT value,
                          PFN_USB_PHY_WAIT wait, PVOID context);
#endif
