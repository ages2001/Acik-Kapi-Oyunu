/*
 * usbplug.c - hot-plug manager for the RLTKLANT USB-NIC front end
 *             (see usbplug.h).
 *
 * Copyright (C) 2026 ages2001.
 */
#include "usbplug.h"

static VOID ReportMedia(PUSBNIC_PORT p, BOOLEAN connected)
{
    if (p->MediaConnected == connected) return;
    p->MediaConnected = connected;
    if (p->Media) p->Media(p->CoreCtx, connected);
}

VOID UsbNicPortInit(PUSBNIC_PORT p, USBNT_OSVER os,
                    PFN_USB_MEDIA media, PVOID coreCtx)
{
    ULONG i;
    for (i = 0; i < sizeof(USBNIC_PORT); i++) ((PUCHAR)p)[i] = 0;
    p->Os      = os;
    p->State   = UsbStateIdle;
    p->Media   = media;
    p->CoreCtx = coreCtx;
}

USBNIC_STATE UsbNicPortArrival(PUSBNIC_PORT p, USBNT_HC_KIND hc,
                               USHORT vendor, USHORT device,
                               PVOID devObject, PFN_USB_SUBMIT submit)
{
    NDIS_STATUS st;

    /* Ignore a duplicate arrival for a port already serving a NIC. */
    if (p->State == UsbStateAttached)
        return p->State;

    /* Not a supported Realtek USB part -> leave it alone. */
    if (!RtlUsbMatch(vendor, device))
        return p->State;

    p->Arrivals++;

    /* Policy: on Windows 2000+ a NIC on a non-SuperSpeed controller is the
       in-box stack's to carry, so RLTKLANT's USB path stands down and the
       port stays idle. */
    if (UsbNicRoute(p->Os, hc) == UsbRouteInbox) {
        p->State = UsbStateIdle;
        return p->State;
    }

    st = UsbNicAttach(&p->Dev, p->Os, hc, vendor, device, devObject, submit);
    if (!USB_OK(st)) {
        p->State = UsbStateFault;
        return p->State;
    }

    p->State = UsbStateAttached;
    UsbNicPortRefreshLink(p);
    return p->State;
}

VOID UsbNicPortRemoval(PUSBNIC_PORT p)
{
    if (p->State == UsbStateAttached) {
        UsbNicDetach(&p->Dev);
        ReportMedia(p, FALSE);
        p->Removals++;
    }
    /* re-arm for the next insertion, whatever the previous state was */
    p->State = UsbStateIdle;
}

NDIS_STATUS UsbNicPortSend(PUSBNIC_PORT p, PUCHAR frame, ULONG len)
{
    if (p->State != UsbStateAttached) return NDIS_STATUS_FAILURE;
    return UsbNicSend(&p->Dev, frame, len);
}

NDIS_STATUS UsbNicPortPoll(PUSBNIC_PORT p, PFN_USB_RX_SINK sink, PVOID ctx)
{
    if (p->State != UsbStateAttached) return NDIS_STATUS_FAILURE;
    if (!USB_OK(UsbNicPortRefreshLink(p))) return NDIS_STATUS_FAILURE;
    return UsbNicPoll(&p->Dev, sink, ctx);
}

BOOLEAN UsbNicPortLinkUp(PUSBNIC_PORT p)
{
    return (BOOLEAN)(p->State == UsbStateAttached && p->MediaConnected);
}

NDIS_STATUS UsbNicPortRefreshLink(PUSBNIC_PORT p)
{
    BOOLEAN connected = FALSE;
    NDIS_STATUS st = NDIS_STATUS_SUCCESS;
    if (p->State == UsbStateAttached) st = UsbNicReadLink(&p->Dev, &connected);
    /* Read errors mean carrier is unknown/down, never a fabricated connect. */
    ReportMedia(p, USB_OK(st) && connected);
    return st;
}
