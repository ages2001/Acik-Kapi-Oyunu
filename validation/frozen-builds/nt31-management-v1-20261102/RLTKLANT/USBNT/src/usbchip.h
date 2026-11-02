/*
 * usbchip.h - Realtek USB LAN controller database for USBNT.
 *
 * Covers the USB-attached Realtek parts (RTL8152 / 8153 / 8154 / 8156 /
 * 8157 / 8159).  These use the Realtek *USB* vendor id 0x0BDA, not
 * the PCI vendor id 0x10EC used by the PCI/PCIe parts handled by the main
 * RLTKLANT miniport, and are driven through USB vendor control requests
 * plus bulk IN/OUT endpoints rather than memory/IO registers.
 *
 * Register offsets and the vendor-request protocol are cross-checked
 * against the Linux r8152 driver (drivers/net/usb/r8152.c) and the public
 * RTL8152/8153 register map and the public Realtek r8152 Linux driver.
 * Pinned source revisions and remaining gaps are recorded in validation.
 *
 * Copyright (C) 2026 ages2001.
 */
#ifndef USBCHIP_H
#define USBCHIP_H

#define RTL_USB_VENDOR_ID       0x0BDA      /* Realtek USB vendor id        */

/*
 * One USB device-id the driver binds to.  MaxSpeed is the media ceiling
 * the part is allowed to negotiate; MinUsbGen is the lowest USB generation
 * the part physically ships on (used only for documentation / telemetry,
 * never to reject a working link).
 */
typedef struct _RTL_USB_ID {
    unsigned short  DeviceId;
    unsigned short  MaxSpeed;       /* megabit/s media ceiling              */
    unsigned char   MinUsbGen;      /* 1 = USB1.1, 2 = USB2.0, 3 = USB3.x   */
    const char     *Name;
} RTL_USB_ID;

/* media-speed ceilings (megabit/s) - shared meaning with rtlchip.h */
#define RTLU_SPEED_10           10
#define RTLU_SPEED_100          100
#define RTLU_SPEED_1000         1000
#define RTLU_SPEED_2500         2500
#define RTLU_SPEED_5000         5000
#define RTLU_SPEED_10000        10000

/* The r8152-family parts (RTL8152 / 8153 / 8154 / 8156 / 8157 / 8159 and
   their B/C/D/E/G die revisions) all share the vendor-request register
   file and the tx/rx descriptor framing implemented in rtl815x.c, so a
   single protocol backend drives the whole USB-LAN line from 10/100 Mbit
   up to 10 Gbit.  This table is the full current Realtek USB NIC lineup;
   die-revision suffixes reuse the base device-id.  The USB1.1 RTL8150 is a
   different, older silicon with its own protocol; it is still supported,
   through the separate legacy backend in rtl8150.c (selected by device-id
   0x8150), so the whole Realtek USB-LAN range is covered without a gap. */
static const RTL_USB_ID RtlUsbIds[] = {
    { 0x8150, RTLU_SPEED_100,   1, "Realtek RTL8150 USB 10/100 Fast Ethernet"           },
    { 0x8151, RTLU_SPEED_100,   2, "Realtek RTL8151 USB 10/100 Fast Ethernet"           },
    { 0x8152, RTLU_SPEED_100,   2, "Realtek RTL8152B USB 10/100 Fast Ethernet"          },
    { 0x8153, RTLU_SPEED_1000,  3, "Realtek RTL8153/8153B/C/D/E USB Gigabit Ethernet"   },
    { 0x8154, RTLU_SPEED_1000,  2, "Realtek RTL8154/8154B USB 2.0 Gigabit Ethernet"     },
    { 0x8155, RTLU_SPEED_2500,  3, "Realtek RTL8156 USB 2.5G Ethernet (8155 bridge)"    },
    { 0x8156, RTLU_SPEED_2500,  3, "Realtek RTL8156/8156B(G) USB 2.5G Ethernet"         },
    { 0x8157, RTLU_SPEED_5000,  3, "Realtek RTL8157 USB 5G Ethernet"                    },
    { 0x8159, RTLU_SPEED_10000, 3, "Realtek RTL8159 USB 10G Ethernet"                   },
    { 0x0000, 0,                0, 0 }
};

/*
 * Vendor control requests (bmRequestType / bRequest).  The MCU register
 * file is reached with a GET/SET REGS request; wValue is the register
 * offset and wIndex selects the PLA or USB block plus the byte-enable
 * mask, exactly as the r8152 driver issues them.
 */
#define RTLU_RT_READ            0xC0        /* IN  | vendor | device        */
#define RTLU_RT_WRITE           0x40        /* OUT | vendor | device        */
#define RTLU_REQ_GET_REGS       0x05
#define RTLU_REQ_SET_REGS       0x05

#define RTLU_MCU_TYPE_PLA       0x0100      /* protocol/link adapter block  */
#define RTLU_MCU_TYPE_USB       0x0000      /* USB block                    */
#define RTLU_BYTE_EN_BYTE       0x01
#define RTLU_BYTE_EN_WORD       0x03
#define RTLU_BYTE_EN_DWORD      0x0F

/* PLA registers (station address, command, receive config) */
#define RTLU_PLA_IDR            0xC000      /* 6-byte MAC                   */
#define RTLU_PLA_RCR            0xC010      /* receive configuration        */
#define RTLU_PLA_CR             0xE813      /* command register             */
#define RTLU_PLA_CRWECR         0xE81C      /* config-write-enable          */
#define RTLU_PLA_CONFIG5        0xC0D8
#define RTLU_PLA_LEDSEL         0xDD90
#define RTLU_PLA_MISC_1         0xE07C

/* Revision-specific MAC reset registers (vendor r8152 v2.22.1). */
#define RTLU_USB_BMU_RESET      0xD4B0
#define RTLU_USB_USB_CTRL       0xD406
#define RTLU_BMU_RESET_EP_IN    0x0001
#define RTLU_CDC_ECM_EN         0x0008

/* PLA_CR bits */
#define RTLU_CR_RE              0x08        /* receiver enable              */
#define RTLU_CR_TE              0x04        /* transmitter enable           */
#define RTLU_CR_RST             0x10        /* soft reset                   */

/* PLA_RCR bits */
#define RTLU_RCR_AAP            0x00000001  /* accept all physical          */
#define RTLU_RCR_APM            0x00000002  /* accept physical match        */
#define RTLU_RCR_AM             0x00000004  /* accept multicast             */
#define RTLU_RCR_AB             0x00000008  /* accept broadcast             */

/*
 * Bulk framing (r8152 family).  Each transmitted frame is prefixed with an
 * 8-byte tx descriptor (two little-endian dwords; opts1 carries the frame
 * length in its low 18 bits).  Each received bulk buffer is a sequence of
 * packets, every one prefixed with a 24-byte rx descriptor whose opts1
 * low 15 bits carry the packet length; packets are 8-byte aligned.
 */
#define RTLU_TX_DESC_SIZE       8
#define RTLU_RX_DESC_SIZE       24
#define RTLU_TX_LEN_MASK        0x0003FFFFUL
#define RTLU_RX_LEN_MASK        0x00007FFFUL
#define RTLU_RX_ALIGN           8
#define RTLU_EP_BULK_IN         0x81
#define RTLU_EP_BULK_OUT        0x02
#define RTLU_EP_INTR_IN         0x83
#define RTLU_MAX_FRAME          1514
#define RTLU_RX_BUF_SIZE        2048

#endif /* USBCHIP_H */
