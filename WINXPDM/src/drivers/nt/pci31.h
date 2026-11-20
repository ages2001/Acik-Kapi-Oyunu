/* PCI configuration mechanism 1 on uniprocessor x86 NT 3.1.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef WXP_PCI31_H
#define WXP_PCI31_H
#include <stdint.h>

static uint32_t wxp_pci31_cycle(unsigned bus, unsigned slot, unsigned offset, unsigned n,
                          uint32_t value, int writing)
{
    uint32_t flags, saved, address = 0x80000000UL | bus << 16 | slot << 8 | (offset & 252);
    unsigned short port = 0xcfc + (offset & 3);
    __asm__ volatile("pushfl; popl %0; cli" : "=r"(flags) : : "memory");
    __asm__ volatile("inl %w1,%0" : "=a"(saved) : "Nd"((unsigned short)0xcf8));
    __asm__ volatile("outl %0,%w1" : : "a"(address), "Nd"((unsigned short)0xcf8));
    if (writing) {
        if (n == 4) __asm__ volatile("outl %0,%w1" : : "a"(value), "d"(port));
        else if (n == 2) __asm__ volatile("outw %w0,%w1" : : "a"(value), "d"(port));
        else __asm__ volatile("outb %b0,%w1" : : "a"(value), "d"(port));
    } else if (n == 4) __asm__ volatile("inl %w1,%0" : "=a"(value) : "d"(port));
    else if (n == 2) {
        unsigned short w;
        __asm__ volatile("inw %w1,%0" : "=a"(w) : "d"(port)); value = w;
    } else {
        unsigned char b;
        __asm__ volatile("inb %w1,%0" : "=a"(b) : "d"(port)); value = b;
    }
    __asm__ volatile("outl %0,%w1" : : "a"(saved), "Nd"((unsigned short)0xcf8));
    __asm__ volatile("pushl %0; popfl" : : "r"(flags) : "memory", "cc");
    return value;
}
#endif
