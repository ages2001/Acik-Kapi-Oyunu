"""Exercise NT3.1 production PCI discovery against hostile config layouts."""
from pathlib import Path
import re, subprocess, tempfile
src=Path(__file__).resolve().parents[1]/'RLTKLANT/src'
code=(src/'drvnt31.c').read_text()
def extract(name):
    m=re.search(r'static \w+ '+name+r'\([^;{]*\)\s*\{',code); assert m
    end=m.end(); depth=1
    while depth:
        depth+=(code[end]=='{')-(code[end]=='}'); end+=1
    return code[m.start():end]
prefix=r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef uint32_t ULONG; typedef uint16_t USHORT; typedef unsigned UINT;
typedef uintptr_t ULONG_PTR;typedef void* PVOID;typedef void VOID;
typedef struct {uint32_t LowPart;int32_t HighPart;} NDIS_PHYSICAL_ADDRESS;
typedef int BOOLEAN;
#define TRUE 1
#define FALSE 0
#include "rtlchip.h"
enum {BK_CPLUS=2,BK_USB=3};
#define N31_MAX 8
typedef unsigned KIRQL;
static struct{ULONG Lock;}pollset,*g_poll=&pollset;
static void KeAcquireSpinLock(ULONG*p,KIRQL*old){assert(!*p);*p=1;*old=0;}
static void KeReleaseSpinLock(ULONG*p,KIRQL old){assert(*p&&!old);*p=0;}
typedef struct { ULONG IoBase,Irq,Bdf,MmioLength; PVOID MappedBase; USHORT DeviceId; UINT Backend; } Adapter,*PN31_ADAPTER;
static unsigned mapped,map_fail,unmapped;
static PVOID MmMapIoSpace(NDIS_PHYSICAL_ADDRESS address,ULONG length,int cache){
 assert(address.LowPart==0xfeb00000&&!address.HighPart&&length==0x8000&&!cache);
 if(map_fail)return NULL;
 mapped++;return (PVOID)(uintptr_t)0xf0000000;
}
static VOID MmUnmapIoSpace(PVOID pointer,ULONG length){assert(pointer==(PVOID)(uintptr_t)0xf0000000&&length==0x8000&&mapped);mapped--;unmapped++;}
static Adapter *adapters[8],**g_ap=adapters;
static UINT n;
static void dbg(const char *s){(void)s;} static void dbgh(ULONG v){(void)v;}
typedef struct { UINT bus,dev,fn; ULONG cfg[16], masks[6]; unsigned probe[6]; } Device;
static Device devices[8]; static UINT writes;
static Device *find(UINT b,UINT d,UINT f){UINT i;for(i=0;i<8;i++)if(devices[i].bus==b&&devices[i].dev==d&&devices[i].fn==f&&devices[i].cfg[0])return &devices[i];return NULL;}
static ULONG pci_cfg(UINT b,UINT d,UINT f,UINT o){Device *p=find(b,d,f);if(!p)return UINT32_MAX;
 if(o>=0x10&&o<=0x24&&p->probe[(o-0x10)/4]){
  unsigned bar=(o-0x10)/4;
  if(p->masks[bar])return p->masks[bar];
  if(bar&&(p->cfg[3+bar]&7)==4)return UINT32_MAX;
  return 0xffff8000U|(p->cfg[4+bar]&15);
 }
 return p->cfg[o/4];}
static void pci_cfg_w(UINT b,UINT d,UINT f,UINT o,ULONG v){Device *p=find(b,d,f);assert(p);
 if(o==4){assert(!(v&0xffff0000));p->cfg[1]=v;writes++;}
 else {unsigned bar=(o-0x10)/4;assert(o>=0x10&&o<=0x24&&!(p->cfg[1]&7));
  if(v==UINT32_MAX)p->probe[bar]=1;
  else{p->probe[bar]=0;p->cfg[o/4]=v;}
 }}
static void setup(UINT i,UINT b,UINT d,UINT f,ULONG id,ULONG port){devices[i]=(Device){.bus=b,.dev=d,.fn=f};devices[i].cfg[0]=id;devices[i].cfg[1]=0xf9000002;devices[i].cfg[2]=0x02000000;devices[i].cfg[4]=port;devices[i].cfg[15]=11;}
'''
tests=r'''
int main(void){
 Adapter a={0},b={0};
 setup(0,0,2,0,0x999910ec,0xc001); /* unknown NIC */
 setup(1,0,3,0,0x812710ec,0xfeb00004); /* MMIO64 upper half looks like IO */
 devices[1].cfg[5]=0xc001;
 setup(2,0,4,0,0x812710ec,0xffffffff); /* absent/unassigned */
 setup(3,0,5,0,0x812710ec,0x10001); /* port address truncation */
 setup(4,255,31,0,0x12348086,0);devices[4].cfg[3]=0x00800000;
 setup(5,255,31,7,0x812710ec,0);devices[5].cfg[6]=0xc001;
 setup(6,0,6,0,0x812710ec,0xc101);devices[6].cfg[2]=0x02010000; /* non-Ethernet */
 setup(7,0,7,0,0x812710ec,1); /* zero base */
 assert(pci_find(&a));assert(a.Bdf==0xff1f07&&a.IoBase==0xc000&&a.Backend==2&&a.DeviceId==0x8127);
 adapters[n++]=&a;assert(!pci_find(&b));
 devices[0].cfg[0]=0x813910ec;
 assert(pci_find(&b)&&b.Bdf==0x200&&b.Backend==1);
 devices[4].cfg[3]=0;n=0;memset(adapters,0,sizeof(adapters));devices[0].cfg[0]=0x999910ec;
 assert(!pci_find(&b)); /* no function 7 when single-function */
 memset(devices,0,sizeof(devices));memset(&b,0,sizeof(b));n=0;memset(adapters,0,sizeof(adapters));
 setup(0,0,2,0,0x812710ec,0xfeb00004); /* below-4GB MMIO64, no I/O BAR */
 assert(pci_find(&b)&&b.IoBase==0xf0000000&&b.MmioLength==0x8000&&mapped==1);
 n31_unmap(&b);assert(!mapped&&unmapped==1&&!b.IoBase&&!b.MappedBase&&!b.MmioLength);
 n31_unmap(&b);assert(unmapped==1);
 map_fail=1;assert(!pci_find(&b)&&!mapped);
 devices[0].cfg[6]=0xc001; /* mapping failure can use an actual I/O BAR */
 assert(pci_find(&b)&&b.IoBase==0xc000&&!b.MmioLength);
 n31_unmap(&b);assert(unmapped==1);
 memset(&b,0,sizeof(b));map_fail=0;
 assert(pci_find(&b)&&b.MmioLength&&mapped==1); /* prefer MMIO */
 n31_unmap(&b);
 devices[0].cfg[6]=0;devices[0].cfg[5]=1; /* above-4GB MMIO is unsupported */
 assert(!pci_find(&b)&&!mapped);
 devices[0].cfg[4]=0;devices[0].cfg[5]=0;devices[0].cfg[9]=0xfeb00004;
 assert(!pci_find(&b)&&!mapped); /* incomplete 64-bit BAR5 */
 /* Probe guards: restored 64-bit BAR and command on every invalid mask. */
 memset(devices,0,sizeof(devices));setup(0,0,2,0,0x812710ec,0xfeb00004);
 assert(pci_memory_length(0,2,0,0)==0x8000);
 assert(devices[0].cfg[1]==2&&devices[0].cfg[4]==0xfeb00004&&!devices[0].cfg[5]);
 assert(!devices[0].probe[0]&&!devices[0].probe[1]);
 devices[0].masks[0]=0xfffff004;assert(pci_memory_length(0,2,0,0)==0x1000);
 devices[0].masks[0]=0xffffa004;assert(!pci_memory_length(0,2,0,0)); /* noncontiguous mask */
 devices[0].masks[0]=0xffff8004;devices[0].masks[1]=0xfffffffe;assert(!pci_memory_length(0,2,0,0)); /* aperture exceeds 4GB */
 assert(devices[0].cfg[1]==2&&devices[0].cfg[4]==0xfeb00004&&!devices[0].cfg[5]);
 assert(!devices[0].probe[0]&&!devices[0].probe[1]);
 devices[0].masks[1]=0xffffffff;devices[0].cfg[4]=0xfeb00104;assert(!pci_memory_length(0,2,0,0)); /* misaligned base */
 devices[0].cfg[4]=0xfeb00000;devices[0].masks[0]=0xffff8000;assert(pci_memory_length(0,2,0,0)==0x8000); /* 32-bit BAR */
 devices[0].masks[0]=0xfffffff0;assert(!pci_memory_length(0,2,0,0)); /* too short */
 assert(!pci_memory_length(0,2,0,6));
 puts("PASS: NT3.1 PCI multifunction/claims/class/W1C, MMIO-only and preferred mapping, map failure/fallback, unmap and 64-bit BAR guards");
}
'''
with tempfile.TemporaryDirectory() as d:
    p=Path(d)/'test.c'; p.write_text(prefix+'\n'.join(extract(n) for n in ['rtl_backend_for','bdf_claimed','pci_memory_length','pci_find','n31_unmap'])+tests)
    exe=Path(d)/'test'
    subprocess.run(['gcc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(src),str(p),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
