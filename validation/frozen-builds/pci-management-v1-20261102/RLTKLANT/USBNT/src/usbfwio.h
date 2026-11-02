/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef RTL_USB_FW_IO_H
#define RTL_USB_FW_IO_H
#include "rtl815x.h"
#include "usbfwexec.h"
#include "usbphyseq.h"
/* Synchronous, exclusive PASSIVE-level upload on a stopped NIC. Required
   power/PHY preparation and image selection are the caller's responsibility.
   Both callbacks are mandatory; waits return nonzero after the full interval.
   The owner keeps the device and immutable firmware alive until this returns. */
enum RTL_UFW_EXEC_RESULT RtlUsbFirmwareApply(PRTL_USB_NIC,const RTL_UFW_U8*,
    RTL_UFW_U32,unsigned,int(*)(void*,unsigned),int(*)(void*),void*,RTL_UFW_EXEC_REPORT*);
/* Same lifetime/stopped-device contract, using the separate vendor PHY policy
   and its embedded patches. MAC cold-init/power preparation is still required. */
enum RTL_UFW_EXEC_RESULT RtlUsbPhyApply(PRTL_USB_NIC,unsigned,
    int(*)(void*,unsigned),int(*)(void*),void*,RTL_UPHY_REPORT*);
enum RTL_UFW_EXEC_RESULT RtlUsbHardwareApply(PRTL_USB_NIC,RTL_USB_HW_STATE*,
    unsigned usb_speed,unsigned vid,unsigned pid,unsigned rx_buffer_size,
    int(*)(void*,unsigned),int(*)(void*),void*,RTL_UPHY_REPORT*);
enum RTL_UFW_EXEC_RESULT RtlUsbHardwareEnableApply(PRTL_USB_NIC,RTL_USB_HW_STATE*,
    int(*)(void*,unsigned),int(*)(void*),void*,RTL_UPHY_REPORT*);
#endif
