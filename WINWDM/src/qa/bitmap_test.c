#include "wdm_bitmap.h"
#include <assert.h>
#include <stdio.h>

extern int WkCheck(void);
#define MISSING UINT32_MAX

static void check_runs(PRTL_BITMAP map, unsigned pattern, unsigned size)
{
    RTL_BITMAP_RUN expected[8], result[8], temp;
    unsigned i=0, n=0, first, j, cap, limit;
    ULONG index, length;
    while (i<size) {
        if ((pattern>>i)&1U) { ++i; continue; }
        first=i++;
        while (i<size && !((pattern>>i)&1U)) ++i;
        expected[n].StartingIndex=first;
        expected[n++].NumberOfBits=i-first;
    }
    for (i=0;i<=size;++i) {
        unsigned k=i;
        while (k<size && ((pattern>>k)&1U)) ++k;
        first=k;
        while (k<size && !((pattern>>k)&1U)) ++k;
        length=RtlFindNextForwardRunClear(map,i,&index);
        assert(length==k-first && (!length || first==index));
        if (i==size) break;
        k=i+1;
        while (k && ((pattern>>(k-1))&1U)) --k;
        limit=k;
        while (k && !((pattern>>(k-1))&1U)) --k;
        length=RtlFindLastBackwardRunClear(map,i,&index);
        assert(length==limit-k && (!length || k==index));
    }
    length=RtlFindFirstRunClear(map,&index);
    assert(n ? length==expected[0].NumberOfBits && index==expected[0].StartingIndex : !length);
    for (cap=0;cap<=5;++cap) {
        limit=cap<n ? cap : n;
        assert(RtlFindClearRuns(map,result,cap,0)==limit);
        for (i=0;i<limit;++i)
            assert(result[i].StartingIndex==expected[i].StartingIndex &&
                   result[i].NumberOfBits==expected[i].NumberOfBits);
    }
    for (i=0;i<n;++i) for (j=1;j<n-i;++j) {
        if (expected[j-1].NumberOfBits<expected[j].NumberOfBits) {
            temp=expected[j-1]; expected[j-1]=expected[j]; expected[j]=temp;
        }
    }
    length=RtlFindLongestRunClear(map,&index);
    assert(n ? length==expected[0].NumberOfBits && index==expected[0].StartingIndex : !length);
    for (cap=0;cap<=5;++cap) {
        limit=cap<n ? cap : n;
        assert(RtlFindClearRuns(map,result,cap,1)==limit);
        for (i=0;i<limit;++i)
            assert(result[i].StartingIndex==expected[i].StartingIndex &&
                   result[i].NumberOfBits==expected[i].NumberOfBits);
    }
}

/* Independent oracle using integer patterns, not the library's search. */
static ULONG expected(unsigned pattern, unsigned size, unsigned count,
                      unsigned hint, unsigned value)
{
    unsigned offset, bit;
    if (count > size) return MISSING;
    if (hint >= size) hint=0;
    if (!count) return hint & ~7U;
    for (offset=0;offset<size;++offset) {
        unsigned start=(hint+offset)%size;
        if (start+count>size) continue;
        for (bit=start;bit<start+count;++bit)
            if (((pattern>>bit)&1U)!=value) break;
        if (bit==start+count) return start;
    }
    return MISSING;
}

int main(void)
{
    RTL_BITMAP b;
    ULONG words[4]={0xa5a5a5a5U,0xffffffffU,0xffffffffU,0x12345678U};
    unsigned size, pattern, start, length, hint, k;
    unsigned long searches=0;
    assert(WkCheck());
    assert(RtlFindLeastSignificantBit(0)==-1 && RtlFindMostSignificantBit(0)==-1);
    for (start=0;start<64;++start) for (k=start;k<64;++k) {
        ULONGLONG v=(UINT64_C(1)<<start)|(UINT64_C(1)<<k);
        assert(RtlFindLeastSignificantBit(v)==(CCHAR)start);
        assert(RtlFindMostSignificantBit(v)==(CCHAR)k);
    }
    assert(RtlNumberOfSetBitsUlongPtr(~(ULONG_PTR)0)==8*sizeof(ULONG_PTR));
    RtlInitializeBitMap(&b,words+1,37);
    assert(RtlNumberOfSetBits(&b)==37);
    RtlClearAllBits(&b); RtlSetBit(&b,36); RtlSetBits(&b,29,4);
    assert(RtlTestBit(&b,36) && RtlNumberOfClearBits(&b)==32);
    RtlClearBit(&b,36); assert(!RtlTestBit(&b,36));
    RtlSetAllBits(&b); RtlClearBits(&b,29,4);
    assert(RtlNumberOfClearBitsInRange(&b,28,8)==4);
    assert(RtlAreBitsClear(&b,29,4) && !RtlAreBitsSet(&b,UINT32_MAX,4));
    assert(!RtlAreBitsClear(&b,1,UINT32_MAX));
    assert(words[0]==0xa5a5a5a5U && words[3]==0x12345678U);
    for (size=0;size<=8;++size) for (pattern=0;pattern<(1U<<size);++pattern) {
        ULONG word=pattern;
        RtlInitializeBitMap(&b,&word,size);
        check_runs(&b,pattern,size);
        for (start=0;start<=size;++start) for (length=0;length<=size-start;++length) {
            unsigned ones=0;
            for (k=start;k<start+length;++k) ones+=(pattern>>k)&1;
            assert(RtlNumberOfSetBitsInRange(&b,start,length)==ones);
            assert(RtlNumberOfClearBitsInRange(&b,start,length)==length-ones);
            assert(RtlAreBitsClear(&b,start,length)==(length!=0 && !ones));
            assert(RtlAreBitsSet(&b,start,length)==(length!=0 && ones==length));
        }
        for (length=0;length<=size+1;++length) for (hint=0;hint<=size+1;++hint) {
            ULONG first=expected(pattern,size,length,hint,0);
            ULONG set=expected(pattern,size,length,hint,1);
            ULONG mask=(1U<<length)-1;
            assert(RtlFindClearBits(&b,length,hint)==first);
            assert(RtlFindClearBitsAndSet(&b,length,hint)==first);
            assert(word==(first==MISSING ? pattern : pattern|(mask<<first)));
            word=pattern;
            assert(RtlFindSetBits(&b,length,hint)==set);
            assert(RtlFindSetBitsAndClear(&b,length,hint)==set);
            assert(word==(set==MISSING ? pattern : pattern&~(mask<<set)));
            word=pattern;
            searches+=2;
        }
    }
    printf("PASS: %lu reference searches, range counts, run enumeration, bit scans, updates and kernel self-test.\n", searches);
    return 0;
}
