/* Negotiate the documented NT5 miniport initialization prefix.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <ntdef.h>
#include <ddk/miniport.h>
#include <ddk/dderror.h>
#include <ddk/video.h>

#ifdef WXP_TEST_TRACE
static void trace_value(const char *caption, ULONG value)
{
    static const char hex[] = "0123456789abcdef";
    int shift;
    while (*caption)
        __asm__ volatile("outb %0, $0xe9" : : "a"((unsigned char)*caption++));
    for (shift = 28; shift >= 0; shift -= 4)
        __asm__ volatile("outb %0, $0xe9" : : "a"(hex[(value >> shift) & 15]));
    __asm__ volatile("outb %0, $0xe9" : : "a"((unsigned char)'\n'));
}
#else
#define trace_value(caption, value) ((void)0)
#endif

ULONG NTAPI WxpVideoInitialize(PVOID driver, PVOID registry, PVIDEO_HW_INITIALIZATION_DATA init, PVOID context)
{
    VIDEO_HW_INITIALIZATION_DATA local;
    ULONG status;
    if (init) {
        trace_value("WXP2K incoming init size 0x", init->HwInitDataSize);
        if (init->HwInitDataSize == SIZE_OF_WXP_VIDEO_HW_INITIALIZATION_DATA && !init->Reserved) {
            /* NT 5.0 has the same callbacks. XP appends one reserved ULONG. */
            local = *init;
            local.HwInitDataSize = SIZE_OF_W2K_VIDEO_HW_INITIALIZATION_DATA;
            init = &local;
        }
        trace_value("WXP2K native init size 0x", init->HwInitDataSize);
    }
    status = VideoPortInitialize(driver, registry, init, context);
    trace_value("WXP2K native port result 0x", status);
    return status;
}
