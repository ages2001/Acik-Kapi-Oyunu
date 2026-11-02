/*
 * rtlport.c - low-level x86 I/O-port access.
 *
 * NDIS exposes NdisRawReadPortUchar() only as a DDK macro that expands to
 * the HAL routine READ_PORT_UCHAR(); it is not an ndis.sys export.  We
 * therefore bind straight to the HAL routines on 32-bit targets and to the
 * in/out instructions on x64 (where the HAL no longer exports them).
 *
 * Copyright (C) 2026 ages2001.
 */
#include "rltklant.h"

#if defined(_WIN64)

/* x64: issue the port instructions directly. */
UCHAR  RtlInByte(ULONG_PTR p){ UCHAR  v; __asm__ volatile("inb %1,%0":"=a"(v):"Nd"((USHORT)p)); return v; }
USHORT RtlInWord(ULONG_PTR p){ USHORT v; __asm__ volatile("inw %1,%0":"=a"(v):"Nd"((USHORT)p)); return v; }
ULONG  RtlInDword(ULONG_PTR p){ ULONG v; __asm__ volatile("inl %1,%0":"=a"(v):"Nd"((USHORT)p)); return v; }
VOID   RtlOutByte(ULONG_PTR p,UCHAR v){ __asm__ volatile("outb %0,%1"::"a"(v),"Nd"((USHORT)p)); }
VOID   RtlOutWord(ULONG_PTR p,USHORT v){ __asm__ volatile("outw %0,%1"::"a"(v),"Nd"((USHORT)p)); }
VOID   RtlOutDword(ULONG_PTR p,ULONG v){ __asm__ volatile("outl %0,%1"::"a"(v),"Nd"((USHORT)p)); }

#else

/* x86: use the HAL-exported port routines. */
UCHAR  STDC READ_PORT_UCHAR(PUCHAR);
USHORT STDC READ_PORT_USHORT(PUSHORT);
ULONG  STDC READ_PORT_ULONG(PULONG);
VOID   STDC WRITE_PORT_UCHAR(PUCHAR,UCHAR);
VOID   STDC WRITE_PORT_USHORT(PUSHORT,USHORT);
VOID   STDC WRITE_PORT_ULONG(PULONG,ULONG);

UCHAR  RtlInByte(ULONG_PTR p){ return READ_PORT_UCHAR((PUCHAR)p); }
USHORT RtlInWord(ULONG_PTR p){ return READ_PORT_USHORT((PUSHORT)p); }
ULONG  RtlInDword(ULONG_PTR p){ return READ_PORT_ULONG((PULONG)p); }
VOID   RtlOutByte(ULONG_PTR p,UCHAR v){ WRITE_PORT_UCHAR((PUCHAR)p,v); }
VOID   RtlOutWord(ULONG_PTR p,USHORT v){ WRITE_PORT_USHORT((PUSHORT)p,v); }
VOID   RtlOutDword(ULONG_PTR p,ULONG v){ WRITE_PORT_ULONG((PULONG)p,v); }

#endif
