#include "patch.h"

/* SHA-256 for verifying the known installer and optical-class inputs on NT 3.1. */
static u32 rotate(u32 v, unsigned n) { return (v >> n) | (v << (32 - n)); }

static void compress(u32 h[8], const byte *block) {
    static const u32 constants[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
        0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
        0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
        0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
        0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
        0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
        0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
        0xc67178f2};
    u32 w[64], v[8];
    unsigned i;
    for (i = 0; i < 16; i++)
        w[i] = (u32)block[4 * i] << 24 | (u32)block[4 * i + 1] << 16 | (u32)block[4 * i + 2] << 8 |
               block[4 * i + 3];
    for (; i < 64; i++) {
        u32 a = w[i - 15], b = w[i - 2];
        w[i] = (rotate(b, 17) ^ rotate(b, 19) ^ (b >> 10)) + w[i - 7] +
               (rotate(a, 7) ^ rotate(a, 18) ^ (a >> 3)) + w[i - 16];
    }
    memcpy(v, h, sizeof(v));
    for (i = 0; i < 64; i++) {
        u32 a = v[0], b = v[1], c = v[2], e = v[4], f = v[5], g = v[6];
        u32 t = v[7] + (rotate(e, 6) ^ rotate(e, 11) ^ rotate(e, 25)) + ((e & f) ^ (~e & g)) +
                constants[i] + w[i];
        u32 sum = (rotate(a, 2) ^ rotate(a, 13) ^ rotate(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        v[7] = g;
        v[6] = f;
        v[5] = e;
        v[4] = v[3] + t;
        v[3] = c;
        v[2] = b;
        v[1] = a;
        v[0] = sum + t;
    }
    for (i = 0; i < 8; i++)
        h[i] += v[i];
}
void sha256(const byte *data, u32 size, char out[65]) {
    static const char hex[] = "0123456789abcdef";
    u32 h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    byte tail[128] = {0};
    u32 left = size;
    while (left >= 64) {
        compress(h, data);
        data += 64;
        left -= 64;
    }
    memcpy(tail, data, left);
    tail[left] = 0x80;
    unsigned end = left < 56 ? 64 : 128;
    tail[end - 5] = (byte)(size >> 29);
    tail[end - 4] = (byte)(size >> 21);
    tail[end - 3] = (byte)(size >> 13);
    tail[end - 2] = (byte)(size >> 5);
    tail[end - 1] = (byte)(size << 3);
    compress(h, tail);
    if (end == 128)
        compress(h, tail + 64);
    for (unsigned i = 0; i < 64; i++)
        out[i] = hex[(h[i / 8] >> (28 - (i % 8) * 4)) & 15];
    out[64] = 0;
}
