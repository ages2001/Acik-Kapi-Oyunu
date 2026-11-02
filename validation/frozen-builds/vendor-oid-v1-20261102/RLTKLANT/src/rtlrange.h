/* Checked page bounds for kernel memory probes. The caller still owns the
 * referenced object's lifetime; a mapping check does not acquire ownership. */
#ifndef RTL_RANGE_H
#define RTL_RANGE_H
static BOOLEAN RtlRangeBounds(ULONG_PTR start, ULONG len,
                              ULONG_PTR *first, ULONG_PTR *last)
{
    ULONG_PTR maximum = ~(ULONG_PTR)0;
    if (!len || (ULONG_PTR)(len - 1) > maximum - start) return FALSE;
    *first = start & ~(ULONG_PTR)0xFFF;
    *last = (start + len - 1) & ~(ULONG_PTR)0xFFF;
    return TRUE;
}
#endif
