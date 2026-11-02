/*
 * backend_8139.c - RTL8129 / RTL8139 / RTL810x register-ring backend.
 * Copyright (C) 2026 ages2001.
 */
#include "rltklant.h"

/* WRAP=1: the chip writes a boundary-crossing frame contiguously past the
   ring end, so reserve 1.5 KB of slack and never split a frame. */
#define RX_ALLOC  (R8139_RXBUF_LEN + 16 + 1536)
#define TX_ALLOC  1536

static BOOLEAN R39Reset(PRLTK_ADAPTER a)
{
    UINT i;
    RtlWrite8(a, R8139_CR, R8139_CR_RST);
    for (i = 0; i < 1000; i++) {
        UCHAR cr = RtlRead8(a, R8139_CR);
        if (cr == 0xFF) return FALSE;
        if (!(cr & (R8139_CR_RST | R8139_CR_TE | R8139_CR_RE))) return TRUE;
        NdisStallExecution(10);
    }
    return FALSE;
}

static VOID R39WriteMac(PRLTK_ADAPTER a)
{
    ULONG lo = a->CurrentAddr[0] | (a->CurrentAddr[1] << 8) |
               (a->CurrentAddr[2] << 16) | ((ULONG)a->CurrentAddr[3] << 24);
    ULONG hi = a->CurrentAddr[4] | (a->CurrentAddr[5] << 8);
    RtlWrite8(a, R8139_9346CR, R8139_9346_EEM_CONFIG);
    RtlWrite32(a, R8139_IDR0, lo);
    RtlWrite32(a, R8139_IDR0 + 4, hi);
    RtlWrite8(a, R8139_9346CR, R8139_9346_EEM_NORMAL);
}

static VOID R39SetRxMode(PRLTK_ADAPTER a)
{
    ULONG rcr = R8139_RCR_RBLEN_32K | R8139_RCR_MXDMA_UNL | R8139_RCR_RXFTH_NO | R8139_RCR_WRAP;
    ULONG mar[2] = { 0, 0 };
    ULONG i;

    if (a->PacketFilter & NDIS_PACKET_TYPE_DIRECTED)  rcr |= R8139_RCR_APM;
    if (a->PacketFilter & NDIS_PACKET_TYPE_BROADCAST) rcr |= R8139_RCR_AB;
    if (a->PacketFilter & NDIS_PACKET_TYPE_PROMISCUOUS) {
        rcr |= R8139_RCR_AAP | R8139_RCR_APM | R8139_RCR_AB | R8139_RCR_AM;
        mar[0] = mar[1] = 0xFFFFFFFFUL;
    } else if (a->PacketFilter & NDIS_PACKET_TYPE_ALL_MULTICAST) {
        rcr |= R8139_RCR_AM;
        mar[0] = mar[1] = 0xFFFFFFFFUL;
    } else if (a->PacketFilter & NDIS_PACKET_TYPE_MULTICAST) {
        rcr |= R8139_RCR_AM;
        for (i = 0; i < a->McastCount; i++) {
            ULONG bit = RtlCrc32Bits(a->McastList[i]);
            mar[bit >> 5] |= 1UL << (bit & 31);
        }
    }
    RtlWrite32(a, R8139_MAR0,     mar[0]);
    RtlWrite32(a, R8139_MAR0 + 4, mar[1]);
    RtlWrite32(a, R8139_RCR, rcr);
}

static VOID R39FreeDma(PRLTK_ADAPTER a);
static BOOLEAN R39Init(PRLTK_ADAPTER a)
{
    UINT i;
    static const UCHAR zero[6] = {0,0,0,0,0,0};

    if (!R39Reset(a)) return FALSE;
    R39FreeDma(a); /* reclaim any partial allocation retained by a failed stop */
    RtlWrite8(a, R8139_CONFIG1, 0x00);              /* power on, LWAKE off */

    /* A soft reset may preserve the configured IDR station address. */
    if (memcmp(a->PermanentAddr, zero, 6) == 0)
        for (i = 0; i < 6; i++) a->PermanentAddr[i] = RtlRead8(a, R8139_IDR0 + i);
    if (memcmp(a->CurrentAddr, zero, 6) == 0)
        NdisMoveMemory(a->CurrentAddr, a->PermanentAddr, 6);
    else
        R39WriteMac(a);

    if (!a->RxRing) {
        NdisMAllocateSharedMemory(a->MiniportHandle, RX_ALLOC, TRUE,
                                  (PVOID *)&a->RxRing, &a->RxRingPhys);
        if (!a->RxRing) return FALSE;
        for (i = 0; i < R8139_TX_DESCS; i++) {
            NdisMAllocateSharedMemory(a->MiniportHandle, TX_ALLOC, TRUE,
                                      (PVOID *)&a->TxBuf[i], &a->TxBufPhys[i]);
            if (!a->TxBuf[i]) return FALSE;
        }
    }
    a->RxOffset = 0;
    a->TxCur = a->TxDirty = 0;
    a->PendingIsr = 0;
    a->RxRecoveryNeeded = FALSE;

    RtlWrite32(a, R8139_RBSTART, a->RxRingPhys.u.LowPart);
    RtlWrite32(a, R8139_TCR, R8139_TCR_MXDMA_2048 | R8139_TCR_IFG_STD);
    for (i = 0; i < R8139_TX_DESCS; i++)
        RtlWrite32(a, R8139_TSAD0 + i * 4, a->TxBufPhys[i].u.LowPart);
    R39SetRxMode(a);
    RtlWrite16(a, R8139_CAPR, (USHORT)(0 - 16));
    RtlDmaBarrier();
    if (RtlRead32(a, R8139_RBSTART) == 0xFFFFFFFFUL) return FALSE;
    RtlWrite8(a, R8139_CR, R8139_CR_TE | R8139_CR_RE);
    RtlWrite16(a, R8139_IMR, 0);                    /* EnableInts arms it */
    RtlWrite16(a, R8139_ISR, 0xFFFF);

    Rtl8139ConfigureMedia(a);
    return TRUE;
}

static VOID R39FreeDma(PRLTK_ADAPTER a)
{
    UINT i;
    if (a->RxRing) {
        NdisMFreeSharedMemory(a->MiniportHandle, RX_ALLOC, TRUE, a->RxRing, a->RxRingPhys);
        a->RxRing = NULL;
    }
    for (i = 0; i < R8139_TX_DESCS; i++) if (a->TxBuf[i]) {
        NdisMFreeSharedMemory(a->MiniportHandle, TX_ALLOC, TRUE, a->TxBuf[i], a->TxBufPhys[i]);
        a->TxBuf[i] = NULL;
    }
}

static VOID R39Halt(PRLTK_ADAPTER a)
{
    RtlWrite16(a, R8139_IMR, 0);
    /* Do not recycle buffers while an unacknowledged DMA engine can use them. */
    if (!R39Reset(a)) return;
    RtlDmaBarrier();
    R39FreeDma(a);
}

static BOOLEAN R39Transmit(PRLTK_ADAPTER a, PUCHAR frame, UINT len)
{
    ULONG slot;
    if ((a->TxCur - a->TxDirty) >= R8139_TX_DESCS) return FALSE;   /* full */
    slot = a->TxCur & (R8139_TX_DESCS - 1);
    if (len > TX_ALLOC) len = TX_ALLOC;
    NdisMoveMemory(a->TxBuf[slot], frame, len);
    RtlWrite32(a, R8139_TSD0 + slot * 4, (R8139_TX_FIFO_THRESH << 11) | (len & R8139_TSD_SIZE_MASK));
    a->TxCur++;
    return TRUE;
}

static VOID R39ReclaimTx(PRLTK_ADAPTER a)
{
    while (a->TxDirty != a->TxCur) {
        ULONG slot = a->TxDirty & (R8139_TX_DESCS - 1);
        ULONG tsd = RtlRead32(a, R8139_TSD0 + slot * 4);
        if (!(tsd & (R8139_TSD_TOK | R8139_TSD_TUN))) break;        /* in flight */
        if (tsd & R8139_TSD_TUN) a->XmitErr++;
        a->TxDirty++;
    }
}

static VOID R39Receive(PRLTK_ADAPTER a)
{
    UINT guard = 256;
    while (guard-- && !(RtlRead8(a, R8139_CR) & R8139_CR_BUFE)) {
        PUCHAR p = a->RxRing + a->RxOffset;
        USHORT status = (USHORT)(p[0] | (p[1] << 8));
        USHORT rxlen  = (USHORT)(p[2] | (p[3] << 8));

        if (rxlen == 0xFFF0) break;                 /* still DMAing */
        if (!(status & R8139_RX_ROK) || rxlen < RLTK_HEADER_SIZE + 4 ||
            rxlen > RLTK_MAX_FRAME + 4) {
            a->RcvErr++;
            /* This is a DPC: freeing/reallocating shared DMA here is illegal
               on legacy NT. Leave the interrupt masked and ask NDIS to call
               Reset through CheckForHang; the passive owner joins this DPC. */
            a->RxRecoveryNeeded = TRUE;
            RtlIndicateComplete(a);
            return;
        }
        RtlIndicateFrame(a, p + 4, rxlen - 4);      /* strip 4-byte CRC */

        a->RxOffset = (a->RxOffset + rxlen + 4 + 3) & ~3UL;
        if (a->RxOffset >= R8139_RXBUF_LEN) a->RxOffset -= R8139_RXBUF_LEN;
        RtlWrite16(a, R8139_CAPR, (USHORT)(a->RxOffset - 16));
    }
    RtlIndicateComplete(a);
}

static BOOLEAN R39Isr(PRLTK_ADAPTER a)
{
    USHORT isr = RtlRead16(a, R8139_ISR);
    if (isr == 0 || isr == 0xFFFF) return FALSE;
    RtlWrite16(a, R8139_IMR, 0);                    /* mask until DPC done */
    RtlWrite16(a, R8139_ISR, isr);                  /* ack */
    a->PendingIsr |= isr;
    return TRUE;
}

static VOID R39HandleInterrupt(PRLTK_ADAPTER a)
{
    ULONG isr = a->PendingIsr;
    a->PendingIsr = 0;
    if (a->RxRecoveryNeeded) return;
    if (isr & (R8139_INT_ROK | R8139_INT_RER | R8139_INT_RXOVW | R8139_INT_FOVW))
        R39Receive(a);
    if (isr & (R8139_INT_TOK | R8139_INT_TER))
        R39ReclaimTx(a);
}

static VOID R39EnableInts(PRLTK_ADAPTER a, BOOLEAN on)
{
    RtlWrite16(a, R8139_IMR, on ? R8139_INT_DEFAULT : 0);
}

static BOOLEAN R39LinkState(PRLTK_ADAPTER a, PULONG mbit, PBOOLEAN fd)
{
    UCHAR msr = RtlRead8(a, R8139_MSR);
    USHORT bmcr;
    if (msr & R8139_MSR_LINKB) { *mbit = 0; *fd = FALSE; return FALSE; }
    *mbit = (msr & R8139_MSR_SPEED_10) ? 10 : 100;
    bmcr = RtlRead16(a, R8139_BMCR);
    *fd = (bmcr & 0x0100) ? TRUE : FALSE;           /* BMCR.DUPLEX */
    return TRUE;
}

RLTK_OPS RtlOps8139 = {
    "RTL8139", R39Init, R39Halt, R39SetRxMode, R39Transmit,
    R39Isr, R39HandleInterrupt, R39EnableInts, R39LinkState
};
