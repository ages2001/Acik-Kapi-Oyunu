#ifndef WINWDM_DMA_H
#define WINWDM_DMA_H

/* Native x86 NT 3.x/4.x. Include ntddk.h first. One caller owns each
 * zero-initialized buffer and quiesces its device before releasing it. */
typedef struct _WK_DMA_COMMON_BUFFER {
    PADAPTER_OBJECT Adapter;
    PVOID VirtualAddress;
    PHYSICAL_ADDRESS LogicalAddress;
    ULONG Length;
    BOOLEAN CacheEnabled;
} WK_DMA_COMMON_BUFFER;

#ifdef __cplusplus
extern "C" {
#endif
NTSTATUS NTAPI WkDmaAllocateCommonBuffer(PADAPTER_OBJECT adapter, ULONG length,
    ULONG highestBusAddress, BOOLEAN cacheEnabled, WK_DMA_COMMON_BUFFER *buffer);
VOID NTAPI WkDmaFreeCommonBuffer(WK_DMA_COMMON_BUFFER *buffer);
#ifdef __cplusplus
}
#endif
#endif
