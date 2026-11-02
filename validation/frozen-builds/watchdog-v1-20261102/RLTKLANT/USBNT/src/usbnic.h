/*
 * usbnic.h - RLTKLANT USB-NIC front end: policy + bring-up + data path.
 *
 * This is the entry the main RLTKLANT driver calls to support a Realtek
 * USB LAN part.  It applies the routing policy (usbpol), matches the chip
 * (usbchip), binds the pipe to the active USB stack (usbbind) and drives
 * the part through the protocol layer (rtl815x).
 *
 * Copyright (C) 2026 ages2001.
 */
#ifndef USBNIC_H
#define USBNIC_H

#include "ndiskit.h"
#include "usbpol.h"
#include "usbbind.h"
#include "rtl815x.h"
#include "usbfwio.h"
typedef struct {
    unsigned BusSpeed;
    int (*WaitMs)(void*,unsigned);
    int (*Cancelled)(void*);
    void *Context;
} USBNIC_PLATFORM;

typedef enum _USBNIC_ROUTE {
    UsbRouteNone = 0,       /* not a supported part / policy says stand off */
    UsbRouteUsbnt,          /* carried by USBNT (NT<=4, or 2000+ xHCI/USB4) */
    UsbRouteInbox           /* carried by the OS in-box USB stack (2000+)   */
} USBNIC_ROUTE;

typedef struct _USBNIC {
    RTL_USB_NIC     Nic;
    USBNT_BIND      Bind;
    USBNT_OSVER     Os;
    USBNT_HC_KIND   Hc;
    USBNIC_ROUTE    Route;
    RTL_USB_HW_STATE Hardware;
    USBNIC_PLATFORM Platform;
    BOOLEAN HardwareReady,HardwareActive;
    ULONG HardwareSpeed;
    BOOLEAN HardwareDuplex;
} USBNIC, *PUSBNIC;

/*
 * Decide how a Realtek USB NIC is routed on this system.  'hc' is the
 * controller tier the NIC sits on (known from the devnode on 2000+, or
 * UsbHcUnknown on NT<=4 where it is irrelevant because USBNT owns every
 * tier).  Returns the route; UsbRouteUsbnt means run on the attached
 * usbnt.sys, UsbRouteInbox means use the OS stack, UsbRouteNone means
 * leave it alone.
 */
USBNIC_ROUTE UsbNicRoute(USBNT_OSVER os, USBNT_HC_KIND hc);

/*
 * Attach to a NIC the active stack has enumerated.  'devObject' is its
 * transport target and 'submit' performs synchronous I/O (see usbbind.h).
 * Resets the part, reads the MAC and enables TX+RX.  Returns success only
 * when the chip id matched and bring-up completed.
 */
NDIS_STATUS UsbNicAttach(PUSBNIC u, USBNT_OSVER os, USBNT_HC_KIND hc,
                         USHORT vendor, USHORT device,
                         PVOID devObject, PFN_USB_SUBMIT submit);
/* Full vendor hardware initialization. Required for actual driver owners;
   the original Attach entry is retained for protocol-only fixture callers. */
NDIS_STATUS UsbNicAttachEx(PUSBNIC,USBNT_OSVER,USBNT_HC_KIND,USHORT,USHORT,
    PVOID,PFN_USB_SUBMIT,const USBNIC_PLATFORM*);
/* Serialized passive owner: gate host RX before a carrier/speed transition,
   then publish carrier only after this function succeeds. */
NDIS_STATUS UsbNicApplyCarrier(PUSBNIC,BOOLEAN,ULONG,BOOLEAN);
/* Detect a lost running MAC after a bus reset. The owner gates receive and
   discards old packet epochs before invoking the synchronous recovery. */
NDIS_STATUS UsbNicCheckReset(PUSBNIC,PBOOLEAN);
NDIS_STATUS UsbNicRecoverReset(PUSBNIC);

/* Data path.  Send copies 'frame' out through the bulk-OUT pipe with the
   tx descriptor prepended (caller frame <= RTLU_MAX_FRAME).  Poll reads
   one bulk-IN buffer and indicates each contained frame through 'sink'. */
NDIS_STATUS UsbNicSend(PUSBNIC u, PUCHAR frame, ULONG len);

typedef VOID (*PFN_USB_RX_SINK)(PVOID ctx, PUCHAR frame, ULONG len);
NDIS_STATUS UsbNicPoll(PUSBNIC u, PFN_USB_RX_SINK sink, PVOID ctx);

/* Called by the serialized passive-level USB owner; attachment is not carrier. */
NDIS_STATUS UsbNicReadLink(PUSBNIC u, PBOOLEAN connected);
NDIS_STATUS UsbNicReadMedia(PUSBNIC u, PBOOLEAN connected, PULONG speed, PBOOLEAN duplex);
VOID UsbNicDetach(PUSBNIC u);

#endif /* USBNIC_H */
