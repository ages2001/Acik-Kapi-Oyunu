/* Source-rebuilt function driver for QEMU's pci-testdev port-I/O function.
 * This module only accesses the translated resource delivered in START_DEVICE. */
#include "pci_device.h"
#include "protocol.h"

static ULONG read32(PUCHAR port)
{
    ULONG value=0;
    unsigned i;
    for (i=0;i<4;++i)
        value |= (ULONG)READ_PORT_UCHAR(port+i) << (i*8);
    return value;
}

static NTSTATUS start(WK_PCI_EXTENSION *state, PCM_RESOURCE_LIST resources)
{
    static const char id[]="portio-no-eventfd";
    PCM_PARTIAL_RESOURCE_DESCRIPTOR item;
    PUCHAR port;
    ULONG i;
    if (state->started || !resources || resources->Count!=1 ||
        resources->List[0].PartialResourceList.Count!=1)
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    item=resources->List[0].PartialResourceList.PartialDescriptors;
    if (item->Type!=CmResourceTypePort || !(item->Flags & CM_RESOURCE_PORT_IO) ||
        item->u.Port.Start.HighPart || item->u.Port.Length<256 ||
        item->u.Port.Start.LowPart==0 || item->u.Port.Start.LowPart>0xff00)
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    port=(PUCHAR)(ULONG_PTR)item->u.Port.Start.LowPart;
    WRITE_PORT_UCHAR(port,0);
    if (READ_PORT_UCHAR(port+1)!=1 || read32(port+12)!=0)
        return STATUS_DEVICE_DATA_ERROR;
    for (i=0;i<sizeof(id);++i)
        if (READ_PORT_UCHAR(port+16+i)!=(UCHAR)id[i]) return STATUS_DEVICE_DATA_ERROR;
    state->port=port;
    state->length=item->u.Port.Length;
    state->started=TRUE;
    state->power=PowerDeviceD0;
    return STATUS_SUCCESS;
}

static NTSTATUS pulse(WK_PCI_EXTENSION *state, PULONG count)
{
    ULONG offset, before;
    if (!state->started || state->power!=PowerDeviceD0) return STATUS_DEVICE_NOT_READY;
    offset=read32(state->port+4);
    if (offset<32 || offset>=state->length) return STATUS_DEVICE_DATA_ERROR;
    before=read32(state->port+12);
    WRITE_PORT_UCHAR(state->port+offset,READ_PORT_UCHAR(state->port+8));
    *count=read32(state->port+12);
    return *count==before+1 ? STATUS_SUCCESS : STATUS_DEVICE_DATA_ERROR;
}

NTSTATUS NTAPI WkPciDispatch(PDEVICE_OBJECT device, PIRP irp)
{
    PIO_STACK_LOCATION stack=IoGetCurrentIrpStackLocation(irp);
    WK_PCI_EXTENSION *state=device->DeviceExtension;
    NTSTATUS status=IoAcquireRemoveLock(&state->removeLock,irp);
    ULONG bytes=0;
    if (!NT_SUCCESS(status)) goto complete;
    status=STATUS_INVALID_DEVICE_REQUEST;
    if (stack->MajorFunction==IRP_MJ_PNP) {
        switch (stack->MinorFunction) {
        case IRP_MN_START_DEVICE:
            status=start(state,stack->Parameters.StartDevice.AllocatedResourcesTranslated);
            break;
        case IRP_MN_STOP_DEVICE:
        case IRP_MN_REMOVE_DEVICE:
            if (stack->MinorFunction==IRP_MN_REMOVE_DEVICE)
                IoReleaseRemoveLockAndWait(&state->removeLock,irp);
            state->started=FALSE;
            state->port=NULL;
            state->length=0;
            state->power=PowerDeviceD3;
            status=STATUS_SUCCESS;
            break;
        }
    } else if (stack->MajorFunction==IRP_MJ_POWER &&
               stack->MinorFunction==IRP_MN_SET_POWER &&
               stack->Parameters.Power.Type==DevicePowerState && state->started) {
        DEVICE_POWER_STATE power=stack->Parameters.Power.State.DeviceState;
        if (power==PowerDeviceD0 || power==PowerDeviceD3) {
            state->power=power;
            status=STATUS_SUCCESS;
        } else status=STATUS_NOT_SUPPORTED;
    } else if (stack->MajorFunction==IRP_MJ_DEVICE_CONTROL &&
               stack->Parameters.DeviceIoControl.IoControlCode==WK_IOCTL_PCI &&
               stack->Parameters.DeviceIoControl.OutputBufferLength>=sizeof(ULONG) &&
               irp->AssociatedIrp.SystemBuffer) {
        status=pulse(state,irp->AssociatedIrp.SystemBuffer);
        if (NT_SUCCESS(status)) bytes=sizeof(ULONG);
    }
    if (stack->MajorFunction!=IRP_MJ_PNP || stack->MinorFunction!=IRP_MN_REMOVE_DEVICE)
        IoReleaseRemoveLock(&state->removeLock,irp);
complete:
    irp->IoStatus.Status=status;
    irp->IoStatus.Information=bytes;
    IoCompleteRequest(irp,IO_NO_INCREMENT);
    return status;
}
