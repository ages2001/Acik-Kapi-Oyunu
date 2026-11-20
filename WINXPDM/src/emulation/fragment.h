/* QEMU fragment execution fixture. SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef WINXPDM_FRAGMENT_H
#define WINXPDM_FRAGMENT_H
#include <stdint.h>

#define WXP_FRAG_SIDE 32
typedef struct {
    uint32_t architecture, program[128], words, parameters[11], parameter;
    uint32_t pixel, error, samples;
    uint32_t color[WXP_FRAG_SIDE * WXP_FRAG_SIDE];
    uint16_t depth[WXP_FRAG_SIDE * WXP_FRAG_SIDE];
} WxpFragment;

uint32_t wxp_fragment_read(WxpFragment *unit, unsigned reg);
void wxp_fragment_write(WxpFragment *unit, unsigned reg, uint32_t value);
#endif
