/*
 * rtl8150.c - Realtek RTL8150 (USB 1.1 10/100) legacy protocol layer
 *             (see rtl8150.h).
 *
 * Copyright (C) 2026 ages2001.
 */
#include "rtl8150.h"

/*
 * The RTL8150 register file is reached with the same vendor REGS request
 * as the r8152 family, but with the register index placed directly in
 * wValue and wIndex held at 0 (no PLA/USB block select, no byte-enable).
 */
static NDIS_STATUS GetRegs(PRTL_USB_NIC n, USHORT idx, PVOID buf, USHORT len)
{
    return n->Pipe.Control(&n->Pipe, RTLU_RT_READ, RTLU_REQ_GET_REGS,
                           idx, 0, buf, len);
}

static NDIS_STATUS SetRegs(PRTL_USB_NIC n, USHORT idx, PVOID buf, USHORT len)
{
    return n->Pipe.Control(&n->Pipe, RTLU_RT_WRITE, RTLU_REQ_SET_REGS,
                           idx, 0, buf, len);
}

static NDIS_STATUS SetByte(PRTL_USB_NIC n, USHORT idx, UCHAR v)
{
    return SetRegs(n, idx, &v, 1);
}

NDIS_STATUS Rtl8150Reset(PRTL_USB_NIC n)
{
    NDIS_STATUS st;
    UCHAR cr;
    ULONG spins;

    n->Running = FALSE;
    st = SetByte(n, R8150_CR, R8150_CR_RST);
    if (!USB_OK(st)) return st;

    /* the reset bit clears itself when the core has finished resetting */
    for (spins = 0; spins < 1000; spins++) {
        st = GetRegs(n, R8150_CR, &cr, 1);
        if (!USB_OK(st)) return st;
        if (!(cr & R8150_CR_RST)) return NDIS_STATUS_SUCCESS;
    }
    return NDIS_STATUS_FAILURE;
}

NDIS_STATUS Rtl8150ReadMac(PRTL_USB_NIC n)
{
    return GetRegs(n, R8150_IDR, n->Mac, 6);
}

NDIS_STATUS Rtl8150Start(PRTL_USB_NIC n)
{
    NDIS_STATUS st;
    UCHAR rcr;

    n->Running = FALSE;
    /* Linux enable_net_traffic: append RX status, configure TX, then enable.
       The byte write preserves the adjacent RCR byte. */
    n->RcrFlags = R8150_RCR_DEFAULT;
    rcr = (UCHAR)R8150_RCR_DEFAULT;
    st = SetRegs(n, R8150_RCR, &rcr, 1);
    if (!USB_OK(st)) return st;
    st = SetByte(n, R8150_TCR, 0xD8);
    if (!USB_OK(st)) return st;

    st = SetByte(n, R8150_CR, (UCHAR)(R8150_CR_RE | R8150_CR_TE));
    if (USB_OK(st)) n->Running = TRUE;
    return st;
}

NDIS_STATUS Rtl8150Stop(PRTL_USB_NIC n)
{
    UCHAR cr;
    NDIS_STATUS st;
    n->Running = FALSE;
    st = GetRegs(n, R8150_CR, &cr, 1);
    if (!USB_OK(st)) return st;
    if (cr == 0xFF) return NDIS_STATUS_FAILURE;
    return SetByte(n, R8150_CR, (UCHAR)(cr & ~(R8150_CR_RE | R8150_CR_TE)));
}

ULONG Rtl8150BuildTx(PUCHAR frame, ULONG len, PUCHAR out)
{
    ULONG i, count = len < 60 ? 60 : len;
    for (i = 0; i < len; i++) out[i] = frame[i];

    /* Match Linux rtl8150_start_xmit: pad to ETH_ZLEN, then ensure a
       terminating short USB packet when the count is a multiple of 64. */
    if ((count % R8150_FS_MAXPKT) == 0) count++;
    for (i = len; i < count; i++) out[i] = 0;
    return count;
}

ULONG Rtl8150NextRx(PUCHAR buf, ULONG bufLen, PUCHAR *frame, PULONG frameLen)
{
    *frame = 0; *frameLen = 0;

    /* one frame per bulk-IN buffer, followed by a 4-byte receive status */
    if (bufLen <= R8150_RX_STATUS_SIZE) return 0;

    *frame    = buf;
    *frameLen = bufLen - R8150_RX_STATUS_SIZE;
    return 0;                    /* RTL8150 never coalesces packets */
}
