/*
 * usbcli.h - RLTKLANT lower edge over the USBNT generic device interface.
 * Copyright (C) 2026 ages2001.
 */
#ifndef USBCLI_H
#define USBCLI_H
#include "usbio.h"
#include "usbbind.h"
#include "usbntcli.h"

typedef struct _RTL_USBNT_TARGET {
    const USBNT_DEV_IF *If;
    ULONG               Id;
} RTL_USBNT_TARGET, *PRTL_USBNT_TARGET;

NDIS_STATUS RtlUsbNtSubmit(PVOID devObject, UCHAR kind, UCHAR ep, PUCHAR setup,
                           PVOID buffer, ULONG length, PULONG done);
VOID RtlUsbNtBind(PUSBNT_BIND bind, PRTL_USBNT_TARGET target,
                  const USBNT_DEV_IF *ifc, ULONG id);
#endif
