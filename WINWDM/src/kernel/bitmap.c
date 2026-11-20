#include "wdm_bitmap.h"

#define NO_BITS ((ULONG)0xffffffffUL)
#define MASK(i) ((ULONG)1 << ((i) & 31))

void WK_CALL RtlInitializeBitMap(PRTL_BITMAP map, ULONG *words, ULONG size)
{
    map->SizeOfBitMap = size;
    map->Buffer = words;
}

static void fill(PRTL_BITMAP map, ULONG word)
{
    ULONG i, count = map->SizeOfBitMap / 32 + !!(map->SizeOfBitMap % 32);
    for (i = 0; i < count; ++i)
        map->Buffer[i] = word;
}

void WK_CALL RtlSetAllBits(PRTL_BITMAP map)   { fill(map, NO_BITS); }
void WK_CALL RtlClearAllBits(PRTL_BITMAP map) { fill(map, 0); }
void WK_CALL RtlSetBit(PRTL_BITMAP map, ULONG i)   { map->Buffer[i / 32] |= MASK(i); }
void WK_CALL RtlClearBit(PRTL_BITMAP map, ULONG i) { map->Buffer[i / 32] &= ~MASK(i); }
BOOLEAN WK_CALL RtlTestBit(PRTL_BITMAP map, ULONG i) { return !!(map->Buffer[i / 32] & MASK(i)); }

void WK_CALL RtlSetBits(PRTL_BITMAP map, ULONG start, ULONG length)
{
    for (; length; --length, ++start)
        RtlSetBit(map, start);
}

void WK_CALL RtlClearBits(PRTL_BITMAP map, ULONG start, ULONG length)
{
    for (; length; --length, ++start)
        RtlClearBit(map, start);
}

ULONG WK_CALL RtlNumberOfSetBitsInRange(PRTL_BITMAP map, ULONG start, ULONG length)
{
    ULONG count = 0;
    for (; length; --length, ++start)
        count += RtlTestBit(map, start);
    return count;
}

ULONG WK_CALL RtlNumberOfClearBitsInRange(PRTL_BITMAP map, ULONG start, ULONG length)
{
    return length - RtlNumberOfSetBitsInRange(map, start, length);
}

ULONG WK_CALL RtlNumberOfSetBits(PRTL_BITMAP map)
{
    return RtlNumberOfSetBitsInRange(map, 0, map->SizeOfBitMap);
}

ULONG WK_CALL RtlNumberOfClearBits(PRTL_BITMAP map)
{
    return map->SizeOfBitMap - RtlNumberOfSetBits(map);
}

static BOOLEAN all(PRTL_BITMAP map, ULONG start, ULONG length, BOOLEAN value)
{
    if (!length || start > map->SizeOfBitMap || length > map->SizeOfBitMap - start)
        return 0;
    for (; length; --length, ++start)
        if (RtlTestBit(map, start) != value)
            return 0;
    return 1;
}

BOOLEAN WK_CALL RtlAreBitsSet(PRTL_BITMAP map, ULONG start, ULONG length)
{
    return all(map, start, length, 1);
}

BOOLEAN WK_CALL RtlAreBitsClear(PRTL_BITMAP map, ULONG start, ULONG length)
{
    return all(map, start, length, 0);
}

static ULONG scan(PRTL_BITMAP map, ULONG start, ULONG stop, ULONG length, BOOLEAN value)
{
    ULONG i, matched = 0;
    for (i = start; i < stop; ++i) {
        matched = RtlTestBit(map, i) == value ? matched + 1 : 0;
        if (matched == length)
            return i - (length - 1);
    }
    return NO_BITS;
}

static ULONG find(PRTL_BITMAP map, ULONG length, ULONG hint, BOOLEAN value)
{
    ULONG found, stop;
    if (length > map->SizeOfBitMap)
        return NO_BITS;
    if (hint >= map->SizeOfBitMap)
        hint = 0;
    if (!length)
        return hint & ~(ULONG)7;
    found = scan(map, hint, map->SizeOfBitMap, length, value);
    if (found != NO_BITS || !hint)
        return found;
    /* A second-pass match may start before the hint and extend past it. */
    stop = hint + (length - 1 < map->SizeOfBitMap - hint ? length - 1 : map->SizeOfBitMap - hint);
    return scan(map, 0, stop, length, value);
}

ULONG WK_CALL RtlFindClearBits(PRTL_BITMAP map, ULONG length, ULONG hint)
{
    return find(map, length, hint, 0);
}

ULONG WK_CALL RtlFindSetBits(PRTL_BITMAP map, ULONG length, ULONG hint)
{
    return find(map, length, hint, 1);
}

ULONG WK_CALL RtlFindClearBitsAndSet(PRTL_BITMAP map, ULONG length, ULONG hint)
{
    ULONG index = RtlFindClearBits(map, length, hint);
    if (index != NO_BITS)
        RtlSetBits(map, index, length);
    return index;
}

ULONG WK_CALL RtlFindSetBitsAndClear(PRTL_BITMAP map, ULONG length, ULONG hint)
{
    ULONG index = RtlFindSetBits(map, length, hint);
    if (index != NO_BITS)
        RtlClearBits(map, index, length);
    return index;
}

ULONG WK_CALL RtlFindNextForwardRunClear(PRTL_BITMAP map, ULONG from, ULONG *start)
{
    ULONG i = from;
    while (i < map->SizeOfBitMap && RtlTestBit(map, i))
        ++i;
    *start = i;
    while (i < map->SizeOfBitMap && !RtlTestBit(map, i))
        ++i;
    return i - *start;
}

ULONG WK_CALL RtlFindLastBackwardRunClear(PRTL_BITMAP map, ULONG from, ULONG *start)
{
    ULONG i = from < map->SizeOfBitMap ? from + 1 : map->SizeOfBitMap;
    ULONG end;
    while (i && RtlTestBit(map, i - 1))
        --i;
    end = i;
    while (i && !RtlTestBit(map, i - 1))
        --i;
    *start = i;
    return end - i;
}

ULONG WK_CALL RtlFindFirstRunClear(PRTL_BITMAP map, ULONG *start)
{
    return RtlFindNextForwardRunClear(map, 0, start);
}

ULONG WK_CALL RtlFindClearRuns(PRTL_BITMAP map, PRTL_BITMAP_RUN runs,
                               ULONG capacity, BOOLEAN longest)
{
    ULONG count = 0, next = 0, start, length, pos;
    if (!capacity)
        return 0;
    while ((length = RtlFindNextForwardRunClear(map, next, &start)) != 0) {
        next = start + length;
        if (count == capacity && length <= runs[count - 1].NumberOfBits)
            continue;
        if (count < capacity)
            ++count;
        pos = count - 1;
        while (longest && pos && runs[pos - 1].NumberOfBits < length) {
            runs[pos] = runs[pos - 1];
            --pos;
        }
        runs[pos].StartingIndex = start;
        runs[pos].NumberOfBits = length;
        if (!longest && count == capacity)
            break;
    }
    return count;
}

ULONG WK_CALL RtlFindLongestRunClear(PRTL_BITMAP map, ULONG *start)
{
    RTL_BITMAP_RUN run = {0, 0};
    RtlFindClearRuns(map, &run, 1, 1);
    *start = run.StartingIndex;
    return run.NumberOfBits;
}

CCHAR WK_CALL RtlFindLeastSignificantBit(ULONGLONG value)
{
    CCHAR bit = 0;
    if (!value)
        return (CCHAR)-1;
    while (!(value & 1)) {
        value >>= 1;
        ++bit;
    }
    return bit;
}

CCHAR WK_CALL RtlFindMostSignificantBit(ULONGLONG value)
{
    CCHAR bit = -1;
    while (value) {
        ++bit;
        value >>= 1;
    }
    return bit;
}

ULONG WK_CALL RtlNumberOfSetBitsUlongPtr(ULONG_PTR value)
{
    ULONG count = 0;
    while (value) {
        value &= value - 1;
        ++count;
    }
    return count;
}
