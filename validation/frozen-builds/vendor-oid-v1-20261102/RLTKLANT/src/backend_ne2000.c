/*
 * backend_ne2000.c - RTL8029(AS) NE2000-compatible backend (DP8390 core).
 * I/O mapped, 10 Mbit, programmed remote-DMA both directions.
 * Copyright (C) 2026 ages2001.
 */
#include "rltklant.h"

static VOID NePioRead(PRLTK_ADAPTER a, UINT src, PUCHAR dst, UINT len)
{
    UINT i;
    RtlWrite8(a, NE_CR, NE_CR_PAGE0 | NE_CR_RD2 | NE_CR_STA);
    RtlWrite8(a, NE_P0_RBCR0, (UCHAR)(len & 0xFF));
    RtlWrite8(a, NE_P0_RBCR1, (UCHAR)(len >> 8));
    RtlWrite8(a, NE_P0_RSAR0, (UCHAR)(src & 0xFF));
    RtlWrite8(a, NE_P0_RSAR1, (UCHAR)(src >> 8));
    RtlWrite8(a, NE_CR, NE_CR_PAGE0 | NE_CR_RD0 | NE_CR_STA);
    for (i = 0; i + 1 < len; i += 2) {
        USHORT w = RtlRead16(a, NE_DATA);
        dst[i] = (UCHAR)w; dst[i + 1] = (UCHAR)(w >> 8);
    }
    if (len & 1) dst[len - 1] = (UCHAR)RtlRead16(a, NE_DATA);
    RtlWrite8(a, NE_P0_ISR, NE_ISR_RDC);
}

static VOID NePioWrite(PRLTK_ADAPTER a, UINT dstpage, const UCHAR *src, UINT len)
{
    UINT i, addr = dstpage * NE_PAGE_SIZE;
    RtlWrite8(a, NE_CR, NE_CR_PAGE0 | NE_CR_RD2 | NE_CR_STA);
    RtlWrite8(a, NE_P0_ISR, NE_ISR_RDC);
    RtlWrite8(a, NE_P0_RBCR0, (UCHAR)(len & 0xFF));
    RtlWrite8(a, NE_P0_RBCR1, (UCHAR)(len >> 8));
    RtlWrite8(a, NE_P0_RSAR0, (UCHAR)(addr & 0xFF));
    RtlWrite8(a, NE_P0_RSAR1, (UCHAR)(addr >> 8));
    RtlWrite8(a, NE_CR, NE_CR_PAGE0 | NE_CR_RD1 | NE_CR_STA);
    for (i = 0; i + 1 < len; i += 2)
        RtlWrite16(a, NE_DATA, (USHORT)(src[i] | (src[i + 1] << 8)));
    if (len & 1) RtlWrite16(a, NE_DATA, src[len - 1]);
    for (i = 0; i < 1000; i++) { if (RtlRead8(a, NE_P0_ISR) & NE_ISR_RDC) break; NdisStallExecution(10); }
    RtlWrite8(a, NE_P0_ISR, NE_ISR_RDC);
}

static VOID NeSetRxMode(PRLTK_ADAPTER a)
{
    UCHAR rcr = 0;
    if (a->PacketFilter & NDIS_PACKET_TYPE_BROADCAST) rcr |= NE_RCR_AB;
    if (a->PacketFilter & (NDIS_PACKET_TYPE_ALL_MULTICAST | NDIS_PACKET_TYPE_MULTICAST)) rcr |= NE_RCR_AM;
    if (a->PacketFilter & NDIS_PACKET_TYPE_PROMISCUOUS) rcr |= NE_RCR_PRO | NE_RCR_AM | NE_RCR_AB;
    RtlWrite8(a, NE_CR, NE_CR_PAGE0 | NE_CR_RD2 | NE_CR_STA);
    RtlWrite8(a, NE_P0_RCR, rcr);
    RtlWrite8(a, NE_CR, NE_CR_PAGE1 | NE_CR_RD2 | NE_CR_STA);
    { UINT i; for (i = 0; i < 8; i++) RtlWrite8(a, NE_P1_MAR0 + i, 0xFF); }
    RtlWrite8(a, NE_CR, NE_CR_PAGE0 | NE_CR_RD2 | NE_CR_STA);
}

static BOOLEAN NeReset(PRLTK_ADAPTER a)
{
    UINT i;
    RtlWrite8(a, NE_RESET, RtlRead8(a, NE_RESET));          /* reset pulse */
    for (i = 0; i < 1000; i++) {
        UCHAR isr = RtlRead8(a, NE_P0_ISR);
        if (isr == 0xFF) return FALSE;
        if (isr & NE_ISR_RST) return TRUE;
        NdisStallExecution(10);
    }
    return FALSE;
}

static BOOLEAN NeInit(PRLTK_ADAPTER a)
{
    UINT i;
    UCHAR prom[32];
    static const UCHAR zero[6] = {0,0,0,0,0,0};
    if (!NeReset(a)) return FALSE;

    RtlWrite8(a, NE_CR, NE_CR_PAGE0 | NE_CR_RD2 | NE_CR_STP);
    RtlWrite8(a, NE_P0_DCR, NE_DCR_DEFAULT);
    RtlWrite8(a, NE_P0_RBCR0, 0); RtlWrite8(a, NE_P0_RBCR1, 0);
    RtlWrite8(a, NE_P0_RCR, 0x20);                          /* monitor */
    RtlWrite8(a, NE_P0_TCR, NE_TCR_LOOPBACK);
    RtlWrite8(a, NE_P0_TPSR, NE_TX_START);
    RtlWrite8(a, NE_P0_PSTART, NE_RX_START);
    RtlWrite8(a, NE_P0_BNRY, NE_RX_START);
    RtlWrite8(a, NE_P0_PSTOP, NE_RX_STOP);
    RtlWrite8(a, NE_P0_ISR, 0xFF);
    RtlWrite8(a, NE_P0_IMR, 0);

    NePioRead(a, 0, prom, 32);                              /* PROM: MAC even bytes */
    for (i = 0; i < 6; i++) a->PermanentAddr[i] = prom[i * 2];
    if (memcmp(a->CurrentAddr, zero, 6) == 0)
        NdisMoveMemory(a->CurrentAddr, a->PermanentAddr, 6);

    RtlWrite8(a, NE_CR, NE_CR_PAGE1 | NE_CR_RD2 | NE_CR_STP);
    for (i = 0; i < 6; i++) RtlWrite8(a, NE_P1_PAR0 + i, a->CurrentAddr[i]);
    for (i = 0; i < 8; i++) RtlWrite8(a, NE_P1_MAR0 + i, 0xFF);
    RtlWrite8(a, NE_P1_CURR, NE_RX_START + 1);
    a->NeNext = NE_RX_START + 1;

    RtlWrite8(a, NE_CR, NE_CR_PAGE0 | NE_CR_RD2 | NE_CR_STA);
    RtlWrite8(a, NE_P0_TCR, 0);
    NeSetRxMode(a);
    a->PendingIsr = 0;
    return TRUE;
}

static VOID NeHalt(PRLTK_ADAPTER a)
{
    RtlWrite8(a, NE_P0_IMR, 0);
    RtlWrite8(a, NE_CR, NE_CR_PAGE0 | NE_CR_RD2 | NE_CR_STP);
}

static BOOLEAN NeTransmit(PRLTK_ADAPTER a, PUCHAR frame, UINT len)
{
    if (RtlRead8(a, NE_CR) & NE_CR_TXP) return FALSE;
    NePioWrite(a, NE_TX_START, frame, len);
    RtlWrite8(a, NE_CR, NE_CR_PAGE0 | NE_CR_RD2 | NE_CR_STA);
    RtlWrite8(a, NE_P0_TPSR, NE_TX_START);
    RtlWrite8(a, NE_P0_TBCR0, (UCHAR)(len & 0xFF));
    RtlWrite8(a, NE_P0_TBCR1, (UCHAR)(len >> 8));
    RtlWrite8(a, NE_CR, NE_CR_PAGE0 | NE_CR_RD2 | NE_CR_TXP | NE_CR_STA);
    return TRUE;
}

/* Re-seed the receive ring (used on a detected ring corruption). */
static VOID NeResetRing(PRLTK_ADAPTER a)
{
    RtlWrite8(a, NE_CR, NE_CR_PAGE0 | NE_CR_RD2 | NE_CR_STP);
    RtlWrite8(a, NE_P0_BNRY, NE_RX_START);
    RtlWrite8(a, NE_CR, NE_CR_PAGE1 | NE_CR_RD2 | NE_CR_STP);
    RtlWrite8(a, NE_P1_CURR, NE_RX_START + 1);
    a->NeNext = NE_RX_START + 1;
    RtlWrite8(a, NE_CR, NE_CR_PAGE0 | NE_CR_RD2 | NE_CR_STA);
}

static VOID NeReceive(PRLTK_ADAPTER a)
{
    UCHAR curr, bnry, hdr[4];
    UINT guard = 128;
    while (guard--) {
        UCHAR  status, next;
        USHORT rxlen;

        RtlWrite8(a, NE_CR, NE_CR_PAGE1 | NE_CR_RD2 | NE_CR_STA);
        curr = RtlRead8(a, NE_P1_CURR);
        RtlWrite8(a, NE_CR, NE_CR_PAGE0 | NE_CR_RD2 | NE_CR_STA);
        if (a->NeNext == curr) break;                 /* ring empty */

        /* our read pointer must stay inside the ring */
        if (a->NeNext < NE_RX_START || a->NeNext >= NE_RX_STOP) { NeResetRing(a); break; }

        NePioRead(a, a->NeNext * NE_PAGE_SIZE, hdr, 4);
        status = hdr[0];
        next   = hdr[1];
        rxlen  = (USHORT)(hdr[2] | (hdr[3] << 8));

        /* the next-page link the NIC wrote must also be inside the ring;
           a bad link means the ring state is corrupt - re-seed and stop */
        if (next < NE_RX_START || next >= NE_RX_STOP) { NeResetRing(a); break; }

        if ((status & 0x01) && rxlen >= RLTK_HEADER_SIZE + 4 && rxlen <= RLTK_MAX_FRAME + 8) {
            /* rxlen counts the 4-byte DP8390 header; strip only that. */
            UINT dlen = rxlen - 4;
            if (dlen > RLTK_MAX_FRAME) dlen = RLTK_MAX_FRAME;
            NePioRead(a, a->NeNext * NE_PAGE_SIZE + 4, a->RxCopy, dlen);
            RtlIndicateFrame(a, a->RxCopy, dlen);
        } else {
            a->RcvErr++;
        }
        a->NeNext = next;
        bnry = (UCHAR)(next - 1);
        if (bnry < NE_RX_START) bnry = NE_RX_STOP - 1;
        RtlWrite8(a, NE_P0_BNRY, bnry);
    }
    RtlIndicateComplete(a);
}

static BOOLEAN NeIsr(PRLTK_ADAPTER a)
{
    UCHAR isr;
    if (RtlRead8(a, NE_CR) == 0xFF) return FALSE;
    RtlWrite8(a, NE_CR, NE_CR_PAGE0 | NE_CR_RD2 | NE_CR_STA);
    isr = RtlRead8(a, NE_P0_ISR);
    if (isr == 0 || isr == 0xFF) return FALSE;
    RtlWrite8(a, NE_P0_IMR, 0);
    RtlWrite8(a, NE_P0_ISR, isr);
    a->PendingIsr |= isr;
    return TRUE;
}

/* DP8390 receive-ring overflow recovery (datasheet sequence). */
static VOID NeOverflow(PRLTK_ADAPTER a)
{
    UINT i;
    BOOLEAN resend;
    UCHAR cr = RtlRead8(a, NE_CR);

    RtlWrite8(a, NE_CR, NE_CR_PAGE0 | NE_CR_RD2 | NE_CR_STP);
    for (i = 0; i < 200; i++) NdisStallExecution(10);      /* ~2 ms */
    RtlWrite8(a, NE_P0_RBCR0, 0);
    RtlWrite8(a, NE_P0_RBCR1, 0);
    resend = (cr & NE_CR_TXP) && !(RtlRead8(a, NE_P0_ISR) & (NE_ISR_PTX | NE_ISR_TXE));
    RtlWrite8(a, NE_P0_TCR, NE_TCR_LOOPBACK);              /* loopback */
    RtlWrite8(a, NE_CR, NE_CR_PAGE0 | NE_CR_RD2 | NE_CR_STA);
    NeReceive(a);
    RtlWrite8(a, NE_P0_ISR, NE_ISR_OVW);
    RtlWrite8(a, NE_P0_TCR, 0);                            /* normal tx */
    if (resend) RtlWrite8(a, NE_CR, NE_CR_PAGE0 | NE_CR_RD2 | NE_CR_TXP | NE_CR_STA);
}

static VOID NeHandleInterrupt(PRLTK_ADAPTER a)
{
    ULONG isr = a->PendingIsr;
    a->PendingIsr = 0;
    if (isr & NE_ISR_OVW)              NeOverflow(a);       /* must run first */
    else if (isr & (NE_ISR_PRX | NE_ISR_RXE)) NeReceive(a);
    if (isr & NE_ISR_TXE) a->XmitErr++;
}

static VOID NeEnableInts(PRLTK_ADAPTER a, BOOLEAN on)
{
    RtlWrite8(a, NE_CR, NE_CR_PAGE0 | NE_CR_RD2 | NE_CR_STA);
    RtlWrite8(a, NE_P0_IMR, on ? (NE_ISR_PRX | NE_ISR_PTX | NE_ISR_RXE | NE_ISR_TXE | NE_ISR_OVW) : 0);
}

static BOOLEAN NeLinkState(PRLTK_ADAPTER a, PULONG mbit, PBOOLEAN fd)
{
    /* This controller has no PHY link-status register. We can still reject
     * an absent I/O function rather than reporting it as connected. */
    *fd = FALSE;
    if (RtlRead8(a, NE_CR) == 0xFF) { *mbit = 0; return FALSE; }
    *mbit = 10; return TRUE;
}

RLTK_OPS RtlOpsNe2000 = {
    "NE2000", NeInit, NeHalt, NeSetRxMode, NeTransmit,
    NeIsr, NeHandleInterrupt, NeEnableInts, NeLinkState
};
