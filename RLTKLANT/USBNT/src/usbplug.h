/*
 * usbplug.h - hot-plug manager for the RLTKLANT USB-NIC front end.
 *
 * Protocol-level lifecycle helper. Its serialized passive-level owner
 * supplies arrivals/removals and polls carrier independently of attachment.
 * The current USBNT worker uses UsbNic directly; NDIS USB presentation and
 * native inbox-stack PnP integration remain unfinished. Routing policy alone
 * does not implement either transport or prove guest hot-plug behavior.
 *
 * The manager keeps one logical port: at most one NIC attached at a time,
 * re-armed after every removal so a later re-insert attaches again.
 *
 * Copyright (C) 2026 ages2001.
 */
#ifndef USBPLUG_H
#define USBPLUG_H

#include "usbnic.h"

typedef enum _USBNIC_STATE {
    UsbStateIdle = 0,       /* no NIC present; waiting for an arrival       */
    UsbStateAttached,       /* transport attached; carrier checked separately */
    UsbStateFault           /* chip matched but bring-up failed             */
} USBNIC_STATE;

/* Called by the port when media (link) state changes, so the RLTKLANT core
   can raise NDIS_STATUS_MEDIA_CONNECT / _DISCONNECT. */
typedef VOID (*PFN_USB_MEDIA)(PVOID coreCtx, BOOLEAN connected);

typedef struct _USBNIC_PORT {
    USBNT_OSVER     Os;
    USBNIC_STATE    State;
    USBNIC          Dev;            /* valid only while UsbStateAttached    */
    PFN_USB_MEDIA   Media;
    PVOID           CoreCtx;
    ULONG           Arrivals;       /* lifetime counters (diagnostics)      */
    ULONG           Removals;
    BOOLEAN         MediaConnected;
} USBNIC_PORT, *PUSBNIC_PORT;

/* Arm the port at driver load.  os is this system's version; media/coreCtx
   let the port report link up/down to the RLTKLANT core.  Safe to call
   before any device is present. */
VOID UsbNicPortInit(PUSBNIC_PORT p, USBNT_OSVER os,
                    PFN_USB_MEDIA media, PVOID coreCtx);

/* Device arrival, from the active stack, at any time after Init (including
   immediately, for a device already present at load).  Applies the routing
   policy, attaches, and reads actual carrier before reporting media connect.  Returns the
   resulting state. */
USBNIC_STATE UsbNicPortArrival(PUSBNIC_PORT p, USBNT_HC_KIND hc,
                               USHORT vendor, USHORT device,
                               PVOID devObject, PFN_USB_SUBMIT submit);

/* Surprise or orderly removal.  Tears the NIC down, reports media
   disconnect, and re-arms the port for the next arrival. */
VOID UsbNicPortRemoval(PUSBNIC_PORT p);

/* Convenience for the core's data path; both no-op unless attached. */
NDIS_STATUS UsbNicPortSend(PUSBNIC_PORT p, PUCHAR frame, ULONG len);
NDIS_STATUS UsbNicPortPoll(PUSBNIC_PORT p, PFN_USB_RX_SINK sink, PVOID ctx);
BOOLEAN     UsbNicPortLinkUp(PUSBNIC_PORT p);
/* Passive-level serialized owner polls link even when no packets arrive. */
NDIS_STATUS UsbNicPortRefreshLink(PUSBNIC_PORT p);

#endif /* USBPLUG_H */
