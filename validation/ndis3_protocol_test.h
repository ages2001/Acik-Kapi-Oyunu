/* Test-only public NDIS 3 protocol ABI subset. The installed mingw-w64
 * ndis.h duplicates ntddndis.h request enums and contains a malformed WAN
 * prototype; keep the required documented prefix here without changing SDKs.
 * Layouts follow NDIS30_PROTOCOL_CHARACTERISTICS and NDIS_REQUEST in that DDK. */
#include <ntddk.h>
#include <ntddndis.h>
typedef unsigned int UINT;
typedef int NDIS_STATUS,*PNDIS_STATUS;
typedef PVOID NDIS_HANDLE,PNDIS_PACKET;
typedef UNICODE_STRING NDIS_STRING;
#define NDIS_STATUS_SUCCESS 0
#define NDIS_STATUS_PENDING 0x103
#define NDIS_STATUS_FAILURE ((NDIS_STATUS)0xc0000001)
#define NDIS_STATUS_NOT_SUPPORTED ((NDIS_STATUS)0xc00000bb)
#define NDIS_STATUS_NOT_ACCEPTED ((NDIS_STATUS)0x00010003)
typedef struct {
    UCHAR MacReserved[4*sizeof(PVOID)];NDIS_REQUEST_TYPE RequestType;
    union {struct {ULONG Oid;PVOID InformationBuffer;ULONG InformationBufferLength;
                   ULONG BytesWritten,BytesNeeded;} QUERY_INFORMATION;} DATA;
} NDIS_REQUEST,*PNDIS_REQUEST;
typedef struct {
    UCHAR MajorNdisVersion,MinorNdisVersion;USHORT Filler;UINT Reserved;
    VOID (NTAPI *OpenAdapterCompleteHandler)(NDIS_HANDLE,NDIS_STATUS,NDIS_STATUS);
    VOID (NTAPI *CloseAdapterCompleteHandler)(NDIS_HANDLE,NDIS_STATUS);
    VOID (NTAPI *SendCompleteHandler)(NDIS_HANDLE,PNDIS_PACKET,NDIS_STATUS);
    VOID (NTAPI *TransferDataCompleteHandler)(NDIS_HANDLE,PNDIS_PACKET,NDIS_STATUS,UINT);
    VOID (NTAPI *ResetCompleteHandler)(NDIS_HANDLE,NDIS_STATUS);
    VOID (NTAPI *RequestCompleteHandler)(NDIS_HANDLE,PNDIS_REQUEST,NDIS_STATUS);
    NDIS_STATUS (NTAPI *ReceiveHandler)(NDIS_HANDLE,NDIS_HANDLE,PVOID,UINT,PVOID,UINT,UINT);
    VOID (NTAPI *ReceiveCompleteHandler)(NDIS_HANDLE);
    VOID (NTAPI *StatusHandler)(NDIS_HANDLE,NDIS_STATUS,PVOID,UINT);
    VOID (NTAPI *StatusCompleteHandler)(NDIS_HANDLE);
    NDIS_STRING Name;
} NDIS30_PROTOCOL_CHARACTERISTICS,*PNDIS_PROTOCOL_CHARACTERISTICS;
typedef struct {
    NDIS30_PROTOCOL_CHARACTERISTICS Base;
    PVOID ReceivePacketHandler;
    VOID (NTAPI *BindAdapterHandler)(PNDIS_STATUS,NDIS_HANDLE,NDIS_STRING*,PVOID,PVOID);
    VOID (NTAPI *UnbindAdapterHandler)(PNDIS_STATUS,NDIS_HANDLE,NDIS_HANDLE);
    NDIS_STATUS (NTAPI *PnPEventHandler)(NDIS_HANDLE,PVOID);
    PVOID UnloadHandler;
    PVOID ReservedHandlers[4],CoHandlers[4];
} NDIS50_PROTOCOL_TEST;
typedef char CheckProtocol50Size[sizeof(NDIS50_PROTOCOL_TEST)==(sizeof(PVOID)==8?208:108)?1:-1];
typedef char CheckProtocolSize[sizeof(NDIS30_PROTOCOL_CHARACTERISTICS)==(sizeof(PVOID)==8?104:56)?1:-1];
typedef char CheckRequestSize[sizeof(NDIS_REQUEST)==(sizeof(PVOID)==8?72:40)?1:-1];
__declspec(dllimport) VOID NTAPI NdisRegisterProtocol(PNDIS_STATUS,NDIS_HANDLE*,PNDIS_PROTOCOL_CHARACTERISTICS,UINT);
__declspec(dllimport) VOID NTAPI NdisDeregisterProtocol(PNDIS_STATUS,NDIS_HANDLE);
__declspec(dllimport) VOID NTAPI NdisOpenAdapter(PNDIS_STATUS,PNDIS_STATUS,NDIS_HANDLE*,UINT*,NDIS_MEDIUM*,UINT,NDIS_HANDLE,NDIS_HANDLE,NDIS_STRING*,UINT,PSTRING);
__declspec(dllimport) VOID NTAPI NdisCloseAdapter(PNDIS_STATUS,NDIS_HANDLE);
__declspec(dllimport) VOID NTAPI NdisRequest(PNDIS_STATUS,NDIS_HANDLE,PNDIS_REQUEST);
__declspec(dllimport) VOID NTAPI NdisReset(PNDIS_STATUS,NDIS_HANDLE);
