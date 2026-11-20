#ifndef WDM_KIT_BITMAP_H
#define WDM_KIT_BITMAP_H

/* Rebuild the consumer with this header. Wk-prefixed symbols keep native
 * kernel imports available to other translation units in the same driver. */
#include <stdint.h>
#ifdef WK_NATIVE_TYPES
#include <wdm.h>
#endif
#if defined(__i386__) && defined(__GNUC__)
#define WK_CALL __attribute__((stdcall))
#elif defined(_M_IX86)
#define WK_CALL __stdcall
#else
#define WK_CALL
#endif

#ifndef WK_NATIVE_TYPES
typedef uint32_t ULONG;
typedef uint8_t BOOLEAN;
typedef uintptr_t ULONG_PTR;
typedef uint64_t ULONGLONG;
typedef char CCHAR;
typedef struct _RTL_BITMAP {
    ULONG SizeOfBitMap;
    ULONG *Buffer;
} RTL_BITMAP, *PRTL_BITMAP;
typedef struct _RTL_BITMAP_RUN {
    ULONG StartingIndex;
    ULONG NumberOfBits;
} RTL_BITMAP_RUN, *PRTL_BITMAP_RUN;
#endif

#define RtlInitializeBitMap       WkRtlInitializeBitMap
#define RtlClearAllBits           WkRtlClearAllBits
#define RtlSetAllBits             WkRtlSetAllBits
#define RtlClearBit               WkRtlClearBit
#define RtlSetBit                 WkRtlSetBit
#define RtlTestBit                WkRtlTestBit
#define RtlClearBits              WkRtlClearBits
#define RtlSetBits                WkRtlSetBits
#define RtlAreBitsClear           WkRtlAreBitsClear
#define RtlAreBitsSet             WkRtlAreBitsSet
#define RtlNumberOfSetBits        WkRtlNumberOfSetBits
#define RtlNumberOfClearBits      WkRtlNumberOfClearBits
#define RtlNumberOfSetBitsInRange WkRtlNumberOfSetBitsInRange
#define RtlNumberOfClearBitsInRange WkRtlNumberOfClearBitsInRange
#define RtlFindSetBits            WkRtlFindSetBits
#define RtlFindClearBits          WkRtlFindClearBits
#define RtlFindClearBitsAndSet    WkRtlFindClearBitsAndSet
#define RtlFindSetBitsAndClear    WkRtlFindSetBitsAndClear
#define RtlFindNextForwardRunClear  WkRtlFindNextForwardRunClear
#define RtlFindLastBackwardRunClear WkRtlFindLastBackwardRunClear
#define RtlFindFirstRunClear        WkRtlFindFirstRunClear
#define RtlFindLongestRunClear      WkRtlFindLongestRunClear
#define RtlFindClearRuns            WkRtlFindClearRuns
#define RtlFindLeastSignificantBit  WkRtlFindLeastSignificantBit
#define RtlFindMostSignificantBit   WkRtlFindMostSignificantBit
#define RtlNumberOfSetBitsUlongPtr  WkRtlNumberOfSetBitsUlongPtr
#ifndef RtlCheckBit
#define RtlCheckBit(map, index) RtlTestBit((map), (index))
#endif

#ifdef __cplusplus
extern "C" {
#endif
void WK_CALL RtlInitializeBitMap(PRTL_BITMAP, ULONG *, ULONG);
void WK_CALL RtlClearAllBits(PRTL_BITMAP);
void WK_CALL RtlSetAllBits(PRTL_BITMAP);
void WK_CALL RtlClearBit(PRTL_BITMAP, ULONG);
void WK_CALL RtlSetBit(PRTL_BITMAP, ULONG);
BOOLEAN WK_CALL RtlTestBit(PRTL_BITMAP, ULONG);
void WK_CALL RtlClearBits(PRTL_BITMAP, ULONG, ULONG);
void WK_CALL RtlSetBits(PRTL_BITMAP, ULONG, ULONG);
BOOLEAN WK_CALL RtlAreBitsClear(PRTL_BITMAP, ULONG, ULONG);
BOOLEAN WK_CALL RtlAreBitsSet(PRTL_BITMAP, ULONG, ULONG);
ULONG WK_CALL RtlNumberOfSetBits(PRTL_BITMAP);
ULONG WK_CALL RtlNumberOfClearBits(PRTL_BITMAP);
ULONG WK_CALL RtlNumberOfSetBitsInRange(PRTL_BITMAP, ULONG, ULONG);
ULONG WK_CALL RtlNumberOfClearBitsInRange(PRTL_BITMAP, ULONG, ULONG);
ULONG WK_CALL RtlFindSetBits(PRTL_BITMAP, ULONG, ULONG);
ULONG WK_CALL RtlFindClearBits(PRTL_BITMAP, ULONG, ULONG);
ULONG WK_CALL RtlFindClearBitsAndSet(PRTL_BITMAP, ULONG, ULONG);
ULONG WK_CALL RtlFindSetBitsAndClear(PRTL_BITMAP, ULONG, ULONG);
ULONG WK_CALL RtlFindNextForwardRunClear(PRTL_BITMAP, ULONG, ULONG *);
ULONG WK_CALL RtlFindLastBackwardRunClear(PRTL_BITMAP, ULONG, ULONG *);
ULONG WK_CALL RtlFindFirstRunClear(PRTL_BITMAP, ULONG *);
ULONG WK_CALL RtlFindLongestRunClear(PRTL_BITMAP, ULONG *);
ULONG WK_CALL RtlFindClearRuns(PRTL_BITMAP, PRTL_BITMAP_RUN, ULONG, BOOLEAN);
CCHAR WK_CALL RtlFindLeastSignificantBit(ULONGLONG);
CCHAR WK_CALL RtlFindMostSignificantBit(ULONGLONG);
ULONG WK_CALL RtlNumberOfSetBitsUlongPtr(ULONG_PTR);

#ifdef __cplusplus
}
#endif

#endif
