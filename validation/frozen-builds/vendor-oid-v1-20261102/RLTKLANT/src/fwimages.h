/* Immutable firmware catalogue. Selection alone does not initialize a PHY. */
#ifndef RLTK_FWIMAGES_H
#define RLTK_FWIMAGES_H
#include "fwvm.h"
typedef struct {
    const char *Name;
    const RTL_FW_U8 *Bytes;
    RTL_FW_U32 Size;
} RTL_FW_IMAGE;
typedef struct {
    RTL_FW_U16 Mask,Xid,MacVersion;
    const RTL_FW_IMAGE *Image;
} RTL_FW_CHIP;
const RTL_FW_CHIP *RtlFwChip(RTL_FW_U16 xid,int gmii);
#endif
