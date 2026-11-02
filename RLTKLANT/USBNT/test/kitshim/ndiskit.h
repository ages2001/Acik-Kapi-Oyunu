/* host-test shim: only the scalar kit surface the USB-NIC module uses */
#ifndef NDISKIT_H
#define NDISKIT_H
#include <stdint.h>
typedef unsigned char   UCHAR,  *PUCHAR;
typedef unsigned short  USHORT, *PUSHORT;
typedef uint32_t        ULONG,  *PULONG;
typedef unsigned int    UINT,   *PUINT;
typedef void            VOID,   *PVOID;
typedef unsigned char   BOOLEAN,*PBOOLEAN;
typedef int             NDIS_STATUS, *PNDIS_STATUS;
#define STDC
#define TRUE  1
#define FALSE 0
#define NDIS_STATUS_SUCCESS           ((NDIS_STATUS)0x00000000L)
#define NDIS_STATUS_FAILURE           ((NDIS_STATUS)0xC0000001L)
#define NDIS_STATUS_ADAPTER_NOT_FOUND ((NDIS_STATUS)0xC0010013L)
#endif
