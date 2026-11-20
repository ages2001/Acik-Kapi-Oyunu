#include "wdm_bitmap.h"

/* Exactly this object is linked into both x86 kernel formats. */
int WkCheck(void)
{
    ULONG words[2] = {0, 0};
    RTL_BITMAP bits;
    RtlInitializeBitMap(&bits, words, 37);
    RtlSetBits(&bits, 2, 27);
    if (RtlFindClearBitsAndSet(&bits, 8, 24) != 29)
        return 0;
    if (!RtlAreBitsSet(&bits, 29, 8) || RtlNumberOfClearBits(&bits) != 2)
        return 0;
    if (RtlFindSetBitsAndClear(&bits, 9, 25) != 25)
        return 0;
    if (!RtlAreBitsClear(&bits, 25, 9) ||
        RtlNumberOfSetBitsInRange(&bits, 29, 8) != 3)
        return 0;
    {
        RTL_BITMAP_RUN runs[2];
        ULONG start;
        if (RtlFindClearRuns(&bits, runs, 2, 1) != 2 ||
            runs[0].StartingIndex != 25 || runs[0].NumberOfBits != 9 ||
            runs[1].StartingIndex != 0 || runs[1].NumberOfBits != 2)
            return 0;
        if (RtlFindLastBackwardRunClear(&bits, 30, &start) != 6 || start != 25)
            return 0;
    }
    return RtlFindMostSignificantBit((ULONGLONG)1 << 63) == 63 &&
           RtlFindLeastSignificantBit((ULONGLONG)1 << 32) == 32 &&
           RtlNumberOfSetBitsUlongPtr(words[1]) == 3;
}
