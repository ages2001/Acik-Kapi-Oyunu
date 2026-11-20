/* Kernel and video-port services for x86 Windows NT 3.1.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <ntddk.h>

/* The NT 3.1 resource contains embedded wait objects and occupies 104 bytes.
   XP clients supply a smaller ERESOURCE. Keep the native object separately. */
typedef struct { ULONG words[26]; } NATIVE_RESOURCE;
typedef struct { NATIVE_RESOURCE *native; } RESOURCE_HANDLE;
C_ASSERT(sizeof(RESOURCE_HANDLE) <= sizeof(ERESOURCE));

#undef ExInitializeResource
#undef ExAcquireResourceShared
#undef ExAcquireResourceExclusive
#undef ExReleaseResource
#undef ExDeleteResource
extern NTSTATUS NTAPI ExInitializeResource(NATIVE_RESOURCE *);
extern BOOLEAN NTAPI ExAcquireResourceShared(NATIVE_RESOURCE *, BOOLEAN);
extern BOOLEAN NTAPI ExAcquireResourceExclusive(NATIVE_RESOURCE *, BOOLEAN);
extern VOID NTAPI ExReleaseResource(NATIVE_RESOURCE *);
extern VOID NTAPI ExDeleteResource(NATIVE_RESOURCE *);
extern KPROCESSOR_MODE NTAPI KeGetPreviousMode(void);
extern VOID NTAPI NativeQueryTicks(PLARGE_INTEGER) __asm__("_KeQueryTickCount@4");
extern VOID NTAPI NativeDereference(PVOID) __asm__("_ObDereferenceObject@4");
extern NTSTATUS NTAPI NativeCall(PDEVICE_OBJECT, PIRP) __asm__("_IoCallDriver@8");

PVOID NTAPI WxpAllocate(POOL_TYPE type, SIZE_T bytes, ULONG tag)
{
    ULONG flags = (ULONG)type;
    PVOID memory;
    UNREFERENCED_PARAMETER(tag);
    /* The cold-allocation bit is a distribution hint in the XP allocator. */
    memory = ExAllocatePool((POOL_TYPE)(flags & ~(0x100UL | POOL_RAISE_IF_ALLOCATION_FAILURE)), bytes);
    if (!memory && (flags & POOL_RAISE_IF_ALLOCATION_FAILURE))
        ExRaiseStatus(STATUS_INSUFFICIENT_RESOURCES);
    return memory;
}

VOID NTAPI WxpFree(PVOID pointer, ULONG tag)
{
    UNREFERENCED_PARAMETER(tag);
    ExFreePool(pointer);
}

NTSTATUS NTAPI WxpInitializeResource(RESOURCE_HANDLE *resource)
{
    NTSTATUS status;
    resource->native = ExAllocatePool(NonPagedPool, sizeof(*resource->native));
    if (!resource->native) return STATUS_INSUFFICIENT_RESOURCES;
    status = ExInitializeResource(resource->native);
    if (!NT_SUCCESS(status)) {
        ExFreePool(resource->native);
        resource->native = NULL;
    }
    return status;
}

BOOLEAN NTAPI WxpAcquireShared(RESOURCE_HANDLE *resource, BOOLEAN wait)
{
    return ExAcquireResourceShared(resource->native, wait);
}

BOOLEAN NTAPI WxpAcquireExclusive(RESOURCE_HANDLE *resource, BOOLEAN wait)
{
    return ExAcquireResourceExclusive(resource->native, wait);
}

VOID FASTCALL WxpReleaseResource(RESOURCE_HANDLE *resource)
{
    ExReleaseResource(resource->native);
}

NTSTATUS NTAPI WxpDeleteResource(RESOURCE_HANDLE *resource)
{
    ExDeleteResource(resource->native);
    ExFreePool(resource->native);
    resource->native = NULL;
    return STATUS_SUCCESS;
}

VOID NTAPI WxpClearEvent(PRKEVENT event)
{
    KeResetEvent(event);
}

KPROCESSOR_MODE NTAPI WxpPreviousMode(void)
{
    return KeGetPreviousMode();
}

NTSTATUS FASTCALL WxpCallDriver(PDEVICE_OBJECT device, PIRP irp)
{
    return NativeCall(device, irp);
}

ULONGLONG NTAPI WxpInterruptTime(void)
{
    LARGE_INTEGER ticks;
    NativeQueryTicks(&ticks);
    return (ULONGLONG)ticks.QuadPart * KeQueryTimeIncrement();
}

KIRQL NTAPI WxpVideoIrql(PVOID extension)
{
    UNREFERENCED_PARAMETER(extension);
    return KeGetCurrentIrql();
}

typedef struct {
    WORK_QUEUE_ITEM queue;
    PDEVICE_OBJECT device;
    PIO_WORKITEM_ROUTINE worker;
    PVOID context;
} WXP_WORK_ITEM;

WXP_WORK_ITEM * NTAPI WxpAllocateWorkItem(PDEVICE_OBJECT device)
{
    WXP_WORK_ITEM *item = ExAllocatePool(NonPagedPool, sizeof(*item));
    if (item) item->device = device;
    return item;
}

static VOID NTAPI run_work(PVOID argument)
{
    WXP_WORK_ITEM *item = argument;
    PDEVICE_OBJECT device = item->device;
    /* The worker may free or requeue its item before returning. */
    item->worker(device, item->context);
    NativeDereference(device);
}

VOID NTAPI WxpQueueWorkItem(WXP_WORK_ITEM *item, PIO_WORKITEM_ROUTINE worker,
                            WORK_QUEUE_TYPE queue, PVOID context)
{
    NTSTATUS status = ObReferenceObjectByPointer(item->device, 0, NULL, KernelMode);
    ASSERT(NT_SUCCESS(status));
    if (!NT_SUCCESS(status)) return;
    item->worker = worker;
    item->context = context;
    ExInitializeWorkItem(&item->queue, run_work, item);
    ExQueueWorkItem(&item->queue, queue);
}

VOID NTAPI WxpFreeWorkItem(WXP_WORK_ITEM *item)
{
    ExFreePool(item);
}

NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry)
{
    UNREFERENCED_PARAMETER(driver);
    UNREFERENCED_PARAMETER(registry);
    return STATUS_SUCCESS;
}
