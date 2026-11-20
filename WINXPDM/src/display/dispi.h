/*
 * WINXPDM - QEMU Standard VGA mode programming.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Include after the target defines dispi_rd(index) and dispi_wr(index, value).
 * All arithmetic here also runs in a 16-bit compiler with a 32-bit long.
 * Register definitions: include/hw/display/bochs-vbe.h and hw/display/vga.c.
 */
#ifndef WINXPDM_DISPI_H
#define WINXPDM_DISPI_H

enum wxp_register {
    WXP_ID, WXP_WIDTH, WXP_HEIGHT, WXP_DEPTH, WXP_ENABLE, WXP_BANK,
    WXP_VIRT_WIDTH, WXP_VIRT_HEIGHT, WXP_X_OFFSET, WXP_Y_OFFSET, WXP_VRAM_64K
};

#define WXP_LINEAR  0x40
#define WXP_KEEP    0x80
#define WXP_DAC8    0x20

/* Return the actual byte stride. A zero result means the mode is unavailable. */
static unsigned long wxp_set_dispi(unsigned short width, unsigned short height,
                                   unsigned short depth, unsigned short flags)
{
    unsigned short id, banks, pixels;
    unsigned long bytes, pitch;

    id = dispi_rd(WXP_ID);
    if (id != 0xb0c5 || !width || width > 16000U || (width & 7) ||
        !height || height > 12000U)
        return 0;
    if (depth != 8 && depth != 15 && depth != 16 && depth != 24 && depth != 32)
        return 0;
    bytes = (depth + 7U) / 8U;
    pitch = (unsigned long)width * bytes;
    banks = dispi_rd(WXP_VRAM_64K);
    if (!banks || height > ((unsigned long)banks << 16) / pitch)
        return 0;

    dispi_wr(WXP_ENABLE, 0);
    dispi_wr(WXP_WIDTH, width);
    dispi_wr(WXP_HEIGHT, height);
    dispi_wr(WXP_DEPTH, depth);
    dispi_wr(WXP_ENABLE, 1 | (flags & (WXP_LINEAR | WXP_KEEP | WXP_DAC8)));
    dispi_wr(WXP_VIRT_WIDTH, width);
    dispi_wr(WXP_X_OFFSET, 0);
    dispi_wr(WXP_Y_OFFSET, 0);

    pixels = dispi_rd(WXP_VIRT_WIDTH);
    if (!(dispi_rd(WXP_ENABLE) & 1) || dispi_rd(WXP_WIDTH) != width ||
        dispi_rd(WXP_HEIGHT) != height || dispi_rd(WXP_DEPTH) != depth ||
        pixels < width) {
        dispi_wr(WXP_ENABLE, 0);
        return 0;
    }
    return (unsigned long)pixels * bytes;
}
#endif
