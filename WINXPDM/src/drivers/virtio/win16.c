/* Win386 enhanced-mode interface, shared by Windows 3.x and Windows 9x.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <windows.h>
#include <i86.h>
#include "gpu.h"
#include "../pci16.h"

static WxpGpu gpu;
static unsigned busy, selector;

/* Virtual DMA Services scatter/gather lock, using the page-table format. */
static struct {
    DWORD size, offset;
    WORD segment, reserved, capacity, used;
    DWORD page[WXP_DMA_SIZE / 4096];
} locked;

static int vds(unsigned function, unsigned flags)
{
    void FAR *description = &locked;
    unsigned failed;
    _asm {
        push es
        push di
        push bx
        les di, description
        mov ax, function
        mov dx, flags
        int 4bh
        sbb ax, ax
        mov failed, ax
        pop bx
        pop di
        pop es
    }
    return !failed;
}

static uint32_t read_cfg(unsigned b, unsigned s, unsigned o, unsigned n)
{ return pci_access(b, s, o, n, 0, 0); }
static void write_cfg(unsigned b, unsigned s, unsigned o, unsigned n, uint32_t v)
{ pci_access(b, s, o, n, v, 1); }
static uint32_t now(void) { return GetTickCount(); }
static void poll_gpu(void) { Yield(); }

static int release_memory(void)
{
    int rc = wxp_gpu_close(&gpu);
    if (rc || !selector) return rc;
    if (!vds(0x8106, 64)) return WXP_MEMORY;
    GlobalDosFree(selector);
    selector = 0; gpu.dma = NULL;
    return WXP_OK;
}
int FAR PASCAL __export WxpClose(void)
{
    int rc;
    if (busy) return WXP_BUSY;
    busy=1; rc=release_memory(); busy=0;
    return rc;
}
int FAR PASCAL __export WxpDrawTriangles(const WxpDraw FAR *draw, WxpFrame FAR *frame)
{
    int result;
    DWORD memory;
    unsigned i, pad;
    if (!frame || !draw) return WXP_BAD_INPUT;
    if (busy) return WXP_BUSY;
    if (!(GetWinFlags() & WF_ENHANCED)) return WXP_NO_DEVICE;
    if (!selector) {
        memory = GlobalDosAlloc(WXP_DMA_SIZE + 4095UL);
        if (!memory) return WXP_MEMORY;
        selector = LOWORD(memory);
        pad = (unsigned)(0U - (HIWORD(memory) << 4)) & 4095;
        gpu.dma = MK_FP(selector, pad);
        locked.size = WXP_DMA_SIZE; locked.offset = pad; locked.segment = selector;
        locked.capacity = WXP_DMA_SIZE / 4096; locked.used = 0;
        for (i = 0; i < locked.capacity; ++i) gpu.dma[i * 4096] = 0;
        if (!vds(0x8105, 64)) {
            GlobalDosFree(selector); selector = 0; gpu.dma = NULL; return WXP_MEMORY;
        }
        if (locked.used != locked.capacity) {
            vds(0x8106, 64); GlobalDosFree(selector); selector = 0; gpu.dma = NULL; return WXP_MEMORY;
        }
        for (i = 0; i < locked.used; ++i) gpu.page[i] = locked.page[i] & 0xfffff000UL;
        gpu.read = read_cfg; gpu.write = write_cfg; gpu.time_ms = now; gpu.poll = poll_gpu;
    }
    busy = 1;
    result = wxp_gpu_draw(&gpu, draw, frame);
    if (result && !gpu.owned) release_memory();
    busy = 0;
    return result;
}

BOOL FAR PASCAL LibMain(HINSTANCE instance, WORD segment, WORD heap, LPSTR args)
{
    (void)instance; (void)segment; (void)heap; (void)args;
    return TRUE;
}
int FAR PASCAL __export WEP(int why) { (void)why; WxpClose(); return 1; }
