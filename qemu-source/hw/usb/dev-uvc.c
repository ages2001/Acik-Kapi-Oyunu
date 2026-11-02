/*
 * QEMU USB Video Class test camera ("usb-uvc").
 *
 * Written for testing the USBNT camera driver.  It offers what real webcams
 * do and drivers must cope with:
 *
 *   UVC 1.1, interface association, camera terminal -> processing unit
 *   (brightness, contrast, hue, saturation, sharpness, white balance
 *   temperature, gain, power line frequency) -> streaming terminal
 *   formats : 1 = uncompressed YUY2 160x120, 320x240, 640x480 (30/15/5 fps)
 *             2 = MJPEG 320x240, 640x480 (30/15 fps), baseline 4:2:2 JPEG
 *                 (mjpeg420=on: 4:2:0) without DHT segment (mjpeg-dht=off,
 *                 like most webcams)
 *   quirks   : force-frame=N answers every probe with frame N (cameras that
 *             pick their own size), small-maxframe=on reports 1024 bytes
 *             as dwMaxVideoFrameSize
 *   streaming: isochronous alternate settings (high speed: 128, 512, 1024,
 *             2x1024, 3x1024 bytes per microframe; full speed: 128, 512, 1023)
 *             or bulk=on: one bulk endpoint in alternate setting 0
 *   payloads : 12-byte headers (PTS + SCR), FID toggles per frame, EOF on
 *             the last payload of a frame
 *
 * The picture is 8 colour bars (white, yellow, cyan, green, magenta, red,
 * blue, black) with a small grey box that moves one bar every frame; the
 * brightness control shifts the luma.  Frames are paced by the virtual clock
 * at the committed frame interval.  debug=1 logs requests and a summary per
 * second on stderr ("uvc: ..."); dump-jpeg=FILE writes the first 640x480
 * MJPEG frame at realize time (for checking the encoder).
 */

#include "qemu/osdep.h"
#include "qemu/module.h"
#include "qemu/timer.h"
#include "qapi/error.h"
#include "hw/qdev-properties.h"
#include "hw/usb.h"
#include "migration/vmstate.h"
#include "desc.h"
#include "qom/object.h"
#include <math.h>

#define UVC_VENDOR   0x46f4
#define UVC_PRODUCT  0x0103

#define EP_VIDEO     0x81
#define USB_CLASS_VIDEO 0x0e

#define ID_CAMERA    1
#define ID_PU        2
#define ID_OT        3

#define NFRAMES_CACHE 8         /* distinct pictures (box positions) */
#define MAX_W        640
#define MAX_H        480
#define FRAME_MAX    (MAX_W * MAX_H * 2)

enum { STR_NULL, STR_MFG, STR_PRODUCT, STR_SERIAL, STR_CONFIG, STR_VC, STR_VS };

static const USBDescStrings uvc_strings = {
    [STR_MFG]     = "QEMU",
    [STR_PRODUCT] = "QEMU UVC Test Camera",
    [STR_SERIAL]  = "1",
    [STR_CONFIG]  = "Video",
    [STR_VC]      = "Video Control",
    [STR_VS]      = "Video Streaming",
};

typedef struct {
    uint16_t w, h;
    uint8_t nint;
    uint32_t interval[3];       /* 100 ns units */
} UVCFrameDef;

static const UVCFrameDef yuy2_frames[] = {
    { 160, 120, 3, { 333333, 666666, 2000000 } },
    { 320, 240, 3, { 333333, 666666, 2000000 } },
    { 640, 480, 3, { 333333, 666666, 2000000 } },
};
static const UVCFrameDef mjpg_frames[] = {
    { 320, 240, 2, { 333333, 666666 } },
    { 640, 480, 2, { 333333, 666666 } },
};

/* processing unit controls: selector, length, min, max, res, def */
typedef struct {
    uint8_t sel, len, bit;
    int32_t min, max, res, def;
} UVCCtlDef;

static const UVCCtlDef ctl_defs[] = {
    { 0x02, 2, 0,  -64,   64, 1,    0 },        /* brightness */
    { 0x03, 2, 1,    0,   95, 1,   32 },        /* contrast */
    { 0x06, 2, 2, -2000, 2000, 100, 0 },        /* hue */
    { 0x07, 2, 3,    0,  100, 1,   64 },        /* saturation */
    { 0x08, 2, 4,    0,    7, 1,    3 },        /* sharpness */
    { 0x0A, 2, 6, 2800, 6500, 10, 4600 },       /* white balance temperature */
    { 0x04, 2, 9,    0,  255, 1,   32 },        /* gain */
    { 0x05, 1, 10,   0,    2, 1,    1 },        /* power line frequency */
};
#define NCTL ARRAY_SIZE(ctl_defs)

typedef struct {
    uint8_t *data;
    uint32_t len;
} JpegCache;

struct UVCState {
    USBDevice dev;

    /* properties */
    bool bulk;
    bool mjpeg420;          /* MJPEG in 4:2:0 instead of 4:2:2 */
    uint8_t force_frame;    /* answers every probe with this frame index */
    bool small_maxframe;    /* dwMaxVideoFrameSize far too small */
    bool mjpeg_dht;
    uint32_t debug;
    char *dump_jpeg;

    /* descriptors */
    USBDesc desc;
    USBDescDevice dev_fs, dev_hs;
    USBDescConfig conf_fs, conf_hs;
    USBDescIfaceAssoc iad_fs, iad_hs;
    USBDescIface ifs_fs[8], ifs_hs[8];

    /* probe / commit */
    uint8_t probe[34];
    uint8_t commit[34];
    bool committed;
    int alt;

    /* controls */
    int32_t ctl[NCTL];

    /* stream */
    uint8_t *frame;             /* frame being sent */
    uint32_t frame_len, frame_pos;
    bool in_frame;
    uint8_t fid;
    uint64_t next_frame_ns;
    uint32_t counter;
    uint32_t cur_fmt, cur_frm, cur_w, cur_h, cur_int;
    JpegCache jcache[NFRAMES_CACHE];
    uint32_t jcache_w, jcache_h;
    int32_t jcache_bright;

    QEMUTimer *wake;            /* bulk: NAKed until the next frame is due */
    uint32_t bulk_left;         /* bulk: data bytes still due in the current payload */

    /* statistics */
    uint64_t frames_sent, payloads, idle, last_report;
};
typedef struct UVCState UVCState;

#define TYPE_USB_UVC "usb-uvc"
OBJECT_DECLARE_SIMPLE_TYPE(UVCState, USB_UVC)

#define DBG(s, ...) do { if ((s)->debug) { fprintf(stderr, "uvc: " __VA_ARGS__); } } while (0)

#define U16(x) ((x) & 0xff), (((x) >> 8) & 0xff)
#define U32(x) ((x) & 0xff), (((x) >> 8) & 0xff), (((x) >> 16) & 0xff), (((x) >> 24) & 0xff)

static void put16(uint8_t *p, uint32_t v) { p[0] = v; p[1] = v >> 8; }
static void put32(uint8_t *p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
static uint32_t get32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

/* ------------------------------------------------------------------ */
/* test picture                                                        */
/* ------------------------------------------------------------------ */

static const uint8_t bar_rgb[8][3] = {
    { 255, 255, 255 }, { 255, 255, 0 }, { 0, 255, 255 }, { 0, 255, 0 },
    { 255, 0, 255 }, { 255, 0, 0 }, { 0, 0, 255 }, { 0, 0, 0 },
};

static uint8_t clamp8(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }

/* BT.601: full range for JPEG (JFIF), video range (Y 16..235) for YUY2 */
static void pixel(UVCState *s, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t n,
                  int *Y, int *Cb, int *Cr, bool video)
{
    int bar = x * 8 / w, r, g, b;
    uint32_t bw = w / 8, box = h / 8;
    uint32_t bx = (n % 8) * bw + (bw - box) / 2, by = (h - box) / 2;
    r = bar_rgb[bar][0]; g = bar_rgb[bar][1]; b = bar_rgb[bar][2];
    if (x >= bx && x < bx + box && y >= by && y < by + box) {
        r = g = b = 128;
    }
    if (video) {
        *Y = clamp8((int)lrint(16 + (0.299 * r + 0.587 * g + 0.114 * b) * 219 / 255) + s->ctl[0]);
        *Cb = clamp8((int)lrint(128 + (-0.168736 * r - 0.331264 * g + 0.5 * b) * 224 / 255));
        *Cr = clamp8((int)lrint(128 + (0.5 * r - 0.418688 * g - 0.081312 * b) * 224 / 255));
        return;
    }
    *Y = clamp8((int)lrint(0.299 * r + 0.587 * g + 0.114 * b) + s->ctl[0]);
    *Cb = clamp8((int)lrint(-0.168736 * r - 0.331264 * g + 0.5 * b + 128));
    *Cr = clamp8((int)lrint(0.5 * r - 0.418688 * g - 0.081312 * b + 128));
}

static uint32_t make_yuy2(UVCState *s, uint8_t *out, uint32_t w, uint32_t h, uint32_t n)
{
    uint32_t x, y;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x += 2) {
            int y0, u0, v0, y1, u1, v1;
            uint8_t *p = out + (y * w + x) * 2;
            pixel(s, x, y, w, h, n, &y0, &u0, &v0, true);
            pixel(s, x + 1, y, w, h, n, &y1, &u1, &v1, true);
            p[0] = y0; p[1] = (u0 + u1 + 1) / 2; p[2] = y1; p[3] = (v0 + v1 + 1) / 2;
        }
    }
    return w * h * 2;
}

/* ------------------------------------------------------------------ */
/* baseline JPEG encoder (4:2:2, Annex K tables)                       */
/* ------------------------------------------------------------------ */

static const uint8_t zigzag[64] = {
     0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63,
};
static const uint8_t std_lum_q[64] = {
    16, 11, 10, 16, 24, 40, 51, 61, 12, 12, 14, 19, 26, 58, 60, 55,
    14, 13, 16, 24, 40, 57, 69, 56, 14, 17, 22, 29, 51, 87, 80, 62,
    18, 22, 37, 56, 68, 109, 103, 77, 24, 35, 55, 64, 81, 104, 113, 92,
    49, 64, 78, 87, 103, 121, 120, 101, 72, 92, 95, 98, 112, 100, 103, 99,
};
static const uint8_t std_chr_q[64] = {
    17, 18, 24, 47, 99, 99, 99, 99, 18, 21, 26, 66, 99, 99, 99, 99,
    24, 26, 56, 99, 99, 99, 99, 99, 47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
};
/* Annex K.3 Huffman tables: 16 code counts, then the symbol values */
static const uint8_t dc_lum_bits[16] = { 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 };
static const uint8_t dc_lum_val[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
static const uint8_t dc_chr_bits[16] = { 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 };
static const uint8_t dc_chr_val[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
static const uint8_t ac_lum_bits[16] = { 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d };
static const uint8_t ac_lum_val[162] = {
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
    0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08, 0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
    0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
    0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
    0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
    0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
    0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa,
};
static const uint8_t ac_chr_bits[16] = { 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77 };
static const uint8_t ac_chr_val[162] = {
    0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
    0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
    0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34, 0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
    0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
    0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
    0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
    0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
    0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa,
};

typedef struct {
    uint16_t code[256];
    uint8_t size[256];
} HuffEnc;

typedef struct {
    uint8_t *out;
    uint32_t pos, cap;
    uint32_t acc;
    int nbits;
    uint8_t qy[64], qc[64];         /* natural order */
    HuffEnc dcy, dcc, acy, acc_;
    double cosv[8][8];
} JEnc;

static void huff_build(HuffEnc *h, const uint8_t *bits, const uint8_t *val)
{
    int i, j, k = 0;
    uint16_t code = 0;
    for (i = 0; i < 16; i++) {
        for (j = 0; j < bits[i]; j++) {
            h->code[val[k]] = code++;
            h->size[val[k]] = i + 1;
            k++;
        }
        code <<= 1;
    }
}

static void jbyte(JEnc *e, uint8_t b)
{
    if (e->pos < e->cap) {
        e->out[e->pos] = b;
    }
    e->pos++;
}

static void jbits(JEnc *e, uint32_t v, int n)
{
    e->acc = (e->acc << n) | (v & ((1u << n) - 1));
    e->nbits += n;
    while (e->nbits >= 8) {
        uint8_t b = e->acc >> (e->nbits - 8);
        jbyte(e, b);
        if (b == 0xFF) {
            jbyte(e, 0);
        }
        e->nbits -= 8;
    }
}

static void jflush(JEnc *e)
{
    if (e->nbits > 0) {
        jbits(e, 0x7F, 8 - e->nbits);
    }
}

static int magnitude(int v, uint32_t *bits)
{
    int a = v < 0 ? -v : v, n = 0;
    while (a) {
        n++;
        a >>= 1;
    }
    *bits = v < 0 ? (uint32_t)(v + (1 << n) - 1) : (uint32_t)v;
    return n;
}

static void block(JEnc *e, const double *in, const uint8_t *q, int *pred, HuffEnc *dc, HuffEnc *ac)
{
    double tmp[64];
    int coef[64], x, y, u, v, k, run, n;
    uint32_t bits;
    for (y = 0; y < 8; y++) {
        for (u = 0; u < 8; u++) {
            double sum = 0;
            for (x = 0; x < 8; x++) {
                sum += in[y * 8 + x] * e->cosv[u][x];
            }
            tmp[y * 8 + u] = sum;
        }
    }
    for (u = 0; u < 8; u++) {
        for (v = 0; v < 8; v++) {
            double sum = 0;
            for (y = 0; y < 8; y++) {
                sum += tmp[y * 8 + u] * e->cosv[v][y];
            }
            coef[v * 8 + u] = (int)lrint(sum / 4.0 / q[v * 8 + u]);
        }
    }
    /* DC */
    n = magnitude(coef[0] - *pred, &bits);
    *pred = coef[0];
    jbits(e, dc->code[n], dc->size[n]);
    if (n) {
        jbits(e, bits, n);
    }
    /* AC */
    run = 0;
    for (k = 1; k < 64; k++) {
        int c = coef[zigzag[k]];
        if (c == 0) {
            run++;
            continue;
        }
        while (run > 15) {
            jbits(e, ac->code[0xF0], ac->size[0xF0]);
            run -= 16;
        }
        n = magnitude(c, &bits);
        jbits(e, ac->code[(run << 4) | n], ac->size[(run << 4) | n]);
        jbits(e, bits, n);
        run = 0;
    }
    if (run) {
        jbits(e, ac->code[0], ac->size[0]);
    }
}

static void marker(JEnc *e, uint8_t m, uint16_t len)
{
    jbyte(e, 0xFF); jbyte(e, m);
    if (len) {
        jbyte(e, len >> 8); jbyte(e, len);
    }
}

static void put_dht(JEnc *e, uint8_t cls_id, const uint8_t *bits, const uint8_t *val)
{
    int i, n = 0;
    for (i = 0; i < 16; i++) {
        n += bits[i];
    }
    marker(e, 0xC4, 2 + 1 + 16 + n);
    jbyte(e, cls_id);
    for (i = 0; i < 16; i++) {
        jbyte(e, bits[i]);
    }
    for (i = 0; i < n; i++) {
        jbyte(e, val[i]);
    }
}

/* encodes the test picture n (YCbCr 4:2:2, quality 85) */
static uint32_t make_jpeg(UVCState *s, uint8_t *out, uint32_t cap, uint32_t w, uint32_t h, uint32_t n, bool dht)
{
    JEnc *e = g_new0(JEnc, 1);
    int i, u, x;
    int pred[3] = { 0, 0, 0 };
    uint32_t mx, my, len;
    int quality = 85, scale = quality < 50 ? 5000 / quality : 200 - quality * 2;
    uint8_t *ycc = g_malloc(w * h * 3);

    for (u = 0; u < 8; u++) {
        for (x = 0; x < 8; x++) {
            e->cosv[u][x] = (u ? 1.0 : M_SQRT1_2) * cos((2 * x + 1) * u * M_PI / 16);
        }
    }
    for (i = 0; i < 64; i++) {
        e->qy[i] = MAX(1, MIN(255, (std_lum_q[i] * scale + 50) / 100));
        e->qc[i] = MAX(1, MIN(255, (std_chr_q[i] * scale + 50) / 100));
    }
    huff_build(&e->dcy, dc_lum_bits, dc_lum_val);
    huff_build(&e->dcc, dc_chr_bits, dc_chr_val);
    huff_build(&e->acy, ac_lum_bits, ac_lum_val);
    huff_build(&e->acc_, ac_chr_bits, ac_chr_val);
    e->out = out;
    e->cap = cap;

    for (my = 0; my < h; my++) {
        for (mx = 0; mx < w; mx++) {
            int Y, Cb, Cr;
            pixel(s, mx, my, w, h, n, &Y, &Cb, &Cr, false);
            ycc[(my * w + mx) * 3] = Y;
            ycc[(my * w + mx) * 3 + 1] = Cb;
            ycc[(my * w + mx) * 3 + 2] = Cr;
        }
    }

    marker(e, 0xD8, 0);                                 /* SOI */
    marker(e, 0xE0, 16);                                /* APP0 "AVI1" like UVC cameras */
    jbyte(e, 'A'); jbyte(e, 'V'); jbyte(e, 'I'); jbyte(e, '1');
    for (i = 0; i < 10; i++) {
        jbyte(e, 0);
    }
    marker(e, 0xDB, 2 + 65 * 2);                        /* DQT */
    jbyte(e, 0);
    for (i = 0; i < 64; i++) {
        jbyte(e, e->qy[zigzag[i]]);
    }
    jbyte(e, 1);
    for (i = 0; i < 64; i++) {
        jbyte(e, e->qc[zigzag[i]]);
    }
    marker(e, 0xC0, 17);                                /* SOF0 */
    jbyte(e, 8); jbyte(e, h >> 8); jbyte(e, h); jbyte(e, w >> 8); jbyte(e, w);
    jbyte(e, 3);
    jbyte(e, 1); jbyte(e, s->mjpeg420 ? 0x22 : 0x21); jbyte(e, 0);   /* Y 2x2 (4:2:0) or 2x1 (4:2:2) */
    jbyte(e, 2); jbyte(e, 0x11); jbyte(e, 1);
    jbyte(e, 3); jbyte(e, 0x11); jbyte(e, 1);
    if (dht) {
        put_dht(e, 0x00, dc_lum_bits, dc_lum_val);
        put_dht(e, 0x10, ac_lum_bits, ac_lum_val);
        put_dht(e, 0x01, dc_chr_bits, dc_chr_val);
        put_dht(e, 0x11, ac_chr_bits, ac_chr_val);
    }
    marker(e, 0xDA, 12);                                /* SOS */
    jbyte(e, 3);
    jbyte(e, 1); jbyte(e, 0x00);
    jbyte(e, 2); jbyte(e, 0x11);
    jbyte(e, 3); jbyte(e, 0x11);
    jbyte(e, 0); jbyte(e, 63); jbyte(e, 0);

    for (my = 0; my < h; my += s->mjpeg420 ? 16 : 8) {
        for (mx = 0; mx < w; mx += 16) {
            double blk[64];
            int b, yy, xx, c, nb = s->mjpeg420 ? 4 : 2;
            for (b = 0; b < nb; b++) {                  /* luma blocks, left to right, top to bottom */
                for (yy = 0; yy < 8; yy++) {
                    for (xx = 0; xx < 8; xx++) {
                        uint32_t px = MIN(mx + (b & 1) * 8 + xx, w - 1), py = MIN(my + (b >> 1) * 8 + yy, h - 1);
                        blk[yy * 8 + xx] = ycc[(py * w + px) * 3] - 128.0;
                    }
                }
                block(e, blk, e->qy, &pred[0], &e->dcy, &e->acy);
            }
            for (c = 1; c <= 2; c++) {                  /* chroma, averaged horizontally (and vertically) */
                for (yy = 0; yy < 8; yy++) {
                    for (xx = 0; xx < 8; xx++) {
                        uint32_t px = MIN(mx + xx * 2, w - 1), px1 = MIN(px + 1, w - 1);
                        if (s->mjpeg420) {
                            uint32_t py = MIN(my + yy * 2, h - 1), py1 = MIN(py + 1, h - 1);
                            blk[yy * 8 + xx] = (ycc[(py * w + px) * 3 + c] + ycc[(py * w + px1) * 3 + c] +
                                                ycc[(py1 * w + px) * 3 + c] + ycc[(py1 * w + px1) * 3 + c]) / 4.0 -
                                               128.0;
                        } else {
                            uint32_t py = MIN(my + yy, h - 1);
                            blk[yy * 8 + xx] = (ycc[(py * w + px) * 3 + c] + ycc[(py * w + px1) * 3 + c]) / 2.0 -
                                               128.0;
                        }
                    }
                }
                block(e, blk, e->qc, &pred[c], &e->dcc, &e->acc_);
            }
        }
    }
    jflush(e);
    marker(e, 0xD9, 0);                                 /* EOI */
    len = e->pos;
    g_free(ycc);
    g_free(e);
    return len <= cap ? len : 0;
}

static JpegCache *jpeg_frame(UVCState *s, uint32_t w, uint32_t h, uint32_t n)
{
    JpegCache *c;
    int i;
    if (s->jcache_w != w || s->jcache_h != h || s->jcache_bright != s->ctl[0]) {
        for (i = 0; i < NFRAMES_CACHE; i++) {
            g_free(s->jcache[i].data);
            s->jcache[i].data = NULL;
        }
        s->jcache_w = w;
        s->jcache_h = h;
        s->jcache_bright = s->ctl[0];
    }
    c = &s->jcache[n % NFRAMES_CACHE];
    if (!c->data) {
        uint8_t *tmp = g_malloc(FRAME_MAX);
        c->len = make_jpeg(s, tmp, FRAME_MAX, w, h, n % NFRAMES_CACHE, s->mjpeg_dht);
        c->data = g_memdup2(tmp, c->len);
        g_free(tmp);
    }
    return c;
}

/* ------------------------------------------------------------------ */
/* descriptors                                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    USBDescOther list[24];
    int n;
} DescList;

static void dl_add(DescList *l, const uint8_t *data, size_t len)
{
    uint8_t *d = g_malloc(len);
    memcpy(d, data, len);
    l->list[l->n].length = len;
    l->list[l->n].data = d;
    l->n++;
}

static USBDescOther *dl_finish(DescList *l)
{
    return g_memdup2(l->list, sizeof(USBDescOther) * l->n);
}

static void build_vc(USBDescIface *ifc)
{
    DescList l = { .n = 0 };
    uint8_t ct[18] = { 18, 0x24, 0x02, ID_CAMERA, U16(0x0201), 0, 0, U16(0), U16(0), U16(0), 3, 0, 0, 0 };
    uint8_t pu[12] = { 12, 0x24, 0x05, ID_PU, ID_CAMERA, U16(0), 2, 0x5F, 0x06, 0, 0 };
    uint8_t ot[9] = { 9, 0x24, 0x03, ID_OT, U16(0x0101), 0, ID_PU, 0 };
    uint16_t total = 13 + sizeof(ct) + sizeof(pu) + sizeof(ot);
    uint8_t hdr[13] = { 13, 0x24, 0x01, U16(0x0110), U16(total), U32(48000000), 1, 1 };

    dl_add(&l, hdr, sizeof(hdr));
    dl_add(&l, ct, sizeof(ct));
    dl_add(&l, pu, sizeof(pu));
    dl_add(&l, ot, sizeof(ot));
    ifc->bInterfaceNumber = 0;
    ifc->bAlternateSetting = 0;
    ifc->bNumEndpoints = 0;
    ifc->bInterfaceClass = USB_CLASS_VIDEO;
    ifc->bInterfaceSubClass = 1;
    ifc->bInterfaceProtocol = 0;
    ifc->iInterface = STR_VC;
    ifc->ndesc = l.n;
    ifc->descs = dl_finish(&l);
}

static void frame_desc(uint8_t *d, uint8_t subtype, int idx, const UVCFrameDef *f, uint32_t maxbuf)
{
    int k;
    d[0] = 26 + 4 * f->nint;
    d[1] = 0x24;
    d[2] = subtype;
    d[3] = idx;
    d[4] = 0;
    put16(d + 5, f->w);
    put16(d + 7, f->h);
    put32(d + 9, maxbuf * 8 * 5);
    put32(d + 13, maxbuf * 8 * 30);
    put32(d + 17, maxbuf);
    put32(d + 21, f->interval[0]);
    d[25] = f->nint;
    for (k = 0; k < f->nint; k++) {
        put32(d + 26 + 4 * k, f->interval[k]);
    }
}

static void build_vs(UVCState *s, USBDescIface *ifc, int *n, bool hs)
{
    static const uint16_t hs_sizes[] = { 128, 512, 1024, 0x0800 | 1024, 0x1000 | 1024 };
    static const uint16_t fs_sizes[] = { 128, 512, 1023 };
    const uint16_t *sizes = hs ? hs_sizes : fs_sizes;
    int nalt = s->bulk ? 0 : (hs ? ARRAY_SIZE(hs_sizes) : ARRAY_SIZE(fs_sizes));
    DescList l = { .n = 0 };
    uint8_t fmt_yuy2[27] = { 27, 0x24, 0x04, 1, ARRAY_SIZE(yuy2_frames),
        'Y', 'U', 'Y', '2', 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71,
        16, 1, 0, 0, 0, 0 };
    uint8_t fmt_mjpg[11] = { 11, 0x24, 0x06, 2, ARRAY_SIZE(mjpg_frames), 1, 1, 0, 0, 0, 0 };
    uint8_t color[6] = { 6, 0x24, 0x0D, 1, 1, 4 };
    uint8_t frames[5][40];
    uint8_t hdr[15];
    uint16_t total;
    int i, k;

    total = sizeof(hdr) + sizeof(fmt_yuy2) + sizeof(fmt_mjpg) + 2 * sizeof(color);
    for (i = 0; i < ARRAY_SIZE(yuy2_frames); i++) {
        frame_desc(frames[i], 0x05, i + 1, &yuy2_frames[i], yuy2_frames[i].w * yuy2_frames[i].h * 2);
        total += frames[i][0];
    }
    for (i = 0; i < ARRAY_SIZE(mjpg_frames); i++) {
        frame_desc(frames[3 + i], 0x07, i + 1, &mjpg_frames[i], mjpg_frames[i].w * mjpg_frames[i].h * 2);
        total += frames[3 + i][0];
    }
    hdr[0] = sizeof(hdr); hdr[1] = 0x24; hdr[2] = 0x01;
    hdr[3] = 2;                                         /* formats */
    put16(hdr + 4, total);
    hdr[6] = EP_VIDEO;
    hdr[7] = 0;                                         /* bmInfo */
    hdr[8] = ID_OT;
    hdr[9] = 0; hdr[10] = 0; hdr[11] = 0;               /* still, trigger */
    hdr[12] = 1;                                        /* bControlSize */
    hdr[13] = 0; hdr[14] = 0;
    dl_add(&l, hdr, sizeof(hdr));
    dl_add(&l, fmt_yuy2, sizeof(fmt_yuy2));
    for (i = 0; i < ARRAY_SIZE(yuy2_frames); i++) {
        dl_add(&l, frames[i], frames[i][0]);
    }
    dl_add(&l, color, sizeof(color));
    dl_add(&l, fmt_mjpg, sizeof(fmt_mjpg));
    for (i = 0; i < ARRAY_SIZE(mjpg_frames); i++) {
        dl_add(&l, frames[3 + i], frames[3 + i][0]);
    }
    dl_add(&l, color, sizeof(color));

    /* alternate setting 0: formats (and the bulk endpoint) */
    ifc[*n].bInterfaceNumber = 1;
    ifc[*n].bAlternateSetting = 0;
    ifc[*n].bInterfaceClass = USB_CLASS_VIDEO;
    ifc[*n].bInterfaceSubClass = 2;
    ifc[*n].iInterface = STR_VS;
    ifc[*n].ndesc = l.n;
    ifc[*n].descs = dl_finish(&l);
    if (s->bulk) {
        USBDescEndpoint *ep = g_new0(USBDescEndpoint, 1);
        ep->bEndpointAddress = EP_VIDEO;
        ep->bmAttributes = USB_ENDPOINT_XFER_BULK;
        ep->wMaxPacketSize = hs ? 512 : 64;
        ifc[*n].bNumEndpoints = 1;
        ifc[*n].eps = ep;
    }
    (*n)++;
    for (k = 0; k < nalt; k++) {
        USBDescEndpoint *ep = g_new0(USBDescEndpoint, 1);
        ep->bEndpointAddress = EP_VIDEO;
        ep->bmAttributes = USB_ENDPOINT_XFER_ISOC | 0x04;   /* asynchronous */
        ep->wMaxPacketSize = sizes[k];
        ep->bInterval = 1;
        ifc[*n].bInterfaceNumber = 1;
        ifc[*n].bAlternateSetting = k + 1;
        ifc[*n].bNumEndpoints = 1;
        ifc[*n].bInterfaceClass = USB_CLASS_VIDEO;
        ifc[*n].bInterfaceSubClass = 2;
        ifc[*n].eps = ep;
        (*n)++;
    }
}

static void build_dev(UVCState *s, USBDescDevice *d, USBDescConfig *c, USBDescIfaceAssoc *iad,
                      USBDescIface *ifs, bool hs)
{
    int n = 0;
    build_vc(&ifs[n++]);
    build_vs(s, ifs, &n, hs);
    d->bcdUSB = 0x0200;
    d->bDeviceClass = 0xEF;
    d->bDeviceSubClass = 0x02;
    d->bDeviceProtocol = 0x01;
    d->bMaxPacketSize0 = 64;
    d->bNumConfigurations = 1;
    d->confs = c;
    c->bNumInterfaces = 2;
    c->bConfigurationValue = 1;
    c->iConfiguration = STR_CONFIG;
    c->bmAttributes = USB_CFG_ATT_ONE;
    c->bMaxPower = 250;
    iad->bFirstInterface = 0;
    iad->bInterfaceCount = 2;
    iad->bFunctionClass = USB_CLASS_VIDEO;
    iad->bFunctionSubClass = 0x03;                      /* video interface collection */
    iad->bFunctionProtocol = 0;
    iad->iFunction = STR_PRODUCT;
    iad->nif = n;
    iad->ifs = ifs;
    c->nif_groups = 1;
    c->if_groups = iad;
}

/* ------------------------------------------------------------------ */
/* probe / commit                                                      */
/* ------------------------------------------------------------------ */

static const UVCFrameDef *frame_def(uint8_t fmt, uint8_t frm)
{
    if (fmt == 2) {
        return &mjpg_frames[MIN(MAX(frm, 1), ARRAY_SIZE(mjpg_frames)) - 1];
    }
    return &yuy2_frames[MIN(MAX(frm, 1), ARRAY_SIZE(yuy2_frames)) - 1];
}

static uint32_t max_frame_size(UVCState *s, uint8_t fmt, const UVCFrameDef *f)
{
    if (fmt == 2) {
        uint32_t i, m = 0;
        for (i = 0; i < NFRAMES_CACHE; i++) {
            m = MAX(m, jpeg_frame(s, f->w, f->h, i)->len);
        }
        return m + 1024;
    }
    return f->w * f->h * 2;
}

/* fills a probe structure from what the host asked for */
static void negotiate(UVCState *s, uint8_t *p)
{
    uint8_t fmt = p[2] == 2 ? 2 : 1;
    uint8_t nfr = fmt == 2 ? ARRAY_SIZE(mjpg_frames) : ARRAY_SIZE(yuy2_frames);
    uint8_t frm = p[3] < 1 ? 1 : p[3] > nfr ? nfr : p[3];
    if (s->force_frame) {
        frm = MIN(s->force_frame, nfr);                 /* a camera that picks its own size */
    }
    const UVCFrameDef *f = frame_def(fmt, frm);
    uint32_t want = get32(p + 4), iv = f->interval[0], frame, payload;
    bool hs = s->dev.speed == USB_SPEED_HIGH;
    int k;
    for (k = 0; k < f->nint; k++) {                     /* nearest supported interval */
        if (want && abs((int)f->interval[k] - (int)want) < abs((int)iv - (int)want)) {
            iv = f->interval[k];
        }
    }
    frame = max_frame_size(s, fmt, f);
    if (s->bulk) {
        payload = hs ? 32768 : 8192;
    } else {
        uint64_t per_sec = (uint64_t)frame * 10000000 / iv;
        uint32_t svc = hs ? 8000 : 1000;
        payload = (uint32_t)((per_sec + svc - 1) / svc) + 12;
        payload = payload * 5 / 4;                      /* headroom, like real cameras */
        payload = MIN(payload, hs ? 3072 : 1023);
    }
    memset(p + 8, 0, sizeof(s->probe) - 8);
    p[0] = 1; p[1] = 0;
    p[2] = fmt;
    p[3] = frm;
    put32(p + 4, iv);
    put32(p + 18, s->small_maxframe ? 1024 : frame); /* some cameras say too little */
    put32(p + 22, payload);
    put32(p + 26, 48000000);                            /* dwClockFrequency */
    p[30] = 0x03;                                       /* bmFramingInfo: FID + EOF */
    p[31] = 1; p[32] = 1; p[33] = 1;                    /* preferred/min/max version */
}

static void start_stream(UVCState *s, const char *why)
{
    s->cur_fmt = s->commit[2];
    s->cur_frm = s->commit[3];
    s->cur_int = get32(s->commit + 4);
    s->cur_w = frame_def(s->cur_fmt, s->cur_frm)->w;
    s->cur_h = frame_def(s->cur_fmt, s->cur_frm)->h;
    s->in_frame = false;
    s->bulk_left = 0;
    s->next_frame_ns = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    DBG(s, "stream on (%s): %s %ux%u interval %u\n", why, s->cur_fmt == 2 ? "MJPG" : "YUY2",
        s->cur_w, s->cur_h, s->cur_int);
}

/* ------------------------------------------------------------------ */
/* streaming                                                           */
/* ------------------------------------------------------------------ */

static void report(UVCState *s, bool force)
{
    uint64_t now = qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL);
    if (!s->debug || (!force && now - s->last_report < 1000)) {
        return;
    }
    s->last_report = now;
    DBG(s, "stats alt=%d frames=%" PRIu64 " payloads=%" PRIu64 " idle=%" PRIu64 " brightness=%d\n",
        s->alt, s->frames_sent, s->payloads, s->idle, s->ctl[0]);
}

static void next_frame(UVCState *s)
{
    uint32_t n = s->counter++;
    if (s->cur_fmt == 2) {
        JpegCache *c = jpeg_frame(s, s->cur_w, s->cur_h, n);
        memcpy(s->frame, c->data, c->len);
        s->frame_len = c->len;
    } else {
        s->frame_len = make_yuy2(s, s->frame, s->cur_w, s->cur_h, n % NFRAMES_CACHE);
    }
    s->frame_pos = 0;
    s->in_frame = true;
    s->fid ^= 1;
}

static void send_payload(UVCState *s, USBPacket *p, uint32_t room, uint32_t cap)
{
    uint8_t hdr[12];
    uint64_t now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    uint32_t n;

    if (!s->in_frame) {
        if (now < s->next_frame_ns) {
            s->idle++;
            return;                                     /* nothing to send: zero length */
        }
        next_frame(s);
        s->next_frame_ns += (uint64_t)s->cur_int * 100;
        if (s->next_frame_ns < now) {                   /* the bus is too slow for the rate */
            s->next_frame_ns = now;
        }
    }
    if (room <= sizeof(hdr)) {
        return;
    }
    n = MIN(room - sizeof(hdr), s->frame_len - s->frame_pos);
    hdr[0] = sizeof(hdr);
    hdr[1] = 0x80 | 0x08 | 0x04 | s->fid;               /* EOH, SCR, PTS, FID */
    put32(hdr + 2, (uint32_t)(s->counter * s->cur_int / 10 * 48));     /* PTS in 48 MHz ticks */
    put32(hdr + 6, (uint32_t)(now / 1000 * 48));
    put16(hdr + 10, (uint16_t)(now / 1000000));
    if (s->frame_pos + MAX(n, cap) >= s->frame_len) {
        hdr[1] |= 0x02;                                 /* EOF: this payload ends the frame */
    }
    usb_packet_copy(p, hdr, sizeof(hdr));
    usb_packet_copy(p, s->frame + s->frame_pos, n);
    s->frame_pos += n;
    s->payloads++;
    if (s->frame_pos >= s->frame_len) {
        s->in_frame = false;
        s->frames_sent++;
    }
    report(s, false);
}

/* bulk: a payload (header + data, up to dwMaxPayloadTransferSize) is one USB
   transfer that the host controller may split into several packets (TDs);
   it ends with a short packet when it is smaller than the transfer */
static void send_bulk(UVCState *s, USBPacket *p, uint32_t pl)
{
    uint32_t room = p->iov.size, n;
    if (!s->bulk_left) {
        uint64_t before = s->payloads;
        uint32_t pos0;
        if (room < 12 + 1) {
            return;
        }
        /* a new payload: the header and as much data as fits in this packet */
        pos0 = s->in_frame ? s->frame_pos : 0;
        send_payload(s, p, MIN(room, pl), pl - 12);
        if (s->payloads == before) {
            return;                                     /* no frame due */
        }
        if (s->in_frame) {
            /* data bytes of this payload after what went in the packet */
            uint32_t sent = s->frame_pos - pos0;
            uint32_t want = MIN(pl - 12, s->frame_len - pos0);
            s->bulk_left = want > sent ? want - sent : 0;
        }
        return;
    }
    n = MIN(room, s->bulk_left);
    n = MIN(n, s->frame_len - s->frame_pos);
    usb_packet_copy(p, s->frame + s->frame_pos, n);
    s->frame_pos += n;
    s->bulk_left -= n;
    if (s->frame_pos >= s->frame_len) {
        s->in_frame = false;
        s->frames_sent++;
        s->bulk_left = 0;
    }
}

static void uvc_wake(void *opaque)
{
    UVCState *s = opaque;
    usb_wakeup(usb_ep_get(&s->dev, USB_TOKEN_IN, EP_VIDEO & 0xf), 0);
}

static void uvc_handle_data(USBDevice *dev, USBPacket *p)
{
    UVCState *s = USB_UVC(dev);

    if (p->pid != USB_TOKEN_IN || p->ep->nr != (EP_VIDEO & 0xf)) {
        p->status = USB_RET_STALL;
        return;
    }
    if (s->bulk) {
        uint32_t pl = get32(s->commit + 22);
        if (!s->committed) {
            p->status = USB_RET_NAK;
            return;
        }
        send_bulk(s, p, pl ? pl : 32768);
        if (p->actual_length == 0) {
            p->status = USB_RET_NAK;                    /* bulk: wait for the next frame */
            timer_mod(s->wake, s->next_frame_ns);
        }
        return;
    }
    if (!s->alt) {
        p->status = USB_RET_STALL;
        return;
    }
    send_payload(s, p, p->iov.size, 0);
}

/* ------------------------------------------------------------------ */
/* class requests                                                      */
/* ------------------------------------------------------------------ */

static int vs_request(UVCState *s, uint8_t req, uint8_t cs, int length, uint8_t *data)
{
    uint8_t *cur = cs == 1 ? s->probe : s->commit;
    uint8_t tmp[34];
    int n = MIN(length, (int)sizeof(tmp));

    if (cs != 1 && cs != 2) {
        return -1;
    }
    switch (req) {
    case 0x01:                                          /* SET_CUR */
        memcpy(tmp, cur, sizeof(tmp));
        memcpy(tmp, data, n);
        if (length < 26) {
            return -1;
        }
        negotiate(s, tmp);
        memcpy(cur, tmp, sizeof(tmp));
        DBG(s, "SET %s fmt %u frame %u interval %u -> frame %u payload %u (len %d)\n",
            cs == 1 ? "probe" : "commit", tmp[2], tmp[3], get32(tmp + 4), get32(tmp + 18), get32(tmp + 22), length);
        if (cs == 2) {
            s->committed = true;
            if (s->bulk || s->alt) {
                start_stream(s, "commit");
            }
        }
        return 0;
    case 0x81:                                          /* GET_CUR */
        memcpy(data, cur, n);
        return n;
    case 0x82: case 0x83: case 0x87:                    /* GET_MIN / MAX / DEF */
        memset(tmp, 0, sizeof(tmp));
        tmp[2] = 1;
        tmp[3] = req == 0x83 ? ARRAY_SIZE(yuy2_frames) : 1;
        put32(tmp + 4, req == 0x83 ? 2000000 : 333333);
        negotiate(s, tmp);
        memcpy(data, tmp, n);
        return n;
    case 0x85:                                          /* GET_LEN */
        data[0] = sizeof(s->probe); data[1] = 0;
        return MIN(length, 2);
    case 0x86:                                          /* GET_INFO */
        data[0] = 3;
        return 1;
    }
    return -1;
}

static int pu_request(UVCState *s, uint8_t req, uint8_t cs, int length, uint8_t *data)
{
    int i;
    int32_t v;
    for (i = 0; i < NCTL; i++) {
        if (ctl_defs[i].sel == cs) {
            break;
        }
    }
    if (i == NCTL) {
        return -1;
    }
    switch (req) {
    case 0x01:
        v = ctl_defs[i].len == 1 ? data[0] : (int16_t)(data[0] | (data[1] << 8));
        if (cs == 0x0A || cs == 0x04) {
            v = (uint16_t)(data[0] | (data[1] << 8));
        }
        if (v < ctl_defs[i].min || v > ctl_defs[i].max) {
            DBG(s, "SET control %02x = %d out of range\n", cs, v);
            return -1;
        }
        s->ctl[i] = v;
        DBG(s, "SET control %02x = %d\n", cs, v);
        return 0;
    case 0x81: v = s->ctl[i]; break;
    case 0x82: v = ctl_defs[i].min; break;
    case 0x83: v = ctl_defs[i].max; break;
    case 0x84: v = ctl_defs[i].res; break;
    case 0x87: v = ctl_defs[i].def; break;
    case 0x85: data[0] = ctl_defs[i].len; data[1] = 0; return MIN(length, 2);
    case 0x86: data[0] = 3; return 1;
    default: return -1;
    }
    data[0] = v;
    if (ctl_defs[i].len == 2) {
        data[1] = v >> 8;
    }
    return MIN(length, ctl_defs[i].len);
}

static void uvc_handle_control(USBDevice *dev, USBPacket *p, int request, int value,
                               int index, int length, uint8_t *data)
{
    UVCState *s = USB_UVC(dev);
    uint8_t req = request & 0xff, cs = value >> 8, ifn = index & 0xff, ent = index >> 8;
    int ret;

    ret = usb_desc_handle_control(dev, p, request, value, index, length, data);
    if (ret >= 0) {
        return;
    }
    ret = -1;
    if (((request >> 8) & 0x60) == 0x20 && ((request >> 8) & 0x1f) == 1) {
        if (ifn == 1 && ent == 0) {
            ret = vs_request(s, req, cs, length, data);
        } else if (ifn == 0 && ent == ID_PU) {
            ret = pu_request(s, req, cs, length, data);
        } else if (ifn == 0 && ent == 0 && req == 0x81 && cs == 0x02) {
            data[0] = 0;                                /* request error code: none */
            ret = 1;
        }
    }
    if (ret < 0) {
        DBG(s, "request %04x value %04x index %04x len %d stalled\n", request, value, index, length);
        p->status = USB_RET_STALL;
        return;
    }
    p->actual_length = ret;
}

static void uvc_set_interface(USBDevice *dev, int iface, int old, int value)
{
    UVCState *s = USB_UVC(dev);

    DBG(s, "set_interface if=%d alt=%d\n", iface, value);
    if (iface != 1) {
        return;
    }
    s->alt = value;
    if (value && s->committed) {
        start_stream(s, "alt");
    } else if (!value) {
        s->in_frame = false;
    }
    report(s, true);
}

static void uvc_handle_reset(USBDevice *dev)
{
    UVCState *s = USB_UVC(dev);

    s->alt = 0;
    s->committed = false;
    s->in_frame = false;
    s->bulk_left = 0;
    memset(s->probe, 0, sizeof(s->probe));
    s->probe[2] = 1; s->probe[3] = 1;
    negotiate(s, s->probe);
    memcpy(s->commit, s->probe, sizeof(s->commit));
}

static void uvc_unrealize(USBDevice *dev)
{
    UVCState *s = USB_UVC(dev);
    int i;

    report(s, true);
    timer_free(s->wake);
    for (i = 0; i < NFRAMES_CACHE; i++) {
        g_free(s->jcache[i].data);
    }
    g_free(s->frame);
}

static void uvc_realize(USBDevice *dev, Error **errp)
{
    UVCState *s = USB_UVC(dev);
    int i;

    for (i = 0; i < NCTL; i++) {
        s->ctl[i] = ctl_defs[i].def;
    }
    s->frame = g_malloc(FRAME_MAX);
    s->wake = timer_new_ns(QEMU_CLOCK_VIRTUAL, uvc_wake, s);
    build_dev(s, &s->dev_fs, &s->conf_fs, &s->iad_fs, s->ifs_fs, false);
    build_dev(s, &s->dev_hs, &s->conf_hs, &s->iad_hs, s->ifs_hs, true);
    s->desc.id.idVendor = UVC_VENDOR;
    s->desc.id.idProduct = UVC_PRODUCT;
    s->desc.id.bcdDevice = 0x0100;
    s->desc.id.iManufacturer = STR_MFG;
    s->desc.id.iProduct = STR_PRODUCT;
    s->desc.id.iSerialNumber = STR_SERIAL;
    s->desc.full = &s->dev_fs;
    s->desc.high = &s->dev_hs;
    s->desc.str = uvc_strings;
    dev->usb_desc = &s->desc;
    usb_desc_create_serial(dev);
    usb_desc_init(dev);
    s->jcache_w = 0;
    if (s->dump_jpeg && *s->dump_jpeg) {
        JpegCache *c = jpeg_frame(s, 640, 480, 0);
        if (!g_file_set_contents(s->dump_jpeg, (const char *)c->data, c->len, NULL)) {
            error_setg(errp, "cannot write %s", s->dump_jpeg);
            return;
        }
        DBG(s, "wrote %u byte JPEG to %s\n", c->len, s->dump_jpeg);
    }
    uvc_handle_reset(dev);
    DBG(s, "camera ready (%s, MJPEG %s DHT)\n", s->bulk ? "bulk" : "isochronous", s->mjpeg_dht ? "with" : "without");
}

static const VMStateDescription vmstate_uvc = {
    .name = TYPE_USB_UVC,
    .unmigratable = 1,
};

static Property uvc_properties[] = {
    DEFINE_PROP_BOOL("bulk", UVCState, bulk, false),
    DEFINE_PROP_BOOL("mjpeg-dht", UVCState, mjpeg_dht, false),
    DEFINE_PROP_BOOL("mjpeg420", UVCState, mjpeg420, false),
    DEFINE_PROP_UINT8("force-frame", UVCState, force_frame, 0),
    DEFINE_PROP_BOOL("small-maxframe", UVCState, small_maxframe, false),
    DEFINE_PROP_UINT32("debug", UVCState, debug, 0),
    DEFINE_PROP_STRING("dump-jpeg", UVCState, dump_jpeg),
    DEFINE_PROP_END_OF_LIST(),
};

static void uvc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    USBDeviceClass *k = USB_DEVICE_CLASS(klass);

    dc->vmsd = &vmstate_uvc;
    device_class_set_props(dc, uvc_properties);
    set_bit(DEVICE_CATEGORY_USB, dc->categories);
    k->product_desc = "QEMU USB Video Class test camera";
    k->realize = uvc_realize;
    k->handle_reset = uvc_handle_reset;
    k->handle_attach = usb_desc_attach;
    k->handle_control = uvc_handle_control;
    k->handle_data = uvc_handle_data;
    k->unrealize = uvc_unrealize;
    k->set_interface = uvc_set_interface;
}

static const TypeInfo uvc_info = {
    .name = TYPE_USB_UVC,
    .parent = TYPE_USB_DEVICE,
    .instance_size = sizeof(UVCState),
    .class_init = uvc_class_init,
};

static void uvc_register_types(void)
{
    type_register_static(&uvc_info);
}

type_init(uvc_register_types)
