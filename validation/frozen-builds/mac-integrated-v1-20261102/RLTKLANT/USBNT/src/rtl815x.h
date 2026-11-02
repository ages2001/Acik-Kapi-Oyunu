/*
 * rtl815x.h - Realtek USB LAN (r8152 family) protocol layer for RLTKLANT.
 *
 * Pure protocol: it turns NIC operations into USB control and bulk
 * transfers through a USBNT_PIPE (usbio.h), with no knowledge of which
 * USB stack carries them.  Register offsets and the vendor-request scheme
 * follow the public RTL8152/8153 register map and the Linux r8152 driver.
 *
 * Copyright (C) 2026 ages2001.
 */
#ifndef RTL815X_H
#define RTL815X_H

#include "ndiskit.h"
#include "usbio.h"
#include "usbchip.h"

typedef struct _RTL_USB_NIC {
    USBNT_PIPE          Pipe;           /* transfers go here                */
    const RTL_USB_ID   *Chip;           /* matched device-id row            */
    UCHAR               Mac[6];
    ULONG               RcrFlags;       /* current receive-config bits      */
    USHORT              HwVersion;
    USHORT              MaxSpeedMbit;
    UCHAR               Version;
    UCHAR               PackageId;      /* PLA_PKG_DET & 0x1e on RTL8159 */
    BOOLEAN             DescriptorV2;
    BOOLEAN             Running;
} RTL_USB_NIC, *PRTL_USB_NIC;

/* Match a USB vendor/device id against the supported table (NULL if not). */
const RTL_USB_ID *RtlUsbMatch(USHORT vendor, USHORT device);

/* Register file access via the GET/SET REGS vendor request. */
NDIS_STATUS RtlUsbReadRegs (PRTL_USB_NIC n, USHORT mcuType, USHORT offset,
                            PVOID buf, USHORT len);
NDIS_STATUS RtlUsbWriteRegs(PRTL_USB_NIC n, USHORT mcuType, USHORT offset,
                            UCHAR byteEn, PVOID buf, USHORT len);

/* Bring-up: soft reset, read the station address, enable TX+RX. */
NDIS_STATUS RtlUsbDetectVersion(PRTL_USB_NIC n);
NDIS_STATUS RtlUsbReset (PRTL_USB_NIC n);
NDIS_STATUS RtlUsbReadMac(PRTL_USB_NIC n);
NDIS_STATUS RtlUsbStart (PRTL_USB_NIC n);
NDIS_STATUS RtlUsbStop  (PRTL_USB_NIC n);

/* Receive filter (RTLU_RCR_* bits). */
NDIS_STATUS RtlUsbSetRxMode(PRTL_USB_NIC n, ULONG rcrFlags);

/* Framing.  BuildTx prepends the tx descriptor into 'out' (caller supplies
   a buffer >= len + RTLU_TX_DESC_SIZE) and returns the total length to put
   on the bulk-OUT pipe.  NextRx walks one rx-descriptor-prefixed packet in
   a bulk-IN buffer: it sets frame and frameLen to the ethernet frame and
   returns the offset of the next packet (0 when the buffer is exhausted). */
ULONG RtlUsbBuildTx(PUCHAR frame, ULONG len, PUCHAR out);
ULONG RtlUsbNextRx (PUCHAR buf, ULONG bufLen, ULONG at,
                    PUCHAR *frame, PULONG frameLen);

/* RTL8157 v2 descriptors: TX requires len + 16 bytes; RX strips the FCS. */
ULONG Rtl8157BuildTx(PUCHAR frame, ULONG len, PUCHAR out);
ULONG Rtl8157NextRx(PUCHAR buf, ULONG bufLen, ULONG at,
                    PUCHAR *frame, PULONG frameLen);

#endif /* RTL815X_H */
