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
#include "winxpdm-sdma.h"

#define TYPE_WXP_BOARD "winxpdm-probe-gpu"
OBJECT_DECLARE_SIMPLE_TYPE(WxpBoard, WXP_BOARD)
#define VRAM_SIZE (256 * MiB)
#define PROM_SIZE MiB
#define PROM_START 0x300000
#define VBIOS_SHADOW (VRAM_SIZE - PROM_SIZE)
/* GC 12 segment 0 starts at byte offset 0x4980. Queue 0 starts at dword 0x80. */
#define SDMA_RB_CNTL (0x4980 + 0x80 * 4)
enum { RB_CONTROL, RB_BASE, RB_BASE_HI, RB_RPTR, RB_RPTR_HI,
       RB_WPTR, RB_WPTR_HI, RB_WB, RB_WB_HI, RB_REGS };

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
    uint16_t subsystem_vendor, subsystem_device;
    MemoryRegion registers, aperture, auxiliary, ports;
    uint32_t index, index_hi, scratch, numerator, denominator, pramin;
    uint32_t straps[2], ring[RB_REGS];
    QEMUTimer *sdma_timer;
    uint64_t tick_epoch, tick_value;
    uint64_t mmio_reads, mmio_writes, identity_reads, unknown_reads, unknown_writes;
    uint64_t first_unknown_read, first_unknown_write;
    uint64_t prom_reads, shadow_reads;
};

static bool prom_window(WxpBoard *s, hwaddr offset, unsigned size)
{
    return s->board->vendor == 0x10de && offset >= PROM_START &&
           offset + size <= PROM_START + PROM_SIZE;
}

static uint32_t read_prom(WxpBoard *s, hwaddr offset, unsigned size)
{
    PCIDevice *dev = &s->parent_obj;
    uint32_t result = UINT32_MAX;
    if (dev->has_rom && offset + size <= dev->romsize) {
        const uint8_t *rom = memory_region_get_ram_ptr(&dev->rom);
        result = ldn_le_p(rom + offset, size);
    }
    return result;
}

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

/* Nouveau's nv50_instobj slow path selects a physical window in BAR0.
   Only the local VRAM target is present on this model. */
static uint8_t *pramin_pointer(WxpBoard *s, hwaddr offset, hwaddr *address)
{
    if (s->pramin >> 24) {
        return NULL;
    }
    *address = ((uint64_t)s->pramin << 16) + offset - 0x700000;
    if (*address > VRAM_SIZE - 4) {
        return NULL;
    }
    return (uint8_t *)memory_region_get_ram_ptr(&s->aperture) + *address;
}

static uint64_t ring_u64(const uint32_t *r, unsigned low)
{
    return r[low] | ((uint64_t)r[low + 1] << 32);
}

/* One queue and a linear local-VRAM address space. A rejected packet stays at
   RPTR. A partial packet can be completed by publishing a later WPTR. */
static void sdma_run(void *opaque)
{
    WxpBoard *s = opaque;
    uint32_t *r = s->ring, order = (r[RB_CONTROL] >> 1) & 31;
    uint64_t rd = ring_u64(r, RB_RPTR), wr = ring_u64(r, RB_WPTR);
    uint64_t units = ring_u64(r, RB_BASE), size = UINT64_C(4) << order;
    uint64_t wb = ring_u64(r, RB_WB);
    bool writeback = (r[RB_CONTROL] & 0x1000) != 0;
    uint32_t packet[8], dst, length, work = 0;
    uint8_t *vram;
    if (!(r[RB_CONTROL] & 1) || rd == wr) {
        return;
    }
    if (units >= VRAM_SIZE / 256 || size > VRAM_SIZE - units * 256 ||
        (rd | wr) & 3 || wr - rd > size ||
        (writeback && ((wb & 3) || wb > VRAM_SIZE - 8))) {
        qemu_log_mask(LOG_GUEST_ERROR, "winxpdm: invalid SDMA ring configuration\n");
        return;
    }
    vram = memory_region_get_ram_ptr(&s->aperture);
    uint8_t *ring = vram + units * 256;
    for (unsigned packets = 0; rd != wr && packets < 32 && work < MiB; ++packets) {
        uint32_t header = ldl_le_p(ring + (rd & (size - 1)));
        bool nop = !(header & 0xc000ffff);
        unsigned words = nop ? 1 + (header >> 16) : wxp_sdma_words(header);
        if (!words) {
            qemu_log_mask(LOG_UNIMP, "winxpdm: unsupported SDMA packet 0x%08x\n", header);
            return;
        }
        if ((uint64_t)words * 4 > wr - rd) {
            return;
        }
        if (!nop) {
            for (unsigned i = 0; i < words; ++i) {
                packet[i] = ldl_le_p(ring + ((rd + i * 4) & (size - 1)));
            }
            if (!wxp_sdma_execute(vram, VRAM_SIZE, packet, &dst, &length)) {
                qemu_log_mask(LOG_GUEST_ERROR, "winxpdm: invalid SDMA local VRAM transfer\n");
                return;
            }
            memory_region_set_dirty(&s->aperture, dst, length);
            work += length;
        }
        rd += 4 * words;
        r[RB_RPTR] = rd;
        r[RB_RPTR_HI] = rd >> 32;
        if (writeback) {
            stq_le_p(vram + wb, rd);
            memory_region_set_dirty(&s->aperture, wb, 8);
        }
    }
    if (rd != wr) {
        timer_mod(s->sdma_timer, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 1000);
    }
}

static uint32_t read32(WxpBoard *s, hwaddr offset)
{
    if (s->board->vendor == 0x10de) {
        switch (offset) {
        case 0:
            s->identity_reads++;
            return ((uint32_t)s->board->chip << 20) | ((s->board->device & 0x3f) << 12) | 0xa1;
        case 0x004: return 0; /* PMC_ENDIAN on the little-endian x86 board. */
        case 0x021c04: return 0; /* This GM/GP model has a display engine. */
        case 0x619f04:
            return s->parent_obj.has_rom ? (VBIOS_SHADOW >> 8) | 9 : 0;
        case 0x001700: return s->pramin;
        case 0x610018:
            /* Model PDISPLAY's free-running counter at the reset crystal rate. */
            return muldiv64(qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL), 135, 10000);
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
        if (offset >= 0x700000 && offset < 0x800000) {
            hwaddr address;
            const uint8_t *p = pramin_pointer(s, offset, &address);
            if (p && address >= VBIOS_SHADOW && s->parent_obj.has_rom) {
                s->shadow_reads++;
            }
            return p ? ldl_le_p(p) : UINT32_MAX;
        }
    } else {
        uint32_t addr = s->index & 0x7fffffff;
        if (offset >= SDMA_RB_CNTL && offset < SDMA_RB_CNTL + RB_REGS * 4) {
            return s->ring[(offset - SDMA_RB_CNTL) / 4];
        }
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

static bool config_mirror(WxpBoard *s, hwaddr offset, unsigned size)
{
    return s->board->vendor == 0x10de && offset >= 0x88000 &&
           offset + size <= 0x88000 + pci_config_size(&s->parent_obj);
}

static bool valid_mmio(void *opaque, hwaddr offset, unsigned size, bool write, MemTxAttrs attrs)
{
    return size == 4 || config_mirror(opaque, offset, size) || prom_window(opaque, offset, size);
}

static uint64_t read_mmio(void *opaque, hwaddr offset, unsigned size)
{
    WxpBoard *s = opaque;
    s->mmio_reads++;
    if (prom_window(s, offset, size)) {
        s->prom_reads++;
        return read_prom(s, offset - PROM_START, size);
    }
    if (config_mirror(s, offset, size)) {
        return pci_default_read_config(&s->parent_obj, offset - 0x88000, size);
    }
    return read32(s, offset);
}

static void write_mmio(void *opaque, hwaddr offset, uint64_t data, unsigned size)
{
    WxpBoard *s = opaque;
    uint32_t value = data;
    s->mmio_writes++;
    if (prom_window(s, offset, size)) {
        return;
    }
    if (config_mirror(s, offset, size)) {
        pci_default_write_config(&s->parent_obj, offset - 0x88000, value, size);
        return;
    }
    if (s->board->vendor == 0x10de) {
        if (offset >= 0x700000 && offset < 0x800000) {
            hwaddr address;
            uint8_t *p = pramin_pointer(s, offset, &address);
            if (p) {
                stl_le_p(p, value);
                memory_region_set_dirty(&s->aperture, address, sizeof(value));
            }
            return;
        }
        switch (offset) {
        case 0: return;
        case 0x001700: s->pramin = value & 0x03ffffff; return;
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
        if (offset >= SDMA_RB_CNTL && offset < SDMA_RB_CNTL + RB_REGS * 4) {
            unsigned reg = (offset - SDMA_RB_CNTL) / 4;
            s->ring[reg] = value;
            if (reg == RB_WPTR || reg == RB_WPTR_HI || reg == RB_CONTROL) {
                sdma_run(s);
            }
            return;
        }
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
    .valid = { .min_access_size = 1, .max_access_size = 4, .accepts = valid_mmio },
    .impl = { .min_access_size = 1, .max_access_size = 4 },
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
    s->index = s->index_hi = s->scratch = s->pramin = 0;
    s->straps[0] = s->straps[1] = 0;
    if (s->board->vendor == 0x10de && s->parent_obj.has_rom) {
        PCIDevice *pci = &s->parent_obj;
        uint8_t *shadow = (uint8_t *)memory_region_get_ram_ptr(&s->aperture) + VBIOS_SHADOW;
        size_t size = MIN((uint64_t)pci->romsize, (uint64_t)PROM_SIZE);
        memcpy(shadow, memory_region_get_ram_ptr(&pci->rom), size);
        memory_region_set_dirty(&s->aperture, VBIOS_SHADOW, size);
    }
    memset(s->ring, 0, sizeof(s->ring));
    if (s->sdma_timer) {
        timer_del(s->sdma_timer);
    }
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
    if (!!s->subsystem_vendor != !!s->subsystem_device) {
        error_setg(errp, "Set both subsystem-vendor-id and subsystem-id for an OEM board");
        return;
    }
    pci_set_word(dev->config + PCI_VENDOR_ID, m->vendor);
    pci_set_word(dev->config + PCI_DEVICE_ID, m->device);
    pci_set_word(dev->config + PCI_SUBSYSTEM_VENDOR_ID,
                 s->subsystem_vendor ? s->subsystem_vendor : (m->subsystem_vendor ? m->subsystem_vendor : m->vendor));
    pci_set_word(dev->config + PCI_SUBSYSTEM_ID,
                 s->subsystem_device ? s->subsystem_device : (m->subsystem_device ? m->subsystem_device : m->device));
    if (nv) {
        /* Nouveau switches PCI ROM shadow control through this vendor bit. */
        dev->wmask[0x50] = 1;
        dev->config[0x50] = 1;
    }
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
    if (!nv) {
        s->sdma_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, sdma_run, s);
    }
    object_property_add_uint64_ptr(obj,"mmio-reads",&s->mmio_reads,OBJ_PROP_FLAG_READ);
    object_property_add_uint64_ptr(obj,"mmio-writes",&s->mmio_writes,OBJ_PROP_FLAG_READ);
    object_property_add_uint64_ptr(obj,"identity-reads",&s->identity_reads,OBJ_PROP_FLAG_READ);
    object_property_add_uint64_ptr(obj,"prom-reads",&s->prom_reads,OBJ_PROP_FLAG_READ);
    object_property_add_uint64_ptr(obj,"shadow-reads",&s->shadow_reads,OBJ_PROP_FLAG_READ);
    object_property_add_uint64_ptr(obj,"unknown-reads",&s->unknown_reads,OBJ_PROP_FLAG_READ);
    object_property_add_uint64_ptr(obj,"unknown-writes",&s->unknown_writes,OBJ_PROP_FLAG_READ);
    object_property_add_uint64_ptr(obj,"first-unknown-read",&s->first_unknown_read,OBJ_PROP_FLAG_READ);
    object_property_add_uint64_ptr(obj,"first-unknown-write",&s->first_unknown_write,OBJ_PROP_FLAG_READ);
}

static const Property board_props[] = {
    DEFINE_PROP_STRING("model", WxpBoard, model),
    DEFINE_PROP_UINT16("subsystem-vendor-id", WxpBoard, subsystem_vendor, 0),
    DEFINE_PROP_UINT16("subsystem-id", WxpBoard, subsystem_device, 0),
};
static const VMStateDescription migration = {.name = TYPE_WXP_BOARD, .unmigratable = 1};
static void board_init(Object *obj)
{
    /* PCI allocates configuration space before realize chooses the parent bus. */
    PCI_DEVICE(obj)->cap_present |= QEMU_PCI_CAP_EXPRESS;
}

static void board_exit(PCIDevice *dev)
{
    WxpBoard *s = WXP_BOARD(dev);
    if (s->sdma_timer) {
        timer_free(s->sdma_timer);
        s->sdma_timer = NULL;
    }
}

static void board_class(ObjectClass *klass, void *data)
{
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);
    pc->realize = realize;
    pc->exit = board_exit;
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
