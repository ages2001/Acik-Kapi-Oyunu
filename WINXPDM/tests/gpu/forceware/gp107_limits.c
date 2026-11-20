/* Exercise the GTX 1050 / 1050 Ti GP107 architectural-limit dispatch.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
typedef unsigned int U32;
extern U32 __attribute__((stdcall)) gp107_chip_limit(void *unused,U32 selector);
void _start(void)
{
    static const unsigned char values[]={2,2,4,3,1,4,0,2,1,0,3,2};
    unsigned int i, rc=0;
    for (i=0;i<256;++i) {
        U32 expected=i>=0x11 && i<=0x1c ? values[i-0x11] : 0;
        if (gp107_chip_limit((void*)0,i)!=expected) { rc=1; break; }
    }
    __asm__ volatile("int $0x80"::"a"(1U),"b"(rc));
    __builtin_unreachable();
}
