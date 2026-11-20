/* Execute the fragment instruction forms used by Mesa's screen-position and
 * level-zero texture tests. Each program and triangle is supplied by the guest.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "qemu/osdep.h"
#include "fpu/softfloat.h"
#include "winxpdm-fragment.h"
#include <math.h>

static uint32_t bits(float f)
{
    uint32_t value;
    memcpy(&value, &f, sizeof(value));
    return value;
}

static float number(uint32_t value)
{
    float f;
    memcpy(&f, &value, sizeof(f));
    return f;
}

static float ftz(float f)
{
    return fpclassify(f) == FP_SUBNORMAL ? copysignf(0.0f, f) : f;
}

static uint32_t rgba(const float color[4])
{
    uint32_t pixel = 0;
    for (unsigned i = 0; i < 4; i++) {
        float f = fmaxf(0.0f, fminf(color[i], 1.0f));
        pixel |= (uint32_t)lrintf(f * 255.0f) << (8 * i);
    }
    return pixel;
}

static void sample(const WxpFragment *u, float x, float y, float color[4])
{
    unsigned col = x >= 0.5f, row = y >= 0.5f;
    uint32_t pixel = u->parameters[row * 2 + col];
    for (unsigned i = 0; i < 4; i++) {
        color[i] = ((pixel >> (8 * i)) & 255) / 255.0f;
    }
}

/* Match a supported instruction form, excluding its encoded operand fields. */
static bool form(uint64_t op, uint64_t encoding, uint64_t operands)
{
    return ((op ^ encoding) & ~operands) == 0;
}

#define NV_BITS(n, shift) (((UINT64_C(1) << (n)) - 1) << (shift))
#define DST NV_BITS(8, 0)
#define SRC NV_BITS(8, 8)
#define SRC1 NV_BITS(8, 20)
#define CONST_OFFSET NV_BITS(16, 20)
#define IMM19 (NV_BITS(19, 20) | NV_BITS(1, 56))

/* Maxwell and Pascal use three 64-bit operations per scheduling-control word.
   The constant buffer specifies an upright viewport, Y scale 1 and bias 0. */
static bool nvidia(const WxpFragment *u, float x, float y, uint32_t *pixel)
{
    float r[256] = {0}, color[4];
    static const float constants[] = {1.0f, 0.0f};
    for (unsigned pc = 0; pc + 1 < u->words; pc += 2) {
        if (!(pc & 7)) {
            continue;
        }
        uint64_t op = u->program[pc] | (uint64_t)u->program[pc + 1] << 32;
        unsigned d = op & 255, a = (op >> 8) & 255, b = (op >> 20) & 255;
        unsigned c = (op >> 39) & 255, k = (op >> 20) & 65535;
        if (form(op, 0xe003ff87cff7ff00ULL, DST)) { /* IPA position.w */
            r[d] = 1.0f;
        } else if (form(op, 0xe043ff870007ff00ULL, DST | SRC1 | NV_BITS(10, 28))) {
            unsigned attr = (op >> 28) & 1023;
            if (attr != 0x70 && attr != 0x74) {
                return false;
            }
            r[d] = (attr == 0x70 ? x : y) * r[b];
        } else if (form(op, 0x5080000000470000ULL, DST | SRC)) { /* MUFU.RCP */
            r[d] = 1.0f / r[a];
        } else if (form(op, 0x4c98078000070000ULL, DST | CONST_OFFSET)) {
            if (k >= ARRAY_SIZE(constants)) {
                return false;
            }
            r[d] = constants[k];
        } else if (form(op, 0x49a0000000070000ULL, DST | SRC | CONST_OFFSET | NV_BITS(8, 39))) {
            if (k >= ARRAY_SIZE(constants)) {
                return false;
            }
            r[d] = ftz(fmaf(ftz(r[a]), constants[k], ftz(r[c])));
        } else if (form(op, 0x3868100000070000ULL, DST | SRC | IMM19)) {
            uint32_t imm = ((op >> 20) & 0x7ffff) << 12;
            imm |= (uint32_t)((op >> 56) & 1) << 31;
            r[d] = ftz(ftz(r[a]) * ftz(number(imm)));
        } else if (form(op, 0x5c58100000070000ULL, DST | SRC | SRC1)) {
            r[d] = ftz(ftz(r[a]) + ftz(r[b]));
        } else if (form(op, 0x010000000007f000ULL, DST | NV_BITS(32, 20))) {
            r[d] = number((uint32_t)(op >> 20));
        } else if (form(op, 0xd850008000070000ULL, DST | SRC | SRC1 | NV_BITS(8, 28))) {
            unsigned second = (op >> 28) & 255;
            if (d >= 254 || second >= 254) {
                return false;
            }
            sample(u, r[a], r[b], color);
            r[d] = color[0]; r[d + 1] = color[1];
            r[second] = color[2]; r[second + 1] = color[3];
        } else if (op == 0xe30000000007000fULL) {
            *pixel = rgba(r);
            return true;
        } else {
            return false;
        }
        r[255] = 0.0f; /* RZ */
    }
    return false;
}

static uint32_t source(unsigned kind, const uint32_t v[256], const uint32_t s[128],
                       uint32_t literal, bool *valid)
{
    if (kind >= 256) {
        return v[kind - 256];
    }
    if (kind <= 105) {
        return s[kind];
    }
    if (kind >= 128 && kind <= 192) {
        return kind - 128;
    }
    if (kind >= 193 && kind <= 208) {
        return -(int)(kind - 192);
    }
    if (kind >= 240 && kind <= 247) {
        static const float f[] = {0.5f, -0.5f, 1, -1, 2, -2, 4, -4};
        return bits(f[kind - 240]);
    }
    if (kind == 255) {
        return literal;
    }
    *valid = false;
    return 0;
}

static uint16_t half(float f, bool truncate)
{
    float_status status = {0};
    if (isnan(f)) return 0x7e00;
    set_float_rounding_mode(truncate ? float_round_to_zero : float_round_nearest_even, &status);
    return float32_to_float16(bits(f), true, &status);
}

static float unpack(uint16_t h)
{
    float_status status = {0};
    if ((h & 0x7fff) > 0x7c00) return number(0x7fc00000);
    return number(float16_to_float32(h, true, &status));
}

/* Region zero of the test descriptor table contains an RGBA8_UNORM, linear
   2x2 image. Byte 0x50 contains its point-filtered clamp-to-edge sampler. */
static const uint32_t descriptors[24] = {
    [0] = 0x10, [1] = (1U << 30) | (42U << 17), [2] = 1U << 14,
    [3] = (9U << 28) | 0xfac, [20] = 2 | (2 << 3) | (2 << 6),
};

/* RDNA 4 pixel ABI for these single-sample shaders supplies POS_X/Y in v2/v3.
   Scalar loads use the real source and destination SGPRs. Both shaders have
   no lane derivatives, branches or cross-lane effects. */
static bool radeon(const WxpFragment *u, float x, float y, uint32_t *pixel)
{
    uint32_t v[256] = {0}, s[128] = {0};
    bool exported = false, valid = true;
    unsigned pc = 0;
    v[2] = bits(x); v[3] = bits(y);
    while (pc < u->words && valid) {
        uint32_t op = u->program[pc++], ext, literal = 0;
        unsigned d = op & 255, d8 = (op >> 17) & 255;
        unsigned a, b, scalar, offset, count;
        if ((op & 0xff800000) == 0xb0000000) { /* S_MOVK_I32 */
            s[(op >> 16) & 127] = (int16_t)op;
        } else if ((op >> 16) == 0xbf85 || (op >> 16) == 0xbf87 ||
                   op == 0xbfc70000 || op == 0xbfc20000 || op == 0xbf800000 ||
                   op == 0xbfb60003) {
            continue; /* Synchronous execution satisfies clauses, delays and waits. */
        } else if ((op & 0xffffe000) == 0xf4006000 || (op & 0xffffe000) == 0xf4004000) {
            if (pc == u->words) return false;
            ext = u->program[pc++];
            scalar = (op & 63) * 2;
            d = (op >> 6) & 127;
            count = (op & 0x2000) ? 8 : 4;
            if ((ext & 0xff000003U) != 0xf8000000U || s[scalar + 1] != 0xffff8000U ||
                s[scalar] || d + count > 128) return false;
            offset = (ext & 0xffffff) >> 2;
            if (offset > ARRAY_SIZE(descriptors) - count) return false;
            memcpy(s + d, descriptors + offset, count * sizeof(*s));
        } else if ((op >> 25) == 3 || (op >> 25) == 8 || (op >> 25) == 47) {
            a = op & 511;
            if (a == 255) {
                if (pc == u->words) return false;
                literal = u->program[pc++];
            }
            float first = number(source(a, v, s, literal, &valid));
            float second = number(v[(op >> 9) & 255]);
            switch (op >> 25) {
            case 3: v[d8] = bits(first + second); break;
            case 8: v[d8] = bits(first * second); break;
            default: v[d8] = half(first, true) | (uint32_t)half(second, true) << 16; break;
            }
        } else if ((op & 0xffffff00) == 0xcc214000 || (op & 0xffffff00) == 0xcc224000) {
            if (pc == u->words) return false;
            ext = u->program[pc++];
            /* V_FMA_MIX{LO,HI}_F16 with f32 operands and a negated zero addend. */
            if ((ext & 0xfffc0000) != 0x82000000) return false;
            a = ext & 511; b = (ext >> 9) & 511;
            if (a == 255 || b == 255) {
                if (pc == u->words) return false;
                literal = u->program[pc++];
            }
            float first = number(source(a, v, s, literal, &valid));
            float second = number(source(b, v, s, literal, &valid));
            uint32_t h = half(fmaf(first, second, -0.0f), false);
            if ((op >> 16 & 255) == 0x21) v[d] = (v[d] & 0xffff0000) | h;
            else v[d] = (v[d] & 0xffff) | h << 16;
        } else if ((op & 0xffffff00) == 0xd52f0000) { /* V_CVT_PK_RTZ_F16_F32 */
            if (pc == u->words) return false;
            ext = u->program[pc++];
            if (ext >> 18) return false;
            a = ext & 511; b = (ext >> 9) & 511;
            if (a == 255 || b == 255) {
                if (pc == u->words) return false;
                literal = u->program[pc++];
            }
            uint16_t lo = half(number(source(a, v, s, literal, &valid)), true);
            uint16_t hi = half(number(source(b, v, s, literal, &valid)), true);
            v[d] = lo | (uint32_t)hi << 16;
        } else if (op == 0xe7c7c001) { /* IMAGE_SAMPLE_LZ, 2D, all RGBA channels */
            if (u->words - pc < 2) return false;
            ext = u->program[pc++];
            uint32_t coords = u->program[pc++];
            unsigned img = (ext >> 9) & 127, sampler = (ext >> 23) & 127;
            d = ext & 255;
            if ((ext & ~0x3f80feffU) || coords >> 16 || img > 120 || sampler > 124 || d > 252 ||
                memcmp(s + img, descriptors, 8 * sizeof(*s)) ||
                memcmp(s + sampler, descriptors + 20, 4 * sizeof(*s))) return false;
            float color[4];
            sample(u, number(v[coords & 255]), number(v[coords >> 8]), color);
            for (unsigned i = 0; i < 4; i++) v[d + i] = bits(color[i]);
        } else if (op == 0xf8000803) { /* compressed RG/BA color export to MRT0 */
            if (pc == u->words) return false;
            ext = u->program[pc++];
            if ((ext >> 16) != 0x8080) return false;
            uint32_t rg = v[ext & 255], ba = v[(ext >> 8) & 255];
            float color[] = {unpack(rg), unpack(rg >> 16), unpack(ba), unpack(ba >> 16)};
            *pixel = rgba(color);
            exported = true;
        } else if (op == 0xbfb00000) {
            return exported;
        } else {
            return false;
        }
    }
    return false;
}

typedef struct { int x, y; uint16_t z; } Vertex;

static int64_t edge(Vertex a, Vertex b, int x, int y)
{
    return (int64_t)(b.x - a.x) * (y - a.y) - (int64_t)(b.y - a.y) * (x - a.x);
}

static bool covered(Vertex a, Vertex b, int64_t value)
{
    return value > 0 || (value == 0 && (b.y < a.y || (b.y == a.y && b.x > a.x)));
}

static bool is_nvidia(uint32_t target)
{
    switch (target) {
    case 0x117: case 0x118: case 0x120: case 0x124: case 0x126:
    case 0x132: case 0x134: case 0x136: case 0x137: case 0x138: return true;
    default: return false;
    }
}

static bool draw(WxpFragment *u)
{
    Vertex v[3];
    uint32_t color[WXP_FRAG_SIDE * WXP_FRAG_SIDE];
    uint16_t depth[WXP_FRAG_SIDE * WXP_FRAG_SIDE];
    unsigned count = 0;
    bool nv = is_nvidia(u->architecture);
    if (!nv && u->architecture != 0x1200 && u->architecture != 0x1201) return false;
    if (!u->words || (nv && (u->words & 1))) return false;
    for (unsigned i = 0; i < 3; i++) {
        uint32_t xy = u->parameters[4 + i];
        v[i].x = (int16_t)xy * 2;
        v[i].y = (int16_t)(xy >> 16) * 2;
        if (u->parameters[7 + i] > UINT16_MAX) return false;
        v[i].z = u->parameters[7 + i];
    }
    int64_t area = edge(v[0], v[1], v[2].x, v[2].y);
    if (area < 0) {
        Vertex swap = v[1]; v[1] = v[2]; v[2] = swap;
        area = -area;
    }
    memcpy(color, u->color, sizeof(color));
    memcpy(depth, u->depth, sizeof(depth));
    for (unsigned y = 0; area && y < WXP_FRAG_SIDE; y++) {
        for (unsigned x = 0; x < WXP_FRAG_SIDE; x++) {
            int64_t w[3];
            unsigned idx = y * WXP_FRAG_SIDE + x;
            for (unsigned i = 0; i < 3; i++)
                w[i] = edge(v[(i + 1) % 3], v[(i + 2) % 3], 2 * x + 1, 2 * y + 1);
            if (!covered(v[1], v[2], w[0]) || !covered(v[2], v[0], w[1]) ||
                !covered(v[0], v[1], w[2])) continue;
            uint16_t z = (w[0] * v[0].z + w[1] * v[1].z + w[2] * v[2].z) / area;
            if (z >= depth[idx]) continue;
            bool ok = nv ? nvidia(u, x + 0.5f, y + 0.5f, &color[idx]) :
                           radeon(u, x + 0.5f, y + 0.5f, &color[idx]);
            if (!ok) return false;
            depth[idx] = z;
            count++;
        }
    }
    memcpy(u->color, color, sizeof(color));
    memcpy(u->depth, depth, sizeof(depth));
    u->samples = count;
    return true;
}

uint32_t wxp_fragment_read(WxpFragment *u, unsigned reg)
{
    switch (reg) {
    case 0: return u->architecture;
    case 4: return u->words;
    case 16: return u->samples;
    case 20: return u->error;
    case 24: return u->depth[u->pixel];
    case 28: return u->color[u->pixel];
    default: return 0;
    }
}

void wxp_fragment_write(WxpFragment *u, unsigned reg, uint32_t value)
{
    if (reg == 0) {
        u->architecture = value;
        u->words = u->error = u->samples = 0;
        return;
    }
    if (reg == 24) {
        if (value < ARRAY_SIZE(u->color)) u->pixel = value;
        else u->error = 2;
        return;
    }
    if (u->error) return;
    switch (reg) {
    case 4:
        if (u->words < ARRAY_SIZE(u->program)) u->program[u->words++] = value;
        else u->error = 2;
        break;
    case 8:
        if (value < ARRAY_SIZE(u->parameters)) u->parameter = value;
        else u->error = 2;
        break;
    case 12: u->parameters[u->parameter] = value; break;
    case 16:
        if (value == 1) {
            for (unsigned i = 0; i < ARRAY_SIZE(u->color); i++) {
                u->color[i] = u->parameters[10];
                u->depth[i] = UINT16_MAX;
            }
            u->samples = 0;
        } else if (value != 2 || !draw(u)) {
            u->error = 1;
        }
        break;
    default: u->error = 2;
    }
}
