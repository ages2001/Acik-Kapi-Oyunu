/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef RTL_USB_PHY_SEQ_H
#define RTL_USB_PHY_SEQ_H
#include "usbfwexec.h"
enum RTL_UPHY_OPTIONS { RtlUsbPhyEee=1, RtlUsbPhyGreen=2 };
typedef struct {
    enum RTL_UFW_EXEC_RESULT Result;
    unsigned Operations, DelayMs, Warnings;
    unsigned LastSpace, LastRegister, LastSize;
    unsigned HostWakeRequested, HostLpmRequested;
} RTL_UPHY_REPORT;
/* Pinned vendor PHY configuration, including its own immutable embedded
 * patches. Do not apply the separate linux-firmware policy in the same pass.
 * The caller must own the stopped device at PASSIVE level and complete MAC
 * cold-init/power preparation first. This does not enable MAC traffic, restart
 * autonegotiation or implement suspend/resume. Stable revision suffix 1..17;
 * revision17 additionally requires the masked package ID from PLA_DC48.
 * Read/Write must serialize PHY/SRAM indirect access. Limits include waits.
 * A failed/cancelled transaction never performs further external I/O; reset
 * and reinitialize before retrying a partially configured device.
 */
enum RTL_UFW_EXEC_RESULT RtlUsbPhyConfigure(unsigned version, unsigned package,
    unsigned options, const RTL_UFW_BUS*, RTL_UPHY_REPORT*);
/* MAC cold-init only, before the explicit PHY phase. usb_speed is the actual
   negotiated speed: 2 full, 3 high, 5 super, 6 super-plus (Linux numeric ABI).
   Host wake/LPM requests are reported, not activated; caller owns host policy.
   This does not run MAC up/enable or claim a usable network connection. */
enum RTL_UFW_EXEC_RESULT RtlUsbMacPrepare(unsigned version,unsigned package,
    unsigned options,unsigned usb_speed,unsigned vid,unsigned pid,
    const RTL_UFW_BUS*,RTL_UPHY_REPORT*);
/* Caller-owned, per-device storage. Do not copy a live state or run concurrent
   phases. Each call rebinds callback/report pointers; they need only live for
   that call. Host packet queues remain the caller's responsibility. */
typedef union {void *Alignment; unsigned char Bytes[512];} RTL_USB_HW_STATE;
enum RTL_UFW_EXEC_RESULT RtlUsbHardwarePrepare(RTL_USB_HW_STATE*,unsigned version,
    unsigned package,unsigned options,unsigned usb_speed,unsigned vid,unsigned pid,
    unsigned rx_buffer_size,const RTL_UFW_BUS*,RTL_UPHY_REPORT*);
enum RTL_UFW_EXEC_RESULT RtlUsbHardwareEnable(RTL_USB_HW_STATE*,
    const RTL_UFW_BUS*,RTL_UPHY_REPORT*);
#endif
