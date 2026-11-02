/*
 * rxbind.c - receive indication to the NDIS ethernet filter.
 *
 * The DDK's NdisMEthIndicateReceive() is a macro that passes the miniport
 * block's ethernet filter (EthDB) to the filter library routine
 * EthFilterDprIndicateReceive(), which ndis.sys exports on every target.
 * The documented miniport prefix supplies EthDB. Use verified fixed fields
 * and fail closed when they do not match; an arbitrary memory scan cannot
 * establish that a candidate is an NDIS-owned filter.
 *
 * Copyright (C) 2026 ages2001.
 */
#include "rltklant.h"
#include "rtlrange.h"

#define RTL_FILTER_SCAN  0x00C0   /* bytes of ETH_FILTER to search     */

static BOOLEAN RtlKernelPtr(PVOID p)
{
#if defined(_WIN64)
    if ((ULONG_PTR)p < 0xFFFF080000000000ULL) return FALSE;
#else
    if ((ULONG_PTR)p < 0x80000000UL) return FALSE;
#endif
    if ((ULONG_PTR)p & (sizeof(PVOID) - 1)) return FALSE;
    return TRUE;
}

static BOOLEAN RtlRangeValid(PUCHAR p, ULONG len)
{
    ULONG_PTR first, last;
    if (!RtlRangeBounds((ULONG_PTR)p, len, &first, &last)) return FALSE;
    for (;;) {
        if (!MmIsAddressValid((PVOID)first)) return FALSE;
        if (first == last) break;
        first += 0x1000;
    }
    return TRUE;
}

/* Does this structure carry our station address?  NDIS stores it in the
   ETH_FILTER so the filter library can match unicast frames. */
static BOOLEAN RtlFilterHasMac(PRLTK_ADAPTER a, PUCHAR f)
{
    ULONG i;
    if (!RtlKernelPtr(f) || !RtlRangeValid(f, RTL_FILTER_SCAN)) return FALSE;
    for (i = 0; i + 6 <= RTL_FILTER_SCAN; i++)
        if (f[i] == a->CurrentAddr[0] && f[i+1] == a->CurrentAddr[1] &&
            f[i+2] == a->CurrentAddr[2] && f[i+3] == a->CurrentAddr[3] &&
            f[i+4] == a->CurrentAddr[4] && f[i+5] == a->CurrentAddr[5]) return TRUE;
    return FALSE;
}

/* Public NDIS miniport prefix: x86 EthDB at 0xd8; XP/2003 x64
 * EthDB at 0x190. The x64 callback slots and filter back-pointer were
 * checked against the supplied NDIS 5.2 image and its live guest memory.
 * Never scan arbitrary pool objects for a coincidental MAC address. */
static PVOID RtlLocateFilter(PRLTK_ADAPTER a)
{
    PUCHAR block = (PUCHAR)a->MiniportHandle, filter;
#if defined(_WIN64)
    if (!RtlRangeValid(block + 0x190, sizeof(PVOID)) ||
        !RtlRangeValid(block + 0x280, 4 * sizeof(PVOID))) return NULL;
    if (*(PVOID *)(block + 0x280) != (PVOID)EthFilterDprIndicateReceive ||
        *(PVOID *)(block + 0x298) != (PVOID)EthFilterDprIndicateReceiveComplete)
        return NULL;
    filter = *(PUCHAR *)(block + 0x190);
    if (!RtlKernelPtr(filter) ||
        !RtlRangeValid(filter + 0x418, sizeof(PVOID)) ||
        *(PUCHAR *)(filter + 0x418) != block) return NULL;
#else
    if (!RtlRangeValid(block + 0xd8, sizeof(PVOID))) return NULL;
    filter = *(PUCHAR *)(block + 0xd8);
    if (filter == (PUCHAR)a || filter == block || !RtlFilterHasMac(a, filter))
        return NULL;
#endif
    return filter;
}

#ifdef RTL_DBG
static VOID rxdbg(const char *s){ while(*s) RtlOutByte(0xE9,(UCHAR)*s++); }
static int g_rxn = 0;
#else
#define rxdbg(s)
#endif

VOID RtlIndicateFrame(PRLTK_ADAPTER a, PUCHAR frame, UINT len)
{
    UINT look;

#ifdef RTL_DBG
    if (g_rxn < 6) { g_rxn++; rxdbg("RXF\n"); }
#endif
    if (len < RLTK_HEADER_SIZE) { a->RcvErr++; return; }
    if (!a->EthFilter) {
        a->EthFilter = RtlLocateFilter(a);
#ifdef RTL_DBG
        rxdbg(a->EthFilter ? "  filt FOUND\n" : "  filt NULL\n");
#endif
        if (!a->EthFilter) { a->RcvNoBuf++; return; }   /* no binding yet */
    }
    a->RcvOk++;
    look = len - RLTK_HEADER_SIZE;
    a->RxFrame = frame;
    a->RxFrameLen = len;
    EthFilterDprIndicateReceive(a->EthFilter, (NDIS_HANDLE)a, (PCHAR)frame,
                                frame, RLTK_HEADER_SIZE,
                                frame + RLTK_HEADER_SIZE, look, look);
    a->RxFrame = NULL;
}

VOID RtlIndicateComplete(PRLTK_ADAPTER a)
{
    if (a->EthFilter)
        EthFilterDprIndicateReceiveComplete(a->EthFilter);
}
