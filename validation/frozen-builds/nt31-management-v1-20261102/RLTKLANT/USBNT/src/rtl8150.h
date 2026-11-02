/*
 * rtl8150.h - Realtek RTL8150 (USB 1.1 10/100) legacy protocol layer.
 *
 * The RTL8150 predates the r8152 family and speaks a different, simpler
 * protocol: a flat register file addressed through the 16-bit wValue field
 * with the same
 * vendor GET/SET REGS request (wValue = register index, wIndex = 0, no
 * MCU-type or byte-enable), raw ethernet frames on the bulk-OUT pipe with
 * no transmit descriptor, and received frames carrying a 4-byte receive
 * status word appended at the tail instead of a leading descriptor.
 *
 * Register offsets and bit meanings follow the public RTL8150 datasheet
 * and the Linux rtl8150 driver (drivers/net/usb/rtl8150.c).  No Realtek or
 * Microsoft source is used.  Transfers go through the same USBNT_PIPE and
 * the same RTL_USB_NIC object as the r8152 backend, so the host-stack
 * routing (USBNT vs in-box) is identical.
 *
 * Copyright (C) 2026 ages2001.
 */
#ifndef RTL8150_H
#define RTL8150_H

#include "ndiskit.h"
#include "rtl815x.h"            /* RTL_USB_NIC, RTLU_RT_*, RTLU_REQ_*       */

#define RTL8150_DEVICE_ID       0x8150

/* Flat register file (16-bit byte offsets in the REGS vendor request). */
#define R8150_IDR               0x0120      /* 6-byte station address       */
#define R8150_MAR               0x0126      /* 8-byte multicast hash        */
#define R8150_CR                0x012E      /* command register (1 byte)    */
#define R8150_TCR               0x012F      /* transmit config (1 byte)     */
#define R8150_RCR               0x0130      /* receive config (2 bytes)     */
#define R8150_TSR               0x0132
#define R8150_RSR               0x0133
#define R8150_MSR               0x0137      /* media status                 */

/* R8150_CR bits */
#define R8150_CR_AUTOLOAD       0x01
#define R8150_CR_EP3CLREN       0x02
#define R8150_CR_TE             0x04        /* transmitter enable           */
#define R8150_CR_RE             0x08        /* receiver enable              */
#define R8150_CR_RST            0x10        /* soft reset (self-clearing)   */

/* R8150_RCR bits (datasheet default 0x009E = AB|AM|APM + good-CRC gating) */
#define R8150_RCR_AAP           0x0001      /* accept all physical          */
#define R8150_RCR_APM           0x0004      /* accept physical match        */
#define R8150_RCR_AM            0x0002      /* accept multicast             */
#define R8150_RCR_AB            0x0008      /* accept broadcast             */
#define R8150_RCR_DEFAULT       0x009E

/* R8150_MSR bits */
#define R8150_MSR_LINK          0x04        /* link up                      */
#define R8150_MSR_SPEED_100     0x08        /* 1 = 100 Mbit, 0 = 10 Mbit    */

/* A received bulk-IN buffer carries the frame followed by a 4-byte status
   word; a full-speed bulk-OUT whose length is a multiple of the 64-byte
   max packet needs one pad byte so it is not seen as a short packet. */
#define R8150_RX_STATUS_SIZE    4
#define R8150_FS_MAXPKT         64

/* Bring-up and framing (mirror of the rtl815x.c entry points). */
NDIS_STATUS Rtl8150Reset  (PRTL_USB_NIC n);
NDIS_STATUS Rtl8150ReadMac(PRTL_USB_NIC n);
NDIS_STATUS Rtl8150Start  (PRTL_USB_NIC n);
NDIS_STATUS Rtl8150Stop   (PRTL_USB_NIC n);

/* BuildTx requires output space for max(len,60)+1 bytes. It pads short
   Ethernet frames to 60 bytes and adds one byte for exact full-speed USB
   packet multiples, without a TX descriptor. NextRx returns the
   single ethernet frame in a bulk-IN buffer (status word stripped). */
ULONG Rtl8150BuildTx(PUCHAR frame, ULONG len, PUCHAR out);
ULONG Rtl8150NextRx (PUCHAR buf, ULONG bufLen, PUCHAR *frame, PULONG frameLen);

#endif /* RTL8150_H */
