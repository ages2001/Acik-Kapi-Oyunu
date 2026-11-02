/*
 * usbpol.h - RLTKLANT USB-NIC routing policy (which stack carries the NIC).
 *
 * The Realtek USB LAN parts (RTL8150/8152/8153/8156/8157) are driven by
 * RLTKLANT's USB function driver, which does its USB transfers through a
 * USB stack underneath it.  Which stack that is depends on the operating
 * system and on the host-controller generation the NIC hangs off:
 *
 *   - Windows NT 4.0 and earlier (NT 3.1 / 3.50 / 3.51 / 4.0) have no USB
 *     support of their own, so RLTKLANT's USB path always runs on top of
 *     USBNT (usbnt.sys), which supplies the whole host stack
 *     (UHCI/OHCI/EHCI/xHCI).  USBNT is used for every Realtek USB NIC on
 *     these systems, whatever controller it is on.
 *
 *   - Windows 2000 and later ship a USB bus driver that already drives
 *     UHCI / OHCI / EHCI (USB 1.1 and USB 2.0).  On those controllers the
 *     NIC reaches RLTKLANT through the in-box USB stack and USBNT is not
 *     involved.  Only on an xHCI controller - a USB 3 or USB 4 port, which
 *     the 2000/XP in-box stack does not support - does the NIC run on
 *     USBNT (which claims xHCI on 2000/XP).  So on 2000+ USBNT is used for
 *     the NIC only in the USB3 / USB4 case.
 *
 * This file is RLTKLANT's own decision logic; it does not drive or modify
 * USBNT.  USBNT (the attached release, unchanged) is the stack it runs on.
 *
 * Copyright (C) 2026 ages2001.
 */
#ifndef USBPOL_H
#define USBPOL_H

#include "ndiskit.h"

/* USB host-controller generation, from the PCI class triple
   (base 0x0C, sub 0x03, prog-if).  USB4 host controllers enumerate as
   xHCI-class (0x30) or as the 0x40 "USB device" prog-if; both are the
   SuperSpeed (USB3/USB4) tier. */
typedef enum _USBNT_HC_KIND {
    UsbHcUnknown = 0,
    UsbHcUhci,              /* prog-if 0x00  - USB 1.1                      */
    UsbHcOhci,              /* prog-if 0x10  - USB 1.1                      */
    UsbHcEhci,              /* prog-if 0x20  - USB 2.0                      */
    UsbHcXhci,              /* prog-if 0x30  - USB 3.x                      */
    UsbHcUsb4               /* prog-if 0x40  - USB4 host                    */
} USBNT_HC_KIND;

#define USB_PCI_CLASS_SERIAL    0x0C
#define USB_PCI_SUB_USB         0x03
#define USB_PROGIF_UHCI         0x00
#define USB_PROGIF_OHCI         0x10
#define USB_PROGIF_EHCI         0x20
#define USB_PROGIF_XHCI         0x30
#define USB_PROGIF_USB4         0x40

/* Operating-system major/minor as reported by the loader / PsGetVersion.
   NT 3.1 = 3.10, 3.50 = 3.50, 3.51 = 3.51, 4.0 = 4.00, 2000 = 5.00,
   XP = 5.01, Server 2003 / XP x64 = 5.02. */
typedef struct _USBNT_OSVER {
    USHORT Major;
    USHORT Minor;
} USBNT_OSVER;

/* Map a PCI prog-if byte to a controller kind. */
USBNT_HC_KIND UsbntClassifyHc(UCHAR progIf);

/* TRUE when the NIC on this controller tier reaches RLTKLANT through USBNT
   (rather than an in-box USB stack) on this OS.  Equivalent to "USBNT is
   the stack in use here". */
BOOLEAN UsbntCarriesNic(USBNT_OSVER os, USBNT_HC_KIND hc);

/*
 * The master gate.  Returns TRUE when RLTKLANT's USB path should run on
 * USBNT for a Realtek USB NIC found behind the given controller on the
 * given OS:
 *
 *   NT 4.0 and earlier (Major < 5)             -> always TRUE  (USBNT)
 *   Windows 2000+ (Major >= 5), xHCI / USB4    -> TRUE         (USB3/USB4)
 *   Windows 2000+ (Major >= 5), UHCI/OHCI/EHCI -> FALSE        (in-box USB)
 *
 * When it returns FALSE, RLTKLANT binds through the OS's own USB stack and
 * USBNT is not used.
 */
BOOLEAN UsbntUseForNic(USBNT_OSVER os, USBNT_HC_KIND hc);

#endif /* USBPOL_H */
