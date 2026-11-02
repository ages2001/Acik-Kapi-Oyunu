/*
 * rtlchip.h - Realtek LAN chip database for RLTKLANT
 *
 * Single source of truth for:
 *   - PCI Vendor/Device IDs supported by the driver
 *   - register-model "backend" each device uses
 *   - exact marketing name resolved at run time (XID / HwVerId)
 *   - maximum media speed each chip is allowed to negotiate
 *
 * Copyright (C) 2026 ages2001.
 *
 * Device-ID and chip-version data cross-checked against the Linux
 * r8169 driver (drivers/net/ethernet/realtek/r8169_main.c) and the
 * RTL8139/RTL8029 datasheets.  Register semantics are also checked against the Realtek r8127 Linux
 * driver; see validation source records for provenance.
 */

#ifndef RTLCHIP_H
#define RTLCHIP_H

#define RTL_VENDOR_ID           0x10EC

/*
 * Register-model backends.  Every supported device is driven through
 * exactly one of these.  The C+ ("8169") backend is descriptor based and
 * scales from the first Gigabit parts up to the 10G RTL8127 through the
 * per-chip feature flags below.
 */
#define RTL_BACKEND_NE2000      0   /* RTL8029(AS)  - DP8390 / NE2000 core   */
#define RTL_BACKEND_8139        1   /* RTL8129/8139/810x - PIO/MMIO ring     */
#define RTL_BACKEND_CPLUS       2   /* RTL8169/8168/8125/8126/8127 - C+ desc */

/*
 * Media-speed ceilings (in megabit/s).  The driver never advertises nor
 * forces a speed above the ceiling of the detected part, even when the
 * PHY reports a higher capability.
 */
#define RTL_SPEED_10            10
#define RTL_SPEED_100           100
#define RTL_SPEED_1000          1000
#define RTL_SPEED_2500          2500
#define RTL_SPEED_5000          5000
#define RTL_SPEED_10000         10000

/* Capability flags held per chip-version row. */
#define RTL_F_MSI               0x0001  /* supports MSI (NDIS5 only)         */
#define RTL_F_JUMBO             0x0002  /* jumbo frame capable               */
#define RTL_F_8125REGS          0x0004  /* RTL8125+ extended register layout */
#define RTL_F_EEE               0x0008  /* energy-efficient ethernet         */
#define RTL_F_PHY_OCP           0x0020  /* PHY uses the PHYOCP window       */
#define RTL_F_PHY_DP            0x0040  /* RTL8168DP PHY access gate        */
#define RTL_F_FASTET            0x0010  /* Fast-Ethernet only (810xE on C+)  */

/*
 * One PCI device-ID the driver binds to.  A single DEV id usually maps to
 * many marketing parts; the exact one is resolved from the XID at runtime
 * (see RTL_XID below), so this table only fixes the backend + the hard
 * ceiling that the DEV id alone guarantees.
 */
typedef struct _RTL_PCI_ID {
    unsigned short  DeviceId;
    unsigned char   Backend;
    unsigned short  MaxSpeed;       /* ceiling if XID yields nothing better */
    const char     *GenericName;    /* fallback friendly name               */
} RTL_PCI_ID;

/*
 * XID-to-name rows for the C+ backend.  (TxConfig >> 20) & Mask == Id.
 * Masks/values verified against r8169_main.c rtl_chip_infos[].  Rows are
 * matched most-specific first (larger mask before its 0x7c8 parent).
 */
typedef struct _RTL_XID {
    unsigned short  Mask;
    unsigned short  Id;
    unsigned short  MaxSpeed;
    unsigned short  Flags;
    const char     *Name;
} RTL_XID;

/* ------------------------------------------------------------------ */
/*  PCI device-ID table                                                */
/* ------------------------------------------------------------------ */
static const RTL_PCI_ID RtlPciIds[] = {
    /* NE2000-compatible 10 Mbit */
    { 0x8029, RTL_BACKEND_NE2000, RTL_SPEED_10,   "Realtek RTL8029(AS) PCI Ethernet" },

    /* RTL8129 / 8139 Fast-Ethernet family */
    { 0x8129, RTL_BACKEND_8139,   RTL_SPEED_100,  "Realtek RTL8129 Fast Ethernet" },
    { 0x8138, RTL_BACKEND_8139,   RTL_SPEED_100,  "Realtek RTL8139B CardBus Fast Ethernet" },
    { 0x8139, RTL_BACKEND_8139,   RTL_SPEED_100,  "Realtek RTL8139 Family Fast Ethernet" },
    { 0x8130, RTL_BACKEND_8139,   RTL_SPEED_100,  "Realtek RTL8139D Fast Ethernet" },
    { 0x8100, RTL_BACKEND_8139,   RTL_SPEED_100,  "Realtek RTL8100 Fast Ethernet" },

    /* RTL8169 / 8110 Gigabit (SerDes/MII, first generation) */
    { 0x8169, RTL_BACKEND_CPLUS,  RTL_SPEED_1000, "Realtek RTL8169/8110 Gigabit Ethernet" },
    { 0x8167, RTL_BACKEND_CPLUS,  RTL_SPEED_1000, "Realtek RTL8110SC/8169SC Gigabit Ethernet" },

    /* RTL8168 / 8111 PCIe Gigabit family (and 810xE Fast-Ethernet on 0x8136) */
    { 0x8168, RTL_BACKEND_CPLUS,  RTL_SPEED_1000, "Realtek RTL8168/8111 PCIe Gigabit Ethernet" },
    { 0x8161, RTL_BACKEND_CPLUS,  RTL_SPEED_1000, "Realtek RTL8168 PCIe Gigabit Ethernet" },
    { 0x8162, RTL_BACKEND_CPLUS,  RTL_SPEED_1000, "Realtek RTL8168 PCIe Gigabit Ethernet" },
    { 0x8136, RTL_BACKEND_CPLUS,  RTL_SPEED_100,  "Realtek RTL810xE PCIe Fast Ethernet" },
    { 0x8101, RTL_BACKEND_CPLUS,  RTL_SPEED_100,  "Realtek RTL8101E/8102E PCIe Fast Ethernet" },
    { 0x8411, RTL_BACKEND_CPLUS,  RTL_SPEED_1000, "Realtek RTL8411 PCIe Gigabit Ethernet" },
    { 0x2502, RTL_BACKEND_CPLUS,  RTL_SPEED_1000, "Realtek RTL8168 PCIe Gigabit Ethernet" },
    { 0x2600, RTL_BACKEND_CPLUS,  RTL_SPEED_1000, "Realtek RTL8168 PCIe Gigabit Ethernet" },

    /* RTL8125 2.5 Gigabit */
    { 0x8125, RTL_BACKEND_CPLUS,  RTL_SPEED_2500, "Realtek RTL8125 2.5GbE Family Controller" },
    { 0x3000, RTL_BACKEND_CPLUS,  RTL_SPEED_2500, "Realtek RTL8125 2.5GbE Controller" },
    { 0x5000, RTL_BACKEND_CPLUS,  RTL_SPEED_2500, "Realtek RTL8125 2.5GbE Controller" },
    { 0x0E10, RTL_BACKEND_CPLUS,  RTL_SPEED_2500, "Realtek RTL8125 2.5GbE Controller" },

    /* RTL8126 5 Gigabit */
    { 0x8126, RTL_BACKEND_CPLUS,  RTL_SPEED_5000, "Realtek RTL8126 5GbE Family Controller" },

    /* RTL8127 10 Gigabit */
    { 0x8127, RTL_BACKEND_CPLUS,  RTL_SPEED_10000,"Realtek RTL8127 10GbE Family Controller" },
};
#define RTL_PCI_ID_COUNT  (sizeof(RtlPciIds)/sizeof(RtlPciIds[0]))

/* ------------------------------------------------------------------ */
/*  C+ backend: XID -> exact marketing name + ceiling                  */
/*  (TxConfig register bits 26..20, read at init)                      */
/* ------------------------------------------------------------------ */
static const RTL_XID RtlXidTable[] = {
    /* RTL8169 / 8110 (Gigabit) */
    { 0xfc8, 0x008, RTL_SPEED_1000, 0,              "Realtek RTL8169s Gigabit Ethernet" },
    { 0xfc8, 0x040, RTL_SPEED_1000, 0,              "Realtek RTL8110s Gigabit Ethernet" },
    { 0xfc8, 0x100, RTL_SPEED_1000, 0,              "Realtek RTL8169sb/8110sb Gigabit Ethernet" },
    { 0xfc8, 0x180, RTL_SPEED_1000, 0,              "Realtek RTL8169sc/8110sc Gigabit Ethernet" },
    { 0xfc8, 0x980, RTL_SPEED_1000, 0,              "Realtek RTL8169sc/8110sc Gigabit Ethernet" },

    /* RTL810xE (PCIe Fast-Ethernet, C+ descriptor model) */
    { 0x7cf, 0x248, RTL_SPEED_100,  RTL_F_FASTET,   "Realtek RTL8102e PCIe Fast Ethernet" },
    { 0x7cf, 0x348, RTL_SPEED_100,  RTL_F_FASTET,   "Realtek RTL8102e PCIe Fast Ethernet" },
    { 0x7cf, 0x249, RTL_SPEED_100,  RTL_F_FASTET,   "Realtek RTL8102e PCIe Fast Ethernet" },
    { 0x7cf, 0x349, RTL_SPEED_100,  RTL_F_FASTET,   "Realtek RTL8102e PCIe Fast Ethernet" },
    { 0x7cf, 0x240, RTL_SPEED_100,  RTL_F_FASTET,   "Realtek RTL8401 PCIe Fast Ethernet" },
    { 0x7c8, 0x340, RTL_SPEED_100,  RTL_F_FASTET,   "Realtek RTL8101e/8100e PCIe Fast Ethernet" },
    { 0x7c8, 0x248, RTL_SPEED_100,  RTL_F_FASTET,   "Realtek RTL8102e/8103e PCIe Fast Ethernet" },
    { 0x7c8, 0x348, RTL_SPEED_100,  RTL_F_FASTET,   "Realtek RTL8102e/8103e PCIe Fast Ethernet" },
    { 0x7cf, 0x409, RTL_SPEED_100,  RTL_F_FASTET,   "Realtek RTL8105e PCIe Fast Ethernet" },
    { 0x7c8, 0x408, RTL_SPEED_100,  RTL_F_FASTET,   "Realtek RTL8105e PCIe Fast Ethernet" },
    { 0x7c8, 0x440, RTL_SPEED_100,  RTL_F_FASTET,   "Realtek RTL8402 PCIe Fast Ethernet" },
    { 0x7c8, 0x448, RTL_SPEED_100,  RTL_F_FASTET,   "Realtek RTL8106e PCIe Fast Ethernet" },

    { 0x7c8, 0x488, RTL_SPEED_1000, RTL_F_EEE, "Realtek RTL8411 PCIe Gigabit Ethernet" },

    /* RTL8168 / 8111 (PCIe Gigabit) */
    { 0x7c8, 0x380, RTL_SPEED_1000, 0,              "Realtek RTL8168b/8111b PCIe Gigabit Ethernet" },
    { 0x7cf, 0x3c8, RTL_SPEED_1000, 0,              "Realtek RTL8168cp/8111cp PCIe Gigabit Ethernet" },
    { 0x7cf, 0x3c9, RTL_SPEED_1000, 0,              "Realtek RTL8168cp/8111cp PCIe Gigabit Ethernet" },
    { 0x7c8, 0x3c8, RTL_SPEED_1000, 0,              "Realtek RTL8168cp/8111cp PCIe Gigabit Ethernet" },
    { 0x7cf, 0x3c0, RTL_SPEED_1000, 0,              "Realtek RTL8168c/8111c PCIe Gigabit Ethernet" },
    { 0x7cf, 0x3c2, RTL_SPEED_1000, 0,              "Realtek RTL8168c/8111c PCIe Gigabit Ethernet" },
    { 0x7cf, 0x3c3, RTL_SPEED_1000, 0,              "Realtek RTL8168c/8111c PCIe Gigabit Ethernet" },
    { 0x7c8, 0x3c0, RTL_SPEED_1000, 0,              "Realtek RTL8168c/8111c PCIe Gigabit Ethernet" },
    { 0x7cf, 0x281, RTL_SPEED_1000, 0,              "Realtek RTL8168d/8111d PCIe Gigabit Ethernet" },
    { 0x7c8, 0x280, RTL_SPEED_1000, 0,              "Realtek RTL8168d/8111d PCIe Gigabit Ethernet" },
    { 0x7cf, 0x28a, RTL_SPEED_1000, RTL_F_PHY_DP,              "Realtek RTL8168dp/8111dp PCIe Gigabit Ethernet" },
    { 0x7cf, 0x28b, RTL_SPEED_1000, RTL_F_PHY_DP,              "Realtek RTL8168dp/8111dp PCIe Gigabit Ethernet" },
    { 0x7cf, 0x2c1, RTL_SPEED_1000, 0,              "Realtek RTL8168e/8111e PCIe Gigabit Ethernet" },
    { 0x7c8, 0x2c0, RTL_SPEED_1000, 0,              "Realtek RTL8168e/8111e PCIe Gigabit Ethernet" },
    { 0x7c8, 0x2c8, RTL_SPEED_1000, RTL_F_JUMBO,    "Realtek RTL8168evl/8111evl PCIe Gigabit Ethernet" },
    { 0x7cf, 0x480, RTL_SPEED_1000, RTL_F_EEE,      "Realtek RTL8168f/8111f PCIe Gigabit Ethernet" },
    { 0x7cf, 0x481, RTL_SPEED_1000, RTL_F_EEE,      "Realtek RTL8168f/8111f PCIe Gigabit Ethernet" },
    { 0x7cf, 0x4c0, RTL_SPEED_1000, RTL_F_EEE|RTL_F_PHY_OCP,      "Realtek RTL8168g/8111g PCIe Gigabit Ethernet" },
    { 0x7cf, 0x509, RTL_SPEED_1000, RTL_F_EEE|RTL_F_PHY_OCP,      "Realtek RTL8168gu/8111gu PCIe Gigabit Ethernet" },
    { 0x7cf, 0x5c8, RTL_SPEED_1000, RTL_F_EEE|RTL_F_PHY_OCP,      "Realtek RTL8411b PCIe Gigabit Ethernet" },
    { 0x7cf, 0x541, RTL_SPEED_1000, RTL_F_EEE|RTL_F_PHY_OCP,      "Realtek RTL8168h/8111h PCIe Gigabit Ethernet" },
    { 0x7cf, 0x6c0, RTL_SPEED_1000, RTL_F_EEE|RTL_F_PHY_OCP,      "Realtek RTL8168M Gigabit Ethernet" },
    { 0x7cf, 0x502, RTL_SPEED_1000, RTL_F_EEE|RTL_F_PHY_OCP,      "Realtek RTL8168ep/8111ep PCIe Gigabit Ethernet" },
    { 0x7cf, 0x54b, RTL_SPEED_1000, RTL_F_EEE|RTL_F_PHY_OCP,      "Realtek RTL8168fp/8117 PCIe Gigabit Ethernet" },
    { 0x7cf, 0x54a, RTL_SPEED_1000, RTL_F_EEE|RTL_F_PHY_OCP,      "Realtek RTL8168fp/8117 PCIe Gigabit Ethernet" },

    /* RTL8125 (2.5G) */
    { 0x7cf, 0x609, RTL_SPEED_2500, RTL_F_8125REGS|RTL_F_MSI|RTL_F_EEE|RTL_F_PHY_OCP, "Realtek RTL8125A 2.5GbE Controller" },
    { 0x7cf, 0x641, RTL_SPEED_2500, RTL_F_8125REGS|RTL_F_MSI|RTL_F_EEE|RTL_F_PHY_OCP, "Realtek RTL8125B 2.5GbE Controller" },
    { 0x7cf, 0x688, RTL_SPEED_2500, RTL_F_8125REGS|RTL_F_MSI|RTL_F_EEE|RTL_F_PHY_OCP, "Realtek RTL8125D 2.5GbE Controller" },
    { 0x7cf, 0x689, RTL_SPEED_2500, RTL_F_8125REGS|RTL_F_MSI|RTL_F_EEE|RTL_F_PHY_OCP, "Realtek RTL8125D 2.5GbE Controller" },
    { 0x7cf, 0x68a, RTL_SPEED_2500, RTL_F_8125REGS|RTL_F_MSI|RTL_F_EEE|RTL_F_PHY_OCP, "Realtek RTL8125K 2.5GbE Controller" },
    { 0x7cf, 0x68b, RTL_SPEED_2500, RTL_F_8125REGS|RTL_F_MSI|RTL_F_EEE|RTL_F_PHY_OCP, "Realtek RTL9151A 2.5GbE Controller" },
    { 0x7cf, 0x708, RTL_SPEED_2500, RTL_F_8125REGS|RTL_F_MSI|RTL_F_EEE|RTL_F_PHY_OCP, "Realtek RTL8125CP 2.5GbE Controller" },
    { 0x7cf, 0x681, RTL_SPEED_2500, RTL_F_8125REGS|RTL_F_MSI|RTL_F_EEE|RTL_F_PHY_OCP, "Realtek RTL8125BP 2.5GbE Controller" },

    /* RTL8126 (5G) */
    { 0x7cf, 0x649, RTL_SPEED_5000, RTL_F_8125REGS|RTL_F_MSI|RTL_F_EEE|RTL_F_PHY_OCP|RTL_F_JUMBO, "Realtek RTL8126A 5GbE Controller" },
    { 0x7cf, 0x64a, RTL_SPEED_5000, RTL_F_8125REGS|RTL_F_MSI|RTL_F_EEE|RTL_F_PHY_OCP|RTL_F_JUMBO, "Realtek RTL8126A 5GbE Controller" },

    /* RTL8127 (10G) */
    { 0x7cf, 0x6c9, RTL_SPEED_10000,RTL_F_8125REGS|RTL_F_MSI|RTL_F_EEE|RTL_F_PHY_OCP|RTL_F_JUMBO, "Realtek RTL8127A 10GbE Controller" },
};
#define RTL_XID_COUNT  (sizeof(RtlXidTable)/sizeof(RtlXidTable[0]))

/*
 * RTL8139 family HwVerId (TxConfig bits 31..26 + bit 23).  Used only by the
 * 8139 backend to put the precise model into the friendly name.
 */
typedef struct _RTL_8139_VER {
    unsigned long   HwVerId;        /* TxConfig & 0x7cc00000 */
    const char     *Name;
} RTL_8139_VER;

static const RTL_8139_VER Rtl8139Versions[] = {
    { 0x40000000UL, "Realtek RTL8139 Fast Ethernet" },
    { 0x60000000UL, "Realtek RTL8139A Fast Ethernet" },
    { 0x70000000UL, "Realtek RTL8139B Fast Ethernet" },
    { 0x78000000UL, "Realtek RTL8139B/8130 Fast Ethernet" },
    { 0x7C000000UL, "Realtek RTL8130 Fast Ethernet" },
    { 0x74000000UL, "Realtek RTL8139C Fast Ethernet" },
    { 0x74400000UL, "Realtek RTL8139D/8100B Fast Ethernet" },
    { 0x74800000UL, "Realtek RTL8139C+ Fast Ethernet" },
    { 0x74C00000UL, "Realtek RTL8101/8101L Fast Ethernet" },
    { 0x78800000UL, "Realtek RTL8100 Fast Ethernet" },
};
#define RTL_8139_VER_COUNT (sizeof(Rtl8139Versions)/sizeof(Rtl8139Versions[0]))

#endif /* RTLCHIP_H */
