/* PCI configuration mechanism 1 for 386 enhanced-mode Windows.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef WXP_PCI16_H
#define WXP_PCI16_H
#include <stdint.h>
static uint32_t pci_access(unsigned bus, unsigned slot, unsigned reg, unsigned width,
                           uint32_t data, unsigned writing)
{
    uint32_t address = 0x80000000UL | ((uint32_t)bus << 16) | ((uint32_t)slot << 8) | (reg & 252);
    unsigned port = 0xcfc + (reg & 3);
    uint32_t saved;
    _asm {
        pushf
        cli
        mov dx, 0cf8h
        in eax, dx
        mov saved, eax
        mov eax, address
        out dx, eax
        mov dx, port
        cmp writing, 0
        jne pci_write
        xor eax, eax
        cmp width, 4
        je read32
        cmp width, 2
        je read16
        in al, dx
        jmp store
    read16: in ax, dx
        jmp store
    read32: in eax, dx
    store: mov data, eax
        jmp restore
    pci_write: mov eax, data
        cmp width, 4
        je write32
        cmp width, 2
        je write16
        out dx, al
        jmp restore
    write16: out dx, ax
        jmp restore
    write32: out dx, eax
    restore: mov dx, 0cf8h
        mov eax, saved
        out dx, eax
        popf
    }
    return data;
}

#endif
