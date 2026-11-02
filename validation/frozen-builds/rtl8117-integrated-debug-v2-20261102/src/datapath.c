/*
 * datapath.c - NDIS send / transfer-data / interrupt / request handlers.
 * Copyright (C) 2026 ages2001.
 */
#include "ndismac.h"
#include "ndisvendor.h"

/* ------------------------------------------------------------------ */
/*  Transmit                                                          */
/* ------------------------------------------------------------------ */
static UINT RtlFlatten(PNDIS_PACKET pkt, PUCHAR dst, UINT cap)
{
    PNDIS_BUFFER b = pkt->Private.Head;
    UINT total = 0;
    while (b) {
        PVOID va = NULL; UINT len = 0;
        NdisQueryBuffer(b, &va, &len);
        /* A partial Ethernet frame is not a successful send. Inspect the
           entire MDL chain, including segments following an exact-capacity
           prefix, before exposing any bytes to the backend. */
        if (len > cap - total || (!va && len)) return cap + 1;
        if (len) {
            NdisMoveMemory(dst + total, va, len);
            total += len;
        }
        b = RtlBufNext(b);
    }
    return total;
}

NDIS_STATUS STDC RtlSend(NDIS_HANDLE ctx, PNDIS_PACKET pkt, UINT flags)
{
    PRLTK_ADAPTER a = (PRLTK_ADAPTER)ctx;
    UCHAR frame[RLTK_MAX_FRAME + 4];
    UINT len;
    (void)flags;

    NdisAcquireSpinLock(&a->Lock);
    if (a->Halting || a->ResetPending || a->ResetFailed || a->RxRecoveryNeeded) {
        NdisReleaseSpinLock(&a->Lock);
        return NDIS_STATUS_RESOURCES;
    }
    NdisReleaseSpinLock(&a->Lock);

    len = RtlFlatten(pkt, frame, RLTK_MAX_FRAME);
    if (len < RLTK_HEADER_SIZE || len > RLTK_MAX_FRAME) { a->XmitErr++; return NDIS_STATUS_INVALID_DATA; }
    if (len < 60) { NdisZeroMemory(frame + len, 60 - len); len = 60; }

    if (!a->Ops->Transmit(a, frame, len)) {
        a->TxStalled = TRUE;
        return NDIS_STATUS_RESOURCES;       /* NDIS queues and retries */
    }
    a->XmitOk++;
    return NDIS_STATUS_SUCCESS;             /* copied: complete inline */
}

/* ------------------------------------------------------------------ */
/*  TransferData: copy the rest of the frame being indicated          */
/* ------------------------------------------------------------------ */
NDIS_STATUS STDC
RtlTransferData(PNDIS_PACKET pkt, PUINT transferred, NDIS_HANDLE ctx,
                NDIS_HANDLE rxctx, UINT offset, UINT want)
{
    PRLTK_ADAPTER a = (PRLTK_ADAPTER)ctx;
    PNDIS_BUFFER b;
    PUCHAR src;
    UINT avail, done = 0;
    (void)rxctx;

    *transferred = 0;
    if (!a->RxFrame || a->RxFrameLen < RLTK_HEADER_SIZE) return NDIS_STATUS_FAILURE;
    avail = a->RxFrameLen - RLTK_HEADER_SIZE;
    if (offset >= avail) return NDIS_STATUS_SUCCESS;
    if (want > avail - offset) want = avail - offset;
    src = a->RxFrame + RLTK_HEADER_SIZE + offset;

    for (b = pkt->Private.Head; b && done < want; b = RtlBufNext(b)) {
        PVOID va = NULL; UINT len = 0;
        NdisQueryBuffer(b, &va, &len);
        if (!va || !len) continue;
        if (len > want - done) len = want - done;
        NdisMoveMemory(va, src + done, len);
        done += len;
    }
    *transferred = done;
    return NDIS_STATUS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/*  Interrupts                                                        */
/* ------------------------------------------------------------------ */
VOID STDC RtlIsr(PBOOLEAN recognised, PBOOLEAN queueDpc, NDIS_HANDLE ctx)
{
    PRLTK_ADAPTER a = (PRLTK_ADAPTER)ctx;
    BOOLEAN ours = a->Ops->Isr(a);          /* latches + masks */
    *recognised = ours;
    *queueDpc   = ours;
}

VOID STDC RtlHandleInterrupt(NDIS_HANDLE ctx)
{
    PRLTK_ADAPTER a = (PRLTK_ADAPTER)ctx;
    NdisAcquireSpinLock(&a->Lock);
    if (a->Halting || a->ResetPending || a->ResetFailed) {
        NdisReleaseSpinLock(&a->Lock);return;
    }
    NdisReleaseSpinLock(&a->Lock);
    a->Ops->HandleInterrupt(a);
    if (a->TxStalled) {
        a->TxStalled = FALSE;
        NdisMSendResourcesAvailable(a->MiniportHandle);
    }
    NdisAcquireSpinLock(&a->Lock);
    if (!a->Halting && !a->ResetPending && !a->ResetFailed && !a->RxRecoveryNeeded) a->Ops->EnableInts(a, TRUE);
    NdisReleaseSpinLock(&a->Lock);
}

/* ------------------------------------------------------------------ */
/*  OIDs                                                              */
/* ------------------------------------------------------------------ */
static const NDIS_OID RtlOids[] = {
    OID_GEN_SUPPORTED_LIST, OID_GEN_HARDWARE_STATUS, OID_GEN_MEDIA_SUPPORTED,
    OID_GEN_MEDIA_IN_USE, OID_GEN_MAXIMUM_LOOKAHEAD, OID_GEN_MAXIMUM_FRAME_SIZE,
    OID_GEN_LINK_SPEED, OID_GEN_TRANSMIT_BUFFER_SPACE, OID_GEN_RECEIVE_BUFFER_SPACE,
    OID_GEN_TRANSMIT_BLOCK_SIZE, OID_GEN_RECEIVE_BLOCK_SIZE, OID_GEN_VENDOR_ID,
    OID_GEN_VENDOR_DESCRIPTION, OID_GEN_CURRENT_PACKET_FILTER, OID_GEN_CURRENT_LOOKAHEAD,
    OID_GEN_DRIVER_VERSION, OID_GEN_MAXIMUM_TOTAL_SIZE, OID_GEN_MAC_OPTIONS,
    OID_GEN_MEDIA_CONNECT_STATUS, OID_GEN_VENDOR_DRIVER_VERSION,
    OID_GEN_XMIT_OK, OID_GEN_RCV_OK, OID_GEN_XMIT_ERROR, OID_GEN_RCV_ERROR,
    OID_GEN_RCV_NO_BUFFER, OID_802_3_PERMANENT_ADDRESS, OID_802_3_CURRENT_ADDRESS,
    OID_802_3_MULTICAST_LIST, OID_802_3_MAXIMUM_LIST_SIZE, OID_802_3_RCV_ERROR_ALIGNMENT,
    OID_802_3_XMIT_ONE_COLLISION, OID_802_3_XMIT_MORE_COLLISIONS
};

NDIS_STATUS STDC
RtlQueryInformation(NDIS_HANDLE ctx, NDIS_OID oid, PVOID buf, ULONG len,
                    PULONG written, PULONG needed)
{
    PRLTK_ADAPTER a = (PRLTK_ADAPTER)ctx;
    ULONG v = 0;
    USHORT vs;
    char name[sizeof(a->FriendlyName)];
    PVOID src = &v;
    ULONG slen = sizeof(ULONG);

    switch (oid) {
    case OID_GEN_SUPPORTED_LIST:      src = (PVOID)RtlOids; slen = sizeof(RtlOids); break;
    case OID_GEN_HARDWARE_STATUS:     v = NdisHardwareStatusReady; break;
    case OID_GEN_MEDIA_SUPPORTED:
    case OID_GEN_MEDIA_IN_USE:        v = NdisMedium802_3; break;
    case OID_GEN_MAXIMUM_LOOKAHEAD:
    case OID_GEN_CURRENT_LOOKAHEAD:
    case OID_GEN_MAXIMUM_FRAME_SIZE:  v = RLTK_MAX_FRAME - RLTK_HEADER_SIZE; break;
    case OID_GEN_MAXIMUM_TOTAL_SIZE:
    case OID_GEN_TRANSMIT_BLOCK_SIZE:
    case OID_GEN_RECEIVE_BLOCK_SIZE:  v = RLTK_MAX_FRAME; break;
    case OID_GEN_LINK_SPEED:          v = (a->LinkSpeedMbit ? a->LinkSpeedMbit : a->MaxSpeedMbit) * 10000UL; break;
    case OID_GEN_TRANSMIT_BUFFER_SPACE: v = RLTK_MAX_FRAME * 4; break;
    case OID_GEN_RECEIVE_BUFFER_SPACE:  v = 32 * 1024; break;
    case OID_GEN_VENDOR_ID:           v = RtlNdisVendorId(a->PermanentAddr); break;
    case OID_GEN_VENDOR_DESCRIPTION:
        NdisAcquireSpinLock(&a->Lock);
        NdisMoveMemory(name,a->FriendlyName,sizeof(name));
        NdisReleaseSpinLock(&a->Lock);
        name[sizeof(name)-1]=0;src=name;slen=0;
        while (name[slen]) slen++;
        slen++;
        break;
    case OID_GEN_DRIVER_VERSION:
        vs = (USHORT)((RLTK_VERSION_MAJOR << 8) | RLTK_VERSION_MINOR);
#if defined(RTL_NDIS5)
        vs = 0x0500;
#else
        vs = 0x0300;
#endif
        src = &vs; slen = sizeof(USHORT);
        break;
    case OID_GEN_VENDOR_DRIVER_VERSION: v = (RLTK_VERSION_MAJOR << 16) | RLTK_VERSION_MINOR; break;
    case OID_GEN_CURRENT_PACKET_FILTER: v = a->PacketFilter; break;
    case OID_GEN_MAC_OPTIONS:
        v = NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA | NDIS_MAC_OPTION_RECEIVE_SERIALIZED |
            NDIS_MAC_OPTION_TRANSFERS_NOT_PEND | NDIS_MAC_OPTION_NO_LOOPBACK;
        break;
    case OID_GEN_MEDIA_CONNECT_STATUS: v = a->LinkUp ? 0 : 1; break;
    case OID_GEN_XMIT_OK:             v = a->XmitOk; break;
    case OID_GEN_RCV_OK:              v = a->RcvOk; break;
    case OID_GEN_XMIT_ERROR:          v = a->XmitErr; break;
    case OID_GEN_RCV_ERROR:           v = a->RcvErr; break;
    case OID_GEN_RCV_NO_BUFFER:       v = a->RcvNoBuf; break;
    case OID_802_3_PERMANENT_ADDRESS: src = a->PermanentAddr; slen = 6; break;
    case OID_802_3_CURRENT_ADDRESS:   src = a->CurrentAddr;   slen = 6; break;
    case OID_802_3_MULTICAST_LIST:    src = a->McastList;     slen = a->McastCount * 6; break;
    case OID_802_3_MAXIMUM_LIST_SIZE: v = RLTK_MAX_MULTICAST; break;
    case OID_802_3_RCV_ERROR_ALIGNMENT: v = a->AlignErr; break;
    case OID_802_3_XMIT_ONE_COLLISION:  v = a->OneColl; break;
    case OID_802_3_XMIT_MORE_COLLISIONS: v = a->MoreColl; break;
    default:
        *written = 0; *needed = 0;
        return NDIS_STATUS_NOT_SUPPORTED;
    }

    if (len < slen) { *written = 0; *needed = slen; return NDIS_STATUS_INVALID_LENGTH; }
    if (slen) NdisMoveMemory(buf, src, slen);
    *written = slen;
    *needed = 0;
    return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS STDC
RtlSetInformation(NDIS_HANDLE ctx, NDIS_OID oid, PVOID buf, ULONG len,
                  PULONG read, PULONG needed)
{
    PRLTK_ADAPTER a = (PRLTK_ADAPTER)ctx;
    ULONG v;

    *read = 0; *needed = 0;
    switch (oid) {
    case OID_GEN_CURRENT_PACKET_FILTER:
        if (len != 4) { *needed = 4; return NDIS_STATUS_INVALID_LENGTH; }
        NdisMoveMemory(&v, buf, 4);
        if (v & ~(NDIS_PACKET_TYPE_DIRECTED | NDIS_PACKET_TYPE_MULTICAST |
                  NDIS_PACKET_TYPE_ALL_MULTICAST | NDIS_PACKET_TYPE_BROADCAST |
                  NDIS_PACKET_TYPE_PROMISCUOUS))
            return NDIS_STATUS_NOT_SUPPORTED;
        a->PacketFilter = v;
        a->Ops->SetRxMode(a);
        *read = 4;
        return NDIS_STATUS_SUCCESS;

    case OID_GEN_CURRENT_LOOKAHEAD:
        if (len != 4) { *needed = 4; return NDIS_STATUS_INVALID_LENGTH; }
        *read = 4;
        return NDIS_STATUS_SUCCESS;          /* we always indicate whole frames */

    case OID_802_3_MULTICAST_LIST:
        if (len % 6) return NDIS_STATUS_INVALID_LENGTH;
        if (len > RLTK_MAX_MULTICAST * 6) return NDIS_STATUS_RESOURCES;  /* MULTICAST_FULL */
        a->McastCount = len / 6;
        if (len) NdisMoveMemory(a->McastList, buf, len);
        a->Ops->SetRxMode(a);
        *read = len;
        return NDIS_STATUS_SUCCESS;

    case OID_GEN_PROTOCOL_OPTIONS:
        *read = len;
        return NDIS_STATUS_SUCCESS;

    default:
        return NDIS_STATUS_NOT_SUPPORTED;
    }
}
