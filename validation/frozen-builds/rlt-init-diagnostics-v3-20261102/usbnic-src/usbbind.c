/* Abstract USB transfer binding for the Realtek protocol layer.
 * The implemented USBNT path calls USBNT_DEV_IF through usbcli.c.
 * NT5 native URBs are implemented by RLTKLANT/src/native_urb.c; these
 * callbacks preserve the same protocol interface for both transports.
 * Copyright (C) 2026 ages2001.
 */
#include "usbio.h"
#include "usbchip.h"
#include "usbbind.h"

static VOID BuildSetup(PUCHAR s, UCHAR rt, UCHAR req,
                       USHORT val, USHORT idx, USHORT len)
{
    s[0] = rt;  s[1] = req;
    s[2] = (UCHAR)val;  s[3] = (UCHAR)(val >> 8);
    s[4] = (UCHAR)idx;  s[5] = (UCHAR)(idx >> 8);
    s[6] = (UCHAR)len;  s[7] = (UCHAR)(len >> 8);
}

static NDIS_STATUS BindControl(PUSBNT_PIPE pipe, UCHAR rt, UCHAR req,
                               USHORT val, USHORT idx, PVOID buf, USHORT len)
{
    PUSBNT_BIND b = (PUSBNT_BIND)pipe->HcContext;
    UCHAR setup[8];
    ULONG done = 0;
    NDIS_STATUS st;
    BuildSetup(setup, rt, req, val, idx, len);
    st = b->Submit(b->DevObject, 0, 0, setup, buf, len, &done);
    if (USB_OK(st) && done != len) return NDIS_STATUS_FAILURE;
    return st;
}

static NDIS_STATUS BindBulk(PUSBNT_PIPE pipe, UCHAR ep,
                            PVOID buf, ULONG len, PULONG done)
{
    PUSBNT_BIND b = (PUSBNT_BIND)pipe->HcContext;
    return b->Submit(b->DevObject, 1, ep, 0, buf, len, done);
}

/* Wire a pipe to the device object + submit primitive of the active stack. */
VOID UsbntBindPipe(PUSBNT_PIPE pipe, PUSBNT_BIND bind)
{
    pipe->HcContext = bind;
    pipe->Control   = BindControl;
    pipe->Bulk      = BindBulk;
}
