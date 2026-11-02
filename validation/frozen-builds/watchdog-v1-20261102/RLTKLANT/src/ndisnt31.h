/*
 * ndis31.h - NDIS 3.0 *MAC-model* kernel definitions for the Windows NT 3.1
 *            build of RLTKLANT.
 *
 * NT 3.1's ndis.sys has NO miniport model (verified: it exports NdisRegisterMac
 * / NdisRegisterAdapter and the Eth* filter package, but no NdisMRegisterMiniport),
 * so the NT 3.1 adapter is a full NDIS 3.0 MAC driver.  It also has no PCI NDIS
 * API, so the card is found by direct PCI config access (0xCF8/0xCFC) and DMA
 * buffers use MmAllocateContiguousMemory + MmGetPhysicalAddress (bus address ==
 * physical address on x86 PCI).  All Realtek register models are covered.
 *
 * Copyright (C) 2026 ages2001.
 */
#ifndef NDIS31_H
#define NDIS31_H

#define STDC __stdcall
#define IN
#define OUT

typedef unsigned char   UCHAR,  *PUCHAR;
typedef unsigned short  USHORT, *PUSHORT;
typedef unsigned short  WCHAR,  *PWCHAR;
typedef unsigned long   ULONG,  *PULONG;
typedef unsigned int    UINT,   *PUINT;
typedef unsigned char   BOOLEAN, *PBOOLEAN; /* NT returns BOOLEAN in AL only */
typedef void            VOID,   *PVOID;
typedef char             CHAR,   *PCHAR;
typedef unsigned long   ULONG_PTR;
typedef int             NDIS_STATUS, *PNDIS_STATUS;
typedef PVOID           NDIS_HANDLE,  *PNDIS_HANDLE;
typedef ULONG           NDIS_OID,     *PNDIS_OID;

#ifndef NULL
#define NULL ((void*)0)
#endif
#define TRUE 1
#define FALSE 0

#define NDIS_STATUS_SUCCESS        ((NDIS_STATUS)0x00000000L)
#define NDIS_STATUS_PENDING        ((NDIS_STATUS)0x00000103L)
#define NDIS_STATUS_FAILURE        ((NDIS_STATUS)0xC0000001L)
#define NDIS_STATUS_RESOURCES      ((NDIS_STATUS)0xC000009AL)
#define NDIS_STATUS_NOT_SUPPORTED  ((NDIS_STATUS)0xC00000BBL)
#define NDIS_STATUS_BUFFER_TOO_SHORT ((NDIS_STATUS)0xC0010016L)
#define NDIS_STATUS_INVALID_OID    ((NDIS_STATUS)0xC0010017L)
#define NDIS_STATUS_INVALID_LENGTH ((NDIS_STATUS)0xC0010014L)
#define NDIS_STATUS_CLOSING_INDICATING ((NDIS_STATUS)0xC001000EL)

typedef struct _NDIS_STRING { USHORT Length, MaximumLength; PWCHAR Buffer; } NDIS_STRING, *PNDIS_STRING;

typedef enum { NdisMedium802_3 = 0 } NDIS_MEDIUM;
typedef enum { NdisInterfaceInternal=0, NdisInterfaceIsa=1, NdisInterfacePci=5 } NDIS_INTERFACE_TYPE;

/* 64-bit physical address (LARGE_INTEGER).  On x86, 8-byte structs are
   returned in EDX:EAX (Windows/MSVC ABI, which mingw-w64 matches). */
typedef struct _NDIS_PHYSICAL_ADDRESS {
    ULONG LowPart;
    long  HighPart;
} NDIS_PHYSICAL_ADDRESS, *PNDIS_PHYSICAL_ADDRESS;

typedef struct _NDIS_CONFIGURATION_PARAMETER {
    int ParameterType;
    union { ULONG IntegerData; NDIS_STRING StringData; } ParameterData;
} NDIS_CONFIGURATION_PARAMETER, *PNDIS_CONFIGURATION_PARAMETER;
#define NdisParameterInteger    0
#define NdisParameterHexInteger 1

/* ---- NDIS_REQUEST (OID query/set) ------------------------------------- */
typedef enum _NDIS_REQUEST_TYPE {
    NdisRequestQueryInformation,
    NdisRequestSetInformation,
    NdisRequestQueryStatistics,
    NdisRequestOpen, NdisRequestClose, NdisRequestSend, NdisRequestTransferData,
    NdisRequestReset, NdisRequestGeneric1, NdisRequestGeneric2,
    NdisRequestGeneric3, NdisRequestGeneric4
} NDIS_REQUEST_TYPE;

typedef struct _NDIS_REQUEST {
    UCHAR MacReserved[16];
    NDIS_REQUEST_TYPE RequestType;
    union _NDIS_REQUEST_DATA {
        struct { NDIS_OID Oid; PVOID InformationBuffer; UINT InformationBufferLength;
                 UINT BytesWritten; UINT BytesNeeded; } QUERY_INFORMATION;
        struct { NDIS_OID Oid; PVOID InformationBuffer; UINT InformationBufferLength;
                 UINT BytesRead; UINT BytesNeeded; } SET_INFORMATION;
    } DATA;
} NDIS_REQUEST, *PNDIS_REQUEST;

/* ---- OID constants we handle ------------------------------------------ */
#define OID_GEN_SUPPORTED_LIST          0x00010101
#define OID_GEN_HARDWARE_STATUS         0x00010102
#define OID_GEN_MEDIA_SUPPORTED         0x00010103
#define OID_GEN_MEDIA_IN_USE            0x00010104
#define OID_GEN_MAXIMUM_LOOKAHEAD       0x00010105
#define OID_GEN_MAXIMUM_FRAME_SIZE      0x00010106
#define OID_GEN_LINK_SPEED              0x00010107
#define OID_GEN_TRANSMIT_BUFFER_SPACE   0x00010108
#define OID_GEN_RECEIVE_BUFFER_SPACE    0x00010109
#define OID_GEN_TRANSMIT_BLOCK_SIZE     0x0001010A
#define OID_GEN_RECEIVE_BLOCK_SIZE      0x0001010B
#define OID_GEN_VENDOR_ID               0x0001010C
#define OID_GEN_VENDOR_DESCRIPTION      0x0001010D
#define OID_GEN_CURRENT_PACKET_FILTER   0x0001010E
#define OID_GEN_CURRENT_LOOKAHEAD       0x0001010F
#define OID_GEN_DRIVER_VERSION          0x00010110
#define OID_GEN_MAXIMUM_TOTAL_SIZE      0x00010111
#define OID_GEN_MAC_OPTIONS             0x00010113
#define OID_GEN_MEDIA_CONNECT_STATUS    0x00010114
#define OID_802_3_PERMANENT_ADDRESS     0x01010101
#define OID_802_3_CURRENT_ADDRESS       0x01010102
#define OID_802_3_MULTICAST_LIST        0x01010103
#define OID_802_3_MAXIMUM_LIST_SIZE     0x01010104

/* packet filter bits */
#define NDIS_PACKET_TYPE_DIRECTED       0x0001
#define NDIS_PACKET_TYPE_MULTICAST      0x0002
#define NDIS_PACKET_TYPE_ALL_MULTICAST  0x0004
#define NDIS_PACKET_TYPE_BROADCAST      0x0008
#define NDIS_PACKET_TYPE_PROMISCUOUS    0x0020
/* MAC options */
#define NDIS_MAC_OPTION_TRANSFERS_NOT_PEND 0x00000001
#define NDIS_MAC_OPTION_RECEIVE_SERIALIZED 0x00000002
#define NDIS_MAC_OPTION_NO_LOOPBACK        0x00000008

/* ---- NDIS_PACKET / NDIS_BUFFER (MDL) ---------------------------------- */
typedef struct _NDIS_BUFFER {          /* == MDL; only Next used directly   */
    struct _NDIS_BUFFER *Next;
    USHORT Size; USHORT MdlFlags;
    PVOID  Process; PVOID MappedSystemVa; PVOID StartVa;
    ULONG  ByteCount; ULONG ByteOffset;
} NDIS_BUFFER, *PNDIS_BUFFER;

typedef struct _NDIS_PACKET_PRIVATE {
    UINT  PhysicalCount;
    UINT  TotalLength;
    PNDIS_BUFFER Head;
    PNDIS_BUFFER Tail;
    NDIS_HANDLE  Pool;
    UINT  Count;
    ULONG Flags;
    BOOLEAN ValidCounts;
} NDIS_PACKET_PRIVATE;

typedef struct _NDIS_PACKET {
    NDIS_PACKET_PRIVATE Private;
    UCHAR  MacReserved[16];
    ULONG  Reserved[2];
    UCHAR  ProtocolReserved[1];
} NDIS_PACKET, *PNDIS_PACKET;

/* ---- NDIS_ADAPTER_INFORMATION (passed to NdisRegisterAdapter) --------- */
typedef struct _NDIS_PORT_DESCRIPTOR {
    ULONG  InitialPort;
    ULONG  NumberOfPorts;
    PVOID *PortOffset;
} NDIS_PORT_DESCRIPTOR;

typedef struct _NDIS_ADAPTER_INFORMATION {
    UINT   DmaChannel;
    UCHAR  Master;              /* NT BOOLEAN is 1 byte - keep layout exact */
    UCHAR  Dma32BitAddresses;
    UCHAR  Pad[2];
    ULONG  AdapterType;         /* NDIS_INTERFACE_TYPE                      */
    ULONG  PhysicalMapRegistersNeeded;
    ULONG  MaximumPhysicalMapping;
    UINT   NumberOfPortDescriptors;
    NDIS_PORT_DESCRIPTOR PortDescriptors[4];
} NDIS_ADAPTER_INFORMATION, *PNDIS_ADAPTER_INFORMATION;

/* MAC characteristics: the handler table NT 3.1 NDIS calls. */
/* NDIS 3.0 MAC MacOpenAdapter: OUT status, OUT MacBindingHandle,
   OUT SelectedMediumIndex, IN MediumArray, IN MediumArraySize,
   IN NdisBindingContext, IN MacAdapterContext, IN OpenOptions, IN AddrInfo. */
typedef NDIS_STATUS (STDC *OPEN_ADAPTER)(PNDIS_STATUS,NDIS_HANDLE*,PUINT,NDIS_MEDIUM*,UINT,NDIS_HANDLE,NDIS_HANDLE,UINT,PVOID);
typedef NDIS_STATUS (STDC *CLOSE_ADAPTER)(NDIS_HANDLE);
typedef NDIS_STATUS (STDC *MAC_SEND)(NDIS_HANDLE,PNDIS_PACKET);
typedef NDIS_STATUS (STDC *TRANSFER_DATA)(NDIS_HANDLE,NDIS_HANDLE,UINT,UINT,PNDIS_PACKET,PUINT);
typedef NDIS_STATUS (STDC *MAC_RESET)(NDIS_HANDLE);
typedef NDIS_STATUS (STDC *MAC_REQUEST)(NDIS_HANDLE,PNDIS_REQUEST);
typedef NDIS_STATUS (STDC *QUERY_GLOBAL_STATS)(NDIS_HANDLE,PNDIS_REQUEST);
typedef NDIS_STATUS (STDC *QUERY_PROTOCOL_STATS)(NDIS_HANDLE,NDIS_HANDLE,PVOID);
typedef VOID        (STDC *UNLOAD_MAC)(NDIS_HANDLE);
typedef NDIS_STATUS (STDC *ADD_ADAPTER)(NDIS_HANDLE,NDIS_HANDLE,PNDIS_STRING);
typedef VOID        (STDC *REMOVE_ADAPTER)(NDIS_HANDLE);

/* NT 3.1 NDIS30_MAC_CHARACTERISTICS (verified against inbox NE2000.SYS
   DriverEntry): a UINT Reserved sits at offset 0x04 (handlers start at 0x08),
   there are exactly 10 handlers (NO QueryProtocolStatistics - that is a
   protocol, not a MAC, handler), and the struct is 0x38 bytes.  Omitting the
   Reserved field shifts every handler by 4, so NDIS calls OpenAdapter where
   CloseAdapter actually sits and corrupts its stack. */
typedef struct _NDIS_MAC_CHARACTERISTICS {
    UCHAR  MajorNdisVersion;    /* 0x00 */
    UCHAR  MinorNdisVersion;    /* 0x01 */
    USHORT Filler;              /* 0x02 */
    UINT   Reserved;            /* 0x04 */
    OPEN_ADAPTER        OpenAdapterHandler;            /* 0x08 */
    CLOSE_ADAPTER       CloseAdapterHandler;           /* 0x0C */
    MAC_SEND            SendHandler;                   /* 0x10 */
    TRANSFER_DATA       TransferDataHandler;           /* 0x14 */
    MAC_RESET           ResetHandler;                  /* 0x18 */
    MAC_REQUEST         RequestHandler;                /* 0x1C */
    QUERY_GLOBAL_STATS  QueryGlobalStatisticsHandler;  /* 0x20 */
    UNLOAD_MAC          UnloadMacHandler;              /* 0x24 */
    ADD_ADAPTER         AddAdapterHandler;             /* 0x28 */
    REMOVE_ADAPTER      RemoveAdapterHandler;          /* 0x2C */
    NDIS_STRING         Name;                          /* 0x30 (0x38 total) */
} NDIS_MAC_CHARACTERISTICS, *PNDIS_MAC_CHARACTERISTICS;

/* ---- Ethernet filter package callbacks -------------------------------- */
typedef NDIS_STATUS (STDC *ETH_ADDRESS_CHANGE)(UINT,PVOID,UINT,PVOID,NDIS_HANDLE,PNDIS_REQUEST,BOOLEAN);
typedef NDIS_STATUS (STDC *ETH_FILTER_CHANGE)(UINT,UINT,NDIS_HANDLE,PNDIS_REQUEST,BOOLEAN);
typedef VOID        (STDC *ETH_DEFERRED_CLOSE)(NDIS_HANDLE);

/* NDIS_SPIN_LOCK: KSPIN_LOCK + saved IRQL; a zeroed lock is unlocked/valid. */
typedef struct _NDIS_SPIN_LOCK { ULONG SpinLock; ULONG OldIrql; } NDIS_SPIN_LOCK, *PNDIS_SPIN_LOCK;

/* NDIS timer object (KTIMER + KDPC) - over-sized to be safe. */
typedef struct _NDIS_TIMER { UCHAR Opaque[96]; } NDIS_TIMER, *PNDIS_TIMER;
typedef VOID (STDC *NDIS_TIMER_FUNCTION)(PVOID,PVOID,PVOID,PVOID);

/* NDIS interrupt object (over-sized). */
typedef struct _NDIS_INTERRUPT { UCHAR Opaque[128]; } NDIS_INTERRUPT, *PNDIS_INTERRUPT;

/* imported NT 3.1 NDIS routines */
VOID        STDC NdisInitializeWrapper(PNDIS_HANDLE,PVOID,PVOID,PVOID);
VOID        STDC NdisTerminateWrapper(NDIS_HANDLE,PVOID);
NDIS_STATUS STDC NdisRegisterMac(PNDIS_STATUS,PNDIS_HANDLE,NDIS_HANDLE,PVOID,PNDIS_MAC_CHARACTERISTICS,UINT);
VOID        STDC NdisDeregisterMac(PNDIS_STATUS,NDIS_HANDLE);
NDIS_STATUS STDC NdisRegisterAdapter(PNDIS_HANDLE,NDIS_HANDLE,NDIS_HANDLE,NDIS_HANDLE,PVOID,PVOID);
NDIS_STATUS STDC NdisDeregisterAdapter(NDIS_HANDLE);
VOID        STDC NdisCompleteOpenAdapter(NDIS_HANDLE,NDIS_STATUS,NDIS_STATUS);
VOID        STDC NdisCompleteCloseAdapter(NDIS_HANDLE,NDIS_STATUS);
NDIS_STATUS STDC NdisAllocateMemory(PVOID*,UINT,UINT,NDIS_PHYSICAL_ADDRESS);
VOID        STDC NdisFreeMemory(PVOID,UINT,UINT);
VOID        STDC NdisOpenConfiguration(PNDIS_STATUS,PNDIS_HANDLE,NDIS_HANDLE);
VOID        STDC NdisReadConfiguration(PNDIS_STATUS,PNDIS_CONFIGURATION_PARAMETER*,NDIS_HANDLE,PNDIS_STRING,UINT);
VOID        STDC NdisCloseConfiguration(NDIS_HANDLE);
VOID        STDC NdisQueryBuffer(PNDIS_BUFFER,PVOID*,PUINT);
VOID        STDC NdisInitializeTimer(PNDIS_TIMER,NDIS_TIMER_FUNCTION,PVOID);
VOID        STDC NdisSetTimer(PNDIS_TIMER,UINT);
BOOLEAN     STDC KeCancelTimer(PVOID);
/* NT 3.1 MAC interrupt (10 args, per inbox NE2000):
   (OUT Status, Interrupt, NdisAdapterHandle, ISR, Context, DPC,
    Vector, Level, SharedInterrupt, InterruptMode). */
VOID        STDC NdisInitializeInterrupt(PNDIS_STATUS,PNDIS_INTERRUPT,NDIS_HANDLE,PVOID,PVOID,PVOID,UINT,UINT,BOOLEAN,UINT);
VOID        STDC NdisRemoveInterrupt(PNDIS_INTERRUPT);

/* Ethernet filter package */
BOOLEAN STDC EthCreateFilter(UINT,ETH_ADDRESS_CHANGE,ETH_FILTER_CHANGE,ETH_DEFERRED_CLOSE,PUCHAR,PNDIS_SPIN_LOCK,PNDIS_HANDLE);
VOID    STDC EthDeleteFilter(NDIS_HANDLE);
BOOLEAN STDC EthNoteFilterOpenAdapter(NDIS_HANDLE,NDIS_HANDLE,NDIS_HANDLE,PNDIS_HANDLE);
NDIS_STATUS STDC EthDeleteFilterOpenAdapter(NDIS_HANDLE,NDIS_HANDLE,PNDIS_REQUEST);
NDIS_STATUS STDC EthFilterAdjust(NDIS_HANDLE,NDIS_HANDLE,PNDIS_REQUEST,UINT,BOOLEAN);
NDIS_STATUS STDC EthChangeFilterAddresses(NDIS_HANDLE,NDIS_HANDLE,PNDIS_REQUEST,UINT,PVOID,BOOLEAN);
VOID    STDC EthFilterIndicateReceive(NDIS_HANDLE,NDIS_HANDLE,PCHAR,PVOID,UINT,PVOID,UINT,UINT);
VOID    STDC EthFilterIndicateReceiveComplete(NDIS_HANDLE);

/* ntoskrnl.exe - contiguous DMA memory + physical address (x86: phys==bus) */
PVOID  STDC MmAllocateContiguousMemory(ULONG,NDIS_PHYSICAL_ADDRESS);
VOID   STDC MmFreeContiguousMemory(PVOID);
NDIS_PHYSICAL_ADDRESS STDC MmGetPhysicalAddress(PVOID);
PVOID  STDC MmMapIoSpace(NDIS_PHYSICAL_ADDRESS,ULONG,int);
VOID   STDC MmUnmapIoSpace(PVOID,ULONG);

/* ntoskrnl.exe - lock the driver image resident.  NT 3.1 loads driver images
   page-faultable: code pages not touched during init are not present, so a DPC
   faults on its own code at DISPATCH_LEVEL (bugcheck 0xA).  Build an MDL over
   the whole driver image [DriverStart, DriverSize] and MmProbeAndLockPages it
   at PASSIVE during DriverEntry; the pages fault in and stay resident. */
#define IO_READ_ACCESS   0
#define KERNEL_MODE      0
PVOID STDC IoAllocateMdl(PVOID,ULONG,BOOLEAN,BOOLEAN,PVOID);
VOID  STDC IoFreeMdl(PVOID);
VOID  STDC MmProbeAndLockPages(PVOID,UCHAR,UINT);
VOID  STDC MmUnlockPages(PVOID);

/* DRIVER_OBJECT: only DriverStart (0x0C) and DriverSize (0x10) are read. */
typedef struct _DRV_OBJ {
    USHORT Type; USHORT Size;
    PVOID  DeviceObject;
    ULONG  Flags;
    PVOID  DriverStart;      /* 0x0C */
    ULONG  DriverSize;       /* 0x10 */
} DRV_OBJ, *PDRV_OBJ;

/* ntoskrnl.exe - PASSIVE-level RX poll thread (NT 3.1 demand-pages driver code
   so a DPC/ISR faults on it at DISPATCH; a system thread runs the poll at
   PASSIVE, where the code faults in normally, and only briefly raises IRQL
   around the receive indication). */
typedef long NTSTATUS;
typedef UCHAR KIRQL, *PKIRQL;
typedef struct _LARGE_INT { ULONG LowPart; long HighPart; } LARGE_INT, *PLARGE_INT;
typedef VOID (STDC *PKSTART_ROUTINE)(PVOID);
#define DISPATCH_LEVEL 2
NTSTATUS STDC PsCreateSystemThread(PVOID,ULONG,PVOID,PVOID,PVOID,PKSTART_ROUTINE,PVOID);
VOID     STDC PsTerminateSystemThread(NTSTATUS);
NTSTATUS STDC KeDelayExecutionThread(UCHAR,BOOLEAN,PLARGE_INT);
VOID     STDC KeStallExecutionProcessor(ULONG);
VOID     STDC KeRaiseIrql(KIRQL,PKIRQL);
VOID     STDC KeLowerIrql(KIRQL);
VOID     STDC KeAcquireSpinLock(PULONG,PKIRQL);
VOID     STDC KeReleaseSpinLock(PULONG,KIRQL);
VOID     STDC KeInitializeEvent(PVOID,int,BOOLEAN);
long     STDC KeSetEvent(PVOID,long,BOOLEAN);
NTSTATUS STDC KeWaitForSingleObject(PVOID,int,int,BOOLEAN,PLARGE_INT);

/* HAL port I/O (NT 3.1 has no NdisRawReadPort*) */
UCHAR  STDC READ_PORT_UCHAR(PUCHAR);
USHORT STDC READ_PORT_USHORT(PUSHORT);
ULONG  STDC READ_PORT_ULONG(PULONG);
VOID   STDC WRITE_PORT_UCHAR(PUCHAR,UCHAR);
VOID   STDC WRITE_PORT_USHORT(PUSHORT,USHORT);
VOID   STDC WRITE_PORT_ULONG(PULONG,ULONG);

/* NT3.1 kernel worker queue, used after the last resident callback returns. */
VOID STDC ExQueueWorkItem(PVOID WorkItem,int QueueType);

#endif /* NDIS31_H */
