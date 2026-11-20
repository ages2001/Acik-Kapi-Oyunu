#include "wdm_target.h"
#include <ntddk.h>
#include "protocol.h"
#include "queue.h"
#include "pci_device.h"

extern int WkCheck(void);
static const WCHAR name[] = L"\\Device\\WdmKit";
static const WCHAR alias[] = L"\\DosDevices\\WdmKit";

typedef struct {
    PDEVICE_OBJECT lower;
    ULONG signature;
} CONTROL_EXTENSION;

static PDEVICE_OBJECT baseDevice, filterDevice;
static BOOLEAN online;
static BOOLEAN attached;
static const UCHAR countFrames = 1;

static VOID detachFilter(void)
{
    if (__atomic_exchange_n(&attached, FALSE, __ATOMIC_ACQ_REL))
        IoDetachDevice(baseDevice);
}

static NTSTATUS NTAPI filterComplete(PDEVICE_OBJECT device, PIRP irp, PVOID markPath)
{
    (void)device;
    if (markPath && NT_SUCCESS(irp->IoStatus.Status) &&
        irp->IoStatus.Information == sizeof(ULONG)) {
        if (markPath == &countFrames)
            ++*(PULONG)irp->AssociatedIrp.SystemBuffer;
        else
            *(PULONG)irp->AssociatedIrp.SystemBuffer |= WK_STACK_FILTER;
    }
    if (irp->PendingReturned)
        IoMarkIrpPending(irp);
    return STATUS_CONTINUE_COMPLETION;
}

static NTSTATUS NTAPI retainIrp(PDEVICE_OBJECT device, PIRP irp, PVOID context)
{
    (void)device;
    *(PIO_STATUS_BLOCK)context = irp->IoStatus;
    return STATUS_MORE_PROCESSING_REQUIRED;
}

/* A private seven-device stack exercises variable extensions and every
 * dispatch/completion frame without attaching to an operating-system device. */
static NTSTATUS checkStack(PDRIVER_OBJECT owner)
{
    PDEVICE_OBJECT devices[7];
    unsigned count = 0, links = 0, i;
    BOOLEAN full = WkTargetVersion() == WK_FULL_TARGET;
    unsigned layers = full ? 7 : 2;
    PIRP irp;
    PIO_STACK_LOCATION stack;
    IO_STATUS_BLOCK result;
    ULONG depth = 0;
    NTSTATUS status = STATUS_SUCCESS;
    for (i = 0; i < layers; ++i) {
        ULONG bytes = full ? (i == 3 ? 5000 : 512 + 48*i) : 64;
        PUCHAR data;
        status = IoCreateDevice(owner, bytes, NULL, FILE_DEVICE_UNKNOWN, 0, FALSE, &devices[i]);
        if (!NT_SUCCESS(status)) goto done;
        ++count;
        data = devices[i]->DeviceExtension;
        if (data[0] || data[bytes-1]) {
            status = STATUS_DEVICE_DATA_ERROR;
            goto done;
        }
        data[bytes-1] = (UCHAR)(i+1);
        devices[i]->Flags &= ~DO_DEVICE_INITIALIZING;
        if (i) {
            status = IoAttachDeviceByPointer(devices[i], devices[i-1]);
            if (!NT_SUCCESS(status)) goto done;
            ++links;
            ((CONTROL_EXTENSION *)data)->lower = devices[i-1];
        }
    }
    if (devices[count-1]->StackSize != (CCHAR)count) {
        status = STATUS_DEVICE_DATA_ERROR;
        goto done;
    }
    irp = IoAllocateIrp((CCHAR)count, FALSE);
    if (!irp) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto done;
    }
    irp->AssociatedIrp.SystemBuffer = &depth;
    irp->Flags = IRP_BUFFERED_IO;
    stack = IoGetNextIrpStackLocation(irp);
    stack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
    stack->Parameters.DeviceIoControl.IoControlCode = WK_IOCTL_STACK_DEPTH;
    stack->Parameters.DeviceIoControl.OutputBufferLength = sizeof(depth);
    result.Status = STATUS_PENDING;
    result.Information = 0;
    IoSetCompletionRoutine(irp, retainIrp, &result, TRUE, TRUE, TRUE);
    status = IoCallDriver(devices[count-1], irp);
    IoFreeIrp(irp);
    if (status != STATUS_SUCCESS || result.Status != STATUS_SUCCESS ||
        result.Information != sizeof(depth) || depth != count)
        status = STATUS_DEVICE_DATA_ERROR;
done:
    while (links) {
        --links;
        IoDetachDevice(devices[links]);
    }
    while (count)
        IoDeleteDevice(devices[--count]);
    return status;
}

static NTSTATUS NTAPI dispatch(PDEVICE_OBJECT device, PIRP irp)
{
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
    CONTROL_EXTENSION *extension = device->DeviceExtension;
    NTSTATUS status = STATUS_SUCCESS;
    ULONG bytes = 0;
#ifndef WK_VMM
    if (stack->MajorFunction==IRP_MJ_DEVICE_CONTROL &&
        stack->Parameters.DeviceIoControl.IoControlCode==WK_IOCTL_QUIESCE) {
        __atomic_store_n(&online, FALSE, __ATOMIC_RELEASE);
        detachFilter();
        irp->IoStatus.Status=STATUS_SUCCESS;
        irp->IoStatus.Information=0;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }
#endif
    if (!__atomic_load_n(&online, __ATOMIC_ACQUIRE) &&
        stack->MajorFunction != IRP_MJ_INTERNAL_DEVICE_CONTROL &&
        stack->MajorFunction != IRP_MJ_CLEANUP && stack->MajorFunction != IRP_MJ_CLOSE) {
        irp->IoStatus.Status = STATUS_DEVICE_NOT_READY;
        irp->IoStatus.Information = 0;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return STATUS_DEVICE_NOT_READY;
    }
    if (extension->lower) {
        BOOLEAN path = stack->MajorFunction == IRP_MJ_DEVICE_CONTROL &&
            stack->Parameters.DeviceIoControl.IoControlCode == WK_IOCTL_STACK;
        BOOLEAN depth = stack->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL &&
            stack->Parameters.DeviceIoControl.IoControlCode == WK_IOCTL_STACK_DEPTH;
        PVOID context = depth ? (PVOID)&countFrames : path ? extension : NULL;
        IoCopyCurrentIrpStackLocationToNext(irp);
        IoSetCompletionRoutine(irp, filterComplete, context, TRUE, TRUE, TRUE);
        return IoCallDriver(extension->lower, irp);
    }
    if (stack->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL &&
        stack->Parameters.DeviceIoControl.IoControlCode == WK_IOCTL_STACK_DEPTH) {
        if (!irp->AssociatedIrp.SystemBuffer || stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(ULONG))
            status = STATUS_BUFFER_TOO_SMALL;
        else {
            *(PULONG)irp->AssociatedIrp.SystemBuffer = 1;
            bytes = sizeof(ULONG);
        }
        irp->IoStatus.Status = status;
        irp->IoStatus.Information = bytes;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return status;
    }
    if (stack->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL ||
        (stack->MajorFunction == IRP_MJ_DEVICE_CONTROL &&
         stack->Parameters.DeviceIoControl.IoControlCode == WK_IOCTL_DEFERRED))
        return WkQueueRequest(irp);
    if (stack->MajorFunction == IRP_MJ_CLEANUP)
        WkCleanupRequests(stack->FileObject);
    if (stack->MajorFunction == IRP_MJ_DEVICE_CONTROL) {
        ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;
        if (code >= WK_IOCTL_TARGET_BASE && code < WK_IOCTL_TARGET_BASE + 4UL * WK_TARGET_COUNT) {
            status = WkMatchTarget(code) ? STATUS_SUCCESS : STATUS_NOT_SUPPORTED;
        } else if (code != WK_IOCTL && code != WK_IOCTL_STACK && code != WK_IOCTL_PCI) {
            status = STATUS_INVALID_DEVICE_REQUEST;
        } else if (stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(ULONG)) {
            status = STATUS_BUFFER_TOO_SMALL;
        } else if (code == WK_IOCTL_PCI) {
            status = WkTestPci(filterDevice, irp->AssociatedIrp.SystemBuffer);
            if (NT_SUCCESS(status)) bytes = sizeof(ULONG);
        } else if (code == WK_IOCTL && (!WkCheck() || !WkQueueReady())) {
            status = STATUS_UNSUCCESSFUL;
        } else {
            *(PULONG)irp->AssociatedIrp.SystemBuffer = code == WK_IOCTL_STACK ?
                WK_STACK_BASE : extension->signature;
            bytes = sizeof(ULONG);
        }
    }
    irp->IoStatus.Status = status;
    irp->IoStatus.Information = bytes;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}

static VOID NTAPI unload(PDRIVER_OBJECT driver)
{
    UNICODE_STRING link;
    (void)driver;
    RtlInitUnicodeString(&link, alias);
    IoDeleteSymbolicLink(&link);
    __atomic_store_n(&online, FALSE, __ATOMIC_RELEASE);
    WkStopQueue();
    detachFilter();
    IoDeleteDevice(filterDevice);
    IoDeleteDevice(baseDevice);
}

NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry)
{
    PDEVICE_OBJECT lower;
    UNICODE_STRING devname, link;
    NTSTATUS status;
    (void)registry;
    if (!WkCheck())
        return STATUS_UNSUCCESSFUL;
    RtlInitUnicodeString(&devname, name);
    status = IoCreateDevice(driver, sizeof(CONTROL_EXTENSION), &devname,
                            WK_DEVICE_TYPE, 0, FALSE, &baseDevice);
    if (!NT_SUCCESS(status))
        return status;
    ((CONTROL_EXTENSION *)baseDevice->DeviceExtension)->signature = WK_SIGNATURE;
    driver->MajorFunction[IRP_MJ_CREATE] = dispatch;
    driver->MajorFunction[IRP_MJ_CLOSE] = dispatch;
    driver->MajorFunction[IRP_MJ_CLEANUP] = dispatch;
    driver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = dispatch;
    driver->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = dispatch;
    driver->DriverUnload = unload;
    status = checkStack(driver);
    if (!NT_SUCCESS(status)) goto deleteBase;
    /* The I/O manager only accepts attachment to an initialized target. */
    baseDevice->Flags &= ~DO_DEVICE_INITIALIZING;
    status = IoCreateDevice(driver, sizeof(CONTROL_EXTENSION), NULL,
                            WK_DEVICE_TYPE, 0, FALSE, &filterDevice);
    if (!NT_SUCCESS(status))
        goto deleteBase;
    /* This private device has no attached filters before publication. */
    lower = baseDevice;
    status = IoAttachDeviceByPointer(filterDevice, lower);
    if (!NT_SUCCESS(status))
        goto deleteFilter;
    __atomic_store_n(&attached, TRUE, __ATOMIC_RELEASE);
    ((CONTROL_EXTENSION *)filterDevice->DeviceExtension)->lower = lower;
    RtlInitUnicodeString(&link, alias);
    status = IoCreateSymbolicLink(&link, &devname);
    if (!NT_SUCCESS(status))
        goto detach;
    status = WkStartQueue(filterDevice);
    if (!NT_SUCCESS(status)) {
        IoDeleteSymbolicLink(&link);
        goto detach;
    }
    filterDevice->Flags &= ~DO_DEVICE_INITIALIZING;
    __atomic_store_n(&online, TRUE, __ATOMIC_RELEASE);
    return STATUS_SUCCESS;
detach:
    detachFilter();
deleteFilter:
    IoDeleteDevice(filterDevice);
deleteBase:
    IoDeleteDevice(baseDevice);
    return status;
}
