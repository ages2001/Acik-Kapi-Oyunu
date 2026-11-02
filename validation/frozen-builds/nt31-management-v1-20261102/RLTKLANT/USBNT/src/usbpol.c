/*
 * usbpol.c - implementation of the RLTKLANT USB-NIC routing policy
 *            (see usbpol.h).
 *
 * Copyright (C) 2026 ages2001.
 */
#include "usbpol.h"

USBNT_HC_KIND UsbntClassifyHc(UCHAR progIf)
{
    switch (progIf) {
    case USB_PROGIF_UHCI: return UsbHcUhci;
    case USB_PROGIF_OHCI: return UsbHcOhci;
    case USB_PROGIF_EHCI: return UsbHcEhci;
    case USB_PROGIF_XHCI: return UsbHcXhci;
    case USB_PROGIF_USB4: return UsbHcUsb4;
    default:              return UsbHcUnknown;
    }
}

/* xHCI and USB4 are the "USB3 or USB4" SuperSpeed tier. */
static BOOLEAN UsbntHcIsSuperSpeed(USBNT_HC_KIND hc)
{
    return (BOOLEAN)(hc == UsbHcXhci || hc == UsbHcUsb4);
}

BOOLEAN UsbntCarriesNic(USBNT_OSVER os, USBNT_HC_KIND hc)
{
    /* Pre-2000 (NT 4.0 and earlier): no in-box USB stack at all, so USBNT
       carries the NIC on every controller tier. */
    if (os.Major < 5)
        return TRUE;

    /* Windows 2000 and later ship UHCI/OHCI/EHCI bus drivers; only the
       SuperSpeed tier (xHCI / USB4) runs on USBNT there. */
    return UsbntHcIsSuperSpeed(hc);
}

BOOLEAN UsbntUseForNic(USBNT_OSVER os, USBNT_HC_KIND hc)
{
    /* NT 4.0 and earlier: always USBNT, on any controller. */
    if (os.Major < 5)
        return TRUE;

    /* Windows 2000 and later: USBNT only for USB3/USB4 (xHCI / USB4);
       otherwise the in-box USB stack carries the NIC. */
    return UsbntHcIsSuperSpeed(hc);
}
