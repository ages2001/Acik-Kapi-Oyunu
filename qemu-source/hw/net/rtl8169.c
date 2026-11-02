/*
 * rtl8169.c - minimal QEMU device model for the Realtek C+ descriptor NICs
 *             (RTL8169/8168 Gigabit and RTL8125/8126/8127 2.5/5/10G).
 *
 * Implements exactly the register/descriptor surface that the RLTKLANT
 * driver uses: IDR/MAR, the TNPDS/RDSAR descriptor rings, CR, the TX poll
 * doorbell, IMR/ISR, RCR, the MDIO window and the MAC-side PHY status, with
 * the XID wired so the driver resolves the right chip.  It is a test target,
 * not a complete Realtek emulation.
 *
 * Copyright (C) 2026 ages2001.
 */
#include "qemu/osdep.h"
#include "hw/pci/pci_device.h"
#include "hw/pci/pcie.h"
#include "qapi/error.h"
#include <zlib.h>
#include "hw/qdev-properties.h"
#include "migration/vmstate.h"
#include "net/net.h"
#include "qemu/module.h"
#include "qom/object.h"

#define TYPE_RTL8169 "rtl8169"
typedef struct RTL8169State RTL8169State;
DECLARE_INSTANCE_CHECKER(RTL8169State, RTL8169, TYPE_RTL8169)

/* register offsets */
#define R_IDR0      0x00
#define R_MAR0      0x08
#define R_TNPDS     0x20
#define R_CR        0x37
#define R_TPPOLL    0x38   /* 8169 (8-bit)  */
#define R_IMR16     0x3C   /* 8169 (16-bit) */
#define R_ISR16     0x3E   /* 8169 (16-bit) */
#define R_IMR32     0x38   /* 8125 (32-bit) */
#define R_ISR32     0x3C   /* 8125 (32-bit) */
#define R_TCR       0x40
#define R_RCR       0x44
#define R_9346CR    0x50
#define R_PHYAR     0x60
#define R_PHYSTATUS 0x6C
#define R_RMS       0xDA
#define R_CPCR      0xE0
#define R_RDSAR     0xE4
#define R_TPPOLL8125 0x90  /* 8125 (16-bit) */

#define CR_TE  0x04
#define CR_RE  0x08
#define CR_RST 0x10

#define TPPOLL_NPQ 0x40

#define INT_ROK 0x0001
#define INT_TOK 0x0004
#define INT_RDU 0x0010
#define INT_LINKCHG 0x0020

#define DESC_OWN 0x80000000u
#define DESC_EOR 0x40000000u
#define DESC_FS  0x20000000u
#define DESC_LS  0x10000000u
#define DESC_LEN 0x00003FFFu

#define PHYAR_FLAG 0x80000000u

#define PHYS_FULLDUP 0x01
#define PHYS_LINKSTS 0x02
#define PHYS_1000M   0x10
#define PHYS_2500M   0x400
#define PHYS_5000M   0x1000
#define PHYS_10000M  0x4000

enum { VAR_8169, VAR_8168, VAR_8125, VAR_8126, VAR_8127 };

struct RTL8169State {
    PCIDevice parent_obj;
    MemoryRegion mmio;
    MemoryRegion bar_io;
    NICState *nic;
    NICConf conf;

    uint32_t variant;
    bool is8125;
    char *chip_profile;
    bool phy_ocp_access, dash_enabled, management_timeout;
    unsigned dash_type; /* 0 none, 1 DP, 2 EP, 3 BP */
    uint16_t device_id;
    uint16_t pci_id_alias;
    uint32_t xid;           /* (xid<<20) seeds TCR */
    uint16_t phystatus;

    uint8_t  mac[6];
    uint8_t  cr;
    uint32_t imr, isr;
    uint32_t tcr, rcr, cpcr;
    uint8_t  cr9346;
    uint16_t rms;
    uint32_t tnpds_lo, tnpds_hi;
    uint32_t rdsar_lo, rdsar_hi;
    uint32_t phyar;
    uint16_t phy[32];
    uint16_t phy_page;
    uint16_t phy_paged[4096][32];
    uint32_t phyocp;
    uint16_t phy_ocp[32768];
    uint32_t macocp;
    uint16_t mac_ocp[32768];
    bool phy_timeout;       /* explicit host-injected handshake fault */
    bool io_bar;            /* false exercises MMIO-only guest discovery */
    /* Pinned r8169 configuration transport. These storage/acknowledgement
     * models do not execute PHY/MCU instructions or analog calibration. */
    uint8_t config_regs[0x8000];
    uint16_t ephy[32];
    uint32_t eri[1024], csi_vendor[1024];
    uint32_t oob[1024], dp[1024], dp_data, dp_command;
    uint32_t ephyar, eriar, eridr, csiar, csidr;
    bool mac_timeout;
    bool tx_stall_until_reset; /* explicit one-shot host fault, not a chip bit */
    uint8_t multicast[8];
    uint32_t tx_cur;        /* descriptor index */
    uint32_t rx_cur;
};

static void rtl8169_update_irq(RTL8169State *s)
{
    int level = (s->isr & s->imr & 0xFFFF) ? 1 : 0;
    pci_set_irq(PCI_DEVICE(s), level);
}

static uint64_t tnpds(RTL8169State *s) { return ((uint64_t)s->tnpds_hi << 32) | (s->tnpds_lo & ~0xFFu); }
static uint64_t rdsar(RTL8169State *s) { return ((uint64_t)s->rdsar_hi << 32) | (s->rdsar_lo & ~0xFFu); }

static void rtl8169_tx(RTL8169State *s)
{
    PCIDevice *d = PCI_DEVICE(s);
    uint8_t buf[16384];
    int guard = 128;

    if (!(s->cr & CR_TE) || s->tx_stall_until_reset) return;
    while (guard--) {
        uint64_t base = tnpds(s);
        uint64_t dptr = base + (uint64_t)s->tx_cur * 16;
        uint32_t desc[4];
        uint32_t opts1, len;
        uint64_t bufaddr;

        if (pci_dma_read(d, dptr, desc, 16) != MEMTX_OK) break;
        opts1 = le32_to_cpu(desc[0]);
        if (!(opts1 & DESC_OWN)) break;
        len = opts1 & DESC_LEN;
        bufaddr = ((uint64_t)le32_to_cpu(desc[3]) << 32) | le32_to_cpu(desc[2]);
        /* Only complete, non-offloaded frames are supported by this model. */
        if (!len || len > sizeof(buf) ||
            (opts1 & (DESC_FS | DESC_LS)) != (DESC_FS | DESC_LS) ||
            pci_dma_read(d, bufaddr, buf, len) != MEMTX_OK) {
            break;
        }
        qemu_send_packet(qemu_get_queue(s->nic), buf, len);

        desc[0] = cpu_to_le32(opts1 & ~DESC_OWN);
        pci_dma_write(d, dptr, &desc[0], 4);

        if (opts1 & DESC_EOR) s->tx_cur = 0; else s->tx_cur++;
        s->isr |= INT_TOK;
    }
    rtl8169_update_irq(s);
}

static bool rtl8169_can_receive(NetClientState *nc)
{
    RTL8169State *s = qemu_get_nic_opaque(nc);
    return (s->cr & CR_RE) ? 1 : 0;
}

static ssize_t rtl8169_receive(NetClientState *nc, const uint8_t *buf, size_t size)
{
    RTL8169State *s = qemu_get_nic_opaque(nc);
    PCIDevice *d = PCI_DEVICE(s);
    uint64_t base = rdsar(s);
    uint64_t dptr = base + (uint64_t)s->rx_cur * 16;
    uint32_t desc[4];
    uint32_t opts1;
    uint64_t bufaddr;
    size_t len = size;
    uint32_t crc;

    if (!(s->cr & CR_RE)) return size;
    if (pci_dma_read(d, dptr, desc, 16) != MEMTX_OK) return size;
    opts1 = le32_to_cpu(desc[0]);
    if (!(opts1 & DESC_OWN)) {           /* no buffer: drop + RDU */
        s->isr |= INT_RDU;
        rtl8169_update_irq(s);
        return size;
    }
    bufaddr = ((uint64_t)le32_to_cpu(desc[3]) << 32) | le32_to_cpu(desc[2]);
    if (len + 4 > (opts1 & DESC_LEN) || len + 4 > s->rms ||
        len + 4 > DESC_LEN) {
        s->isr |= INT_RDU;
        rtl8169_update_irq(s);
        return size;
    }
    crc = cpu_to_le32(crc32(0, buf, len));
    if (pci_dma_write(d, bufaddr, buf, len) != MEMTX_OK ||
        pci_dma_write(d, bufaddr + len, &crc, 4) != MEMTX_OK) return size;

    desc[0] = cpu_to_le32(DESC_FS | DESC_LS | ((len + 4) & DESC_LEN) |
                          (opts1 & DESC_EOR));
    pci_dma_write(d, dptr, &desc[0], 4);

    if (opts1 & DESC_EOR) s->rx_cur = 0; else s->rx_cur++;
    s->isr |= INT_ROK;
    rtl8169_update_irq(s);
    return size;
}

static void rtl8169_reset(DeviceState *dev)
{
    RTL8169State *s = RTL8169(dev);
    s->tx_stall_until_reset = false;
    s->cr = 0;
    s->imr = s->isr = 0;
    s->tcr = s->xid << 20;
    s->rcr = 0;
    s->cpcr = 0;
    s->cr9346 = 0;
    s->tx_cur = s->rx_cur = 0;
    s->phyar = 0;
    s->tnpds_lo = s->tnpds_hi = s->rdsar_lo = s->rdsar_hi = 0;
    s->rms = 0;
    s->phyocp = 0;
    s->macocp = 0;
    memset(s->config_regs, 0, sizeof(s->config_regs));
    memset(s->ephy, 0, sizeof(s->ephy));
    memset(s->eri, 0, sizeof(s->eri));
    s->ephyar = s->eriar = s->eridr = s->csiar = s->csidr = 0;
    memset(s->multicast, 0, sizeof(s->multicast));
    memcpy(s->mac, s->conf.macaddr.a, 6);
    pci_set_irq(PCI_DEVICE(s), 0);
}

static void rtl8169_phyar_write(RTL8169State *s, uint32_t val)
{
    uint32_t reg = (val >> 16) & 0x1F;
    uint16_t *bank = s->phy_page ?
        (s->phy_page < 4096 ? s->phy_paged[s->phy_page] : NULL) : s->phy;
    if (s->phy_timeout) { s->phyar = val; return; }
    if (val & PHYAR_FLAG) {              /* PHY write */
        if (reg == 31) s->phy_page = val;
        else if (bank) {
            /* Standard BMCR reset completes immediately in this protocol
             * model. It does not execute uploaded PHY MCU instructions. */
            bank[reg] = !s->phy_page && !reg ? val & ~0x8000u : val;
        }
        s->phyar = val & ~PHYAR_FLAG;    /* flag clears when done */
    } else {                            /* PHY read request */
        s->phyar = PHYAR_FLAG | (reg << 16) |
            (reg == 31 ? s->phy_page : bank ? bank[reg] : 0xffff);
    }
}

/* Only source-referenced configuration cells are modeled. Reserved cells
 * stay read-as-zero/write-ignored. CSI/ERI/EPHY use explicit protocols below. */
static bool rtl8169_config_cell(RTL8169State *s, hwaddr addr, unsigned size)
{
    if (size == 1) {
        if ((addr >= 0x51 && addr <= 0x56) || addr == 0xb6 ||
            (addr >= 0xd0 && addr <= 0xd3) || addr == 0xd8 ||
            addr == 0xec || addr == 0xf2 || addr == 0xf8 || addr == 0xf9)
            return true;
        if (s->is8125 && addr == 0x34) return true;
    }
    if (size == 2 && addr == 0xe2) return true;
    if (size == 4 && (addr == 0x7c || addr == 0xf0)) return true;
    if (s->is8125) {
        if (size == 2 && (addr == 0x7a || addr == 0x382 ||
            addr == 0x1880 || addr == 0x4800 || addr == 0x6048)) return true;
        if (size == 4 && (addr == 0x4500 ||
            (addr >= 0xa00 && addr < 0xb00 && !(addr & 3)))) return true;
    }
    return false;
}

static void rtl8169_csi_command(RTL8169State *s, uint32_t value)
{
    PCIDevice *dev = PCI_DEVICE(s);
    unsigned addr = value & 0xffc;
    s->csiar = value;
    if (s->mac_timeout || (value & 3) ||
        ((value >> 16) & 7) != PCI_FUNC(dev->devfn)) return;
    if (value & PHYAR_FLAG) {
        for (unsigned i = 0; i < 4; i++) {
            if (!(value & (1u << (12 + i)))) continue;
            if (addr < 256) {
                pci_default_write_config(dev, addr + i, s->csidr >> (8 * i), 1);
            } else {
                s->csi_vendor[addr / 4] &= ~(0xffu << (8 * i));
                s->csi_vendor[addr / 4] |= s->csidr & (0xffu << (8 * i));
            }
        }
        s->csiar &= ~PHYAR_FLAG;
    } else {
        s->csidr = addr < 256 ? pci_get_long(dev->config + addr) : s->csi_vendor[addr / 4];
        s->csiar |= PHYAR_FLAG;
    }
}

/* The management engine is a separate synthetic endpoint. MAC reset does
 * not erase its enable bit or acknowledge a pending ownership command.
 * Only the source-documented doorbell completes START/STOP; payload writes
 * alone do not. No management firmware instructions are executed. */
static void rtl_management_notify(RTL8169State *s, unsigned command)
{
    unsigned status = s->xid == 0x28b ? 0xb8 / 4 : 0x10 / 4;
    if (!s->dash_enabled || s->management_timeout) return;
    if (s->dash_type == 1) {
        if (command == 5) s->dp[status] |= 0x800;
        if (command == 6) s->dp[status] &= ~0x800u;
    } else if (s->dash_type == 2) {
        if (command == 5) s->oob[0x124 / 4] |= 1;
        if (command == 6) s->oob[0x124 / 4] &= ~1u;
    }
}

static void rtl_eri_command(RTL8169State *s, uint32_t value)
{
    unsigned type = (value >> 16) & 3, index = (value & 0xffc) / 4;
    uint32_t extra = value & 0x7ffc0000u;
    uint32_t *bank = s->eri;
    s->eriar = value;
    if (s->mac_timeout || (value & 3) || !(value & 0xf000)) return;
    if (type == 2 && s->dash_type >= 2) {
        if (extra != (s->xid == 0x54a ? (0xf70u << 18) : 0)) return;
        bank = s->oob;
    } else if (type != 0 || extra) return;
    if (value & PHYAR_FLAG) {
        for (unsigned i = 0; i < 4; i++) if (value & (1u << (12 + i))) {
            bank[index] &= ~(0xffu << (8 * i));
            bank[index] |= s->eridr & (0xffu << (8 * i));
        }
        if (type == 2 && (value & 0x1000)) {
            if (s->dash_type == 2 && index == 0x30 / 4 && (s->eridr & 1))
                rtl_management_notify(s, bank[0x180 / 4] & 0xff);
            /* BP has no completion flag in the pinned host protocol. */
        }
        s->eriar &= ~PHYAR_FLAG;
    } else {
        s->eridr = bank[index];
        s->eriar |= PHYAR_FLAG;
    }
}

static void rtl_dp_command(RTL8169State *s, uint32_t value)
{
    unsigned index = (value & 0xffc) / 4;
    s->dp_command = value;
    if (s->mac_timeout || (value & 0x7fff0003u) || !(value & 0xf000)) return;
    if (value & PHYAR_FLAG) {
        for (unsigned i = 0; i < 4; i++) if (value & (1u << (12 + i))) {
            s->dp[index] &= ~(0xffu << (8 * i));
            s->dp[index] |= s->dp_data & (0xffu << (8 * i));
        }
        if (index == 0x30 / 4 && (value & 0x1000) && (s->dp_data & 1))
            rtl_management_notify(s, s->eri[0xe8 / 4] & 0xff);
        s->dp_command &= ~PHYAR_FLAG;
    } else {
        s->dp_data = s->dp[index];
        s->dp_command |= PHYAR_FLAG;
    }
}

static void rtl8169_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    RTL8169State *s = opaque;

    if (addr < 6) {
        for (unsigned i = 0; i < size && addr + i < 6; i++)
            s->mac[addr + i] = val >> (i * 8);
        return;
    }
    if (addr >= R_MAR0 && addr + size <= R_MAR0 + 8) {
        for (unsigned i = 0; i < size; i++)
            s->multicast[addr - R_MAR0 + i] = val >> (i * 8);
        return;
    }
    switch (addr) {
    case R_IDR0: case R_IDR0+1: case R_IDR0+2: case R_IDR0+3:
    case R_IDR0+4: case R_IDR0+5:
        if (addr - R_IDR0 < 6) s->mac[addr - R_IDR0] = val & 0xFF;
        return;
    case 0xf9:
        if (size == 1 && s->dash_type == 2) {
            s->config_regs[0xf9] = val;
            if (!(val & 1) && !s->management_timeout) s->config_regs[0xfb] |= 0x20;
            return;
        }
        break;
    case 0xfb:
        if (size == 1 && s->dash_type == 2) s->config_regs[0xfb] &= ~val;
        return;
    case R_CR:
        if (val & CR_RST) { rtl8169_reset(DEVICE(s)); return; }
        s->cr = val & 0xFF;
        return;
    case R_TCR: s->tcr = (val & ~0x7cf00000u) | (s->xid << 20); return;
    case R_RCR: s->rcr = val; return;
    case R_9346CR: s->cr9346 = val & 0xFF; return;
    case R_RMS: s->rms = val & 0xFFFF; return;
    case R_CPCR: s->cpcr = val & 0xFFFF; return;
    case R_TNPDS: s->tnpds_lo = val; return;
    case R_TNPDS+4: s->tnpds_hi = val; return;
    case R_RDSAR: s->rdsar_lo = val; return;
    case R_RDSAR+4: s->rdsar_hi = val; return;
    case R_PHYAR:
        if (!s->is8125) rtl8169_phyar_write(s, val);
        return;
    case 0x64:
        if (size == 4 && s->variant != VAR_8169) s->csidr = val;
        return;
    case 0x68:
        if (size == 4 && s->variant != VAR_8169) rtl8169_csi_command(s, val);
        return;
    case 0x70:
        if (size == 4 && s->variant != VAR_8169) s->eridr = val;
        return;
    case 0x74:
        if (size == 4 && s->variant != VAR_8169) rtl_eri_command(s, val);
        return;
    case 0x80:
        if (size == 4 && s->variant != VAR_8169) {
            unsigned index = (val >> 16) & 31;
            s->ephyar = val;
            if (s->mac_timeout) return;
            if (val & PHYAR_FLAG) {
                s->ephy[index] = val;
                s->ephyar &= ~PHYAR_FLAG;
            } else {
                s->ephyar |= PHYAR_FLAG | s->ephy[index];
            }
        }
        return;
    case 0xB4:
        if (size == 4 && s->dash_type == 1) rtl_dp_command(s, val);
        return;
    case 0xB0:
        if (s->dash_type == 1) { if (size == 4) s->dp_data = val; return; }
        if (s->variant != VAR_8169) {
            unsigned index = (val >> 16) & 0x7fff;
            if (val & PHYAR_FLAG) s->mac_ocp[index] = val;
            s->macocp = (index << 16) | s->mac_ocp[index];
        }
        return;
    case 0xB8:
        if (s->phy_ocp_access) {
            unsigned index = (val >> 16) & 0x7fff;
            if (s->phy_timeout) { s->phyocp = val; return; }
            if (val & PHYAR_FLAG) {
                s->phy_ocp[index] = index == 0xa400 / 2 ? val & ~0x8000u : val;
                s->phyocp = val & ~PHYAR_FLAG;
            } else {
                s->phyocp = PHYAR_FLAG | (index << 16) | s->phy_ocp[index];
            }
        }
        return;
    case R_MAR0: case R_MAR0+4: return;
    }

    if (s->is8125) {
        switch (addr) {
        case R_IMR32: s->imr = val; rtl8169_update_irq(s); return;
        case R_ISR32: s->isr &= ~(uint32_t)val; rtl8169_update_irq(s); return;
        case R_TPPOLL8125: if (size == 4 && (val & 1)) rtl8169_tx(s); return;
        }
    } else {
        switch (addr) {
        case R_IMR16: s->imr = val & 0xFFFF; rtl8169_update_irq(s); return;
        case R_ISR16: s->isr &= ~((uint32_t)val & 0xFFFF); rtl8169_update_irq(s); return;
        case R_TPPOLL: if (val & TPPOLL_NPQ) rtl8169_tx(s); return;
        }
    }
    if (rtl8169_config_cell(s, addr, size)) {
        if (addr >= 0x51 && addr <= 0x56 && (s->cr9346 & 0xc0) != 0xc0) return;
        for (unsigned i = 0; i < size; i++) s->config_regs[addr + i] = val >> (8 * i);
    }
}

static uint64_t rtl8169_read(void *opaque, hwaddr addr, unsigned size)
{
    RTL8169State *s = opaque;
    uint32_t result = 0;
    if (addr < 6) {
        for (unsigned i = 0; i < size && addr + i < 6; i++)
            result |= (uint32_t)s->mac[addr + i] << (i * 8);
        return result;
    }
    if (addr >= R_MAR0 && addr + size <= R_MAR0 + 8) {
        for (unsigned i = 0; i < size; i++)
            result |= (uint32_t)s->multicast[addr - R_MAR0 + i] << (i * 8);
        return result;
    }
    switch (addr) {
    case R_IDR0: case R_IDR0+1: case R_IDR0+2: case R_IDR0+3:
    case R_IDR0+4: case R_IDR0+5:
        return (addr - R_IDR0 < 6) ? s->mac[addr - R_IDR0] : 0;
    case R_CR: return s->cr;
    case R_TCR: return s->tcr | (s->management_timeout || s->tx_stall_until_reset ? 0 : 0x800);
    case R_RCR: return s->rcr;
    case R_9346CR: return s->cr9346;
    case R_CPCR: return s->cpcr;
    case R_RMS: return s->rms;
    case R_PHYAR: return s->phyar;
    case R_PHYSTATUS: return size >= 2 ? s->phystatus : s->phystatus & 0xFF;
    case 0xB8: return s->phyocp;
    case 0xB0: return s->dash_type == 1 ? s->dp_data : s->macocp;
    case 0xB4: return s->dash_type == 1 ? s->dp_command : 0;
    case 0xd3:
        /* TX/RX callbacks are synchronous. No queued FIFO remains between
         * MMIO transactions; injected faults explicitly suppress readiness. */
        return (s->config_regs[0xd3] & ~0x32u) |
            (s->management_timeout || s->tx_stall_until_reset ? 0 : 0x32);
    case 0xe2:
        if (s->is8125) return s->management_timeout || s->tx_stall_until_reset ? 0 : 0x103;
        break;
    case 0xfb: return s->dash_type == 2 ? s->config_regs[0xfb] : 0;
    case 0x64: return size == 4 ? s->csidr : 0;
    case 0x68: return size == 4 ? s->csiar : 0;
    case 0x70: return size == 4 ? s->eridr : 0;
    case 0x74: return size == 4 ? s->eriar : 0;
    case 0x80: return size == 4 ? s->ephyar : 0;
    case R_TNPDS: return s->tnpds_lo;
    case R_TNPDS+4: return s->tnpds_hi;
    case R_RDSAR: return s->rdsar_lo;
    case R_RDSAR+4: return s->rdsar_hi;
    case R_PHYSTATUS+1: return (s->phystatus >> 8) & 0xFF;
    }

    if (s->is8125) {
        switch (addr) {
        case R_IMR32: return s->imr;
        case R_ISR32: return s->isr;
        }
    } else {
        switch (addr) {
        case R_IMR16: return s->imr & 0xFFFF;
        case R_ISR16: return s->isr & 0xFFFF;
        }
    }
    if (rtl8169_config_cell(s, addr, size)) {
        for (unsigned i = 0; i < size; i++) result |= (uint32_t)s->config_regs[addr + i] << (8 * i);
    }
    return result;
}

static const MemoryRegionOps rtl8169_ops = {
    .read = rtl8169_read,
    .write = rtl8169_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = { .min_access_size = 1, .max_access_size = 4 },
};

static void rtl8169_link_changed(NetClientState *nc)
{
    RTL8169State *s = qemu_get_nic_opaque(nc);
    if (nc->link_down) s->phystatus &= ~PHYS_LINKSTS;
    else s->phystatus |= PHYS_LINKSTS;
    s->isr |= INT_LINKCHG;
    rtl8169_update_irq(s);
}

static NetClientInfo net_rtl8169_info = {
    .type = NET_CLIENT_DRIVER_NIC,
    .size = sizeof(NICState),
    .can_receive = rtl8169_can_receive,
    .receive = rtl8169_receive,
    .link_status_changed = rtl8169_link_changed,
};

static void rtl8169_set_variant(RTL8169State *s)
{
    switch (s->variant) {
    case VAR_8168: s->device_id = 0x8168; s->xid = 0x380; s->is8125 = false; s->phystatus = PHYS_LINKSTS|PHYS_FULLDUP|PHYS_1000M; break;
    case VAR_8125: s->device_id = 0x8125; s->xid = 0x609; s->is8125 = true;  s->phystatus = PHYS_LINKSTS|PHYS_FULLDUP|PHYS_2500M; break;
    case VAR_8126: s->device_id = 0x8126; s->xid = 0x649; s->is8125 = true;  s->phystatus = PHYS_LINKSTS|PHYS_FULLDUP|PHYS_5000M; break;
    case VAR_8127: s->device_id = 0x8127; s->xid = 0x6c9; s->is8125 = true;  s->phystatus = PHYS_LINKSTS|PHYS_FULLDUP|PHYS_10000M; break;
    case VAR_8169: default: s->device_id = 0x8169; s->xid = 0x180; s->is8125 = false; s->phystatus = PHYS_LINKSTS|PHYS_FULLDUP|PHYS_1000M; break;
    }
}

static void pci_rtl8169_realize(PCIDevice *dev, Error **errp)
{
    RTL8169State *s = RTL8169(dev);
    DeviceState *d = DEVICE(dev);

    if (s->variant > VAR_8127) {
        error_setg(errp, "Unsupported Realtek variant %u", s->variant);
        return;
    }
    rtl8169_set_variant(s);
    s->phy_ocp_access = s->is8125;
    if (s->chip_profile) {
        if (s->variant == VAR_8168 && !strcmp(s->chip_profile, "8111dp")) {
            s->xid = 0x28a; s->dash_type = 1;
        } else if (s->variant == VAR_8168 && !strcmp(s->chip_profile, "8111dp-b")) {
            s->xid = 0x28b; s->dash_type = 1;
        } else if (s->variant == VAR_8168 && !strcmp(s->chip_profile, "8111ep")) {
            s->xid = 0x502; s->dash_type = 2; s->phy_ocp_access = true;
        } else if (s->variant == VAR_8168 && !strcmp(s->chip_profile, "8117")) {
            s->xid = 0x54a; s->dash_type = 2; s->phy_ocp_access = true;
        } else if (s->variant == VAR_8125 && !strcmp(s->chip_profile, "8125bp")) {
            s->xid = 0x681; s->dash_type = 3;
        } else if (s->variant == VAR_8127 && !strcmp(s->chip_profile, "8127bp")) {
            s->dash_type = 3; dev->config[PCI_REVISION_ID] = 4;
        } else {
            error_setg(errp, "chip-profile does not match the Realtek PCIe family");
            return;
        }
        if (!pci_is_express(dev)) {
            error_setg(errp, "chip-profile requires a PCIe endpoint");
            return;
        }
    }
    if (s->dash_enabled && !s->dash_type) {
        error_setg(errp, "dash-enabled requires a management-capable chip-profile");
        return;
    }
    s->dp[(s->xid == 0x28b ? 0xb8 : 0x10) / 4] = s->dash_enabled ? 0x8000 : 0;
    s->oob[0x128 / 4] = s->dash_enabled ? 1 : 0;
    /* Test the documented 8125-family PCI aliases against the same register
     * contract. This does not model independent alias-specific silicon. */
    if (s->pci_id_alias) {
        if (s->variant != VAR_8125 || !pci_is_express(dev) ||
            (s->pci_id_alias != 0x3000 && s->pci_id_alias != 0x5000 &&
             s->pci_id_alias != 0x0e10)) {
            error_setg(errp, "pci-id-alias requires PCIe RTL8125 and ID 0x3000, 0x5000 or 0x0e10");
            return;
        }
        s->device_id = s->pci_id_alias;
    }
    if (pci_is_express(dev)) {
        if (pcie_endpoint_cap_init(dev, 0x80) < 0) {
            error_setg(errp, "Cannot initialize Realtek PCIe capability");
            return;
        }
    }
    pci_set_word(dev->config + PCI_DEVICE_ID, s->device_id);

    dev->config[PCI_INTERRUPT_PIN] = 1;
    memory_region_init_io(&s->mmio, OBJECT(s), &rtl8169_ops, s, "rtl8169-mmio", s->is8125 ? 0x8000 : 0x1000);
    memory_region_init_io(&s->bar_io, OBJECT(s), &rtl8169_ops, s, "rtl8169-io", 0x100);
    if (s->io_bar) pci_register_bar(dev, 0, PCI_BASE_ADDRESS_SPACE_IO, &s->bar_io);
    /* BAR2 memory, 64-bit (as real 8168/8125). */
    pci_register_bar(dev, 2, PCI_BASE_ADDRESS_SPACE_MEMORY |
                     PCI_BASE_ADDRESS_MEM_TYPE_64, &s->mmio);

    qemu_macaddr_default_if_unset(&s->conf.macaddr);
    memcpy(s->mac, s->conf.macaddr.a, 6);
    s->nic = qemu_new_nic(&net_rtl8169_info, &s->conf,
                          object_get_typename(OBJECT(dev)), d->id,
                          &d->mem_reentrancy_guard, s);
    qemu_format_nic_info_str(qemu_get_queue(s->nic), s->conf.macaddr.a);
}

static void pci_rtl8169_exit(PCIDevice *dev)
{
    RTL8169State *s = RTL8169(dev);
    pci_set_irq(dev, 0);
    if (pci_is_express(dev)) pcie_cap_exit(dev);
    qemu_del_nic(s->nic);
}

static const VMStateDescription vmstate_rtl8169 = {
    .name = "rtl8169",
    .unmigratable = true,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_PCI_DEVICE(parent_obj, RTL8169State),
        VMSTATE_END_OF_LIST()
    }
};

static Property rtl8169_properties[] = {
    DEFINE_NIC_PROPERTIES(RTL8169State, conf),
    DEFINE_PROP_UINT32("variant", RTL8169State, variant, VAR_8169),
    DEFINE_PROP_UINT16("pci-id-alias", RTL8169State, pci_id_alias, 0),
    DEFINE_PROP_STRING("chip-profile", RTL8169State, chip_profile),
    DEFINE_PROP_BOOL("dash-enabled", RTL8169State, dash_enabled, false),
    DEFINE_PROP_BOOL("io-bar", RTL8169State, io_bar, true),
    DEFINE_PROP_END_OF_LIST(),
};

static bool rtl_phy_timeout_get(Object *obj, Error **errp)
{
    (void)errp;
    return RTL8169(obj)->phy_timeout;
}

static void rtl_phy_timeout_set(Object *obj, bool value, Error **errp)
{
    (void)errp;
    RTL8169(obj)->phy_timeout = value;
}

static bool rtl_mac_timeout_get(Object *obj, Error **errp)
{
    (void)errp;
    return RTL8169(obj)->mac_timeout;
}

static void rtl_mac_timeout_set(Object *obj, bool value, Error **errp)
{
    (void)errp;
    RTL8169(obj)->mac_timeout = value;
}

static bool rtl_management_timeout_get(Object *obj, Error **errp)
{
    (void)errp;
    return RTL8169(obj)->management_timeout;
}

static void rtl_management_timeout_set(Object *obj, bool value, Error **errp)
{
    (void)errp;
    RTL8169(obj)->management_timeout = value;
}

static bool rtl_tx_stall_get(Object *obj, Error **errp)
{
    (void)errp;
    return RTL8169(obj)->tx_stall_until_reset;
}

static void rtl_tx_stall_set(Object *obj, bool value, Error **errp)
{
    (void)errp;
    RTL8169(obj)->tx_stall_until_reset = value;
}

static void rtl8169_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->realize = pci_rtl8169_realize;
    k->exit = pci_rtl8169_exit;
    k->vendor_id = PCI_VENDOR_ID_REALTEK;
    k->device_id = 0x8169;
    k->revision = 0x10;
    k->class_id = PCI_CLASS_NETWORK_ETHERNET;
    dc->reset = rtl8169_reset;
    dc->vmsd = &vmstate_rtl8169;
    device_class_set_props(dc, rtl8169_properties);
    object_class_property_add_bool(klass, "phy-timeout",
                                   rtl_phy_timeout_get, rtl_phy_timeout_set);
    object_class_property_add_bool(klass, "mac-timeout",
                                   rtl_mac_timeout_get, rtl_mac_timeout_set);
    object_class_property_add_bool(klass, "management-timeout",
                                   rtl_management_timeout_get, rtl_management_timeout_set);
    object_class_property_add_bool(klass, "tx-stall-until-reset",
                                   rtl_tx_stall_get, rtl_tx_stall_set);
    set_bit(DEVICE_CATEGORY_NETWORK, dc->categories);
}

static const TypeInfo rtl8169_info = {
    .name          = TYPE_RTL8169,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(RTL8169State),
    .class_init    = rtl8169_class_init,
    .interfaces = (InterfaceInfo[]) {
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { INTERFACE_PCIE_DEVICE },
        { },
    },
};

static void rtl_pcie_realize(PCIDevice *dev, Error **errp)
{
    switch (PCI_DEVICE_GET_CLASS(dev)->device_id) {
    case 0x8168: RTL8169(dev)->variant = VAR_8168; break;
    case 0x8125: RTL8169(dev)->variant = VAR_8125; break;
    case 0x8126: RTL8169(dev)->variant = VAR_8126; break;
    case 0x8127: RTL8169(dev)->variant = VAR_8127; break;
    default:
        error_setg(errp, "Unsupported Realtek PCIe identity");
        return;
    }
    pci_rtl8169_realize(dev, errp);
}

static void rtl_pcie_class_init(ObjectClass *klass, void *data)
{
    PCI_DEVICE_CLASS(klass)->realize = rtl_pcie_realize;
    PCI_DEVICE_CLASS(klass)->device_id = (uintptr_t)data;
}

static void rtl_pcie_instance_init(Object *obj)
{
    PCI_DEVICE(obj)->cap_present |= QEMU_PCI_CAP_EXPRESS;
}

static const TypeInfo rtl_pcie_info[] = {
    { .name = "rtl8168", .parent = TYPE_RTL8169,
      .class_init = rtl_pcie_class_init, .class_data = (void *)0x8168,
      .instance_init = rtl_pcie_instance_init },
    { .name = "rtl8125", .parent = TYPE_RTL8169,
      .class_init = rtl_pcie_class_init, .class_data = (void *)0x8125,
      .instance_init = rtl_pcie_instance_init },
    { .name = "rtl8126", .parent = TYPE_RTL8169,
      .class_init = rtl_pcie_class_init, .class_data = (void *)0x8126,
      .instance_init = rtl_pcie_instance_init },
    { .name = "rtl8127", .parent = TYPE_RTL8169,
      .class_init = rtl_pcie_class_init, .class_data = (void *)0x8127,
      .instance_init = rtl_pcie_instance_init },
};

static void rtl8169_register_types(void)
{
    type_register_static(&rtl8169_info);
    type_register_static_array(rtl_pcie_info, ARRAY_SIZE(rtl_pcie_info));
}

type_init(rtl8169_register_types)
