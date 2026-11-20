#ifndef WINWDM_QA_DMA_NTDDK_H
#define WINWDM_QA_DMA_NTDDK_H
#include <stddef.h>
#include <stdint.h>
#define NTAPI
typedef void VOID;
typedef void *PVOID;
typedef struct _ADAPTER_OBJECT *PADAPTER_OBJECT;
typedef unsigned char BOOLEAN;
typedef uint32_t ULONG;
typedef int32_t NTSTATUS;
typedef union {
    struct { ULONG LowPart; int32_t HighPart; };
    int64_t QuadPart;
} PHYSICAL_ADDRESS;
#define FALSE 0
#define STATUS_SUCCESS 0
#define STATUS_DEVICE_BUSY ((NTSTATUS)0x80000011u)
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xc000000du)
#define STATUS_CONFLICTING_ADDRESSES ((NTSTATUS)0xc0000018u)
#define STATUS_INSUFFICIENT_RESOURCES ((NTSTATUS)0xc000009au)
PVOID HalAllocateCommonBuffer(PADAPTER_OBJECT, ULONG, PHYSICAL_ADDRESS *, BOOLEAN);
VOID HalFreeCommonBuffer(PADAPTER_OBJECT, ULONG, PHYSICAL_ADDRESS, PVOID, BOOLEAN);
#endif
