/* PCI bootstrap and aperture model for Windows display-miniport bring-up.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "qemu/osdep.h"
#include "qemu/timer.h"
#include "qemu/bswap.h"
#include "qemu/units.h"
#include "qemu/host-utils.h"
#include "qemu/log.h"
#include "qapi/error.h"
#include "hw/pci/pci_device.h"
#include "hw/pci/pcie.h"
#include "hw/qdev-properties.h"
#include "migration/vmstate.h"

#define TYPE_WXP_BOARD "winxpdm-probe-gpu"
OBJECT_DECLARE_SIMPLE_TYPE(WxpBoard, WXP_BOARD)
#define VRAM_SIZE (256 * MiB)

typedef struct {
    const char *name;
    uint16_t vendor, device, subsystem_vendor, subsystem_device, chip;
} BoardModel;

/* Subsystem identities come from pci.ids where a specific board is named.
   Other profiles use the chip vendor and device as a laboratory subsystem. */
static const BoardModel boards[] = {
    {"gtx950",0x10de,0x1402,0,0,0x126}, {"gtx960",0x10de,0x1401,0,0,0x126},
    {"gtx970",0x10de,0x13c2,0,0,0x124}, {"gtx980",0x10de,0x13c0,0x1043,0x8504,0x124},
    {"gtx980ti",0x10de,0x17c8,0,0,0x120}, {"gtx1050",0x10de,0x1c81,0,0,0x137},
    {"gtx1050ti",0x10de,0x1c82,0x1043,0x8613,0x137},
    {"gtx1060-gp104",0x10de,0x1b83,0,0,0x134}, {"gtx1060",0x10de,0x1c03,0,0,0x136},
    {"gtx1070",0x10de,0x1b81,0,0,0x134}, {"gtx1070ti",0x10de,0x1b82,0,0,0x134},
    {"gtx1080",0x10de,0x1b80,0,0,0x134}, {"gtx1080ti",0x10de,0x1b06,0,0,0x132},
    {"rx9060xt",0x1002,0x7590,0x1043,0x0639,0x1200},
    {"rx9070xt",0x1002,0x7550,0x1458,0x2437,0x1201},
    {"r9700",0x1002,0x7551,0,0,0x1201},
};

struct WxpBoard {
    PCIDevice parent_obj;
    const BoardModel *board;
    char *model;
    MemoryRegion registers, aperture, auxiliary, ports;
    uint32_t index, index_hi, scratch, numerator, denominator;
    uint32_t straps[2];
    uint64_t tick_epoch, tick_value;
    uint64_t mmio_reads, mmio_writes, identity_reads, unknown_reads, unknown_writes;
    uint64_t first_unknown_read, first_unknown_write;
};

static uint64_t ticks(WxpBoard *s)
{
    uint64_t elapsed = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) - s->tick_epoch;
    return s->tick_value + (s->numerator ? muldiv64(elapsed, s->denominator, s->numerator) : 0);
}

static void set_ticks(WxpBoard *s, uint64_t value)
{
    s->tick_epoch = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    s->tick_value = value;
}

static void unknown_access(WxpBoard *s, bool write, hwaddr offset)
{
    qemu_log_mask(LOG_UNIMP, "winxpdm %s unimplemented %s at 0x%" PRIx64 "\n",
                  s->board->name, write ? "write" : "read", (uint64_t)offset);
    if (write) {
        if (!s->unknown_writes++) s->first_unknown_write = offset;
    } else {
        if (!s->unknown_reads++) s->first_unknown_read = offset;
    }
}

static uint32_t read32(WxpBoard *s, hwaddr offset)
{
    if (s->board->vendor == 0x10de) {
        switch (offset) {
        case 0:
            s->identity_reads++;
            return ((uint32_t)s->board->chip << 20) | ((s->board->device & 0x3f) << 12) | 0xa1;
        case 0x008: return 0; /* BOOT_2 strap for the modeled board. */
        case 0xa00:
            s->identity_reads++;
            return ((uint32_t)s->board->chip << 20) | (0xa1 << 12) | (s->board->device & 0x1f);
        case 0x9200: return s->numerator;
        case 0x9210: return s->denominator;
        case 0x9400: return (uint32_t)ticks(s) & ~31U;
        case 0x9410: return ticks(s) >> 32;
        case 0x101000: return s->straps[0];
        case 0x10100c: return s->straps[1];
        }
        if (offset >= 0x88000 && offset + 4 <= 0x88000 + pci_config_size(&s->parent_obj)) {
            return pci_default_read_config(&s->parent_obj, offset - 0x88000, 4);
        }
    } else {
        uint32_t addr = s->index & 0x7fffffff;
        switch (offset) {
        case 0: return s->index;
        case 0x18: return s->index_hi;
        case 4:
            if ((s->index >> 31) && !s->index_hi && addr <= VRAM_SIZE - 4 && !(addr & 3))
                return ldl_le_p((uint8_t *)memory_region_get_ram_ptr(&s->aperture) + addr);
            break;
        case 0x94 * 4: return s->scratch;
        case 0xde3 * 4:
            s->identity_reads++;
            return VRAM_SIZE >> 20;
        }
    }
    unknown_access(s, false, offset);
    return 0;
}

static uint64_t read_mmio(void *opaque, hwaddr offset, unsigned size)
{
    WxpBoard *s = opaque;
    s->mmio_reads++;
    return read32(s, offset);
}

static void write_mmio(void *opaque, hwaddr offset, uint64_t data, unsigned size)
{
    WxpBoard *s = opaque;
    uint32_t value = data;
    s->mmio_writes++;
    if (s->board->vendor == 0x10de) {
        switch (offset) {
        case 0: return;
        case 0x101000:
        case 0x10100c:
            /* Bit 31 overrides the pin sample. Clearing it restores the lab's reset straps. */
            s->straps[offset == 0x10100c] = value & (1U << 31) ? value : 0;
            return;
        case 0x9200: set_ticks(s, ticks(s)); s->numerator = value & 0xffff; return;
        case 0x9210: set_ticks(s, ticks(s)); s->denominator = value & 0xffff; return;
        case 0x9400: set_ticks(s, (ticks(s) & ~UINT64_C(0xffffffff)) | (value & ~31U)); return;
        case 0x9410: set_ticks(s, (uint64_t)value << 32 | (uint32_t)ticks(s)); return;
        }
    } else {
        uint32_t addr = s->index & 0x7fffffff;
        switch (offset) {
        case 0: s->index = value; return;
        case 0x18: s->index_hi = value; return;
        case 4:
            if ((s->index >> 31) && !s->index_hi && addr <= VRAM_SIZE - 4 && !(addr & 3)) {
                stl_le_p((uint8_t *)memory_region_get_ram_ptr(&s->aperture) + addr, value);
                memory_region_set_dirty(&s->aperture, addr, 4);
                return;
            }
            break;
        case 0x94 * 4: s->scratch = value; return;
        case 0xde3 * 4: return;
        }
    }
    unknown_access(s, true, offset);
}

static const MemoryRegionOps mmio = {
    .read = read_mmio, .write = write_mmio, .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = { .min_access_size = 4, .max_access_size = 4 },
};

static uint64_t read_aux(void *opaque, hwaddr offset, unsigned size)
{
    unknown_access(opaque, false, UINT64_C(0x100000000) + offset);
    return 0;
}
static void write_aux(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    unknown_access(opaque, true, UINT64_C(0x100000000) + offset);
}
static const MemoryRegionOps aux = {
    .read = read_aux, .write = write_aux, .endianness = DEVICE_LITTLE_ENDIAN,
};

static void reset(DeviceState *dev)
{
    WxpBoard *s = WXP_BOARD(dev);
    s->numerator = s->denominator = 1;
    s->index = s->index_hi = s->scratch = 0;
    s->straps[0] = s->straps[1] = 0;
    set_ticks(s, 0);
}

static void realize(PCIDevice *dev, Error **errp)
{
    WxpBoard *s = WXP_BOARD(dev);
    Object *obj = OBJECT(dev);
    const BoardModel *m = NULL;
    for (unsigned i = 0; i < ARRAY_SIZE(boards); i++) {
        if (s->model && !strcmp(s->model, boards[i].name)) {
            m = &boards[i];
            break;
        }
    }
    if (!m) {
        error_setg(errp, "Unknown WINXPDM PCI profile '%s'", s->model ? s->model : "");
        return;
    }
    s->board = m;
    bool nv = m->vendor == 0x10de;
    pci_set_word(dev->config + PCI_VENDOR_ID, m->vendor);
    pci_set_word(dev->config + PCI_DEVICE_ID, m->device);
    pci_set_word(dev->config + PCI_SUBSYSTEM_VENDOR_ID, m->subsystem_vendor ? m->subsystem_vendor : m->vendor);
    pci_set_word(dev->config + PCI_SUBSYSTEM_ID, m->subsystem_device ? m->subsystem_device : m->device);
    pci_config_set_revision(dev->config, nv ? 0xa1 : 0xc0);
    pci_config_set_interrupt_pin(dev->config, 1);
    if (pci_bus_is_express(pci_get_bus(dev))) {
        if (pcie_endpoint_cap_init(dev, 0x80) < 0) {
            error_setg(errp, "Cannot add PCI Express endpoint capability");
            return;
        }
    } else {
        dev->cap_present &= ~QEMU_PCI_CAP_EXPRESS;
    }
    if (!memory_region_init_ram(&s->aperture, obj, "winxpdm-gpu-aperture", VRAM_SIZE, errp)) return;
    memory_region_init_io(&s->registers, obj, &mmio, s, "winxpdm-gpu-registers", nv ? 16*MiB : MiB);
    memory_region_init_io(&s->auxiliary, obj, &aux, s, "winxpdm-gpu-aux", nv ? 32*MiB : 2*MiB);
    memory_region_init_io(&s->ports, obj, &aux, s, "winxpdm-gpu-io", nv ? 128 : 256);
    unsigned prefetch = PCI_BASE_ADDRESS_MEM_TYPE_64 | PCI_BASE_ADDRESS_MEM_PREFETCH;
    pci_register_bar(dev, nv ? 1 : 0, prefetch, &s->aperture);
    pci_register_bar(dev, nv ? 3 : 2, prefetch, &s->auxiliary);
    pci_register_bar(dev, nv ? 0 : 5, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->registers);
    pci_register_bar(dev, nv ? 5 : 4, PCI_BASE_ADDRESS_SPACE_IO, &s->ports);
    object_property_add_uint64_ptr(obj,"mmio-reads",&s->mmio_reads,OBJ_PROP_FLAG_READ);
    object_property_add_uint64_ptr(obj,"mmio-writes",&s->mmio_writes,OBJ_PROP_FLAG_READ);
    object_property_add_uint64_ptr(obj,"identity-reads",&s->identity_reads,OBJ_PROP_FLAG_READ);
    object_property_add_uint64_ptr(obj,"unknown-reads",&s->unknown_reads,OBJ_PROP_FLAG_READ);
    object_property_add_uint64_ptr(obj,"unknown-writes",&s->unknown_writes,OBJ_PROP_FLAG_READ);
    object_property_add_uint64_ptr(obj,"first-unknown-read",&s->first_unknown_read,OBJ_PROP_FLAG_READ);
    object_property_add_uint64_ptr(obj,"first-unknown-write",&s->first_unknown_write,OBJ_PROP_FLAG_READ);
}

static const Property board_props[] = { DEFINE_PROP_STRING("model", WxpBoard, model) };
static const VMStateDescription migration = {.name = TYPE_WXP_BOARD, .unmigratable = 1};
static void board_init(Object *obj)
{
    /* PCI allocates configuration space before realize chooses the parent bus. */
    PCI_DEVICE(obj)->cap_present |= QEMU_PCI_CAP_EXPRESS;
}

static void board_class(ObjectClass *klass, void *data)
{
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);
    pc->realize = realize;
    pc->class_id = PCI_CLASS_DISPLAY_VGA;
    dc->vmsd = &migration;
    device_class_set_props(dc, board_props);
    device_class_set_legacy_reset(dc, reset);
    set_bit(DEVICE_CATEGORY_DISPLAY, dc->categories);
}
static const TypeInfo info = {
    .name = TYPE_WXP_BOARD, .parent = TYPE_PCI_DEVICE, .instance_size = sizeof(WxpBoard),
    .instance_init = board_init, .class_init = board_class, .interfaces = (InterfaceInfo[]){
        { INTERFACE_CONVENTIONAL_PCI_DEVICE }, { INTERFACE_PCIE_DEVICE }, {}
    }
};
static void register_board(void) { type_register_static(&info); }
type_init(register_board)
