/*
 * ndismac.h - NDIS buffer-chain helpers.
 *
 * NDIS_BUFFER is an MDL.  The chain link (MDL.Next) is the first field on
 * every architecture; the virtual address and length come from the
 * exported NdisQueryBuffer(), which maps the buffer when necessary.
 *
 * Copyright (C) 2026 ages2001.
 */
#ifndef NDISMAC_H
#define NDISMAC_H

#include "rltklant.h"

VOID STDC NdisQueryBuffer(PNDIS_BUFFER, PVOID *, PUINT);

static __inline PNDIS_BUFFER RtlBufNext(PNDIS_BUFFER b)
{
    return *(PNDIS_BUFFER *)b;
}

#endif /* NDISMAC_H */
