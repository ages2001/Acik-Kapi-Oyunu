#include <assert.h>
#include <stdio.h>
#include <ntddk.h>
#include "wdm_dma.h"

struct _ADAPTER_OBJECT { int unused; };
static struct _ADAPTER_OBJECT adapter;
static unsigned char storage;
static PHYSICAL_ADDRESS nextAddress;
static ULONG lastLength;
static BOOLEAN lastCache;
static int allocations, releases, failAllocation;

PVOID HalAllocateCommonBuffer(PADAPTER_OBJECT owner, ULONG length,
                              PHYSICAL_ADDRESS *logical, BOOLEAN cached)
{
    assert(owner == &adapter);
    ++allocations;
    lastLength = length;
    lastCache = cached;
    *logical = nextAddress;
    return failAllocation ? NULL : &storage;
}

VOID HalFreeCommonBuffer(PADAPTER_OBJECT owner, ULONG length, PHYSICAL_ADDRESS logical,
                         PVOID address, BOOLEAN cached)
{
    assert(owner == &adapter && length == lastLength && cached == lastCache);
    assert(logical.QuadPart == nextAddress.QuadPart && address == &storage);
    ++releases;
}

int main(void)
{
    WK_DMA_COMMON_BUFFER buffer = {0};
    assert(WkDmaAllocateCommonBuffer(NULL, 1, 0, 0, &buffer) == STATUS_INVALID_PARAMETER);
    assert(WkDmaAllocateCommonBuffer(&adapter, 0, 0, 0, &buffer) == STATUS_INVALID_PARAMETER);
    assert(WkDmaAllocateCommonBuffer(&adapter, 1, 0, 0, NULL) == STATUS_INVALID_PARAMETER);
    assert(WkDmaAllocateCommonBuffer(&adapter, 4097, 4095, 0, &buffer) == STATUS_CONFLICTING_ADDRESSES);
    assert(allocations == 0);

    nextAddress.LowPart = 0xfffff000u;
    assert(WkDmaAllocateCommonBuffer(&adapter, 4096, UINT32_MAX, 1, &buffer) == STATUS_SUCCESS);
    assert(buffer.LogicalAddress.QuadPart == nextAddress.QuadPart && buffer.VirtualAddress == &storage);
    assert(buffer.Adapter == &adapter && buffer.Length == 4096 && buffer.CacheEnabled == 1);
    assert(WkDmaAllocateCommonBuffer(&adapter, 1, UINT32_MAX, 0, &buffer) == STATUS_DEVICE_BUSY);
    WkDmaFreeCommonBuffer(&buffer);
    WkDmaFreeCommonBuffer(&buffer);
    WkDmaFreeCommonBuffer(NULL);
    assert(allocations == 1 && releases == 1 && !buffer.Adapter && !buffer.VirtualAddress);
    assert(!buffer.Length && !buffer.CacheEnabled && !buffer.LogicalAddress.QuadPart);

    assert(WkDmaAllocateCommonBuffer(&adapter, 4097, UINT32_MAX, 0, &buffer) == STATUS_CONFLICTING_ADDRESSES);
    assert(WkDmaAllocateCommonBuffer(&adapter, 1, 0xffffffu, 0, &buffer) == STATUS_CONFLICTING_ADDRESSES);
    nextAddress.QuadPart = (int64_t)1 << 32;
    assert(WkDmaAllocateCommonBuffer(&adapter, 1, UINT32_MAX, 0, &buffer) == STATUS_CONFLICTING_ADDRESSES);
    assert(allocations == 4 && releases == 4 && !buffer.VirtualAddress);

    nextAddress.QuadPart = 0;
    assert(WkDmaAllocateCommonBuffer(&adapter, 1, 0, 0, &buffer) == STATUS_SUCCESS);
    WkDmaFreeCommonBuffer(&buffer);
    failAllocation = 1;
    assert(WkDmaAllocateCommonBuffer(&adapter, 1, 0, 0, &buffer) == STATUS_INSUFFICIENT_RESOURCES);
    WkDmaFreeCommonBuffer(&buffer);
    assert(allocations == 6 && releases == 5 && !buffer.VirtualAddress);
    puts("NT DMA common buffers: inclusive device limits, 32-bit overflow, allocation failure and release passed.");
    return 0;
}
