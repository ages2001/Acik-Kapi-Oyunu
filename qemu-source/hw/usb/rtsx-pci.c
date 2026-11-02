/*
 * Realtek PCIe card reader (RTS5209 .. RTS5264, RTL8411 family) - test model
 * for the USBNT rtsxpci driver.
 *
 *   -device rtsx-pci,id=rts[,device-id=0x5227]
 *   -drive if=none,id=c,file=card.img,format=raw -device sd-card,drive=c,bus=rts.0
 *
 * MMIO: HCBAR/HCBCTLR (command buffer with auto response), HDBAR/HDBCTLR
 * (ADMA descriptor list), HAIMR (single register access), BIPR (pending:
 * card present, transfer ok / fail, card change), BIER.  Internal registers
 * are plain storage except the SD engine (rtsx-engine.h) and the PHY window.
 * A command buffer run that starts a ring buffer transfer finishes after the
 * DMA run that moves its data; then TRANS_OK (or TRANS_FAIL) is set.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 */

#include "qemu/osdep.h"
#include "qemu/module.h"
#include "qemu/timer.h"
#include "qapi/error.h"
#include "hw/qdev-properties.h"
#include "hw/pci/pci_device.h"
#include "sysemu/dma.h"
#include "qom/object.h"
#include "rtsx-engine.h"

#define TYPE_RTSX_PCI "rtsx-pci"
OBJECT_DECLARE_SIMPLE_TYPE(RtsxPciState, RTSX_PCI)

#define R(a)            (0xC000 | ((a) & 0x3FFF))
#define P_DATA_SOURCE   0xFD5B
#define P_CARD_STOP     0xFD54
#define P_PHYRWCTL      0xFE3C
#define P_PHYDATA0      0xFE3D
#define P_PHYDATA1      0xFE3E
#define P_PHYADDR       0xFE3F

#define B_SD_EXIST      0x00010000u
#define B_SD_INT        0x02000000u
#define B_TRANS_FAIL    0x10000000u
#define B_TRANS_OK      0x20000000u

struct RtsxPciState {
    PCIDevice parent_obj;
    MemoryRegion mmio;
    RtsxEngine e;
    SDBus sdbus;
    uint16_t device_id;
    uint32_t hcbar, hcbctlr, hdbar, hdbctlr, haimr, bipr, bier, dum;
    uint16_t phy[256];
    /* command buffer run in progress */
    uint32_t cmds[4096];
    uint32_t ncmd, next, out;
    bool running, failed;
    bool last_inserted;
    bool sg_long;
    uint32_t runs, dmas, fails;
};

static void rtsx_phy(RtsxPciState *s, uint8_t v)
{
    RtsxEngine *e = &s->e;
    uint8_t a = e->regs[R(P_PHYADDR)];

    if (v & 0x80) {
        if (v & 0x01) {
            s->phy[a] = e->regs[R(P_PHYDATA0)] | (e->regs[R(P_PHYDATA1)] << 8);
        } else {
            e->regs[R(P_PHYDATA0)] = s->phy[a];
            e->regs[R(P_PHYDATA1)] = s->phy[a] >> 8;
        }
    }
    e->regs[R(P_PHYRWCTL)] = v & 0x7F;
}

static void reg_write(RtsxPciState *s, uint16_t reg, uint8_t mask, uint8_t data)
{
    if (reg == R(P_PHYRWCTL)) {
        rtsx_phy(s, (s->e.regs[reg] & ~mask) | (data & mask));
        return;
    }
    rtsx_reg_write(&s->e, reg, mask, data);
}

static void run_cmds(RtsxPciState *s)
{
    PCIDevice *pd = PCI_DEVICE(s);
    uint8_t rsp[4096];
    uint32_t nr = 0;

    while (s->next < s->ncmd) {
        uint32_t v = s->cmds[s->next];
        int type = v >> 30;
        uint16_t reg = R((v >> 16) & 0x3FFF);
        uint8_t mask = v >> 8, data = v;

        if (s->e.pending) {
            break;
        }
        switch (type) {
        case 0:
            rsp[nr++] = rtsx_reg_read(&s->e, reg);
            break;
        case 1:
            reg_write(s, reg, mask, data);
            if (reg == R(RX_SD_TRANSFER) && (s->e.regs[reg] & RX_ERR)) {
                s->failed = true;
            }
            break;
        case 2:
            if ((rtsx_reg_read(&s->e, reg) & mask) != (data & mask)) {
                s->failed = true;
            }
            rsp[nr++] = rtsx_reg_read(&s->e, reg);
            break;
        default:
            break;
        }
        s->next++;
    }
    /* auto response: the read results go back to the command buffer */
    if (nr && (s->hcbctlr & 0x40000000u)) {
        pci_dma_write(pd, s->hcbar + s->out, rsp, nr);
        s->out += nr;
    }
    if (s->next >= s->ncmd && s->running) {
        s->running = false;
        if (s->failed) {
            s->bipr |= B_TRANS_FAIL;
            s->fails++;
        } else {
            s->bipr |= B_TRANS_OK;
        }
    }
}

static void start_cmds(RtsxPciState *s)
{
    PCIDevice *pd = PCI_DEVICE(s);
    uint32_t len = s->hcbctlr & 0x00FFFFFF;
    uint32_t i;
    uint8_t buf[16384];

    len = MIN(len, sizeof(buf));
    pci_dma_read(pd, s->hcbar, buf, len);
    s->ncmd = len / 4;
    for (i = 0; i < s->ncmd; i++) {
        s->cmds[i] = ldl_le_p(buf + i * 4);
    }
    s->next = 0;
    s->out = 0;
    s->failed = false;
    s->running = true;
    s->runs++;
    run_cmds(s);
}

static uint64_t ldq_le_pci_dma_ret(PCIDevice *pd, dma_addr_t a)
{
    uint8_t b[8];

    pci_dma_read(pd, a, b, 8);
    return ldq_le_p(b);
}

static void start_dma(RtsxPciState *s)
{
    PCIDevice *pd = PCI_DEVICE(s);
    bool from_card = s->hdbctlr & (1u << 29);
    uint32_t pos = 0, total = from_card ? s->e.ring_len : s->e.pend_bytes;
    uint8_t *wb = NULL;
    int k;

    s->dmas++;
    if (s->e.pending != (from_card ? 1 : 2)) {
        s->bipr |= B_TRANS_FAIL;
        s->fails++;
        return;
    }
    if (!from_card) {
        wb = g_malloc0(total);
    }
    for (k = 0; k < 256 && pos < total; k++) {
        uint64_t d = ldq_le_pci_dma_ret(pd, s->hdbar + k * 8);
        uint32_t lo = d, addr = d >> 32;
        uint32_t len = s->sg_long ? (lo >> 16) : (lo >> 12);
        if (!(lo & 1)) {
            break;
        }
        if (len == 0) {
            len = s->sg_long ? 0x10000 : 0x100000;
        }
        len = MIN(len, total - pos);
        if (lo & 0x20) {
            if (from_card) {
                pci_dma_write(pd, addr, s->e.ring + pos, len);
            } else {
                pci_dma_read(pd, addr, wb + pos, len);
            }
            pos += len;
        }
        if (lo & 2) {
            break;
        }
    }
    if (from_card) {
        rtsx_dma_read_done(&s->e);
    } else {
        rtsx_dma_write(&s->e, wb, pos);
        g_free(wb);
    }
    if (pos < total) {
        s->failed = true;
    }
    run_cmds(s);
}

static uint64_t rtsx_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    RtsxPciState *s = opaque;
    bool in = sdbus_get_inserted(&s->sdbus);

    if (in != s->last_inserted) {
        s->last_inserted = in;
        s->bipr |= B_SD_INT;
    }
    switch (addr) {
    case 0x00: return s->hcbar;
    case 0x04: return s->hcbctlr & ~0x80000000u;
    case 0x08: return s->hdbar;
    case 0x0C: return s->hdbctlr & ~0x80000000u;
    case 0x10: return s->haimr;
    case 0x14: return (s->bipr & ~B_SD_EXIST) | (in ? B_SD_EXIST : 0);
    case 0x18: return s->bier;
    case 0x1C: return s->dum;
    default: return 0;
    }
}

static void rtsx_mmio_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    RtsxPciState *s = opaque;
    uint32_t v = val;

    switch (addr) {
    case 0x00: s->hcbar = v; break;
    case 0x04:
        s->hcbctlr = v;
        if (v & 0x10000000u) {              /* STOP_CMD */
            s->running = false;
            s->e.pending = 0;
        } else if (v & 0x80000000u) {
            start_cmds(s);
        }
        break;
    case 0x08: s->hdbar = v; break;
    case 0x0C:
        s->hdbctlr = v;
        if (v & 0x10000000u) {              /* STOP_DMA */
            break;
        }
        if (v & 0x80000000u) {
            start_dma(s);
        }
        break;
    case 0x10:
        if (v & 0x80000000u) {
            uint16_t a = R((v >> 16) & 0x3FFF);
            if (v & 0x40000000u) {
                reg_write(s, a, v >> 8, v);
            }
            s->haimr = (v & 0x7FFFFF00u) | rtsx_reg_read(&s->e, a);
        }
        break;
    case 0x14: s->bipr &= ~(v & ~B_SD_EXIST); break;
    case 0x18: s->bier = v; break;
    case 0x1C: s->dum = v; break;
    default: break;
    }
}

static const MemoryRegionOps rtsx_mmio_ops = {
    .read = rtsx_mmio_read,
    .write = rtsx_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = { .min_access_size = 4, .max_access_size = 4 },
};

static void rtsx_reset(DeviceState *d)
{
    RtsxPciState *s = RTSX_PCI(d);

    rtsx_engine_init(&s->e, &s->sdbus, R(P_DATA_SOURCE), R(P_CARD_STOP));
    s->hcbar = s->hcbctlr = s->hdbar = s->hdbctlr = s->haimr = s->bipr = s->bier = s->dum = 0;
    s->running = false;
    s->last_inserted = sdbus_get_inserted(&s->sdbus);
    memset(s->phy, 0, sizeof(s->phy));
}

static void rtsx_realize(PCIDevice *pd, Error **errp)
{
    RtsxPciState *s = RTSX_PCI(pd);
    int bar = (s->device_id == 0x525A || s->device_id == 0x5264) ? 1 : 0;

    pci_config_set_device_id(pd->config, s->device_id);
    pci_config_set_class(pd->config, 0xFF00);
    pd->config[PCI_INTERRUPT_PIN] = 1;
    s->sg_long = (s->device_id == 0x5261 || s->device_id == 0x5228);
    memory_region_init_io(&s->mmio, OBJECT(s), &rtsx_mmio_ops, s, "rtsx-mmio", 0x1000);
    pci_register_bar(pd, bar, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->mmio);
    qbus_init(&s->sdbus, sizeof(s->sdbus), TYPE_SD_BUS, DEVICE(pd), NULL);
}

static Property rtsx_props[] = {
    DEFINE_PROP_UINT16("device-id", RtsxPciState, device_id, 0x5227),
    DEFINE_PROP_UINT32("runs", RtsxPciState, runs, 0),
    DEFINE_PROP_UINT32("dmas", RtsxPciState, dmas, 0),
    DEFINE_PROP_UINT32("fails", RtsxPciState, fails, 0),
    DEFINE_PROP_END_OF_LIST(),
};

static void rtsx_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->realize = rtsx_realize;
    k->vendor_id = 0x10EC;
    k->device_id = 0x5227;
    k->class_id = 0xFF00;
    dc->reset = rtsx_reset;
    dc->desc = "Realtek PCIe card reader (test model)";
    device_class_set_props(dc, rtsx_props);
    set_bit(DEVICE_CATEGORY_STORAGE, dc->categories);
}

static const TypeInfo rtsx_info = {
    .name = TYPE_RTSX_PCI,
    .parent = TYPE_PCI_DEVICE,
    .instance_size = sizeof(RtsxPciState),
    .class_init = rtsx_class_init,
    .interfaces = (InterfaceInfo[]) {
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { },
    },
};

static void rtsx_register(void)
{
    type_register_static(&rtsx_info);
}

type_init(rtsx_register)
