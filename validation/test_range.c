#include <stdint.h>
#include <stdio.h>
#ifndef TEST_BITS
#define TEST_BITS 32
#endif
#if TEST_BITS == 32
typedef uint32_t ULONG_PTR;
#else
typedef uint64_t ULONG_PTR;
#endif
typedef uint32_t ULONG;
typedef int BOOLEAN;
#define TRUE 1
#define FALSE 0
#include "../RLTKLANT/src/rtlrange.h"
int main(void) {
 ULONG_PTR first,last,max=~(ULONG_PTR)0;
 if (RtlRangeBounds(max-3,0xC0,&first,&last)) return 1;
 if (RtlRangeBounds(0x80001000,0,&first,&last)) return 2;
 if (!RtlRangeBounds(0x80001FF0,32,&first,&last) || first!=0x80001000 || last!=0x80002000) return 3;
 if (!RtlRangeBounds(max-3,4,&first,&last) || first!=(max&~(ULONG_PTR)0xFFF) || last!=first) return 4;
 printf("PASS: %d-bit range boundaries, overflow and empty ranges\n",TEST_BITS);
 return 0;
}
