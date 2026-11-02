/*
 * usbbind.h - lower-edge binding of a NIC pipe to the active USB stack.
 *
 * Copyright (C) 2026 ages2001.
 */
#ifndef USBBIND_H
#define USBBIND_H

#include "usbio.h"

/* See usbbind.c.  kind 0 = control (setup + data stage), 1 = bulk (ep). */
typedef NDIS_STATUS (*PFN_USB_SUBMIT)(PVOID devObject, UCHAR kind, UCHAR ep,
                                      PUCHAR setup, PVOID buffer,
                                      ULONG length, PULONG done);

typedef struct _USBNT_BIND {
    PVOID           DevObject;      /* NIC PDO from the active USB stack    */
    PFN_USB_SUBMIT  Submit;
} USBNT_BIND, *PUSBNT_BIND;

VOID UsbntBindPipe(PUSBNT_PIPE pipe, PUSBNT_BIND bind);

#endif /* USBBIND_H */
