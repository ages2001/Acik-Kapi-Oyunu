/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef RTL_USB_FW_H
#define RTL_USB_FW_H
/* Stable version suffixes (1..17), not the Linux enum ABI, which has a test
   version interspersed. The caller supplies SHA256; validation performs no
   register writes or allocation. A checksum is not a vendor signature. */
typedef unsigned char RTL_UFW_U8;
typedef unsigned int RTL_UFW_U32;
typedef char RTL_UFW_REQUIRE_U32[(sizeof(RTL_UFW_U32)==4)?1:-1];
enum RTL_UFW_RESULT {RtlUsbFwOk=0,RtlUsbFwArgument,RtlUsbFwDigest,
    RtlUsbFwBounds,RtlUsbFwUnsupported,RtlUsbFwSequence,RtlUsbFwRegister};
typedef int (*RTL_UFW_DIGEST)(void*,const RTL_UFW_U8*,RTL_UFW_U32,const RTL_UFW_U8*);
typedef struct {
    RTL_UFW_U32 Blocks,PayloadBytes,ErrorOffset,EndMarker;
    enum RTL_UFW_RESULT Result;
} RTL_UFW_REPORT;
enum RTL_UFW_RESULT RtlUsbFwValidate(const RTL_UFW_U8*,RTL_UFW_U32,
    unsigned,RTL_UFW_DIGEST,void*,RTL_UFW_REPORT*);
/* Returns zero for invalid pointers. A NULL input is valid for an empty hash.
   Output is exactly 32 bytes and may overlap the input. */
int RtlUsbFwSha256(const RTL_UFW_U8*,RTL_UFW_U32,RTL_UFW_U8*);
int RtlUsbFwCheckDigest(void*,const RTL_UFW_U8*,RTL_UFW_U32,const RTL_UFW_U8*);
#endif
