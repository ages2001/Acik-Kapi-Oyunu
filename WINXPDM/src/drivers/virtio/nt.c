/* Buffered NT draw interface. PCI configuration goes through the HAL on
 * NT 3.5 and newer. The uniprocessor NT 3.1 HAL predates PCI writes.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <ntddk.h>
#include "gpu.h"

#ifdef WXP_NT31
#undef IoCompleteRequest
__declspec(dllimport) void NTAPI IoCompleteRequest(PIRP, CCHAR);
#endif

typedef struct {
    KEVENT lock;
    WxpGpu gpu;
    WxpDraw input;
    unsigned open_count;
} WxpDevice;

static ULONG ticks_per_ms;
static UNICODE_STRING link_name;

#ifdef WXP_NT31
#include "../nt/pci31.h"
#endif

static uint32_t read_cfg(unsigned b, unsigned s, unsigned o, unsigned n)
{
#ifdef WXP_NT31
    return wxp_pci31_cycle(b, s, o, n, 0, 0);
#else
    ULONG data = 0, slot = (s >> 3) | ((s & 7) << 5);
    return HalGetBusDataByOffset(PCIConfiguration, b, slot, &data, o, n) == n ? data : 0xffffffffUL;
#endif
}
static void write_cfg(unsigned b, unsigned s, unsigned o, unsigned n, uint32_t v)
{
#ifdef WXP_NT31
    wxp_pci31_cycle(b, s, o, n, v, 1);
#else
    ULONG slot = (s >> 3) | ((s & 7) << 5);
    HalSetBusDataByOffset(PCIConfiguration, b, slot, &v, o, n);
#endif
}
static uint32_t now(void)
{
    return (uint32_t)((ULONGLONG)KeQueryPerformanceCounter(NULL).QuadPart / ticks_per_ms);
}
static void poll_gpu(void)
{
    LARGE_INTEGER duration;
    duration.QuadPart = -10000;
    KeDelayExecutionThread(KernelMode, FALSE, &duration);
}

static NTSTATUS NTAPI dispatch(PDEVICE_OBJECT dev, PIRP irp)
{
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
    WxpDevice *d = dev->DeviceExtension;
    ULONG_PTR bytes = 0;
    NTSTATUS status = STATUS_SUCCESS;
    if (stack->MajorFunction == IRP_MJ_CREATE || stack->MajorFunction == IRP_MJ_CLOSE) {
        KeWaitForSingleObject(&d->lock, Executive, KernelMode, FALSE, NULL);
        if (stack->MajorFunction == IRP_MJ_CREATE) ++d->open_count;
        else if (d->open_count && !--d->open_count && !wxp_gpu_close(&d->gpu) && d->gpu.dma) {
            MmFreeContiguousMemory((PVOID)d->gpu.dma);
            d->gpu.dma = NULL;
        }
        KeSetEvent(&d->lock, IO_NO_INCREMENT, FALSE);
    } else if (stack->MajorFunction == IRP_MJ_DEVICE_CONTROL) {
        WxpFrame *frame = irp->AssociatedIrp.SystemBuffer;
        if (stack->Parameters.DeviceIoControl.IoControlCode != WXP_GPU_IOCTL) status = STATUS_INVALID_DEVICE_REQUEST;
        else if (!frame || stack->Parameters.DeviceIoControl.InputBufferLength != sizeof(WxpDraw) ||
                 stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(*frame)) status = STATUS_BUFFER_TOO_SMALL;
        else {
            PHYSICAL_ADDRESS highest;
            unsigned i;
            KeWaitForSingleObject(&d->lock, Executive, KernelMode, FALSE, NULL);
            highest.QuadPart = 0xffffffffUL;
            if (!d->gpu.dma) d->gpu.dma = MmAllocateContiguousMemory(WXP_DMA_SIZE, highest);
            if (!d->gpu.dma) status = STATUS_INSUFFICIENT_RESOURCES;
            else {
                for (i = 0; i < WXP_DMA_SIZE / 4096; ++i)
                    d->gpu.page[i] = MmGetPhysicalAddress((PVOID)(d->gpu.dma + i * 4096)).LowPart;
                for (i = 0; i < sizeof(d->input); ++i)
                    ((unsigned char *)&d->input)[i] = ((const unsigned char *)frame)[i];
                wxp_gpu_draw(&d->gpu, &d->input, frame);
                if (!d->gpu.owned) {
                    MmFreeContiguousMemory((PVOID)d->gpu.dma);
                    d->gpu.dma = NULL;
                }
                bytes = sizeof(*frame);
            }
            KeSetEvent(&d->lock, IO_NO_INCREMENT, FALSE);
        }
    }
    irp->IoStatus.Status = status; irp->IoStatus.Information = bytes;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}

NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry)
{
    UNICODE_STRING device_name;
    LARGE_INTEGER frequency;
    PDEVICE_OBJECT dev;
    WxpDevice *d;
    NTSTATUS status;
    UNREFERENCED_PARAMETER(registry);
#ifdef WXP_NT31
    if ((unsigned char)KeNumberProcessors != 1) return STATUS_NOT_SUPPORTED;
#endif
    RtlInitUnicodeString(&device_name, L"\\Device\\WxpGpu");
    RtlInitUnicodeString(&link_name, L"\\DosDevices\\WxpGpu");
    KeQueryPerformanceCounter(&frequency);
    ticks_per_ms = (ULONG)((ULONGLONG)frequency.QuadPart / 1000);
    if (!ticks_per_ms) return STATUS_NOT_SUPPORTED;
    status = IoCreateDevice(driver, sizeof(WxpDevice), &device_name, FILE_DEVICE_UNKNOWN, 0, FALSE, &dev);
    if (!NT_SUCCESS(status)) return status;
    d = dev->DeviceExtension;
    KeInitializeEvent(&d->lock, SynchronizationEvent, TRUE);
    d->gpu.read = read_cfg; d->gpu.write = write_cfg; d->gpu.time_ms = now; d->gpu.poll = poll_gpu;
    driver->MajorFunction[IRP_MJ_CREATE] = driver->MajorFunction[IRP_MJ_CLOSE] = dispatch;
    driver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = dispatch;
    status = IoCreateSymbolicLink(&link_name, &device_name);
    if (!NT_SUCCESS(status)) { IoDeleteDevice(dev); return status; }
    dev->Flags &= ~DO_DEVICE_INITIALIZING;
    return STATUS_SUCCESS;
}
