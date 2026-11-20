/* NT 3.1 PCI video-port companion, x86.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef WXP_NT31_VIDEO_H
#define WXP_NT31_VIDEO_H
#include <ntddk.h>
/* miniport.h also supplies the two video-only types omitted by ntddk.h. */
#define __BROKEN__
#include <ddk/miniport.h>
#undef __BROKEN__
#define _NTOS_
#define _NTOSDEF_
#include <ddk/dderror.h>
#include <ddk/video.h>

/* The 3.1 kernel exports RtlMoveMemory and RtlZeroMemory but no C runtime. */
#undef RtlCopyMemory
#undef RtlZeroMemory
extern VOID NTAPI wxp_copy(PVOID, const VOID *, SIZE_T) __asm__("_RtlMoveMemory@12");
extern VOID NTAPI wxp_zero(PVOID, SIZE_T) __asm__("_RtlZeroMemory@8");
#define RtlCopyMemory wxp_copy
#define RtlZeroMemory wxp_zero
#define ERROR_INVALID_DATA 13

typedef struct {
    ULONG bus, slot;
    BOOLEAN vga_route;
} WXP_PCI_ADDRESS;

BOOLEAN wxp_find_display(ULONG id, WXP_PCI_ADDRESS *address);
VP_STATUS wxp_pci_ranges(const WXP_PCI_ADDRESS *address, PVIDEO_ACCESS_RANGE ranges,
                         PULONG count);

typedef struct {
    ULONG index, uid, type;
    UCHAR descriptor[256];
} WXP_CHILD_RECORD;

typedef struct {
    ULONG version, count;
    WXP_CHILD_RECORD child[1];
} WXP_CHILD_LIST;

typedef struct {
    PVIDEO_HW_GET_CHILD_DESCRIPTOR describe;
    PVOID hardware;
    ULONG extension_size;
    struct wxp_child_slot *slots;
    WORK_QUEUE_ITEM work;
    BOOLEAN busy, pending, ready;
} WXP_CHILD_PORT;

void wxp_children_init(WXP_CHILD_PORT *, PVOID, PVIDEO_HW_INITIALIZATION_DATA);
void wxp_children_request(WXP_CHILD_PORT *, BOOLEAN);
#endif
