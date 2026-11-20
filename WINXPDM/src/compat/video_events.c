/* Native event services used by the XP video miniport.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#define _X86_ 1
#include <ntddk.h>

typedef struct {
    PKEVENT object;
    ULONG flags;
    KEVENT storage;
} WXP_VIDEO_EVENT;
C_ASSERT(FIELD_OFFSET(WXP_VIDEO_EVENT, storage) == 8);
C_ASSERT(sizeof(WXP_VIDEO_EVENT) == 24);

ULONG NTAPI WxpCreateEvent(PVOID device, ULONG flags, PVOID reserved, WXP_VIDEO_EVENT **out)
{
    WXP_VIDEO_EVENT *event;
    UNREFERENCED_PARAMETER(device);
    UNREFERENCED_PARAMETER(reserved);
    if (!out) return 87; /* ERROR_INVALID_PARAMETER */
#ifdef WXP_NT31
    event = ExAllocatePool(NonPagedPool, sizeof(*event));
#else
    event = ExAllocatePoolWithTag(NonPagedPool, sizeof(*event), 'ePxW');
#endif
    if (!event) return 8; /* ERROR_NOT_ENOUGH_MEMORY */
    event->object = &event->storage;
    event->flags = 0;
    KeInitializeEvent(event->object, flags & 1 ? NotificationEvent : SynchronizationEvent, (flags & 2) != 0);
    *out = event;
    return 0;
}

ULONG NTAPI WxpDeleteEvent(PVOID device, WXP_VIDEO_EVENT *event)
{
    UNREFERENCED_PARAMETER(device);
    if (!event || !event->object || (event->flags & 1)) return 87;
    ExFreePool(event);
    return 0;
}

ULONG NTAPI WxpWait(PVOID device, WXP_VIDEO_EVENT *event, PLARGE_INTEGER timeout)
{
    NTSTATUS status;
    UNREFERENCED_PARAMETER(device);
    if (!event || !event->object || (event->flags & 1)) return 87;
    status = KeWaitForSingleObject(event->object, Executive, KernelMode, FALSE, timeout);
    if (status == STATUS_TIMEOUT) return 258; /* WAIT_TIMEOUT */
    return NT_SUCCESS(status) ? 0 : 87;
}
