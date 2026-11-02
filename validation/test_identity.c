#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define RLTKLANT_H
#include "../RLTKLANT/src/rtlchip.h"
typedef unsigned int UINT;
typedef int INT;
typedef uint32_t ULONG;
typedef uint16_t USHORT;
typedef uint8_t UCHAR;
#define VOID void
#define R8139_TCR_HWVERMASK 0x7cc00000UL
typedef struct { USHORT PciDeviceId; UCHAR Backend; char FriendlyName[128]; ULONG MaxSpeedMbit,Flags; } RLTK_ADAPTER,*PRLTK_ADAPTER;
#include "../RLTKLANT/src/hwdetect.c"
int main(void) {
 RLTK_ADAPTER a; unsigned i;
 memset(&a,0,sizeof(a));a.Backend=RTL_BACKEND_CPLUS;a.PciDeviceId=0x8136;
 RtlResolveName(&a,0x54100000);assert(a.MaxSpeedMbit==100);assert(a.Flags&RTL_F_FASTET);assert(strstr(a.FriendlyName,"RTL8107e"));
 RtlResolveName(&a,0x50900000);assert(a.MaxSpeedMbit==100);assert(strstr(a.FriendlyName,"RTL8106eus"));
 a.PciDeviceId=0x8168;RtlResolveName(&a,0x54100000);assert(a.MaxSpeedMbit==1000);assert(a.Flags&RTL_F_PHY_OCP);
 a.PciDeviceId=0x8127;RtlResolveName(&a,0x6c900000);assert(a.MaxSpeedMbit==10000);assert(a.Flags&RTL_F_8125REGS);assert(a.Flags&RTL_F_PHY_OCP);
 for(i=0;i<RTL_XID_COUNT;i++) assert((RtlXidTable[i].Id&RtlXidTable[i].Mask)==RtlXidTable[i].Id);
 puts("PASS: Fast-Ethernet PHY ceilings with shared Gigabit XIDs, OCP family flags, 10G identity, table masks");return 0;
}
