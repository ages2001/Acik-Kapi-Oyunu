/* SDMA 7 linear-copy and constant-fill packets emitted by amdgpu.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef WINXPDM_SDMA_H
#define WINXPDM_SDMA_H

/* Both callers supply little-endian local VRAM and count packet lengths in dwords. */
static unsigned wxp_sdma_words(uint32_t header)
{
    switch (header) {
    case 0x0001000b: return 5; /* CONSTANT_FILL with COMPRESS set. */
    case 0x00080001: return 8; /* COPY_LINEAR with CPV set, no DCC metadata. */
    default: return 0;
    }
}

static bool wxp_sdma_range(uint32_t start, uint32_t bytes, uint32_t capacity)
{
    return bytes && start < capacity && bytes <= capacity - start;
}

static bool wxp_sdma_execute(uint8_t *vram, uint32_t capacity, const uint32_t *p,
                             uint32_t *destination, uint32_t *length)
{
    uint32_t dst, count;
    if (p[0] == 0x0001000b) {
        dst = p[1];
        count = p[4] + 1;
        if (p[2] || p[4] >> 30 || ((dst | count) & 3) ||
            !wxp_sdma_range(dst, count, capacity)) {
            return false;
        }
        for (uint32_t i = 0; i < count; i += 4) {
            stl_le_p(vram + dst + i, p[3]);
        }
    } else if (p[0] == 0x00080001) {
        dst = p[5];
        count = p[1] + 1;
        if (p[1] >> 30 || p[2] || p[4] || p[6] || p[7] ||
            !wxp_sdma_range(p[3], count, capacity) || !wxp_sdma_range(dst, count, capacity)) {
            return false;
        }
        memmove(vram + dst, vram + p[3], count);
    } else {
        return false;
    }
    *destination = dst;
    *length = count;
    return true;
}
#endif
