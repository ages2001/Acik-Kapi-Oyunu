/* Virtio 1.x PCI configuration-access capability and split control queue.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "gpu.h"

enum { DESCRIPTORS = 0, AVAIL = 128, USED = 160, REQUEST = 256,
       RESPONSE = 4352, TEXTURE = 5760, VERTICES = 6144, PIXELS = 8192, SECONDARY = 12288, QUEUE_SIZE = 8 };
enum { GET_DISPLAY_INFO = 0x100, UNREF = 0x102, SET_SCANOUT = 0x103, FLUSH = 0x104, GET_EDID = 0x10a, ATTACH_BACKING = 0x106,
       CTX_CREATE = 0x200, CTX_DESTROY = 0x201, CTX_ATTACH = 0x202, CTX_DETACH = 0x203, CREATE_3D = 0x204,
       TO_HOST = 0x205, FROM_HOST = 0x206, SUBMIT_3D = 0x207 };
enum { STATUS = 20, SIZE = 24, ENABLE = 28, NOTIFY_OFF = 30 };

static void clear_bytes(volatile void *data, unsigned bytes)
{
    volatile uint8_t *p = data;
    while (bytes--) *p++ = 0;
}

static uint32_t config(WxpGpu *g, unsigned offset, unsigned bytes)
{
    return g->read(g->bus, g->slot, offset, bytes);
}

static void configure(WxpGpu *g, unsigned offset, unsigned bytes, uint32_t value)
{
    g->write(g->bus, g->slot, offset, bytes, value);
}

static int capabilities(WxpGpu *g)
{
    unsigned cap, hops, common = 0, notify = 0, device = 0;
    if (!(config(g, 6, 2) & 16)) return 0;
    cap = (unsigned)config(g, 0x34, 1) & 0xfc;
    g->access = 0;
    for (hops = 0; cap >= 0x40 && cap <= 0xfc && hops < 48; ++hops) {
        uint32_t hdr = config(g, cap, 4);
        unsigned next = (unsigned)(hdr >> 8) & 0xfc;
        unsigned size = (unsigned)(hdr >> 16) & 255;
        if ((hdr & 255) == 9 && size >= 16 && cap + size <= 256) {
            uint32_t off = config(g, cap + 8, 4);
            unsigned bar = (unsigned)config(g, cap + 4, 1);
            switch (hdr >> 24) {
            case 1: g->bar_common = bar; g->common = off; common = 1; break;
            case 2:
                if (size >= 20) {
                    g->bar_notify = bar; g->notify = off;
                    g->multiplier = config(g, cap + 16, 4); notify = 1;
                }
                break;
            case 4: g->bar_device = bar; g->device = off; device = 1; break;
            case 5: if (size >= 20) g->access = cap; break;
            }
        }
        cap = next;
    }
    return common && notify && device && g->access;
}

static void select_bar(WxpGpu *g, unsigned bar, uint32_t off, unsigned n)
{
    configure(g, g->access + 4, 1, bar);
    configure(g, g->access + 8, 4, off);
    configure(g, g->access + 12, 4, n);
}

static uint32_t cget(WxpGpu *g, unsigned off, unsigned n)
{
    select_bar(g, g->bar_common, g->common + off, n);
    return config(g, g->access + 16, n);
}

static void cset(WxpGpu *g, unsigned off, unsigned n, uint32_t v)
{
    select_bar(g, g->bar_common, g->common + off, n);
    configure(g, g->access + 16, n, v);
}

static uint32_t dma_address(const WxpGpu *g, unsigned offset)
{
    return g->page[offset / 4096] + (offset & 4095);
}

static void address(WxpGpu *g, unsigned reg, unsigned off)
{
    cset(g, reg, 4, dma_address(g, off));
    cset(g, reg + 4, 4, 0);
}

static int initialize(WxpGpu *g)
{
    unsigned bus, slot;
    /* Start with bus zero. Secondary buses are discovered through PCI bridges. */
    unsigned limit = 0;
    int found = 0;
    for (bus = 0; !found && bus <= limit; ++bus) {
        for (slot = 0; slot < 256; ++slot) {
            uint32_t id = g->read(bus, slot, 0, 4);
            if ((id & 0xffffU) == 0xffffU) {
                if (!(slot & 7)) slot += 7;
                continue;
            }
            if (id == 0x10501af4UL) {
                g->bus = bus; g->slot = slot; found = 1; break;
            }
            if ((g->read(bus, slot, 8, 4) >> 16) == 0x0604) {
                unsigned end = (unsigned)g->read(bus, slot, 0x1a, 1);
                if (end > limit) limit = end;
            }
            if (!(slot & 7) && !(g->read(bus, slot, 0x0e, 1) & 0x80)) slot += 7;
        }
    }
    if (!found) return WXP_NO_DEVICE;
    if (!capabilities(g)) return WXP_PCI_CAP;
    if (cget(g, STATUS, 1)) return WXP_BUSY;
    g->cfg_status = (unsigned)config(g, 4, 2);
    configure(g, 4, 2, g->cfg_status | 6);
    g->owned = 1;
    cset(g, STATUS, 1, 3);
    cset(g, 0, 4, 1);
    if (!(cget(g, 4, 4) & 1)) return WXP_FEATURES;
    cset(g, 0, 4, 0);
    g->features = cget(g, 4, 4) & 3;
    if (!(g->features & 1)) return WXP_FEATURES;
    cset(g, 8, 4, 0); cset(g, 12, 4, g->features);
    cset(g, 8, 4, 1); cset(g, 12, 4, 1);
    cset(g, STATUS, 1, 11);
    if (!(cget(g, STATUS, 1) & 8)) return WXP_FEATURES;
    cset(g, 22, 2, 0);
    if (cget(g, SIZE, 2) < QUEUE_SIZE || cget(g, ENABLE, 2)) return WXP_QUEUE;
    cset(g, SIZE, 2, QUEUE_SIZE);
    address(g, 32, DESCRIPTORS); address(g, 40, AVAIL); address(g, 48, USED);
    g->notify += cget(g, NOTIFY_OFF, 2) * g->multiplier;
    g->avail = g->used = 0;
    g->fence = 0;
    *(volatile uint16_t *)(g->dma + AVAIL) = 1; /* polling, no interrupts */
    cset(g, ENABLE, 2, 1);
    cset(g, STATUS, 1, 15);
    return WXP_OK;
}

static void descriptor(WxpGpu *g, unsigned id, unsigned off, unsigned bytes, unsigned flags)
{
    volatile uint32_t *p = (volatile uint32_t *)(g->dma + 16 * id);
    p[0] = dma_address(g, off); p[1] = 0; p[2] = bytes;
    p[3] = flags | (id == 0 ? 1UL << 16 : 0); /* first descriptor links to 1 */
}

static volatile uint32_t *begin(WxpGpu *g, unsigned command, unsigned context)
{
    volatile uint32_t *p = (volatile uint32_t *)(g->dma + REQUEST);
    clear_bytes(p, 128);
    p[0] = command; p[4] = context;
    /* Only rendering and transfers require a GPU completion fence. Ordinary
     * control responses acknowledge mode changes and resource lifetime. */
    if (command == SUBMIT_3D || command == TO_HOST || command == FROM_HOST) {
        p[1] = 1; p[2] = ++g->fence;
    }
    g->out->request = command;
    return p + 6;
}

static int finish(WxpGpu *g, unsigned payload_words, unsigned response_type, unsigned response_size)
{
    volatile uint16_t *available = (volatile uint16_t *)(g->dma + AVAIL);
    volatile uint16_t *used = (volatile uint16_t *)(g->dma + USED);
    volatile uint32_t *reply = (volatile uint32_t *)(g->dma + RESPONSE);
    uint32_t start;
    unsigned index = g->used % QUEUE_SIZE;
    uint32_t fenced = ((volatile uint32_t *)(g->dma + REQUEST))[1];
    clear_bytes(reply, response_size);
    descriptor(g, 0, REQUEST, 24 + payload_words * 4, 1);
    descriptor(g, 1, RESPONSE, response_size, 2);
    available[2 + (g->avail % QUEUE_SIZE)] = 0;
    /* x86 stores are ordered. Volatile objects preserve the publication order. */
    available[1] = ++g->avail;
    select_bar(g, g->bar_notify, g->notify, 2);
    configure(g, g->access + 16, 2, 0);
    start = g->time_ms();
    while (used[1] == g->used) {
        if ((uint32_t)(g->time_ms() - start) >= 15000) return WXP_TIMEOUT;
        g->poll();
    }
    ++g->used;
    g->out->response = reply[0];
    if (used[1] != g->used || ((volatile uint32_t *)(used + 2))[index * 2] != 0 ||
        ((volatile uint32_t *)(used + 2))[index * 2 + 1] != response_size ||
        reply[0] != response_type || (reply[1] & 1) != fenced ||
        (fenced && (reply[2] != g->fence || reply[3])))
        return WXP_RESPONSE;
    ++g->out->completed;
    if (fenced) g->out->fence = g->fence;
    return WXP_OK;
}

static int query_outputs(WxpGpu *g)
{
    unsigned i;
    int err;
    volatile uint32_t *p = (volatile uint32_t *)(g->dma + RESPONSE + 24);
    select_bar(g, g->bar_device, g->device + 8, 4);
    g->out->outputs = config(g, g->access + 16, 4);
    begin(g, GET_DISPLAY_INFO, 0);
    err = finish(g, 0, 0x1101, 24 + 16 * 24);
    if (err) return err;
    for (i = 0; i < g->out->outputs && i < 16; ++i) {
        if (p[i * 6 + 4]) g->out->enabled_outputs |= 1UL << i;
    }
    if (!(g->features & 2)) return WXP_OK;
    for (i = 0; i < g->out->outputs && i < 16; ++i) {
        unsigned j, checksum = 0;
        volatile uint8_t *edid = g->dma + RESPONSE + 32;
        begin(g, GET_EDID, 0)[0] = i;
        err = finish(g, 2, 0x1104, 24 + 8 + 1024);
        if (err) return err;
        if (p[0] >= 128 && p[0] <= 1024 && !edid[0] && edid[1] == 255) {
            for (j = 0; j < 128; ++j) checksum += edid[j];
            if (!(checksum & 255)) g->out->edid_outputs |= 1UL << i;
        }
    }
    return WXP_OK;
}

static int resource(WxpGpu *g, unsigned id, unsigned target, unsigned format, uint32_t bind,
                    unsigned width, unsigned height, unsigned backing, unsigned bytes)
{
    volatile uint32_t *p = begin(g, CREATE_3D, 0);
    int err;
    p[0] = id; p[1] = target; p[2] = format; p[3] = bind;
    p[4] = width; p[5] = height; p[6] = 1; p[7] = 1;
    err = finish(g, 12, 0x1100, 24);
    if (err) return err;
    p = begin(g, CTX_ATTACH, 1); p[0] = id;
    err = finish(g, 2, 0x1100, 24);
    if (err || !bytes) return err;
    p = begin(g, ATTACH_BACKING, 0);
    p[0] = id; p[1] = 1; p[2] = dma_address(g, backing); p[4] = bytes;
    return finish(g, 6, 0x1100, 24);
}

static int transfer(WxpGpu *g, unsigned dir, unsigned id, unsigned w, unsigned h)
{
    volatile uint32_t *p = begin(g, dir, 1);
    p[3] = w; p[4] = h; p[5] = 1; p[8] = id;
    return finish(g, 12, 0x1100, 24);
}

static int show_output(WxpGpu *g, unsigned head, unsigned id)
{
    volatile uint32_t *p = begin(g, SET_SCANOUT, 0);
    int err;
    p[2] = WXP_SIDE; p[3] = WXP_SIDE; p[4] = head; p[5] = id;
    err = finish(g, 6, 0x1100, 24);
    if (err) return err;
    p = begin(g, FLUSH, 0);
    p[2] = WXP_SIDE; p[3] = WXP_SIDE; p[4] = id;
    return finish(g, 6, 0x1100, 24);
}

static uint32_t stripe(unsigned i)
{
    return (i / (WXP_SIDE * 8)) & 1 ? 0xff0040ffUL : 0xffffcc00UL;
}

static int test_outputs(WxpGpu *g)
{
    unsigned i;
    volatile uint32_t *backing = (volatile uint32_t *)(g->dma + SECONDARY);
    int err = resource(g, 4, 2, 67, (1UL << 18) | 2, WXP_SIDE, WXP_SIDE,
                       SECONDARY, WXP_SIDE * WXP_SIDE * 4);
    if (err) return err;
    for (i = 0; i < WXP_SIDE * WXP_SIDE; ++i) backing[i] = stripe(i);
    if ((err = transfer(g, TO_HOST, 4, WXP_SIDE, WXP_SIDE)) != 0) return err;
    if ((err = show_output(g, 0, 1)) != 0 || (err = show_output(g, 1, 4)) != 0) return err;
    if (g->observe_outputs) g->observe_outputs();
    /* The GPU must return the second texture after both heads have been flushed. */
    clear_bytes(backing, WXP_SIDE * WXP_SIDE * 4);
    if ((err = transfer(g, FROM_HOST, 4, WXP_SIDE, WXP_SIDE)) != 0) return err;
    for (i = 0; i < WXP_SIDE * WXP_SIDE; ++i)
        if (backing[i] != stripe(i)) return WXP_RESPONSE;
    g->out->tested_outputs = 3;
    return WXP_OK;
}

static int retire_frame(WxpGpu *g, const WxpDraw *draw)
{
    volatile uint32_t *p;
    unsigned id;
    int err;
    if (g->out->tested_outputs) {
        unsigned head;
        for (head = 2; head; --head) {
            p = begin(g, SET_SCANOUT, 0); p[4] = head - 1;
            if ((err = finish(g, 6, 0x1100, 24)) != 0) return err;
        }
    }
    for (id = 1; id <= 5; ++id) {
        if ((id == 4 && !g->out->tested_outputs) || (id == 5 && !(draw->flags & WXP_DRAW_TEXTURED))) continue;
        p = begin(g, CTX_DETACH, 1); p[0] = id;
        if ((err = finish(g, 2, 0x1100, 24)) != 0) return err;
        p = begin(g, UNREF, 0); p[0] = id;
        if ((err = finish(g, 2, 0x1100, 24)) != 0) return err;
    }
    begin(g, CTX_DESTROY, 1);
    return finish(g, 0, 0x1100, 24);
}

static int render(WxpGpu *g, const WxpDraw *draw)
{
    volatile uint32_t *p;
    unsigned i, words, bytes = (unsigned)draw->count * sizeof(WxpVertex);
    uint32_t bind = 2;
    int err = query_outputs(g);
    if (err) return err;
    p = begin(g, CTX_CREATE, 1);
    p[0] = 7; p[2] = 0x58504e57UL; p[3] = 0x004d4450UL;
    if ((err = finish(g, 18, 0x1100, 24)) != 0) return err;
    if ((draw->flags & WXP_DRAW_TEST_OUTPUTS) && g->out->outputs >= 2) bind |= 1UL << 18;
    if ((err = resource(g, 1, 2, 67, bind, WXP_SIDE, WXP_SIDE, PIXELS, WXP_SIDE * WXP_SIDE * 4)) != 0) return err;
    if ((err = resource(g, 2, 0, 64, 16, bytes, 1, VERTICES, bytes)) != 0) return err;
    if ((err = resource(g, 3, 2, 21, 1, WXP_SIDE, WXP_SIDE, 0, 0)) != 0) return err;
    for (i = 0; i < bytes; ++i) g->dma[VERTICES + i] = ((const uint8_t *)draw->vertices)[i];
    if ((err = transfer(g, TO_HOST, 2, bytes, 1)) != 0) return err;
    if (draw->flags & WXP_DRAW_TEXTURED) {
        if ((err = resource(g, 5, 2, 67, 8, 2, 2, TEXTURE, 16)) != 0) return err;
        for (i = 0; i < 4; ++i) ((volatile uint32_t *)(g->dma + TEXTURE))[i] = draw->texture[i];
        if ((err = transfer(g, TO_HOST, 5, 2, 2)) != 0) return err;
    }
    p = begin(g, SUBMIT_3D, 1);
    words = wxp_virgl_encode((uint32_t *)(p + 2), draw);
    p[0] = 4 * words;
    if ((err = finish(g, 2 + words, 0x1100, 24)) != 0) return err;
    if ((draw->flags & WXP_DRAW_TEST_OUTPUTS) && g->out->outputs >= 2) {
        if ((err = test_outputs(g)) != 0) return err;
    }
    if ((err = transfer(g, FROM_HOST, 1, WXP_SIDE, WXP_SIDE)) != 0) return err;
    for (i = 0; i < WXP_SIDE * WXP_SIDE; ++i)
        g->out->pixels[i] = ((volatile uint32_t *)(g->dma + PIXELS))[i];
    return retire_frame(g, draw);
}

/* Keep the DMA pages pinned until a device we own acknowledges a reset.
 * A later caller can finish an interrupted reset before reusing the queue. */
int wxp_gpu_close(WxpGpu *g)
{
    uint32_t start;
    g->ready = 0;
    if (!g->owned) return WXP_OK;
    start = g->time_ms();
    cset(g, STATUS, 1, 0);
    while (cget(g, STATUS, 1)) {
        if ((uint32_t)(g->time_ms() - start) >= 15000) return WXP_TIMEOUT;
        g->poll();
    }
    configure(g, 4, 2, g->cfg_status);
    g->owned = 0;
    return WXP_OK;
}

int wxp_gpu_draw(WxpGpu *g, const WxpDraw *draw, WxpFrame *frame)
{
    int err;
    clear_bytes(frame, sizeof(*frame));
    frame->abi = WXP_GPU_ABI;
    if (!draw || draw->abi != WXP_GPU_ABI || !draw->count || draw->count > WXP_MAX_VERTICES || draw->count % 3 || (draw->flags & ~((uint32_t)(WXP_DRAW_TEST_OUTPUTS | WXP_DRAW_TEXTURED))))
        return frame->status = WXP_BAD_INPUT;
    if (g->owned && !g->ready && wxp_gpu_close(g)) return frame->status = WXP_TIMEOUT;
    if (!g->dma) return frame->status = WXP_MEMORY;
    {
        unsigned i;
        for (i = 0; i < WXP_DMA_SIZE / 4096; ++i)
            if (!g->page[i] || (g->page[i] & 4095)) return frame->status = WXP_MEMORY;
    }
    g->out = frame;
    err = WXP_OK;
    if (!g->ready) {
        clear_bytes(g->dma, WXP_DMA_SIZE);
        err = initialize(g);
        if (!err) g->ready = 1;
    }
    if (!err) err = render(g, draw);
    if (err && wxp_gpu_close(g)) err = WXP_TIMEOUT;
    g->out = 0;
    frame->status = err;
    return err;
}
