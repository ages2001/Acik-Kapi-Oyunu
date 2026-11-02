/*
 * hwdetect.c - PCI id / XID resolution and media-speed capping.
 * Copyright (C) 2026 ages2001.
 */
#include "rltklant.h"

static VOID RtlStrCopy(char *d, const char *s, UINT max)
{
    UINT i = 0;
    while (s[i] && i < max - 1) { d[i] = s[i]; i++; }
    d[i] = 0;
}

const RTL_PCI_ID *RtlFindPciId(USHORT dev)
{
    UINT i;
    for (i = 0; i < RTL_PCI_ID_COUNT; i++)
        if (RtlPciIds[i].DeviceId == dev)
            return &RtlPciIds[i];
    return NULL;
}

/*
 * Resolve the exact marketing name and the real media ceiling from the
 * TxConfig XID for the C+ backend (or HwVerId for the 8139 backend).
 * Falls back to the PCI id's generic name/ceiling when no row matches.
 */
VOID RtlResolveName(PRLTK_ADAPTER a, ULONG txConfig)
{
    const RTL_PCI_ID *pid = RtlFindPciId(a->PciDeviceId);

    if (pid)
        RtlStrCopy(a->FriendlyName, pid->GenericName, sizeof(a->FriendlyName));
    else
        RtlStrCopy(a->FriendlyName, "Realtek LAN Controller", sizeof(a->FriendlyName));

    if (a->Backend == RTL_BACKEND_CPLUS) {
        UINT xid = (UINT)((txConfig >> 20) & 0xFFF);
        UINT i;
        for (i = 0; i < RTL_XID_COUNT; i++) {
            if ((xid & RtlXidTable[i].Mask) == RtlXidTable[i].Id) {
                RtlStrCopy(a->FriendlyName, RtlXidTable[i].Name, sizeof(a->FriendlyName));
                a->MaxSpeedMbit = RtlXidTable[i].MaxSpeed;
                a->Flags        = RtlXidTable[i].Flags;
                /* Fast-Ethernet PHYs can share the Gigabit MAC XID. */
                if (pid && pid->MaxSpeed < a->MaxSpeedMbit) {
                    a->MaxSpeedMbit = pid->MaxSpeed;
                    if (pid->MaxSpeed == RTL_SPEED_100) {
                        a->Flags |= RTL_F_FASTET;
                        if ((xid & 0x7cf) == 0x509)
                            RtlStrCopy(a->FriendlyName, "Realtek RTL8106eus PCIe Fast Ethernet", sizeof(a->FriendlyName));
                        else if ((xid & 0x7cf) == 0x541 || (xid & 0x7cf) == 0x6c0)
                            RtlStrCopy(a->FriendlyName, "Realtek RTL8107e PCIe Fast Ethernet", sizeof(a->FriendlyName));
                    }
                }
                return;
            }
        }
        /* unknown XID: keep PCI-id ceiling */
        if (pid) a->MaxSpeedMbit = pid->MaxSpeed;
    } else if (a->Backend == RTL_BACKEND_8139) {
        ULONG ver = txConfig & R8139_TCR_HWVERMASK;
        UINT i;
        for (i = 0; i < RTL_8139_VER_COUNT; i++) {
            if (Rtl8139Versions[i].HwVerId == ver) {
                RtlStrCopy(a->FriendlyName, Rtl8139Versions[i].Name, sizeof(a->FriendlyName));
                break;
            }
        }
        a->MaxSpeedMbit = RTL_SPEED_100;
    } else { /* NE2000 */
        a->MaxSpeedMbit = RTL_SPEED_10;
    }
}

/* Never let negotiated speed exceed the part's hard ceiling. */
ULONG RtlCapSpeed(PRLTK_ADAPTER a, ULONG mbit)
{
    if (a->MaxSpeedMbit && mbit > a->MaxSpeedMbit)
        return a->MaxSpeedMbit;
    return mbit;
}

/* Ethernet CRC-32 of a 6-byte address; returns the top 6 bits (0..63)
   used by every Realtek multicast hash register. */
ULONG RtlCrc32Bits(const UCHAR *addr)
{
    ULONG crc = 0xFFFFFFFFUL;
    INT i, j;
    for (i = 0; i < 6; i++) {
        UCHAR b = addr[i];
        for (j = 0; j < 8; j++, b >>= 1) {
            ULONG bit = ((crc >> 31) ^ b) & 1;
            crc <<= 1;
            if (bit) crc ^= 0x04C11DB7UL;
        }
    }
    return crc >> 26;
}
