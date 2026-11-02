"""Production legacy PCI discovery/claim/BAR routines with a simulated HAL."""
from pathlib import Path
import re, subprocess, tempfile
src=Path(__file__).resolve().parents[1]/'RLTKLANT/src'
code=(src/'init.c').read_text()
def function(text,name):
    m=re.search(r'(?:static )?\w+ '+name+r'\([^;{]*\)\s*\{',text);assert m,name
    pos=m.end();depth=1
    while depth:
        depth+=(text[pos]=='{')-(text[pos]=='}');pos+=1
    return text[m.start():pos]
functions='\n'.join(function(code,n) for n in ['RtlSlotClaimed','RtlClaimSlot',
    'RtlReleaseSlot','PciConfigRead32','PciConfigWrite32','PciReadBar','RtlSupported','RtlFindLegacySlot'])
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint32_t ULONG,*PULONG;
typedef uint16_t USHORT;
typedef unsigned UINT;
typedef int BOOLEAN;
typedef void *PVOID;
#define VOID void
#define TRUE 1
#define FALSE 0
#include "rtlchip.h"
typedef struct { ULONG PciSlot; } Adapter,*PRLTK_ADAPTER;
static ULONG g_ClaimedSlots[2048];
static int g_SlotLock,g_SlotLockReady;
#define RTL_SLOT_TAG(bus,slot) (((bus)<<8)|(slot))
static void NdisAcquireSpinLock(int *p){(void)p;}
static void NdisReleaseSpinLock(int *p){(void)p;}
static const RTL_PCI_ID *RtlFindPciId(USHORT dev){UINT i;for(i=0;i<RTL_PCI_ID_COUNT;i++)if(RtlPciIds[i].DeviceId==dev)return &RtlPciIds[i];return NULL;}
typedef struct { ULONG bus,slot,id,header,bar[6],mask[6]; } Device;
static Device devices[5];
static unsigned writes;
static Device *lookup(ULONG bus,ULONG slot){unsigned i;for(i=0;i<5;i++)if(devices[i].bus==bus&&devices[i].slot==slot)return &devices[i];return NULL;}
static ULONG HalGetBusDataByOffset(ULONG type,ULONG bus,ULONG slot,PVOID buffer,ULONG off,ULONG size){
 Device *d;ULONG v=0;assert(type==4&&bus<256&&slot<256&&size==4);d=lookup(bus,slot);if(!d)return 0;
 if(off==0)v=d->id;else if(off==12)v=d->header;else if(off>=16&&off<40){unsigned n=(off-16)/4;v=d->bar[n]==UINT32_MAX?d->mask[n]:d->bar[n];}
 memcpy(buffer,&v,4);return 4;
}
static ULONG HalSetBusDataByOffset(ULONG type,ULONG bus,ULONG slot,PVOID buffer,ULONG off,ULONG size){
 Device *d=lookup(bus,slot);assert(type==4&&d&&size==4&&off>=16&&off<40);memcpy(&d->bar[(off-16)/4],buffer,4);writes++;return 4;
}
'''
tests=r'''
int main(void){
 const ULONG buses[]={0,2,2,255},slots[]={5,5,101,255};unsigned i;ULONG bus,slot,base,len;BOOLEAN io;Adapter a;
 devices[0]=(Device){.bus=0,.slot=5,.id=0x813910ec};
 devices[1]=(Device){.bus=2,.slot=5,.id=0x812710ec,.header=0x00800000};
 devices[2]=(Device){.bus=2,.slot=101,.id=0x812710ec};
 devices[3]=(Device){.bus=255,.slot=31,.id=0x12348086,.header=0x00800000};
 devices[4]=(Device){.bus=255,.slot=255,.id=0x812710ec};
 for(i=0;i<4;i++){
  bus=slot=UINT32_MAX;assert(RtlFindLegacySlot(&bus,&slot));assert(bus==buses[i]&&slot==slots[i]);
  RtlClaimSlot(bus,slot);assert(RtlSlotClaimed(bus,slot));
 }
 bus=slot=UINT32_MAX;assert(!RtlFindLegacySlot(&bus,&slot));assert(!writes);
 a.PciSlot=RTL_SLOT_TAG(2,5)+1;RtlReleaseSlot(&a);assert(!a.PciSlot);
 assert(!RtlSlotClaimed(2,5)&&RtlSlotClaimed(0,5)&&RtlSlotClaimed(2,101));
 bus=slot=UINT32_MAX;assert(RtlFindLegacySlot(&bus,&slot)&&bus==2&&slot==5);
 a.PciSlot=65536;RtlReleaseSlot(&a);assert(!RtlSlotClaimed(255,255));
 bus=255;slot=255;assert(RtlFindLegacySlot(&bus,&slot)&&bus==255&&slot==255);
 devices[1].bar[0]=0xc001;devices[1].mask[0]=0xffffff01;
 PciReadBar(2,5,16,&base,&len,&io);assert(io&&base==0xc000&&len==256&&devices[1].bar[0]==0xc001);
 devices[1].bar[2]=0xfeb00004;devices[1].mask[2]=0xfffff004;
 PciReadBar(2,5,24,&base,&len,&io);assert(!io&&base==0xfeb00000&&len==4096&&devices[1].bar[2]==0xfeb00004);
 devices[1].bar[3]=1;writes=0;PciReadBar(2,5,24,&base,&len,&io);assert(!base&&!len&&!writes);
 puts("PASS: production HAL PCI discovery across buses/functions, independent claims, high BDF bounds, BAR sizing/restoration and >4GB rejection");return 0;
}
'''
with tempfile.TemporaryDirectory() as td:
    td=Path(td);unit=td/'pci.c';exe=td/'pci'
    unit.write_text(prefix+functions+tests)
    subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                    '-I'+str(src),str(unit),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
