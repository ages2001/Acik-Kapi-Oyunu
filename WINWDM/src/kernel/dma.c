#include <ntddk.h>
#include "wdm_dma.h"

NTSTATUS NTAPI WkDmaAllocateCommonBuffer(PADAPTER_OBJECT adapter, ULONG length,
    ULONG highestBusAddress, BOOLEAN cacheEnabled, WK_DMA_COMMON_BUFFER *buffer)
{
    PVOID address;
    PHYSICAL_ADDRESS logical;
    if (!adapter || !buffer || !length) return STATUS_INVALID_PARAMETER;
    if (buffer->VirtualAddress) return STATUS_DEVICE_BUSY;
    if (length - 1 > highestBusAddress) return STATUS_CONFLICTING_ADDRESSES;

    address = HalAllocateCommonBuffer(adapter, length, &logical, cacheEnabled);
    if (!address) return STATUS_INSUFFICIENT_RESOURCES;
    if (logical.HighPart || logical.LowPart > highestBusAddress ||
        length - 1 > highestBusAddress - logical.LowPart) {
        HalFreeCommonBuffer(adapter, length, logical, address, cacheEnabled);
        return STATUS_CONFLICTING_ADDRESSES;
    }
    buffer->Adapter = adapter;
    buffer->VirtualAddress = address;
    buffer->LogicalAddress = logical;
    buffer->Length = length;
    buffer->CacheEnabled = cacheEnabled;
    return STATUS_SUCCESS;
}

VOID NTAPI WkDmaFreeCommonBuffer(WK_DMA_COMMON_BUFFER *buffer)
{
    if (buffer && buffer->VirtualAddress) {
        HalFreeCommonBuffer(buffer->Adapter, buffer->Length, buffer->LogicalAddress,
                            buffer->VirtualAddress, buffer->CacheEnabled);
        buffer->VirtualAddress = NULL;
        buffer->LogicalAddress.QuadPart = 0;
        buffer->Length = 0;
        buffer->Adapter = NULL;
        buffer->CacheEnabled = FALSE;
    }
}
