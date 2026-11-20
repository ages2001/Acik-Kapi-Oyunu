/* Issue native fragment programs and check every color and depth word.
 * The caller supplies the PCI read/write functions. SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef WINXPDM_FRAGMENT_TEST_H
#define WINXPDM_FRAGMENT_TEST_H
#include "fragment_words.h"

static const unsigned fragment_targets[] = {
    0x117,0x118,0x120,0x124,0x126,0x132,0x134,0x136,0x137,0x138,0x1200,0x1201
};

#define FRAG_COUNT(v) (sizeof(v) / sizeof((v)[0]))

static void frag_parameter(unsigned port, unsigned index, uint32_t value)
{
    reg_write(port, 40, index);
    reg_write(port, 44, value);
}

static void frag_triangle(unsigned port, int full, unsigned depth)
{
    unsigned i;
    static const uint32_t large[] = {0xffffffffUL, 0xffff0041UL, 0x0041ffffUL};
    static const uint32_t small[] = {0, 32, 0x00200000UL};
    for (i = 0; i < 3; ++i) {
        frag_parameter(port, 4 + i, full ? large[i] : small[i]);
        frag_parameter(port, 7 + i, full ? depth : depth + 64 * i);
    }
}

static void frag_program(unsigned port, unsigned arch, const uint32_t *words, unsigned count,
                         int mutate)
{
    unsigned i;
    reg_write(port, 32, arch);
    for (i = 0; i < count; ++i) {
        uint32_t word = words[i];
        /* Change only the multiply immediate used for the blue channel. */
        if (mutate && arch < 0x1200 && i + 1 < count && words[i+1] == 0x3868103cUL)
            word &= 0x000fffffUL;
        if (mutate && word == 0x3868103cUL) word &= 0xffffff80UL;
        if (mutate && word == 0x3c800000UL) word = 0;
        reg_write(port, 36, word);
    }
}

static uint32_t frag_gradient(unsigned x, unsigned y, int blue)
{
    uint32_t r = ((2*x+1)*255UL + 32)/64;
    uint32_t g = ((2*y+1)*255UL + 32)/64;
    uint32_t b = blue ? ((x+y+1)*255UL + 32)/64 : 0;
    return 0xff000000UL | b << 16 | g << 8 | r;
}

/* Three ordered surfaces, a changed instruction, and rejected invalid input. */
static int frag_case(unsigned port, unsigned arch)
{
    static const uint32_t texels[] = {0xe711396dUL,0xbd872343UL,0xf34d73a1UL,0x8193d749UL};
    const uint32_t *gradient = arch < 0x1200 ? nv_gradient : amd_gradient;
    const uint32_t *texture = arch < 0x1200 ? nv_sample : amd_sample;
    unsigned ng = arch < 0x1200 ? FRAG_COUNT(nv_gradient) : FRAG_COUNT(amd_gradient);
    unsigned nt = arch < 0x1200 ? FRAG_COUNT(nv_sample) : FRAG_COUNT(amd_sample);
    unsigned x, y, i;
    frag_program(port, arch, gradient, ng, 0);
    for (i = 0; i < 4; ++i) frag_parameter(port, i, texels[i]);
    frag_parameter(port, 10, 0x1254ba39UL);
    reg_write(port, 48, 1);
    frag_triangle(port, 1, 1000);
    reg_write(port, 48, 2);
    if (reg_read(port, 52) || reg_read(port, 48) != 1024) return 1;
    for (y = 0; y < 32; ++y) for (x = 0; x < 32; ++x) {
        reg_write(port, 56, 32*y+x);
        if (reg_read(port, 60) != frag_gradient(x,y,1) || reg_read(port,56) != 1000) return 2;
    }
    /* A full quad samples all four texels before testing partial coverage. */
    frag_program(port, arch, texture, nt, 0);
    reg_write(port,48,1);
    reg_write(port,48,2);
    if (reg_read(port,52) || reg_read(port,48) != 1024) return 3;
    for (y = 0; y < 32; ++y) for (x = 0; x < 32; ++x) {
        reg_write(port,56,32*y+x);
        if (reg_read(port,60) != texels[(y >> 4)*2 + (x >> 4)]) return 4;
    }
    frag_program(port, arch, gradient, ng, 1);
    frag_triangle(port, 0, 100);
    reg_write(port,48,2);
    if (reg_read(port,52) || reg_read(port,48) != 496) return 5;
    frag_triangle(port, 1, 60000);
    reg_write(port,48,2);
    if (reg_read(port,52) || reg_read(port,48)) return 6;
    for (y = 0; y < 32; ++y) for (x = 0; x < 32; ++x) {
        int inside = x+y < 31;
        uint32_t expected = inside ? frag_gradient(x,y,0) : texels[(y >> 4)*2 + (x >> 4)];
        unsigned z = inside ? (103 + 2*x + 4*y) : 1000;
        reg_write(port,56,32*y+x);
        if (reg_read(port,60) != expected || reg_read(port,56) != z) return 7;
    }
    reg_write(port,32,arch);
    for (i = 0; i < 6; ++i) reg_write(port,36,0xffffffffUL);
    frag_triangle(port,1,0);
    reg_write(port,48,2);
    if (reg_read(port,52) != 1) return 8;
    reg_write(port,56,0);
    if (reg_read(port,60) != frag_gradient(0,0,0) || reg_read(port,56) != 103) return 9;
    return 0;
}
#endif
