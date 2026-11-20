/* Enumerate XP video children from a PASSIVE_LEVEL worker on NT 3.1.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "video.h"

typedef struct wxp_child_slot {
    struct wxp_child_slot *next;
    ULONG index;
    UCHAR extension[1];
} CHILD_SLOT;

/* Bound a scan when a miniport never returns NO_MORE_DEVICES. Keep the
 * previous complete snapshot if the callback reaches this limit. */
#define CHILD_LIMIT 256

static ULONG list_bytes(ULONG n)
{
    return FIELD_OFFSET(WXP_CHILD_LIST, child) + n * sizeof(WXP_CHILD_RECORD);
}

static BOOLEAN present(const WXP_CHILD_LIST *list, ULONG index)
{
    ULONG i;
    for (i = 0; i < list->count; ++i) if (list->child[i].index == index) return TRUE;
    return FALSE;
}

static void enumerate(WXP_CHILD_PORT *port)
{
    ULONG capacity = 4, i;
    WXP_CHILD_LIST *list = ExAllocatePool(PagedPool, list_bytes(capacity));
    if (!list) return;
    list->version = 1;
    list->count = 0;
    for (i = 1; i <= CHILD_LIMIT; ++i) {
        VIDEO_CHILD_ENUM_INFO info = {0};
        WXP_CHILD_RECORD record = {0};
        CHILD_SLOT *slot = port->slots;
        ULONG unused = 0, j;
        VIDEO_CHILD_TYPE type = 0;
        VP_STATUS status;
        BOOLEAN new_slot = FALSE;
        while (slot && slot->index != i) slot = slot->next;
        if (port->extension_size && !slot) {
            if (port->extension_size > MAXULONG - FIELD_OFFSET(CHILD_SLOT, extension)) break;
            slot = ExAllocatePool(NonPagedPool, FIELD_OFFSET(CHILD_SLOT, extension) + port->extension_size);
            if (!slot) break;
            RtlZeroMemory(slot->extension, port->extension_size);
            slot->index = i;
            new_slot = TRUE;
        }
        info.Size = sizeof(info);
        info.ChildIndex = i;
        info.ChildDescriptorSize = sizeof(record.descriptor);
        info.ChildHwDeviceExtension = slot ? slot->extension : NULL;
        status = port->describe(port->hardware, &info, &type, record.descriptor, &record.uid, &unused);
        if (new_slot) {
            if (status == VIDEO_ENUM_MORE_DEVICES) {
                slot->next = port->slots;
                port->slots = slot;
            } else ExFreePool(slot);
        }
        if (status == VIDEO_ENUM_NO_MORE_DEVICES) {
            extern VP_STATUS NTAPI WxpSetRegistry(PVOID, PWSTR, PVOID, ULONG);
            if (WxpSetRegistry(port->hardware, L"WinxpdmChildren", list, list_bytes(list->count)) == NO_ERROR) {
                CHILD_SLOT **link = &port->slots;
                while (*link) {
                    CHILD_SLOT *old = *link;
                    if (present(list, old->index)) link = &old->next;
                    else { *link = old->next; ExFreePool(old); }
                }
            }
            break;
        }
        if (status == VIDEO_ENUM_INVALID_DEVICE) continue;
        if (status != VIDEO_ENUM_MORE_DEVICES || unused || type < Monitor || type > Other) break;
        for (j = 0; j < list->count; ++j) if (list->child[j].uid == record.uid) break;
        if (j != list->count) break;
        if (list->count == capacity) {
            WXP_CHILD_LIST *larger;
            capacity *= 2;
            larger = ExAllocatePool(PagedPool, list_bytes(capacity));
            if (!larger) break;
            RtlCopyMemory(larger, list, list_bytes(list->count));
            ExFreePool(list);
            list = larger;
        }
        record.index = i;
        record.type = type;
        RtlCopyMemory(&list->child[list->count++], &record, sizeof(record));
    }
    ExFreePool(list);
}

static VOID NTAPI worker(PVOID context)
{
    WXP_CHILD_PORT *port = context;
    KIRQL old;
    for (;;) {
        old = KfRaiseIrql(DISPATCH_LEVEL);
        if (!port->pending) {
            port->busy = FALSE;
            KfLowerIrql(old);
            return;
        }
        port->pending = FALSE;
        KfLowerIrql(old);
        enumerate(port);
    }
}

void wxp_children_init(WXP_CHILD_PORT *port, PVOID ext, PVIDEO_HW_INITIALIZATION_DATA info)
{
    port->describe = info->HwGetVideoChildDescriptor;
    port->extension_size = info->HwChildDeviceExtensionSize;
    port->hardware = ext;
    ExInitializeWorkItem(&port->work, worker, port);
}

void wxp_children_request(WXP_CHILD_PORT *port, BOOLEAN start)
{
    KIRQL old;
    if (!port->describe) return;
    old = KfRaiseIrql(DISPATCH_LEVEL);
    if (start) port->ready = TRUE;
    port->pending = TRUE;
    if (port->ready && !port->busy) {
        port->busy = TRUE;
        ExQueueWorkItem(&port->work, DelayedWorkQueue);
    }
    KfLowerIrql(old);
}
