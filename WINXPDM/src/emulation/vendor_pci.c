/* Guest-facing command test port. The PCI function belongs to the laboratory.
 * Its payloads follow NVIDIA 906F/902D and the Linux SDMA 7 packet encoders.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "qemu/osdep.h"
#include "qemu/bswap.h"
#include "hw/pci/pci.h"
#include "hw/pci/pci_device.h"
#include "qapi/error.h"
#include "qom/object.h"
#include "migration/vmstate.h"

#define TYPE_WXP_ENGINES "winxpdm-engines"
OBJECT_DECLARE_SIMPLE_TYPE(WxpEngines, WXP_ENGINES)
struct WxpEngines {
    PCIDevice parent_obj;
    MemoryRegion io;
    Object *nv;
    uint8_t sdma_memory[65536];
    uint32_t packet[8], used, needed, read_address, error;
    uint32_t link_address;
    uint8_t source[32], training[7], scdc_version, scdc_config;
    bool clock_recovered, equalized;
};

static bool range_valid(uint32_t start, uint32_t bytes)
{
    return bytes && start < 65536 && bytes <= 65536 - start;
}

static void sdma_word(WxpEngines *s, uint32_t word)
{
    uint32_t *p = s->packet;
    if (!s->used) {
        switch (word) {
        case 0x0001000b: s->needed = 5; break; /* SDMA 7 CONSTANT_FILL, COMPRESS */
        case 0x00080001: s->needed = 8; break; /* COPY_LINEAR, CPV, uncompressed */
        default: s->error = 2; return;
        }
    }
    p[s->used++] = word;
    if (s->used < s->needed) {
        return;
    }
    s->used = 0;
    if (p[0] == 0x0001000b) {
        uint32_t bytes = p[4] + 1;
        if (p[2] || ((p[1] | bytes) & 3) || !range_valid(p[1], bytes)) {
            s->error = 3;
            return;
        }
        for (unsigned i = 0; i < bytes; i += 4) {
            stl_le_p(s->sdma_memory + p[1] + i, p[3]);
        }
    } else {
        uint32_t bytes = p[1] + 1;
        if (p[2] || p[4] || p[6] || p[7] ||
            !range_valid(p[3], bytes) || !range_valid(p[5], bytes)) {
            s->error = 3;
            return;
        }
        memmove(s->sdma_memory + p[5], s->sdma_memory + p[3], bytes);
    }
}

/* The receiver reports clock recovery separately from channel equalization.
 * One voltage increase is required on every lane. Transmitter inputs and
 * receiver registers occupy separate banks in the diagnostic adapter. */
static uint8_t link_read(WxpEngines *s)
{
    uint32_t addr = s->link_address;
    if (addr & 0x80000000U) {
        uint32_t clock = ldl_le_p(s->source + 16);
        bool lock = s->scdc_version == 1 && clock && clock <= 600000 &&
                    s->scdc_config == (clock > 340000 ? 3 : 0) &&
                    s->source[20] == s->scdc_config;
        switch (addr & 0x7fffffffU) {
        case 1: return 1;
        case 2: return s->scdc_version;
        case 0x20: return s->scdc_config;
        case 0x21: return lock && (s->scdc_config & 1);
        case 0x40: return lock ? 15 : 0;
        default: return 0;
        }
    }
    if (addr & 0x40000000U) {
        return addr < 0x40000020U ? s->source[addr & 31] : 0;
    }
    if (addr >= 0x202 && addr < 0x208) {
        bool valid = s->source[0] == 0x1e && s->source[1] == 4 &&
                     s->training[0] == 0x1e && (s->training[1] & 31) == 4;
        for (unsigned i=0; i<4; ++i) {
            valid &= (s->training[i+3] & 3) >= 1 &&
                     s->training[i+3] == s->source[i+3];
        }
        if (valid && s->training[2] == 0x21 && s->source[2] == 1) {
            s->clock_recovered = true;
        }
        if (valid && s->clock_recovered && s->training[2] == 7 && s->source[2] == 7) {
            s->equalized = true;
        }
        if (addr == 0x202 || addr == 0x203) {
            return s->equalized ? 0x77 : s->clock_recovered ? 0x11 : 0;
        }
        if (addr == 0x204) return s->equalized;
        return addr >= 0x206 ? 0x11 : 0;
    }
    if (addr >= 0x100 && addr < 0x107) return s->training[addr-0x100];
    switch (addr) {
    case 0: return 0x14;
    case 1: return 0x1e;
    case 2: return 0x84;
    case 3: return 0x80;
    default: return 0;
    }
}

static void link_write(WxpEngines *s, uint8_t v)
{
    uint32_t addr = s->link_address;
    if (addr >= 0x40000000U && addr < 0x40000020U) {
        s->source[addr & 31] = v;
    } else if (addr >= 0x100 && addr < 0x107) {
        s->training[addr-0x100] = v;
        if (addr == 0x100) {
            s->clock_recovered = s->equalized = false;
        }
    } else if (addr == 0x80000002U) {
        s->scdc_version = v;
    } else if (addr == 0x80000020U) {
        s->scdc_config = v & 3;
    }
}

static uint64_t port_read(void *opaque, hwaddr reg, unsigned size)
{
    WxpEngines *s = opaque;
    uint32_t address = s->read_address & 0x7fffffff;
    switch (reg) {
    case 0: return 0x31505857;
    case 12:
        if (s->read_address >> 31) {
            return ldl_le_p(s->sdma_memory + address);
        }
        return object_property_get_uint(s->nv, "read-value", &error_abort);
    case 20: return s->error;
    case 28: return link_read(s);
    default: return 0;
    }
}
static void port_write(void *opaque, hwaddr reg, uint64_t val, unsigned size)
{
    WxpEngines *s = opaque;
    Error *err = NULL;
    uint32_t value = val;
    if (reg == 20 && !value) {
        s->error = 0; s->used = 0; return;
    }
    if (s->error) {
        return;
    }
    switch (reg) {
    case 4:
        object_property_set_uint(s->nv, "push-word", value, &err);
        break;
    case 8: {
        uint32_t address = value & 0x7fffffff;
        if ((address & 3) || address > sizeof(s->sdma_memory)-4) {
            s->error = 3;
            return;
        }
        s->read_address = value;
        if (!(value >> 31)) {
            object_property_set_uint(s->nv, "read-address", address, &err);
        }
        break;
    }
    case 24: s->link_address = value; break;
    case 28: link_write(s, value); break;
    case 16:
        sdma_word(s, value);
        break;
    }
    if (err) {
        s->error = 1;
        error_free(err);
    }
}
/* The config-space window also reaches the fixture on Windows 9x, whose
 * PnP manager may disable unclaimed diagnostic I/O BARs. */
static uint32_t config_read(PCIDevice *dev, uint32_t reg, int size)
{
    if (reg >= 0x80 && reg < 0xa0 && size == 4 && !(reg & 3)) {
        return port_read(WXP_ENGINES(dev), reg - 0x80, size);
    }
    return pci_default_read_config(dev, reg, size);
}
static void config_write(PCIDevice *dev, uint32_t reg, uint32_t value, int size)
{
    if (reg >= 0x80 && reg < 0xa0 && size == 4 && !(reg & 3)) {
        port_write(WXP_ENGINES(dev), reg - 0x80, value, size);
    } else {
        pci_default_write_config(dev, reg, value, size);
    }
}
static const MemoryRegionOps operations = {
    .read = port_read, .write = port_write, .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = { .min_access_size = 4, .max_access_size = 4 },
};
static void realize(PCIDevice *pci, Error **errp)
{
    WxpEngines *s = WXP_ENGINES(pci);
    s->nv = object_new("winxpdm-nv-color");
    object_property_add_child(OBJECT(s), "nvidia", s->nv);
    object_unref(s->nv);
    memory_region_init_io(&s->io, OBJECT(s), &operations, s, "engine ports", 32);
    pci_register_bar(pci, 0, PCI_BASE_ADDRESS_SPACE_IO, &s->io);
}
static const VMStateDescription migration = {
    .name = TYPE_WXP_ENGINES, .unmigratable = 1
};
static void init_class(ObjectClass *class, void *data)
{
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(class);
    pc->realize = realize;
    pc->config_read = config_read; pc->config_write = config_write;
    pc->vendor_id = PCI_VENDOR_ID_REDHAT;
    pc->device_id = 0xff01;
    pc->class_id = PCI_CLASS_OTHERS;
    DEVICE_CLASS(class)->vmsd = &migration;
}
static const TypeInfo engines_type = {
    .name = TYPE_WXP_ENGINES, .parent = TYPE_PCI_DEVICE,
    .instance_size = sizeof(WxpEngines), .class_init = init_class,
    .interfaces = (InterfaceInfo[]) { { INTERFACE_CONVENTIONAL_PCI_DEVICE }, {} },
};
static void register_engines(void) { type_register_static(&engines_type); }
type_init(register_engines)
