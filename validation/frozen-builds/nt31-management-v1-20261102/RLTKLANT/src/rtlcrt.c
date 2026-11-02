/*
 * rtlcrt.c - freestanding C runtime stubs the compiler may emit calls to
 *            (memcpy/memset/memmove/memcmp) for aggregate copies/inits.
 * Copyright (C) 2026 ages2001.
 */
typedef __SIZE_TYPE__ rtl_size;

void *memcpy(void *d, const void *s, rtl_size n)
{
    unsigned char *dd = (unsigned char*)d; const unsigned char *ss = (const unsigned char*)s;
    while (n--) *dd++ = *ss++;
    return d;
}
void *memmove(void *d, const void *s, rtl_size n)
{
    unsigned char *dd = (unsigned char*)d; const unsigned char *ss = (const unsigned char*)s;
    if (dd < ss) while (n--) *dd++ = *ss++;
    else { dd += n; ss += n; while (n--) *--dd = *--ss; }
    return d;
}
void *memset(void *d, int c, rtl_size n)
{
    unsigned char *dd = (unsigned char*)d;
    while (n--) *dd++ = (unsigned char)c;
    return d;
}
int memcmp(const void *a, const void *b, rtl_size n)
{
    const unsigned char *p = (const unsigned char*)a, *q = (const unsigned char*)b;
    while (n--) { if (*p != *q) return *p - *q; p++; q++; }
    return 0;
}
