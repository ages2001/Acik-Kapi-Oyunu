/*
 * backend_cplus.c - C+ descriptor backend for RTL8169/8168/8125/8126/8127.
 *
 * One descriptor engine for every Gigabit-and-faster Realtek part; the
 * 8125+ deltas (32-bit interrupt registers, relocated TX doorbell, the
 * 2.5/5/10G link bits) are handled through a->Is8125.
 * Copyright (C) 2026 ages2001.
 */
#include "rltklant.h"

#define CP_TX_RING_BYTES (RCP_NUM_TX_DESC * sizeof(RCP_DESC))
#define CP_RX_RING_BYTES (RCP_NUM_RX_DESC * sizeof(RCP_DESC))

static VOID CpImr(PRLTK_ADAPTER a, ULONG v)
{
    if (a->Is8125) RtlWrite32(a, RCP_IMR0_8125, v);
    else           RtlWrite16(a, RCP_IMR, (USHORT)v);
}
static ULONG CpIsrRead(PRLTK_ADAPTER a)
{
    return a->Is8125 ? RtlRead32(a, RCP_ISR0_8125) : (ULONG)RtlRead16(a, RCP_ISR);
}
static VOID CpIsrAck(PRLTK_ADAPTER a, ULONG v)
{
    if (a->Is8125) RtlWrite32(a, RCP_ISR0_8125, v);
    else           RtlWrite16(a, RCP_ISR, (USHORT)v);
}
static VOID CpTxPoll(PRLTK_ADAPTER a)
{
    if (a->Is8125) RtlWrite32(a, RCP_TPPOLL_8125, 1);
    else           RtlWrite8(a, RCP_TPPOLL, RCP_TPPOLL_NPQ);
}

/* Report completion separately from the PHY word: 0xffff may be data, but
   an all-ones bus read or a handshake timeout must never mean success. */
BOOLEAN RtlCpPhyOcpRead(PRLTK_ADAPTER a, USHORT address, USHORT *value)
{
    UINT i;
    *value = 0xFFFF;
    if (address & 1) return FALSE;
    RtlWrite32(a, 0xB8, (ULONG)(address >> 1) << 16);
    for (i = 0; i < 2000; i++) {
        ULONG v = RtlRead32(a, 0xB8);
        if (v == 0xFFFFFFFFUL) return FALSE;
        if (v & 0x80000000UL) { *value = (USHORT)v; return TRUE; }
        NdisStallExecution(20);
    }
    return FALSE;
}
BOOLEAN RtlCpPhyOcpWrite(PRLTK_ADAPTER a, USHORT address, USHORT value)
{
    UINT i;
    if (address & 1) return FALSE;
    RtlWrite32(a, 0xB8, 0x80000000UL | ((ULONG)(address >> 1) << 16) | value);
    for (i = 0; i < 2000; i++) {
        ULONG v = RtlRead32(a, 0xB8);
        if (v == 0xFFFFFFFFUL) return FALSE;
        if (!(v & 0x80000000UL)) return TRUE;
        NdisStallExecution(20);
    }
    return FALSE;
}
static BOOLEAN CpPhyAddress(PRLTK_ADAPTER a, UINT reg, USHORT *address)
{
    ULONG base=a->PhyPage?(ULONG)a->PhyPage<<4:0xA400;
    ULONG offset=reg*2;
    /* Page zero and explicit page 0xa40 share the standard MII window.
       Other pages use register 0x10 as their origin. Reject underflow and
       overflow instead of truncating an invalid address to another bank. */
    if(base!=0xA400){
        if(base+offset<32)return FALSE;
        base-=32;
    }
    base+=offset;
    if(base>0xFFFE)return FALSE;
    *address=(USHORT)base;return TRUE;
}

static BOOLEAN CpLegacyMdio(PRLTK_ADAPTER a, UINT reg, USHORT *value, BOOLEAN write)
{
    UINT i;
    ULONG gate = 0;
    BOOLEAN ok = FALSE;
    if (a->Flags & RTL_F_PHY_DP) {
        if (!write && reg == 3) { *value = 0xc912; return TRUE; }
        gate = RtlRead32(a, 0xD0);
        if (gate == 0xFFFFFFFFUL) return FALSE;
        RtlWrite32(a, 0xD0, gate & ~0x00020000UL);
    }
    RtlWrite32(a, RCP_PHYAR, ((ULONG)reg << 16) |
               (write ? RCP_PHYAR_FLAG | *value : 0));
    for (i = 0; i < 2000; i++) {
        ULONG v = RtlRead32(a, RCP_PHYAR);
        if (v == 0xFFFFFFFFUL) break;
        if (write ? !(v & RCP_PHYAR_FLAG) : !!(v & RCP_PHYAR_FLAG)) {
            if (!write) *value = (USHORT)v;
            ok = TRUE;
            break;
        }
        NdisStallExecution(20);
    }
    NdisStallExecution(20);
    if (a->Flags & RTL_F_PHY_DP) RtlWrite32(a, 0xD0, gate);
    return ok;
}
BOOLEAN RtlCpMdioRead(PRLTK_ADAPTER a, UINT reg, USHORT *value)
{
    USHORT address;
    *value = 0xFFFF;
    if (reg > 31) return FALSE;
    if ((a->Flags & RTL_F_PHY_OCP) || a->Is8125) {
        if (reg == 31) { *value = a->PhyPage == 0xA40 ? 0 : a->PhyPage; return TRUE; }
        if(!CpPhyAddress(a,reg,&address))return FALSE;
        return RtlCpPhyOcpRead(a,address,value);
    }
    return CpLegacyMdio(a, reg, value, FALSE);
}
BOOLEAN RtlCpMdioWrite(PRLTK_ADAPTER a, UINT reg, USHORT value)
{
    USHORT address;
    if (reg > 31) return FALSE;
    if ((a->Flags & RTL_F_PHY_OCP) || a->Is8125) {
        if (reg == 31) { if(value>0xFFF)return FALSE; a->PhyPage = value; return TRUE; }
        if(!CpPhyAddress(a,reg,&address))return FALSE;
        return RtlCpPhyOcpWrite(a,address,value);
    }
    return CpLegacyMdio(a, reg, &value, TRUE);
}

static BOOLEAN CpReset(PRLTK_ADAPTER a)
{
    UINT i;
    RtlWrite8(a, RCP_CR, RCP_CR_RST);
    for (i = 0; i < 1000; i++) {
        UCHAR command = RtlRead8(a, RCP_CR);
        if (command == 0xFF) return FALSE;
        if (!(command & (RCP_CR_RST | RCP_CR_TE | RCP_CR_RE))) return TRUE;
        NdisStallExecution(10);
    }
    return FALSE;
}

static VOID CpWriteMac(PRLTK_ADAPTER a)
{
    ULONG lo = a->CurrentAddr[0] | (a->CurrentAddr[1] << 8) |
               (a->CurrentAddr[2] << 16) | ((ULONG)a->CurrentAddr[3] << 24);
    ULONG hi = a->CurrentAddr[4] | (a->CurrentAddr[5] << 8);
    RtlWrite8(a, RCP_9346CR, 0xC0);             /* config write enable */
    RtlWrite32(a, RCP_IDR0, lo);
    RtlWrite32(a, RCP_IDR0 + 4, hi);
    RtlWrite8(a, RCP_9346CR, 0x00);
}

static VOID CpSetRxMode(PRLTK_ADAPTER a)
{
    ULONG rcr = RtlRead32(a, RCP_RCR) & ~0x3FUL;
    ULONG mar[2] = { 0, 0 };
    ULONG i;

    rcr |= RCP_RCR_MXDMA_UNL | RCP_RCR_RXFTH_UNL;
    if (a->PacketFilter & NDIS_PACKET_TYPE_DIRECTED)  rcr |= RCP_RCR_APM;
    if (a->PacketFilter & NDIS_PACKET_TYPE_BROADCAST) rcr |= RCP_RCR_AB;
    if (a->PacketFilter & NDIS_PACKET_TYPE_PROMISCUOUS) {
        rcr |= RCP_RCR_AAP | RCP_RCR_APM | RCP_RCR_AB | RCP_RCR_AM;
        mar[0] = mar[1] = 0xFFFFFFFFUL;
    } else if (a->PacketFilter & NDIS_PACKET_TYPE_ALL_MULTICAST) {
        rcr |= RCP_RCR_AM;
        mar[0] = mar[1] = 0xFFFFFFFFUL;
    } else if (a->PacketFilter & NDIS_PACKET_TYPE_MULTICAST) {
        rcr |= RCP_RCR_AM;
        for (i = 0; i < a->McastCount; i++) {
            ULONG bit = RtlCrc32Bits(a->McastList[i]);
            mar[bit >> 5] |= 1UL << (bit & 31);
        }
    }
    RtlWrite32(a, RCP_MAR0,     mar[0]);
    RtlWrite32(a, RCP_MAR0 + 4, mar[1]);
    RtlWrite32(a, RCP_RCR, rcr);
}

static VOID CpFreeDma(PRLTK_ADAPTER a);
static BOOLEAN CpInit(PRLTK_ADAPTER a)
{
    UINT i;
    static const UCHAR zero[6] = {0,0,0,0,0,0};

    if (!CpReset(a)) return FALSE;
    /* A preceding failed stop may have retained even a partial allocation.
       This successful reset finally makes those buffers safe to release. */
    CpFreeDma(a);
    if (!RtlPreparePci(a)) return FALSE;

    for (i = 0; i < 6; i++) a->PermanentAddr[i] = RtlRead8(a, RCP_IDR0 + i);
    if (memcmp(a->CurrentAddr, zero, 6) == 0)
        NdisMoveMemory(a->CurrentAddr, a->PermanentAddr, 6);

    if (!a->TxDesc) {
        NdisMAllocateSharedMemory(a->MiniportHandle, CP_TX_RING_BYTES, FALSE,
                                  (PVOID *)&a->TxDesc, &a->TxDescPhys);
        NdisMAllocateSharedMemory(a->MiniportHandle, CP_RX_RING_BYTES, FALSE,
                                  (PVOID *)&a->RxDesc, &a->RxDescPhys);
        if (!a->TxDesc || !a->RxDesc) return FALSE;
        for (i = 0; i < RCP_NUM_TX_DESC; i++) {
            NdisMAllocateSharedMemory(a->MiniportHandle, RCP_RX_BUF_SIZE, TRUE,
                                      (PVOID *)&a->CpTxBuf[i], &a->CpTxBufPhys[i]);
            if (!a->CpTxBuf[i]) return FALSE;
        }
        for (i = 0; i < RCP_NUM_RX_DESC; i++) {
            NdisMAllocateSharedMemory(a->MiniportHandle, RCP_RX_BUF_SIZE, TRUE,
                                      (PVOID *)&a->CpRxBuf[i], &a->CpRxBufPhys[i]);
            if (!a->CpRxBuf[i]) return FALSE;
        }
    }

    NdisZeroMemory(a->TxDesc, CP_TX_RING_BYTES);
    NdisZeroMemory(a->RxDesc, CP_RX_RING_BYTES);
    for (i = 0; i < RCP_NUM_RX_DESC; i++) {
        ULONG opts1 = RCP_DESC_OWN | RCP_RX_BUF_SIZE;
        if (i == RCP_NUM_RX_DESC - 1) opts1 |= RCP_DESC_EOR;
        a->RxDesc[i].opts1   = opts1;
        a->RxDesc[i].addr_lo = a->CpRxBufPhys[i].u.LowPart;
        a->RxDesc[i].addr_hi = a->CpRxBufPhys[i].u.HighPart;
    }
    a->TxDesc[RCP_NUM_TX_DESC - 1].opts1 = RCP_DESC_EOR;
    a->CpTxHead = a->CpTxTail = 0;
    a->CpRxCur = 0;
    a->PendingIsr = 0;

    RtlDmaBarrier();
    RtlWrite8(a, RCP_9346CR, 0xC0);
    /* The source MAC sequence set CPlusCmd for this exact revision. Do not
       overwrite it with RTL8139C+ TX/RX enable bits on Gigabit parts. */
    RtlWrite32(a, RCP_TNPDS,     a->TxDescPhys.u.LowPart);
    RtlWrite32(a, RCP_TNPDS + 4, a->TxDescPhys.u.HighPart);
    RtlWrite32(a, RCP_RDSAR,     a->RxDescPhys.u.LowPart);
    RtlWrite32(a, RCP_RDSAR + 4, a->RxDescPhys.u.HighPart);
    RtlWrite16(a, RCP_RMS, RCP_RX_BUF_SIZE);
    /* Commit both complete ring addresses before enabling bus-master DMA.
       A live cable must not let RX fetch reset/stale descriptor pointers. */
    if(RtlRead32(a,RCP_RDSAR)==0xFFFFFFFFUL)return FALSE;
    RtlWrite8(a, RCP_CR, RCP_CR_TE | RCP_CR_RE);
    RtlWrite32(a, RCP_TCR, 0x03000700);
    if (memcmp(a->CurrentAddr, a->PermanentAddr, 6) != 0) CpWriteMac(a);
    CpSetRxMode(a);
    RtlWrite8(a, RCP_9346CR, 0x00);

    CpIsrAck(a, 0xFFFFFFFFUL);
    CpImr(a, 0);
    return RtlConfigurePhy(a);
}

static VOID CpFreeDma(PRLTK_ADAPTER a)
{
    UINT i;
    if (a->TxDesc) { NdisMFreeSharedMemory(a->MiniportHandle, CP_TX_RING_BYTES, FALSE, a->TxDesc, a->TxDescPhys); a->TxDesc = NULL; }
    if (a->RxDesc) { NdisMFreeSharedMemory(a->MiniportHandle, CP_RX_RING_BYTES, FALSE, a->RxDesc, a->RxDescPhys); a->RxDesc = NULL; }
    for (i = 0; i < RCP_NUM_TX_DESC; i++) if (a->CpTxBuf[i]) { NdisMFreeSharedMemory(a->MiniportHandle, RCP_RX_BUF_SIZE, TRUE, a->CpTxBuf[i], a->CpTxBufPhys[i]); a->CpTxBuf[i] = NULL; }
    for (i = 0; i < RCP_NUM_RX_DESC; i++) if (a->CpRxBuf[i]) { NdisMFreeSharedMemory(a->MiniportHandle, RCP_RX_BUF_SIZE, TRUE, a->CpRxBuf[i], a->CpRxBufPhys[i]); a->CpRxBuf[i] = NULL; }
}
static VOID CpHalt(PRLTK_ADAPTER a)
{
    CpImr(a, 0);
    /* CR=0 alone does not acknowledge outstanding descriptor DMA. The
       pinned r8169 cleanup resets the MAC before clearing its rings. Require
       reset completion with both engines disabled before releasing memory.
       On an unresponsive/disappearing function retain the DMA allocations:
       leaking until reboot is safer than DMA into recycled kernel memory.
       A later Init must itself complete CpReset before reusing these rings. */
    if (!CpReset(a)) return;
    RtlDmaBarrier();
    CpFreeDma(a);
}

static BOOLEAN CpTransmit(PRLTK_ADAPTER a, PUCHAR frame, UINT len)
{
    ULONG idx = a->CpTxTail & (RCP_NUM_TX_DESC - 1);
    PRCP_DESC d = &a->TxDesc[idx];
    ULONG opts1;

    if ((a->CpTxTail - a->CpTxHead) >= RCP_NUM_TX_DESC) return FALSE;
    if (d->opts1 & RCP_DESC_OWN) return FALSE;
    RtlDmaBarrier(); /* do not overwrite a buffer before DMA releases it */
    if (len > RCP_RX_BUF_SIZE) return FALSE;

    NdisMoveMemory(a->CpTxBuf[idx], frame, len);
    d->addr_lo = a->CpTxBufPhys[idx].u.LowPart;
    d->addr_hi = a->CpTxBufPhys[idx].u.HighPart;
    d->opts2 = 0;
    opts1 = RCP_DESC_OWN | RCP_DESC_FS | RCP_DESC_LS | (len & RCP_DESC_FRAGLEN);
    if (idx == RCP_NUM_TX_DESC - 1) opts1 |= RCP_DESC_EOR;
    RtlDmaBarrier(); /* complete payload/address before publishing OWN */
    d->opts1 = opts1;
    a->CpTxTail++;
    RtlDmaBarrier(); /* publish OWN before the MMIO/port doorbell */
    CpTxPoll(a);
    return TRUE;
}

static VOID CpReclaimTx(PRLTK_ADAPTER a)
{
    while (a->CpTxHead != a->CpTxTail) {
        ULONG idx = a->CpTxHead & (RCP_NUM_TX_DESC - 1);
        if (a->TxDesc[idx].opts1 & RCP_DESC_OWN) break;
        a->CpTxHead++;
    }
}

static VOID CpReceive(PRLTK_ADAPTER a)
{
    UINT guard = RCP_NUM_RX_DESC * 2;
    while (guard--) {
        ULONG idx = a->CpRxCur & (RCP_NUM_RX_DESC - 1);
        PRCP_DESC d = &a->RxDesc[idx];
        ULONG opts1 = d->opts1;
        UINT framelen;

        if (opts1 & RCP_DESC_OWN) break;
        RtlDmaBarrier(); /* consume payload only after DMA releases OWN */
        framelen = opts1 & RCP_DESC_FRAGLEN;
        if ((opts1 & (RCP_DESC_FS | RCP_DESC_LS)) == (RCP_DESC_FS | RCP_DESC_LS) &&
            !(opts1 & RCP_RDESC_RES) && framelen >= RLTK_HEADER_SIZE + 4 && framelen <= RCP_RX_BUF_SIZE)
            RtlIndicateFrame(a, a->CpRxBuf[idx], framelen - 4);
        else
            a->RcvErr++;

        RtlDmaBarrier(); /* finish reading before returning the buffer */
        d->opts1 = RCP_DESC_OWN | RCP_RX_BUF_SIZE |
                   (idx == RCP_NUM_RX_DESC - 1 ? RCP_DESC_EOR : 0);
        a->CpRxCur++;
    }
    RtlIndicateComplete(a);
}

static BOOLEAN CpIsr(PRLTK_ADAPTER a)
{
    ULONG isr = CpIsrRead(a);
    /* A missing function returns all ones at the register's own width. */
    if (isr == 0 || isr == (a->Is8125 ? 0xFFFFFFFFUL : 0xFFFFUL)) return FALSE;
    CpImr(a, 0);
    CpIsrAck(a, isr);
    a->PendingIsr |= isr;
    return TRUE;
}

static VOID CpHandleInterrupt(PRLTK_ADAPTER a)
{
    ULONG isr = a->PendingIsr;
    a->PendingIsr = 0;
    if (isr & (RCP_INT_ROK | RCP_INT_RER | RCP_INT_RXFOVW | RCP_INT_RDU))
        CpReceive(a);
    if (isr & (RCP_INT_TOK | RCP_INT_TER | RCP_INT_TDU))
        CpReclaimTx(a);
}

static VOID CpEnableInts(PRLTK_ADAPTER a, BOOLEAN on)
{
    CpImr(a, on ? RCP_INT_DEFAULT : 0);
}

static BOOLEAN CpLinkState(PRLTK_ADAPTER a, PULONG mbit, PBOOLEAN fd)
{
    UCHAR ps = RtlRead8(a, RCP_PHYSTATUS);
    if (ps == 0xFF || !(ps & RCP_PHYS_LINKSTS)) {
        *mbit = 0; *fd = FALSE; return FALSE;
    }
    *fd = (ps & RCP_PHYS_FULLDUP) ? TRUE : FALSE;
    if (a->Is8125) {
        USHORT ps16 = RtlRead16(a, RCP_PHYSTATUS);
        if (ps16 == 0xFFFF) { *mbit = 0; *fd = FALSE; return FALSE; }
        if (ps16 & RCP_PHYS_10000M)     *mbit = 10000;
        else if (ps16 & RCP_PHYS_5000M) *mbit = 5000;
        else if (ps16 & RCP_PHYS_2500M) *mbit = 2500;
        else if (ps & RCP_PHYS_1000M)   *mbit = 1000;
        else if (ps & RCP_PHYS_100M)    *mbit = 100;
        else                            *mbit = 10;
    } else {
        if (ps & RCP_PHYS_1000M)        *mbit = 1000;
        else if (ps & RCP_PHYS_100M)    *mbit = 100;
        else                            *mbit = 10;
    }
    return TRUE;
}

RLTK_OPS RtlOpsCplus = {
    "C+", CpInit, CpHalt, CpSetRxMode, CpTransmit,
    CpIsr, CpHandleInterrupt, CpEnableInts, CpLinkState
};
