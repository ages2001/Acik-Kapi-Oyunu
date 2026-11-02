/*
 * ndiskit.h - minimal, self-contained NDIS miniport kernel definitions
 *             for building RLTKLANT with a free (GCC/binutils) toolchain.
 *
 * Declares only the NDIS / HAL / kernel types, structures and imports that
 * RLTKLANT uses.  Every imported name was checked against the export table
 * of ndis.sys / hal.dll / ntoskrnl.exe on each target OS.
 *
 * Build selectors (define exactly one):
 *   RTL_NDIS3   - NDIS 3.0 miniport     (NT 3.5 / 3.51 / 4.0)
 *   RTL_NDIS5   - NDIS 5.0 miniport     (2000 / XP / XP x64 / 2003)
 *
 * Copyright (C) 2026 ages2001.
 */
#ifndef NDISKIT_H
#define NDISKIT_H

#if !defined(RTL_NDIS3) && !defined(RTL_NDIS5)
#error "Define RTL_NDIS3 or RTL_NDIS5"
#endif

#if defined(_WIN64)
#define STDC
#else
#define STDC   __stdcall
#endif
#define NDISAPI
#define IN
#define OUT
#define OPTIONAL

/* ---- base types (LLP64: ULONG is 32-bit on x64 too) --------------- */
typedef unsigned char   UCHAR,  *PUCHAR;
typedef unsigned short  USHORT, *PUSHORT;
typedef unsigned short  WCHAR,  *PWCHAR;
typedef unsigned long   ULONG,  *PULONG;
typedef unsigned int    UINT,   *PUINT;
typedef int             INT;
typedef long            LONG;
typedef short           SHORT;
typedef char            CHAR,   *PCHAR;
typedef void            VOID,   *PVOID;
typedef const void     *PCVOID;
typedef UCHAR           BOOLEAN, *PBOOLEAN;     /* NDIS BOOLEAN is 1 byte */
typedef PBOOLEAN        PBOOLEAN_PH;
typedef unsigned long long ULONGLONG;
#if defined(_WIN64)
typedef unsigned long long ULONG_PTR, *PULONG_PTR;
typedef long long          LONG_PTR;
#else
typedef unsigned long      ULONG_PTR, *PULONG_PTR;
typedef long               LONG_PTR;
#endif
typedef LONG NTSTATUS;

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif
#ifndef NULL
#define NULL ((void*)0)
#endif

typedef union _LARGE_INTEGER {
    struct { ULONG LowPart; LONG HighPart; } u;
    ULONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;
typedef LARGE_INTEGER NDIS_PHYSICAL_ADDRESS, *PNDIS_PHYSICAL_ADDRESS;

typedef struct _NDIS_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWCHAR Buffer;
} NDIS_STRING, *PNDIS_STRING, UNICODE_STRING, *PUNICODE_STRING;

typedef int          NDIS_STATUS, *PNDIS_STATUS;
typedef PVOID        NDIS_HANDLE, *PNDIS_HANDLE;
typedef ULONG        NDIS_OID,    *PNDIS_OID;

#define NDIS_STATUS_SUCCESS            ((NDIS_STATUS)0x00000000L)
#define NDIS_STATUS_PENDING            ((NDIS_STATUS)0x00000103L)
#define NDIS_STATUS_FAILURE            ((NDIS_STATUS)0xC0000001L)
#define NDIS_STATUS_RESOURCES          ((NDIS_STATUS)0xC000009AL)
#define NDIS_STATUS_NOT_SUPPORTED      ((NDIS_STATUS)0xC00000BBL)
#define NDIS_STATUS_INVALID_LENGTH     ((NDIS_STATUS)0xC0010014L)
#define NDIS_STATUS_BUFFER_TOO_SHORT   ((NDIS_STATUS)0xC0010016L)
#define NDIS_STATUS_INVALID_DATA       ((NDIS_STATUS)0xC0010015L)
#define NDIS_STATUS_ADAPTER_NOT_FOUND  ((NDIS_STATUS)0xC0010013L)
#define NDIS_STATUS_BAD_VERSION        ((NDIS_STATUS)0xC0010004L)
#define NDIS_STATUS_UNSUPPORTED_MEDIA  ((NDIS_STATUS)0xC001001EL)
#define NDIS_STATUS_MEDIA_CONNECT      ((NDIS_STATUS)0x4001000BL)
#define NDIS_STATUS_MEDIA_DISCONNECT   ((NDIS_STATUS)0x4001000CL)

typedef enum _NDIS_MEDIUM { NdisMedium802_3 = 0 } NDIS_MEDIUM, *PNDIS_MEDIUM;
typedef enum _NDIS_INTERRUPT_MODE { NdisInterruptLevelSensitive = 0, NdisInterruptLatched } NDIS_INTERRUPT_MODE;
typedef enum _NDIS_INTERFACE_TYPE { NdisInterfaceInternal = 0, NdisInterfaceIsa = 1, NdisInterfacePci = 5 } NDIS_INTERFACE_TYPE;
typedef enum _NDIS_PARAMETER_TYPE {
    NdisParameterInteger = 0, NdisParameterHexInteger, NdisParameterString,
    NdisParameterMultiString, NdisParameterBinary
} NDIS_PARAMETER_TYPE;

typedef struct _NDIS_CONFIGURATION_PARAMETER {
    NDIS_PARAMETER_TYPE ParameterType;
    union {
        ULONG       IntegerData;
        NDIS_STRING StringData;
    } ParameterData;
} NDIS_CONFIGURATION_PARAMETER, *PNDIS_CONFIGURATION_PARAMETER;

/* ---- opaque NDIS objects: generously sized, NDIS owns the layout -- */
typedef struct _NDIS_SPIN_LOCK { ULONG_PTR SpinLock; UCHAR OldIrql; } NDIS_SPIN_LOCK, *PNDIS_SPIN_LOCK;
typedef struct _NDIS_MINIPORT_TIMER { UCHAR Opaque[sizeof(PVOID) * 24]; } NDIS_MINIPORT_TIMER, *PNDIS_MINIPORT_TIMER;
typedef struct _NDIS_MINIPORT_INTERRUPT { UCHAR Opaque[sizeof(PVOID) * 24]; } NDIS_MINIPORT_INTERRUPT, *PNDIS_MINIPORT_INTERRUPT;
typedef VOID (STDC *PNDIS_TIMER_FUNCTION)(PVOID, PVOID, PVOID, PVOID);

/* ---- packet / buffer (public layout) ------------------------------ */
typedef struct _NDIS_BUFFER NDIS_BUFFER, *PNDIS_BUFFER;   /* == MDL */
typedef struct _NDIS_PACKET_PRIVATE {
    UINT          PhysicalCount;
    UINT          TotalLength;
    PNDIS_BUFFER  Head;
    PNDIS_BUFFER  Tail;
    PVOID         Pool;
    UINT          Count;
    ULONG         Flags;
    BOOLEAN       ValidCounts;
    UCHAR         NdisPacketFlags;
    USHORT        NdisPacketOobOffset;
} NDIS_PACKET_PRIVATE;
typedef struct _NDIS_PACKET {
    NDIS_PACKET_PRIVATE Private;
    UCHAR MiniportReserved[2 * sizeof(PVOID)];
} NDIS_PACKET, *PNDIS_PACKET;

/* ---- OIDs ---------------------------------------------------------- */
#define OID_GEN_SUPPORTED_LIST              0x00010101
#define OID_GEN_HARDWARE_STATUS             0x00010102
#define OID_GEN_MEDIA_SUPPORTED             0x00010103
#define OID_GEN_MEDIA_IN_USE                0x00010104
#define OID_GEN_MAXIMUM_LOOKAHEAD           0x00010105
#define OID_GEN_MAXIMUM_FRAME_SIZE          0x00010106
#define OID_GEN_LINK_SPEED                  0x00010107
#define OID_GEN_TRANSMIT_BUFFER_SPACE       0x00010108
#define OID_GEN_RECEIVE_BUFFER_SPACE        0x00010109
#define OID_GEN_TRANSMIT_BLOCK_SIZE         0x0001010A
#define OID_GEN_RECEIVE_BLOCK_SIZE          0x0001010B
#define OID_GEN_VENDOR_ID                   0x0001010C
#define OID_GEN_VENDOR_DESCRIPTION          0x0001010D
#define OID_GEN_CURRENT_PACKET_FILTER       0x0001010E
#define OID_GEN_CURRENT_LOOKAHEAD           0x0001010F
#define OID_GEN_DRIVER_VERSION              0x00010110
#define OID_GEN_MAXIMUM_TOTAL_SIZE          0x00010111
#define OID_GEN_PROTOCOL_OPTIONS            0x00010112
#define OID_GEN_MAC_OPTIONS                 0x00010113
#define OID_GEN_MEDIA_CONNECT_STATUS        0x00010114
#define OID_GEN_MAXIMUM_SEND_PACKETS        0x00010115
#define OID_GEN_VENDOR_DRIVER_VERSION       0x00010116
#define OID_GEN_XMIT_OK                     0x00020101
#define OID_GEN_RCV_OK                      0x00020102
#define OID_GEN_XMIT_ERROR                  0x00020103
#define OID_GEN_RCV_ERROR                   0x00020104
#define OID_GEN_RCV_NO_BUFFER               0x00020105
#define OID_802_3_PERMANENT_ADDRESS         0x01010101
#define OID_802_3_CURRENT_ADDRESS           0x01010102
#define OID_802_3_MULTICAST_LIST            0x01010103
#define OID_802_3_MAXIMUM_LIST_SIZE         0x01010104
#define OID_802_3_RCV_ERROR_ALIGNMENT       0x01020101
#define OID_802_3_XMIT_ONE_COLLISION        0x01020102
#define OID_802_3_XMIT_MORE_COLLISIONS      0x01020103

#define NDIS_PACKET_TYPE_DIRECTED           0x0001
#define NDIS_PACKET_TYPE_MULTICAST          0x0002
#define NDIS_PACKET_TYPE_ALL_MULTICAST      0x0004
#define NDIS_PACKET_TYPE_BROADCAST          0x0008
#define NDIS_PACKET_TYPE_PROMISCUOUS        0x0020

#define NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA 0x00000001
#define NDIS_MAC_OPTION_RECEIVE_SERIALIZED  0x00000002
#define NDIS_MAC_OPTION_TRANSFERS_NOT_PEND  0x00000004
#define NDIS_MAC_OPTION_NO_LOOPBACK         0x00000008

typedef enum _NDIS_HARDWARE_STATUS {
    NdisHardwareStatusReady = 0, NdisHardwareStatusInitializing,
    NdisHardwareStatusReset, NdisHardwareStatusClosing, NdisHardwareStatusNotReady
} NDIS_HARDWARE_STATUS;

#define NDIS_ATTRIBUTE_BUS_MASTER           0x00000008

/* ---- miniport characteristics (NDIS 3.0 / 4.0 / 5.0 layouts) ------ */
typedef BOOLEAN     (STDC *W_CHECK_FOR_HANG)(NDIS_HANDLE);
typedef VOID        (STDC *W_DISABLE_INTERRUPT)(NDIS_HANDLE);
typedef VOID        (STDC *W_ENABLE_INTERRUPT)(NDIS_HANDLE);
typedef VOID        (STDC *W_HALT)(NDIS_HANDLE);
typedef VOID        (STDC *W_HANDLE_INTERRUPT)(NDIS_HANDLE);
typedef NDIS_STATUS (STDC *W_INITIALIZE)(PNDIS_STATUS, PUINT, PNDIS_MEDIUM, UINT, NDIS_HANDLE, NDIS_HANDLE);
typedef VOID        (STDC *W_ISR)(PBOOLEAN, PBOOLEAN, NDIS_HANDLE);
typedef NDIS_STATUS (STDC *W_QUERY_INFORMATION)(NDIS_HANDLE, NDIS_OID, PVOID, ULONG, PULONG, PULONG);
typedef NDIS_STATUS (STDC *W_RECONFIGURE)(PNDIS_STATUS, NDIS_HANDLE, NDIS_HANDLE);
typedef NDIS_STATUS (STDC *W_RESET)(PBOOLEAN, NDIS_HANDLE);
typedef NDIS_STATUS (STDC *W_SEND)(NDIS_HANDLE, PNDIS_PACKET, UINT);
typedef NDIS_STATUS (STDC *W_SET_INFORMATION)(NDIS_HANDLE, NDIS_OID, PVOID, ULONG, PULONG, PULONG);
typedef NDIS_STATUS (STDC *W_TRANSFER_DATA)(PNDIS_PACKET, PUINT, NDIS_HANDLE, NDIS_HANDLE, UINT, UINT);

typedef struct _NDIS_MINIPORT_CHARACTERISTICS {
    /* NDIS 3.0 */
    UCHAR  MajorNdisVersion;
    UCHAR  MinorNdisVersion;
    UINT   Reserved;
    W_CHECK_FOR_HANG     CheckForHangHandler;
    W_DISABLE_INTERRUPT  DisableInterruptHandler;
    W_ENABLE_INTERRUPT   EnableInterruptHandler;
    W_HALT               HaltHandler;
    W_HANDLE_INTERRUPT   HandleInterruptHandler;
    W_INITIALIZE         InitializeHandler;
    W_ISR                ISRHandler;
    W_QUERY_INFORMATION  QueryInformationHandler;
    W_RECONFIGURE        ReconfigureHandler;
    W_RESET              ResetHandler;
    W_SEND               SendHandler;
    W_SET_INFORMATION    SetInformationHandler;
    W_TRANSFER_DATA      TransferDataHandler;
#if defined(RTL_NDIS5)
    /* NDIS 4.0 */
    PVOID ReturnPacketHandler;
    PVOID SendPacketsHandler;
    PVOID AllocateCompleteHandler;
    /* NDIS 5.0 (CoNDIS, unused for connectionless) */
    PVOID CoCreateVcHandler;
    PVOID CoDeleteVcHandler;
    PVOID CoActivateVcHandler;
    PVOID CoDeactivateVcHandler;
    PVOID CoSendPacketsHandler;
    PVOID CoRequestHandler;
    /* NDIS 5.1: the registration length includes these fields even when
       optional handlers are NULL. Keep the 5.0 prefix length separately. */
    PVOID CancelSendPacketsHandler;
    PVOID PnPEventNotifyHandler;
    VOID (STDC *AdapterShutdownHandler)(PVOID);
    PVOID Reserved1;
    PVOID Reserved2;
    PVOID Reserved3;
    PVOID Reserved4;
#endif
} NDIS_MINIPORT_CHARACTERISTICS, *PNDIS_MINIPORT_CHARACTERISTICS;

#if defined(RTL_NDIS5)
#define RTL_NDIS50_CHARACTERISTICS_SIZE \
    ((UINT)__builtin_offsetof(NDIS_MINIPORT_CHARACTERISTICS, CancelSendPacketsHandler))
#endif

/* ================================================================== */
/*  NDIS.SYS imports                                                  */
/* ================================================================== */
VOID        STDC NdisInitializeWrapper(PNDIS_HANDLE, PVOID, PVOID, PVOID);
VOID        STDC NdisTerminateWrapper(NDIS_HANDLE, PVOID);
NDIS_STATUS STDC NdisMRegisterMiniport(NDIS_HANDLE, PNDIS_MINIPORT_CHARACTERISTICS, UINT);
VOID        STDC NdisMSetAttributes(NDIS_HANDLE, NDIS_HANDLE, BOOLEAN, NDIS_INTERFACE_TYPE);
VOID        STDC NdisMSetAttributesEx(NDIS_HANDLE, NDIS_HANDLE, UINT, ULONG, NDIS_INTERFACE_TYPE);

VOID        STDC NdisMQueryAdapterResources(PNDIS_STATUS, NDIS_HANDLE, PVOID, PUINT);
NDIS_STATUS STDC NdisMPciAssignResources(NDIS_HANDLE, ULONG, PVOID *);
ULONG       STDC NdisReadPciSlotInformation(NDIS_HANDLE, ULONG, ULONG, PVOID, ULONG);
NDIS_STATUS STDC NdisMRegisterIoPortRange(PVOID *, NDIS_HANDLE, UINT, UINT);
VOID        STDC NdisMDeregisterIoPortRange(NDIS_HANDLE, UINT, UINT, PVOID);
NDIS_STATUS STDC NdisMMapIoSpace(PVOID *, NDIS_HANDLE, NDIS_PHYSICAL_ADDRESS, UINT);
VOID        STDC NdisMUnmapIoSpace(NDIS_HANDLE, PVOID, UINT);
NDIS_STATUS STDC NdisMRegisterInterrupt(PNDIS_MINIPORT_INTERRUPT, NDIS_HANDLE, UINT, UINT, BOOLEAN, BOOLEAN, NDIS_INTERRUPT_MODE);
VOID        STDC NdisMDeregisterInterrupt(PNDIS_MINIPORT_INTERRUPT);
NDIS_STATUS STDC NdisMAllocateMapRegisters(NDIS_HANDLE, UINT, BOOLEAN, ULONG, ULONG);
VOID        STDC NdisMFreeMapRegisters(NDIS_HANDLE);
VOID        STDC NdisMAllocateSharedMemory(NDIS_HANDLE, ULONG, BOOLEAN, PVOID *, PNDIS_PHYSICAL_ADDRESS);
VOID        STDC NdisMFreeSharedMemory(NDIS_HANDLE, ULONG, BOOLEAN, PVOID, NDIS_PHYSICAL_ADDRESS);

NDIS_STATUS STDC NdisAllocateMemory(PVOID *, UINT, UINT, NDIS_PHYSICAL_ADDRESS);
VOID        STDC NdisFreeMemory(PVOID, UINT, UINT);

VOID        STDC NdisAllocateSpinLock(PNDIS_SPIN_LOCK);
VOID        STDC NdisFreeSpinLock(PNDIS_SPIN_LOCK);
VOID        STDC NdisAcquireSpinLock(PNDIS_SPIN_LOCK);
VOID        STDC NdisReleaseSpinLock(PNDIS_SPIN_LOCK);
VOID        STDC NdisDprAcquireSpinLock(PNDIS_SPIN_LOCK);
VOID        STDC NdisDprReleaseSpinLock(PNDIS_SPIN_LOCK);

VOID        STDC NdisMInitializeTimer(PNDIS_MINIPORT_TIMER, NDIS_HANDLE, PNDIS_TIMER_FUNCTION, PVOID);
VOID        STDC NdisSetTimer(PNDIS_MINIPORT_TIMER, UINT);
VOID        STDC NdisMCancelTimer(PNDIS_MINIPORT_TIMER, PBOOLEAN);

VOID        STDC NdisOpenConfiguration(PNDIS_STATUS, PNDIS_HANDLE, NDIS_HANDLE);
VOID        STDC NdisCloseConfiguration(NDIS_HANDLE);
VOID        STDC NdisReadConfiguration(PNDIS_STATUS, PNDIS_CONFIGURATION_PARAMETER *, NDIS_HANDLE, PNDIS_STRING, NDIS_PARAMETER_TYPE);
VOID        STDC NdisWriteConfiguration(PNDIS_STATUS, NDIS_HANDLE, PNDIS_STRING, PNDIS_CONFIGURATION_PARAMETER);
VOID        STDC NdisReadNetworkAddress(PNDIS_STATUS, PVOID *, PUINT, NDIS_HANDLE);

VOID        STDC NdisMIndicateStatus(NDIS_HANDLE, NDIS_STATUS, PVOID, UINT);
VOID        STDC NdisMIndicateStatusComplete(NDIS_HANDLE);
VOID        STDC NdisMSendResourcesAvailable(NDIS_HANDLE);

/* Ethernet filter library: the targets of NdisMEthIndicateReceive[Complete] */
__declspec(dllimport) VOID STDC EthFilterDprIndicateReceive(PVOID, NDIS_HANDLE, PCHAR, PVOID, UINT, PVOID, UINT, UINT);
__declspec(dllimport) VOID STDC EthFilterDprIndicateReceiveComplete(PVOID);

#if defined(RTL_NDIS5)
VOID        STDC NdisMGetDeviceProperty(NDIS_HANDLE, PVOID *, PVOID *, PVOID *, PVOID, PVOID);
#endif

/* ================================================================== */
/*  HAL.DLL / NTOSKRNL.EXE imports                                    */
/* ================================================================== */
VOID    STDC KeStallExecutionProcessor(ULONG);          /* HAL */
BOOLEAN STDC MmIsAddressValid(PVOID);                   /* NTOSKRNL */

/* DDK macros that are not exports: implemented locally */
extern void *memcpy(void *, const void *, __SIZE_TYPE__);
extern void *memset(void *, int, __SIZE_TYPE__);
extern int   memcmp(const void *, const void *, __SIZE_TYPE__);
#define NdisMoveMemory(d,s,l)   memcpy((void*)(d), (const void*)(s), (__SIZE_TYPE__)(l))
#define NdisZeroMemory(d,l)     memset((void*)(d), 0, (__SIZE_TYPE__)(l))
#define NdisStallExecution(us)  KeStallExecutionProcessor((ULONG)(us))

static __inline PVOID RtlAlloc(UINT n)
{
    PVOID p = NULL; NDIS_PHYSICAL_ADDRESS h;
    h.QuadPart = ~0ULL;
    if (NdisAllocateMemory(&p, n, 0, h) != NDIS_STATUS_SUCCESS) return NULL;
    return p;
}
static __inline VOID RtlFree(PVOID p, UINT n) { NdisFreeMemory(p, n, 0); }

#endif /* NDISKIT_H */

/* ---- minimal WDM device-open shims (USBNT \Device\UsbNtDev client) ---- */
#ifndef RTL_WDM_SHIMS
#define RTL_WDM_SHIMS
typedef struct _DEVICE_OBJECT {
    short  Type; unsigned short Size; long ReferenceCount;
    PVOID  DriverObject, NextDevice, AttachedDevice, CurrentIrp, Timer;
    ULONG  Flags, Characteristics; PVOID Vpb, DeviceExtension;
} DEVICE_OBJECT, *PDEVICE_OBJECT;
typedef struct _FILE_OBJECT FILE_OBJECT, *PFILE_OBJECT;
#ifndef NT_SUCCESS
#define NT_SUCCESS(s) (((NTSTATUS)(s)) >= 0)
#endif
#define FILE_READ_DATA 0x0001
NTSTATUS STDC IoGetDeviceObjectPointer(PUNICODE_STRING, ULONG, PFILE_OBJECT *, PDEVICE_OBJECT *);
#if defined(RTL_NT31)
VOID STDC ObDereferenceObject(PVOID);
#else
#if defined(_WIN64)
VOID ObfDereferenceObject(PVOID);
#else
/* NT 3.5+ takes the object in ECX. A stdcall declaration corrupts both
   the object reference and stack on the legacy client's unload paths. */
VOID __attribute__((fastcall)) ObfDereferenceObject(PVOID);
#endif
#define ObDereferenceObject(p) ObfDereferenceObject(p)
#endif
#endif
