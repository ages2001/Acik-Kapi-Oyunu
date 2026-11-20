/* Virtio 1.x GPU control queue for legacy x86 Windows.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef WINXPDM_VIRTIO_GPU_H
#define WINXPDM_VIRTIO_GPU_H
#include <stdint.h>

#define WXP_GPU_ABI 1
#define WXP_DRAW_TEST_OUTPUTS 1U
#define WXP_DRAW_TEXTURED 2U
#define WXP_SIDE 32
#define WXP_MAX_VERTICES 48
#define WXP_DMA_SIZE 16384
#define WXP_GPU_IOCTL 0x0022a004UL /* FILE_DEVICE_UNKNOWN, write access, buffered */

typedef struct {
    uint32_t position[4], color[4], uv[2]; /* IEEE binary32 clip coordinates and RGBA */
} WxpVertex;

typedef struct {
    uint32_t abi, count, clear[4];
    WxpVertex vertices[WXP_MAX_VERTICES]; /* triangle list */
    uint32_t flags, texture[4]; /* 2 x 2 RGBA8, increasing U within each row */
} WxpDraw;

typedef struct {
    uint32_t abi, status, request, response, completed, outputs;
    uint32_t edid_outputs, enabled_outputs, fence, tested_outputs;
    uint32_t pixels[WXP_SIDE * WXP_SIDE]; /* little-endian R8G8B8A8 */
} WxpFrame;

enum {
    WXP_OK, WXP_NO_DEVICE, WXP_BUSY, WXP_PCI_CAP, WXP_FEATURES,
    WXP_QUEUE, WXP_TIMEOUT, WXP_RESPONSE, WXP_BAD_INPUT, WXP_MEMORY
};

/* Synchronous sessions have exclusive ownership of the provisioned device.
 * Each entry describes one locked 4 KiB page below 4 GiB. */
typedef struct {
    unsigned bus, slot;
    uint32_t (*read)(unsigned bus, unsigned slot, unsigned offset, unsigned bytes);
    void (*write)(unsigned bus, unsigned slot, unsigned offset, unsigned bytes, uint32_t value);
    uint32_t (*time_ms)(void);
    void (*poll)(void);
    void (*observe_outputs)(void); /* optional test observer while both scanouts are live */
    volatile uint8_t *dma;
    uint32_t page[WXP_DMA_SIZE / 4096];
    unsigned access, bar_common, bar_notify, bar_device, cfg_status;
    uint32_t common, notify, device, multiplier, features, fence;
    uint16_t avail, used;
    unsigned owned, ready;
    WxpFrame *out;
} WxpGpu;

int wxp_gpu_close(WxpGpu *gpu);
int wxp_gpu_draw(WxpGpu *gpu, const WxpDraw *draw, WxpFrame *frame);
unsigned wxp_virgl_encode(uint32_t *words, const WxpDraw *draw);
#endif
