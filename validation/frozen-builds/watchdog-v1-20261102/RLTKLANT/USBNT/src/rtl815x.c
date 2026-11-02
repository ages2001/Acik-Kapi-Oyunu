/*
 * rtl815x.c - Realtek USB LAN (r8152 family) protocol layer (see rtl815x.h).
 *
 * Copyright (C) 2026 ages2001.
 */
#include "rtl815x.h"

static VOID put32le(PUCHAR p, ULONG v)
{
    p[0] = (UCHAR)(v);        p[1] = (UCHAR)(v >> 8);
    p[2] = (UCHAR)(v >> 16);  p[3] = (UCHAR)(v >> 24);
}

static ULONG get32le(const UCHAR *p)
{
    return (ULONG)p[0] | ((ULONG)p[1] << 8) |
           ((ULONG)p[2] << 16) | ((ULONG)p[3] << 24);
}

const RTL_USB_ID *RtlUsbMatch(USHORT vendor, USHORT device)
{
    const RTL_USB_ID *id;
    if (vendor != RTL_USB_VENDOR_ID) return 0;
    for (id = RtlUsbIds; id->DeviceId; id++)
        if (id->DeviceId == device) return id;
    return 0;
}

/*
 * The vendor REGS request carries the register offset in wValue and the
 * MCU block (PLA/USB) plus the byte-enable mask in wIndex, exactly as the
 * r8152 driver forms generic_ocp_read / generic_ocp_write.
 */
NDIS_STATUS RtlUsbReadRegs(PRTL_USB_NIC n, USHORT mcuType, USHORT offset,
                           PVOID buf, USHORT len)
{
    ULONG done = 0;
    if (!buf || !len || (ULONG)offset + len > 0x10000UL)
        return NDIS_STATUS_FAILURE;
    while (done < len) {
        UCHAR data[4];
        ULONG pos = (ULONG)offset + done, shift = pos & 3, count = 4 - shift, i;
        NDIS_STATUS st;
        if (count > len - done) count = len - done;
        st = n->Pipe.Control(&n->Pipe, RTLU_RT_READ, RTLU_REQ_GET_REGS,
                             (USHORT)(pos & ~3UL), mcuType, data, 4);
        if (!USB_OK(st)) return st;
        for (i = 0; i < count; i++) ((PUCHAR)buf)[done + i] = data[shift + i];
        done += count;
    }
    return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS RtlUsbWriteRegs(PRTL_USB_NIC n, USHORT mcuType, USHORT offset,
                            UCHAR byteEn, PVOID buf, USHORT len)
{
    ULONG done = 0;
    if (!buf || !len || !(byteEn & 15) || (ULONG)offset + len > 0x10000UL)
        return NDIS_STATUS_FAILURE;
    while (done < len) {
        UCHAR data[4] = {0, 0, 0, 0};
        ULONG pos = (ULONG)offset + done, shift = pos & 3, count = 4 - shift, i;
        USHORT mask = 0;
        NDIS_STATUS st;
        if (count > len - done) count = len - done;
        for (i = 0; i < count; i++) {
            data[shift + i] = ((PUCHAR)buf)[done + i];
            if (byteEn & (1U << ((done + i) & 3))) mask |= 1U << (shift + i);
        }
        if (mask) {
            st = n->Pipe.Control(&n->Pipe, RTLU_RT_WRITE, RTLU_REQ_SET_REGS,
                                 (USHORT)(pos & ~3UL),
                                 (USHORT)(mcuType | mask | (mask << 4)), data, 4);
            if (!USB_OK(st)) return st;
        }
        done += count;
    }
    return NDIS_STATUS_SUCCESS;
}

/* PID is not a silicon revision: newer chips can retain an older USB ID.
 * Signatures are PLA_TCR1 & 0x7cf0 (Linux rtl8152_get_version).
 * Unknown revisions must not silently use the legacy descriptor format. */
NDIS_STATUS RtlUsbDetectVersion(PRTL_USB_NIC n)
{
    static const struct { USHORT Signature, Speed; UCHAR Version; } versions[] = {
        {0x4c00,100,1}, {0x4c10,100,2},
        {0x5c00,1000,3}, {0x5c10,1000,4}, {0x5c20,1000,5}, {0x5c30,1000,6},
        {0x4800,100,7}, {0x6000,1000,8}, {0x6010,1000,9},
        {0x7020,2500,10}, {0x7030,2500,11}, {0x7400,2500,12},
        {0x7410,2500,13}, {0x6400,1000,14}, {0x7420,1000,15},
        {0x1030,5000,16}, {0x2020,10000,17}
    };
    UCHAR data[4];
    ULONG i;
    NDIS_STATUS st;
    n->HwVersion = 0;
    n->Version = 0;
    n->PackageId = 0;
    n->DescriptorV2 = FALSE;
    n->MaxSpeedMbit = 0;
    st = RtlUsbReadRegs(n, RTLU_MCU_TYPE_PLA, 0xE610, data, 4);
    if (!USB_OK(st)) return st;
    n->HwVersion = (USHORT)((get32le(data) >> 16) & 0x7cf0);
    for (i = 0; i < sizeof(versions)/sizeof(versions[0]); i++) {
        if (n->HwVersion == versions[i].Signature) {
            if (versions[i].Version == 17) {
                ULONG package;
                /* Vendor v2.22.1 distinguishes two incompatible RTL8159
                   package paths despite their shared 0x2020 signature. */
                st = RtlUsbReadRegs(n, RTLU_MCU_TYPE_PLA, 0xDC48, data, 4);
                if (!USB_OK(st)) return st;
                package = get32le(data);
                if (package == 0xFFFFFFFFUL) return NDIS_STATUS_ADAPTER_NOT_FOUND;
                package &= 0x1e;
                if (package != 0x1e && package != 0x1c &&
                    package != 0x18 && package != 0x1a)
                    return NDIS_STATUS_ADAPTER_NOT_FOUND;
                n->PackageId = (UCHAR)package;
            }
            n->Version = versions[i].Version;
            n->MaxSpeedMbit = versions[i].Speed;
            n->DescriptorV2 = n->Version >= 16;
            return NDIS_STATUS_SUCCESS;
        }
    }
    return NDIS_STATUS_ADAPTER_NOT_FOUND;
}

static NDIS_STATUS WriteByte(PRTL_USB_NIC n, USHORT mcuType, USHORT off, UCHAR v)
{
    return RtlUsbWriteRegs(n, mcuType, off, RTLU_BYTE_EN_BYTE, &v, 1);
}

static NDIS_STATUS WriteDword(PRTL_USB_NIC n, USHORT mcuType, USHORT off, ULONG v)
{
    UCHAR b[4];
    put32le(b, v);
    return RtlUsbWriteRegs(n, mcuType, off, RTLU_BYTE_EN_DWORD, b, 4);
}

/* Read/modify/write only the requested byte/word, preserving other state. */
static NDIS_STATUS UpdateBits(PRTL_USB_NIC n, USHORT block, USHORT off,
                              USHORT clear, USHORT set, USHORT size)
{
    UCHAR data[2];
    USHORT value;
    NDIS_STATUS st = RtlUsbReadRegs(n, block, off, data, size);
    if (!USB_OK(st)) return st;
    value = data[0];
    if (size == 2) value |= (USHORT)data[1] << 8;
    value = (USHORT)((value & ~clear) | set);
    data[0] = (UCHAR)value; data[1] = (UCHAR)(value >> 8);
    return RtlUsbWriteRegs(n, block, off,
                          size == 2 ? RTLU_BYTE_EN_WORD : RTLU_BYTE_EN_BYTE,
                          data, size);
}

NDIS_STATUS RtlUsbReset(PRTL_USB_NIC n)
{
    NDIS_STATUS st;
    UCHAR cr;
    ULONG spins;
    n->Running = FALSE;
    /* Vendor r8152 v2.22.1 rtl8152_nic_reset: versions 16/17 quiesce
       TX/RX instead of asserting the older self-clearing reset bit. */
    if (n->Version == 16 || n->Version == 17)
        return UpdateBits(n, RTLU_MCU_TYPE_PLA, RTLU_PLA_CR,
                          RTLU_CR_RE | RTLU_CR_TE, 0, 1);
    /* RTL8156 versions 10/11 reset the receive BMU through CDC ECM mode.
       The order matters: stop TX, gate BMU IN, select ECM, stop RX,
       release BMU IN, and leave ECM. Do not touch unrelated register bits. */
    if (n->Version == 10 || n->Version == 11) {
        st = UpdateBits(n, RTLU_MCU_TYPE_PLA, RTLU_PLA_CR, RTLU_CR_TE, 0, 1);
        if (!USB_OK(st)) return st;
        st = UpdateBits(n, RTLU_MCU_TYPE_USB, RTLU_USB_BMU_RESET,
                        RTLU_BMU_RESET_EP_IN, 0, 2);
        if (!USB_OK(st)) return st;
        st = UpdateBits(n, RTLU_MCU_TYPE_USB, RTLU_USB_USB_CTRL,
                        0, RTLU_CDC_ECM_EN, 2);
        if (!USB_OK(st)) return st;
        st = UpdateBits(n, RTLU_MCU_TYPE_PLA, RTLU_PLA_CR, RTLU_CR_RE, 0, 1);
        if (!USB_OK(st)) return st;
        st = UpdateBits(n, RTLU_MCU_TYPE_USB, RTLU_USB_BMU_RESET,
                        0, RTLU_BMU_RESET_EP_IN, 2);
        if (!USB_OK(st)) return st;
        return UpdateBits(n, RTLU_MCU_TYPE_USB, RTLU_USB_USB_CTRL,
                          RTLU_CDC_ECM_EN, 0, 2);
    }
    if (!n->Version || n->Version > 17) return NDIS_STATUS_ADAPTER_NOT_FOUND;
    st = WriteByte(n, RTLU_MCU_TYPE_PLA, RTLU_PLA_CR, RTLU_CR_RST);
    if (!USB_OK(st)) return st;
    for (spins = 0; spins < 1000; spins++) {
        st = RtlUsbReadRegs(n, RTLU_MCU_TYPE_PLA, RTLU_PLA_CR, &cr, 1);
        if (!USB_OK(st)) return st;
        if (!(cr & RTLU_CR_RST)) return NDIS_STATUS_SUCCESS;
    }
    return NDIS_STATUS_FAILURE;
}

NDIS_STATUS RtlUsbReadMac(PRTL_USB_NIC n)
{
    return RtlUsbReadRegs(n, RTLU_MCU_TYPE_PLA, RTLU_PLA_IDR, n->Mac, 6);
}

NDIS_STATUS RtlUsbSetRxMode(PRTL_USB_NIC n, ULONG rcrFlags)
{
    n->RcrFlags = rcrFlags;
    return WriteDword(n, RTLU_MCU_TYPE_PLA, RTLU_PLA_RCR, rcrFlags);
}

NDIS_STATUS RtlUsbStart(PRTL_USB_NIC n)
{
    NDIS_STATUS st;

    st = RtlUsbSetRxMode(n, RTLU_RCR_APM | RTLU_RCR_AB);
    if (!USB_OK(st)) return st;

    st = WriteByte(n, RTLU_MCU_TYPE_PLA, RTLU_PLA_CR,
                   (UCHAR)(RTLU_CR_RE | RTLU_CR_TE));
    if (USB_OK(st)) n->Running = TRUE;
    return st;
}

NDIS_STATUS RtlUsbStop(PRTL_USB_NIC n)
{
    n->Running = FALSE;
    return WriteByte(n, RTLU_MCU_TYPE_PLA, RTLU_PLA_CR, 0);
}

ULONG RtlUsbBuildTx(PUCHAR frame, ULONG len, PUCHAR out)
{
    /* tx_desc.opts1 = length (low 18 bits); opts2 = 0.  No checksum or
       segmentation offload is requested, so the rest stays clear. */
    put32le(out + 0, (len & RTLU_TX_LEN_MASK) | 0xC0000000UL);
    put32le(out + 4, 0);
    {
        ULONG i;
        for (i = 0; i < len; i++) out[RTLU_TX_DESC_SIZE + i] = frame[i];
    }
    return RTLU_TX_DESC_SIZE + len;
}

ULONG RtlUsbNextRx(PUCHAR buf, ULONG bufLen, ULONG at,
                   PUCHAR *frame, PULONG frameLen)
{
    ULONG pktLen, total, next;

    *frame = 0; *frameLen = 0;
    if (at > bufLen || bufLen - at < RTLU_RX_DESC_SIZE) return 0;

    pktLen = get32le(buf + at) & RTLU_RX_LEN_MASK;
    if (pktLen <= 4) return 0;
    if (pktLen > bufLen - at - RTLU_RX_DESC_SIZE) return 0;

    *frame    = buf + at + RTLU_RX_DESC_SIZE;
    *frameLen = pktLen - 4;

    /* next packet starts 8-byte aligned after this descriptor + payload */
    total = RTLU_RX_DESC_SIZE + pktLen;
    total = (total + (RTLU_RX_ALIGN - 1)) & ~(ULONG)(RTLU_RX_ALIGN - 1);
    next  = at + total;
    if (next > bufLen || bufLen - next < RTLU_RX_DESC_SIZE) return 0;
    return next;
}

/* RTL8157 uses the v2 16-byte descriptors and 16-byte RX alignment.
   RX length includes the four-byte Ethernet FCS, which NDIS must not see. */
ULONG Rtl8157BuildTx(PUCHAR frame, ULONG len, PUCHAR out)
{
    ULONG i;
    for (i = 0; i < 16; i++) out[i] = 0;
    put32le(out + 0, (len & RTLU_TX_LEN_MASK) | 0xC0000000UL);
    put32le(out + 8, len << 4);
    put32le(out + 12, 0xA8000000UL);
    for (i = 0; i < len; i++) out[16 + i] = frame[i];
    return 16 + len;
}

ULONG Rtl8157NextRx(PUCHAR buf, ULONG bufLen, ULONG at,
                    PUCHAR *frame, PULONG frameLen)
{
    ULONG length, used, remaining;
    *frame = 0;
    *frameLen = 0;
    if (at > bufLen || bufLen - at < 16) return 0;
    remaining = bufLen - at;
    length = (get32le(buf + at) >> 17) & 0x7FFF;
    if (length <= 4 || length > remaining - 16) return 0;
    *frame = buf + at + 16;
    *frameLen = length - 4;
    used = (16 + length + 15) & ~15UL;
    if (used > remaining || remaining - used < 16) return 0;
    return at + used;
}
