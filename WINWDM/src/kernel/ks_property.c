/* Static pin and topology queries for the x86 KS property-handler ABI.
 * The caller supplies the captured property and system-mapped data buffer. */
#define _KSDDK_
#include "nt_legacy.h"
#include <ks.h>

static NTSTATUS reserve(PIRP irp, PVOID output, ULONG bytes)
{
    ULONG capacity = IoGetCurrentIrpStackLocation(irp)->Parameters.DeviceIoControl.OutputBufferLength;
    if (!capacity) {
        irp->IoStatus.Information = bytes;
        return STATUS_BUFFER_OVERFLOW;
    }
    if (capacity < bytes) return STATUS_BUFFER_TOO_SMALL;
    if (!output) return STATUS_INVALID_PARAMETER;
    irp->IoStatus.Information = bytes;
    return STATUS_SUCCESS;
}

static NTSTATUS singleValue(PIRP irp, PVOID output, const VOID *value, ULONG bytes)
{
    NTSTATUS status;
    if (!value) return STATUS_NOT_FOUND;
    status = reserve(irp, output, bytes);
    if (NT_SUCCESS(status)) RtlCopyMemory(output, value, bytes);
    return status;
}

static NTSTATUS itemTable(PIRP irp, PVOID output, const VOID *items, ULONG count, ULONG stride)
{
    KSMULTIPLE_ITEM *list = output;
    ULONG bytes;
    NTSTATUS status;
    if (count && !items) return STATUS_INVALID_PARAMETER;
    if (count > (MAXULONG - sizeof(*list)) / stride) return STATUS_INTEGER_OVERFLOW;
    bytes = sizeof(*list) + count * stride;
    status = reserve(irp, output, bytes);
    if (!NT_SUCCESS(status)) return status;
    list->Size = bytes;
    list->Count = count;
    if (count) RtlCopyMemory(list + 1, items, bytes - sizeof(*list));
    return STATUS_SUCCESS;
}

/* KS packs data ranges on eight-byte boundaries. Extended audio formats
 * retain their entire FormatSize, including the driver-specific tail. */
static NTSTATUS dataRanges(PIRP irp, PVOID output, const PKSDATARANGE *ranges, ULONG count)
{
    KSMULTIPLE_ITEM *list = output;
    ULONG bytes = sizeof(*list), i;
    UCHAR *cursor;
    NTSTATUS status;
    if (count && !ranges) return STATUS_INVALID_PARAMETER;
    for (i = 0; i < count; ++i) {
        ULONG size;
        if (!ranges[i] || ranges[i]->FormatSize < sizeof(KSDATARANGE)) return STATUS_INVALID_PARAMETER;
        if (ranges[i]->Flags & KSDATARANGE_ATTRIBUTES) return STATUS_NOT_SUPPORTED;
        size = ranges[i]->FormatSize;
        if (size > MAXULONG - 7 || ((size + 7) & ~7UL) > MAXULONG - bytes)
            return STATUS_INTEGER_OVERFLOW;
        bytes += (size + 7) & ~7UL;
    }
    status = reserve(irp, output, bytes);
    if (!NT_SUCCESS(status)) return status;
    list->Size = bytes;
    list->Count = count;
    cursor = (UCHAR *)(list + 1);
    for (i = 0; i < count; ++i) {
        ULONG size = ranges[i]->FormatSize, aligned = (size + 7) & ~7UL;
        RtlCopyMemory(cursor, ranges[i], size);
        RtlZeroMemory(cursor + size, aligned - size);
        cursor += aligned;
    }
    return STATUS_SUCCESS;
}

NTSTATUS NTAPI KsPinPropertyHandler(PIRP irp, PKSPROPERTY property, PVOID output,
                                    ULONG count, const KSPIN_DESCRIPTOR *pins)
{
    const KSPIN_DESCRIPTOR *pin;
    ULONG input;
    if (!irp) return STATUS_INVALID_PARAMETER;
    irp->IoStatus.Information = 0;
    if (!property) return STATUS_INVALID_PARAMETER;
    input = IoGetCurrentIrpStackLocation(irp)->Parameters.DeviceIoControl.InputBufferLength;
    if (input < sizeof(*property)) return STATUS_INVALID_BUFFER_SIZE;
    if (!(property->Flags & KSPROPERTY_TYPE_GET)) return STATUS_NOT_SUPPORTED;
    if (property->Id == KSPROPERTY_PIN_CTYPES)
        return singleValue(irp, output, &count, sizeof(count));
    if (input < sizeof(KSP_PIN)) return STATUS_INVALID_BUFFER_SIZE;
    if (!pins || ((PKSP_PIN)property)->PinId >= count) return STATUS_INVALID_PARAMETER;
    pin = pins + ((PKSP_PIN)property)->PinId;
    switch (property->Id) {
    case KSPROPERTY_PIN_DATAFLOW:
        return singleValue(irp, output, &pin->DataFlow, sizeof(pin->DataFlow));
    case KSPROPERTY_PIN_COMMUNICATION:
        return singleValue(irp, output, &pin->Communication, sizeof(pin->Communication));
    case KSPROPERTY_PIN_CATEGORY:
        return singleValue(irp, output, pin->Category, sizeof(GUID));
    case KSPROPERTY_PIN_INTERFACES:
        return itemTable(irp, output, pin->Interfaces, pin->InterfacesCount, sizeof(KSPIN_INTERFACE));
    case KSPROPERTY_PIN_MEDIUMS:
        return itemTable(irp, output, pin->Mediums, pin->MediumsCount, sizeof(KSPIN_MEDIUM));
    case KSPROPERTY_PIN_DATARANGES:
        return dataRanges(irp, output, pin->DataRanges, pin->DataRangesCount);
    default:
        return STATUS_NOT_SUPPORTED;
    }
}

NTSTATUS NTAPI KsTopologyPropertyHandler(PIRP irp, PKSPROPERTY property, PVOID output,
                                         const KSTOPOLOGY *topology)
{
    if (!irp) return STATUS_INVALID_PARAMETER;
    irp->IoStatus.Information = 0;
    if (!property || !topology) return STATUS_INVALID_PARAMETER;
    if (IoGetCurrentIrpStackLocation(irp)->Parameters.DeviceIoControl.InputBufferLength < sizeof(*property))
        return STATUS_INVALID_BUFFER_SIZE;
    if (!(property->Flags & KSPROPERTY_TYPE_GET)) return STATUS_NOT_SUPPORTED;
    switch (property->Id) {
    case KSPROPERTY_TOPOLOGY_CATEGORIES:
        return itemTable(irp, output, topology->Categories, topology->CategoriesCount, sizeof(GUID));
    case KSPROPERTY_TOPOLOGY_NODES:
        return itemTable(irp, output, topology->TopologyNodes, topology->TopologyNodesCount, sizeof(GUID));
    case KSPROPERTY_TOPOLOGY_CONNECTIONS:
        return itemTable(irp, output, topology->TopologyConnections, topology->TopologyConnectionsCount,
                         sizeof(KSTOPOLOGY_CONNECTION));
    default:
        return STATUS_NOT_SUPPORTED;
    }
}

static VOID NTAPI unload(PDRIVER_OBJECT driver)
{
    (void)driver;
}

NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registryPath)
{
    (void)registryPath;
    driver->DriverUnload = unload;
    return STATUS_SUCCESS;
}
