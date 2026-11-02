/* Abstract USB transfer binding for the Realtek protocol layer.
 * The implemented USBNT path calls USBNT_DEV_IF through usbcli.c.
 * An inbox Windows USB URB transport is still an integration requirement;
 * these callbacks alone do not implement that transport or an NDIS adapter.
 * Copyright (C) 2026 ages2001.
 */
#ifndef USBIO_H
#define USBIO_H

#include "ndiskit.h"

typedef struct _USBNT_PIPE USBNT_PIPE, *PUSBNT_PIPE;

/* A control (endpoint 0) transfer: setup packet + optional data stage. */
typedef NDIS_STATUS (*PFN_USB_CONTROL)(PUSBNT_PIPE pipe,
                                       UCHAR  bmRequestType,
                                       UCHAR  bRequest,
                                       USHORT wValue,
                                       USHORT wIndex,
                                       PVOID  buffer,
                                       USHORT length);

/* A bulk transfer on the given endpoint address (0x81 IN, 0x02 OUT, ...).
   Returns the number of bytes actually transferred in *done. */
typedef NDIS_STATUS (*PFN_USB_BULK)(PUSBNT_PIPE pipe,
                                    UCHAR  endpoint,
                                    PVOID  buffer,
                                    ULONG  length,
                                    PULONG done);

typedef struct _USBNT_PIPE {
    PVOID            HcContext;     /* host-stack private handle            */
    PFN_USB_CONTROL  Control;
    PFN_USB_BULK     Bulk;
} USBNT_PIPE;

#define USB_OK(st)  ((st) == NDIS_STATUS_SUCCESS)

#endif /* USBIO_H */
