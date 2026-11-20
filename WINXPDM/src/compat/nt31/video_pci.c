/* Discover firmware-assigned PCI display functions and their BARs on x86 NT 3.1.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "video.h"

static ULONG pci_read(ULONG bus, ULONG slot, ULONG offset, ULONG size)
{
    ULONG value = 0;
    if (HalGetBusDataByOffset(PCIConfiguration, bus, slot, &value, offset, size) != size)
        return MAXULONG;
    return value;
}

static VOID pci_write(const WXP_PCI_ADDRESS *a, ULONG offset, ULONG value, ULONG size)
{
    HalSetBusDataByOffset(PCIConfiguration, a->bus, a->slot, &value, offset, size);
}

/* A 16-bit identity selects a vendor. A full identity selects a device and vendor. */
BOOLEAN wxp_find_display(ULONG id, WXP_PCI_ADDRESS *address)
{
    enum { PRESENT = 1, VGA_ROUTE = 2 };
    UCHAR buses[256] = {PRESENT | VGA_ROUTE};
    ULONG bus, dev, fn, last = 0;
    for (bus = 0; bus <= last; ++bus) {
        if (!buses[bus]) continue;
        for (dev = 0; dev < 32; ++dev) {
            ULONG header = pci_read(bus, dev, 0x0e, 1);
            ULONG functions = header & 0x80 ? 8 : 1;
            if (header == MAXULONG) continue;
            for (fn = 0; fn < functions; ++fn) {
                ULONG slot = dev | (fn << 5), type = pci_read(bus, slot, 0x0a, 2);
                if (type == 0x0604) {
                    ULONG secondary = pci_read(bus, slot, 0x19, 1);
                    if (secondary > bus && secondary <= 255 &&
                        secondary <= pci_read(bus, slot, 0x1a, 1)) {
                        ULONG ctl = pci_read(bus, slot, 0x3e, 2);
                        buses[secondary] = PRESENT | ((ctl & 8) ? (buses[bus] & VGA_ROUTE) : 0);
                        if (secondary > last) last = secondary;
                    }
                } else if ((type >> 8) == 3 && !(pci_read(bus, slot, 0x0e, 1) & 0x7f)) {
                    ULONG dev_id = pci_read(bus, slot, 0, 4);
                    if (id != dev_id && id != (USHORT)dev_id) continue;
                    address->bus = bus;
                    address->slot = slot;
                    address->vga_route = type == 0x0300 && (buses[bus] & VGA_ROUTE) != 0;
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}

VP_STATUS wxp_pci_ranges(const WXP_PCI_ADDRESS *a, PVIDEO_ACCESS_RANGE ranges, PULONG count)
{
    ULONG i, flags, command, n = 0;
    VP_STATUS status = NO_ERROR;
    /* W3H restricts configuration mechanism 1 to a uniprocessor. Keep decoding
       disabled only across this sizing transaction. No allocation or OS calls that
       may block are allowed while another interrupt cannot access the device. */
    __asm__ volatile("pushfl; popl %0; cli" : "=r"(flags) : : "memory");
    command = pci_read(a->bus, a->slot, 4, 2);
    if (command == MAXULONG) {
        status = ERROR_DEV_NOT_EXIST;
        goto done;
    }
    pci_write(a, 4, command & ~3UL, 2);
    for (i = 0; i < 6; ++i) {
        ULONG reg = 0x10 + 4 * i, low = pci_read(a->bus, a->slot, reg, 4);
        BOOLEAN io = (low & 1) != 0, wide = !io && (low & 6) == 4;
        ULONG mask = io ? ~3UL : ~15UL, high = 0, high_mask = MAXULONG, low_mask;
        ULONGLONG base, size;
        if (wide && i == 5) { status = ERROR_INVALID_DATA; break; }
        if (wide) high = pci_read(a->bus, a->slot, reg + 4, 4);
        pci_write(a, reg, MAXULONG, 4);
        if (wide) pci_write(a, reg + 4, MAXULONG, 4);
        low_mask = pci_read(a->bus, a->slot, reg, 4) & mask;
        if (wide) {
            high_mask = pci_read(a->bus, a->slot, reg + 4, 4);
            pci_write(a, reg + 4, high, 4);
            ++i;
        }
        pci_write(a, reg, low, 4);
        base = ((ULONGLONG)high << 32) | (low & mask);
        if (!base) continue; /* Includes absent and unassigned BARs. */
        /* A 16-bit I/O decoder can return zero in the upper mask bits. */
        if (io && !(low_mask & 0xffff0000UL)) low_mask |= 0xffff0000UL;
        size = -(((ULONGLONG)high_mask << 32) | low_mask);
        if (!size || size > MAXULONG || (size & (size - 1))) {
            status = ERROR_INVALID_DATA;
            break;
        }
        ranges[n].RangeStart.QuadPart = base;
        ranges[n].RangeLength = (ULONG)size;
        ranges[n].RangeInIoSpace = io;
        ++n;
    }
    pci_write(a, 4, command, 2);
    if (!n && status == NO_ERROR) status = ERROR_DEV_NOT_EXIST;
done:
    __asm__ volatile("pushl %0; popfl" : : "r"(flags) : "memory", "cc");
    *count = n;
    return status;
}
