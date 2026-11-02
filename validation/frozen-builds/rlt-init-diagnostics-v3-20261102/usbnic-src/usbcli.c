/*
 * usbcli.c - RLTKLANT lower edge over the USBNT generic device interface.
 *
 * USBNT exposes every USB device through \Device\UsbNtDev (usbntcli.h).
 * RLTKLANT registers as a client, and for each Realtek USB NIC it is told
 * about it drives the chip through this seam: the usbbind submit primitive
 * (control setup + data stage, or a bulk transfer on an endpoint) is turned
 * into a USBNT_DEV_IF Control / Transfer call on the device's handle.  This
 * is the counterpart, for the USBNT-carried case, of the in-box USBDI path;
 * the RTL815x/RTL8150 protocol layer above it does not change.
 *
 * Copyright (C) 2026 ages2001.
 */
#include "usbio.h"
#include "usbchip.h"
#include "usbbind.h"
#include "usbntcli.h"
#include "usbcli.h"

/* usbbind submit primitive backed by the USBNT generic device interface.
   kind 0 = control (8-byte setup + data stage), 1 = bulk/interrupt (ep). */
NDIS_STATUS RtlUsbNtSubmit(PVOID devObject, UCHAR kind, UCHAR ep, PUCHAR setup,
                           PVOID buffer, ULONG length, PULONG done)
{
    PRTL_USBNT_TARGET t = (PRTL_USBNT_TARGET)devObject;
    ULONG act = 0;
    int r;

    if (done) *done = 0;
    if (!t || !t->If) return NDIS_STATUS_FAILURE;

    if (kind == 0) {
        USHORT val = (USHORT)(setup[2] | (setup[3] << 8));
        USHORT idx = (USHORT)(setup[4] | (setup[5] << 8));
        USHORT len = (USHORT)(setup[6] | (setup[7] << 8));
        r = t->If->Control(t->Id, setup[0], setup[1], val, idx, buffer, len, 5000, &act);
    } else {
        /* Bounded receive polling keeps an idle NIC from starving another
           adapter or removal notification. OUT retains a longer timeout. */
        r = t->If->Transfer(t->Id, ep, buffer, length, (ep & 0x80) ? 20 : 1000, &act);
    }
    if (done) *done = act;
    return (r == UC_OK) ? NDIS_STATUS_SUCCESS : NDIS_STATUS_FAILURE;
}

/* Fill a USBNT_BIND so a NIC pipe is carried by the USBNT device interface. */
VOID RtlUsbNtBind(PUSBNT_BIND bind, PRTL_USBNT_TARGET target,
                  const USBNT_DEV_IF *ifc, ULONG id)
{
    target->If = ifc;
    target->Id = id;
    bind->DevObject = target;
    bind->Submit    = RtlUsbNtSubmit;
}
