/* Gallium VirGL command encoder for a depth-tested, interpolated triangle list.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "gpu.h"

enum { CREATE = 1, BIND = 2, VIEWPORT = 4, FRAMEBUFFER = 5, VERTEX_BUFFERS = 6,
       CLEAR = 7, DRAW_VBO = 8, SET_VIEWS = 10, BIND_SAMPLERS = 18, BIND_SHADER = 31 };
enum { BLEND = 1, RASTERIZER = 2, DSA = 3, SHADER = 4, ELEMENTS = 5, SAMPLER_VIEW = 6, SAMPLER = 7, SURFACE = 8 };
enum { RGBA8 = 67, Z24X8 = 21, FLOAT4 = 31, COLOR = 1, VBO = 2, DEPTH = 3 };
#define F_ONE 0x3f800000UL
#define N(a) (sizeof(a)/sizeof(*(a)))

static void packet(uint32_t **dst, unsigned cmd, unsigned object, unsigned count, const uint32_t *words)
{
    unsigned i;
    *(*dst)++ = cmd | (uint32_t)object << 8 | (uint32_t)count << 16;
    for (i = 0; i < count; ++i) *(*dst)++ = words[i];
}

static void shader(uint32_t **dst, unsigned handle, unsigned type, const char *text, unsigned bytes)
{
    unsigned i, words = (bytes + 3) / 4;
    uint32_t bind[2];
    uint32_t *p = *dst;
    *p++ = CREATE | (SHADER << 8) | ((uint32_t)(5 + words) << 16);
    *p++ = handle; *p++ = type; *p++ = bytes;
    *p++ = 128; /* token capacity for TGSI translation */
    *p++ = 0;   /* stream-output declarations */
    for (i = 0; i < words; ++i) {
        unsigned j;
        uint32_t value = 0;
        for (j = 0; j < 4 && 4 * i + j < bytes; ++j)
            value |= (uint32_t)(uint8_t)text[4 * i + j] << (8 * j);
        *p++ = value;
    }
    *dst = p;
    bind[0] = handle; bind[1] = type;
    packet(dst, BIND_SHADER, 0, 2, bind);
}

unsigned wxp_virgl_encode(uint32_t *words, const WxpDraw *draw)
{
    static const char vs[] =
        "VERT\nDCL IN[0]\nDCL IN[1]\nDCL OUT[0], POSITION\nDCL OUT[1], GENERIC[0]\n"
        "MOV OUT[0], IN[0]\nMOV OUT[1], IN[1]\nEND\n";
    static const char fs[] =
        "FRAG\nDCL IN[0], GENERIC[0], PERSPECTIVE\nDCL OUT[0], COLOR[0]\n"
        "MOV OUT[0], IN[0]\nEND\n";
    static const char texture_vs[] =
        "VERT\nDCL IN[0]\nDCL IN[1]\nDCL IN[2]\nDCL OUT[0], POSITION\n"
        "DCL OUT[1], GENERIC[0]\nDCL OUT[2], GENERIC[1]\nMOV OUT[0], IN[0]\n"
        "MOV OUT[1], IN[1]\nMOV OUT[2], IN[2]\nEND\n";
    static const char texture_fs[] =
        "FRAG\nDCL IN[0], GENERIC[0], PERSPECTIVE\nDCL IN[1], GENERIC[1], PERSPECTIVE\n"
        "DCL SAMP[0]\nDCL SVIEW[0], 2D, FLOAT\nDCL OUT[0], COLOR[0]\nDCL TEMP[0]\n"
        "TEX TEMP[0], IN[1], SAMP[0], 2D\nMUL OUT[0], TEMP[0], IN[0]\nEND\n";
    static const uint32_t color_surface[] = {1, COLOR, RGBA8, 0, 0};
    static const uint32_t depth_surface[] = {2, DEPTH, Z24X8, 0, 0};
    static const uint32_t framebuffer[] = {1, 2, 1};
    static const uint32_t blend[] = {3, 0, 0, 15UL << 27, 0, 0, 0, 0, 0, 0, 0};
    static const uint32_t rasterizer[] = {4, 2UL | (1UL << 29), F_ONE, 0, 0, F_ONE, 0, 0, 0};
    static const uint32_t depth_state[] = {5, 7, 0, 0, 0}; /* write and compare LESS */
    static const uint32_t elements[] = {6, 0, 0, 0, FLOAT4, 16, 0, 0, FLOAT4, 32, 0, 0, 29};
    static const uint32_t buffers[] = {sizeof(WxpVertex), 0, VBO};
    static const uint32_t view[] = {9, 5, RGBA8, 0, 0, (1U<<3)|(2U<<6)|(3U<<9)};
    static const uint32_t sampler[] = {10, 2U|(2U<<3)|(2U<<6)|(2U<<11), 0, 0, 0, 0, 0, 0, 0};
    static const uint32_t views[] = {1, 0, 9}, samplers[] = {1, 0, 10};
    /* Scale and translate NDC to 32x32, Z in [0,1]. */
    static const uint32_t viewport[] = {0, 0x41800000UL, 0x41800000UL, 0x3f000000UL,
                                          0x41800000UL, 0x41800000UL, 0x3f000000UL};
    uint32_t clear[8], triangles[12] = {0};
    uint32_t *p = words;
    unsigned i;
    packet(&p, CREATE, SURFACE, N(color_surface), color_surface);
    packet(&p, CREATE, SURFACE, N(depth_surface), depth_surface);
    packet(&p, FRAMEBUFFER, 0, N(framebuffer), framebuffer);
    packet(&p, CREATE, BLEND, N(blend), blend); packet(&p, BIND, BLEND, 1, blend);
    packet(&p, CREATE, RASTERIZER, N(rasterizer), rasterizer); packet(&p, BIND, RASTERIZER, 1, rasterizer);
    packet(&p, CREATE, DSA, N(depth_state), depth_state); packet(&p, BIND, DSA, 1, depth_state);
    packet(&p, CREATE, ELEMENTS, draw->flags & WXP_DRAW_TEXTURED ? N(elements) : 9, elements); packet(&p, BIND, ELEMENTS, 1, elements);
    if (draw->flags & WXP_DRAW_TEXTURED) {
        packet(&p, CREATE, SAMPLER_VIEW, N(view), view); packet(&p, SET_VIEWS, 0, N(views), views);
        packet(&p, CREATE, SAMPLER, N(sampler), sampler); packet(&p, BIND_SAMPLERS, 0, N(samplers), samplers);
        shader(&p, 7, 0, texture_vs, sizeof(texture_vs)); shader(&p, 8, 1, texture_fs, sizeof(texture_fs));
    } else {
        shader(&p, 7, 0, vs, sizeof(vs)); shader(&p, 8, 1, fs, sizeof(fs));
    }
    packet(&p, VIEWPORT, 0, N(viewport), viewport);
    packet(&p, VERTEX_BUFFERS, 0, N(buffers), buffers);
    clear[0] = 5;
    for (i = 0; i < 4; ++i) clear[i + 1] = draw->clear[i];
    clear[5] = 0; clear[6] = 0x3ff00000UL; clear[7] = 0; /* IEEE double depth = 1 */
    packet(&p, CLEAR, 0, N(clear), clear);
    triangles[1] = draw->count; triangles[2] = 4; triangles[4] = 1; triangles[10] = 0xffffffffUL;
    packet(&p, DRAW_VBO, 0, N(triangles), triangles);
    return (unsigned)(p - words);
}
