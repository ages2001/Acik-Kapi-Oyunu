#include <stdio.h>
#include <stdint.h>
#include <string.h>
typedef unsigned char Byte;
typedef uint32_t U32;
/* SHA-256, FIPS 180-4. Open Watcom has 32-bit unsigned long on both targets. */
static const U32 k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
#define R(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
static void block(U32 *h, const Byte *data) {
    U32 w[64], a, b, c, d, e, f, g, t, p, q, x, y;
    int i;
    for (i = 0; i < 16; i++, data += 4)
        w[i] = ((U32)data[0] << 24) | ((U32)data[1] << 16) | ((U32)data[2] << 8) | data[3];
    for (i = 16; i < 64; i++) {
        x = w[i - 15];
        y = w[i - 2];
        w[i] = w[i - 16] + (R(x, 7) ^ R(x, 18) ^ (x >> 3)) + w[i - 7] +
               (R(y, 17) ^ R(y, 19) ^ (y >> 10));
    }
    a = h[0];
    b = h[1];
    c = h[2];
    d = h[3];
    e = h[4];
    f = h[5];
    g = h[6];
    t = h[7];
    for (i = 0; i < 64; i++) {
        p = t + (R(e, 6) ^ R(e, 11) ^ R(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
        q = (R(a, 2) ^ R(a, 13) ^ R(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        t = g;
        g = f;
        f = e;
        e = d + p;
        d = c;
        c = b;
        b = a;
        a = p + q;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += t;
}
static int hash_stream(FILE *f, long limit, Byte *out) {
    U32 h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    static Byte buf[2048];
    U32 total = 0, bits;
    unsigned n, j, i, tail;
    unsigned count;
    do {
        count = limit >= 0 && limit-(long)total < (long)sizeof(buf) ?
                (unsigned)(limit-total) : sizeof(buf);
        n = fread(buf, 1, count, f);
        if (n < count && (ferror(f) || limit >= 0)) return 0;
        total += n;
        for (j = 0; j + 64 <= n; j += 64)
            block(h, buf + j);
    } while (n == sizeof(buf));
    if (ferror(f)) return 0;
    tail = n - j;
    memmove(buf, buf + j, tail);
    buf[tail++] = 0x80;
    if (tail > 56) {
        memset(buf + tail, 0, 64 - tail);
        block(h, buf);
        tail = 0;
    }
    memset(buf + tail, 0, 64 - tail);
    bits = total << 3;
    buf[59] = (Byte)(total >> 29);
    for (i = 0; i < 4; i++)
        buf[63 - i] = (Byte)(bits >> (i * 8));
    block(h, buf);
    for (i = 0; i < 32; i++)
        out[i] = (Byte)(h[i / 4] >> (24 - 8 * (i % 4)));
    return 1;
}
int digest_at(FILE *f, long start, long length, Byte *actual) {
    return start>=0 && length>=0 && !fseek(f,start,SEEK_SET) &&
           hash_stream(f,length,actual);
}
int hashfile(const char *path, Byte *out) {
    FILE *f=fopen(path,"rb");
    int ok;
    if (!f) return 0;
    ok=hash_stream(f,-1L,out);
    fclose(f);
    return ok;
}

void hexhash(const Byte *d, char *h) {
    static const char digits[] = "0123456789abcdef";
    int i;
    for (i = 0; i < 32; i++) {
        *h++ = digits[d[i] >> 4];
        *h++ = digits[d[i] & 15];
    }
    *h = 0;
}
int matches(const char *p, const char *s) {
    Byte d[32];
    char h[65];
    if (!hashfile(p, d))
        return 0;
    hexhash(d, h);
    return !strcmp(h, s);
}
