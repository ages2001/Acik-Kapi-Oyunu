"""Fault-inject each production NT3.1 add allocation and registration stage."""
from pathlib import Path
import re,subprocess,tempfile
src=Path(__file__).resolve().parents[1]/'RLTKLANT/src'
text=(src/'drvnt31.c').read_text()
def function(name):
 m=re.search(r'static \w+(?: STDC)? '+name+r'\([^;{]*\)\s*\{',text);assert m,name
 end=m.end();depth=1
 while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[m.start():end]
struct=text[text.index('typedef struct _N31_ADAPTER'):text.index('#include "nt31poll.h"')]
struct=struct.replace('static NDIS_STATUS n31_queue_reset(PN31_BINDING binding);','')
struct+=text[text.index('typedef struct _N31_POLL_SET'):text.index('/* One reference owns the driver lifetime;')]
prefix=r'''
#define __stdcall
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ndisnt31.h"
#include "rltkhw.h"
#define N31_TXN 4
#define N31_MAX 8
#define MAXFRAME 1514
enum { BK_NE2000=0,BK_8139=1,BK_CPLUS=2,BK_USB=3 };
'''
shim=r'''
static PN31_ADAPTER adapters[N31_MAX],*g_ap=adapters;
static UINT n,*g_apN=&n;
static N31_POLL_SET pollset,*g_poll=&pollset;
VOID KeAcquireSpinLock(PULONG p,PKIRQL old){assert(p==&g_poll->Lock&&!*p);*p=1;*old=0;}
VOID KeReleaseSpinLock(PULONG p,KIRQL old){assert(p==&g_poll->Lock&&*p&&!old);*p=0;}
VOID EthDeleteFilter(NDIS_HANDLE h){assert(h==(PVOID)0x55);}
static NDIS_HANDLE g_mac=(PVOID)0x1234;
static PVOID allocations[512];
static unsigned alloc_calls,fail_alloc,live,backend,stage,registered,hardware,timers,filters,stops,usb_live,mappings;
static unsigned stop_fails;
static void dbg(const char *s){(void)s;}static void dbgh(ULONG v){(void)v;}
static PVOID dma_alloc(ULONG len,PULONG phys){
 PVOID p;alloc_calls++;*phys=0;
 if(alloc_calls==fail_alloc)return NULL;
 p=malloc(len);assert(p);assert(alloc_calls<512);allocations[alloc_calls]=p;live++;
 *phys=0x100007; /* force descriptor pointers to be interior aligned pointers */
 return p;
}
VOID MmFreeContiguousMemory(PVOID p){
 unsigned i;assert(!registered);assert(p);
 assert(!hardware||(stop_fails&&p==allocations[1]));
 for(i=1;i<512;i++)if(allocations[i]==p){allocations[i]=NULL;live--;free(p);return;}
 assert(!"free of an interior pointer, unknown pointer or double free");
}
VOID KeInitializeEvent(PVOID p,int type,BOOLEAN state){(void)p;(void)type;(void)state;}
static BOOLEAN pci_find(PN31_ADAPTER a){
 a->Backend=backend;a->IoBase=0xc000;
 if(backend==BK_CPLUS){assert(!mappings);mappings=1;a->MmioLength=0x100;a->MappedBase=(PVOID)0xc000;}
 return stage!=1;
}
static BOOLEAN hw_init(PN31_ADAPTER a){hardware=a->Backend!=BK_USB;return stage!=3&&(a->Backend!=BK_CPLUS||stage!=6);}
static BOOLEAN n31_start_recovery(PN31_ADAPTER a){(void)a;return TRUE;}
static VOID n31_stop_recovery(PN31_ADAPTER a){(void)a;}
static VOID n31_unmap(PN31_ADAPTER a){
 assert((!hardware||stop_fails)&&!registered);
 if(a->MmioLength){assert(mappings==1);mappings=0;a->MmioLength=0;a->MappedBase=NULL;}
}
static int N31UsbCreate(PVOID cfg,PVOID owner,PVOID *ctx,PUCHAR mac){
 (void)cfg;(void)owner;(void)mac;*ctx=NULL;if(backend!=BK_USB)return 0;
 if(stage==5)return -1;
 usb_live=1;*ctx=(PVOID)1;return 1;
}
static VOID N31UsbReset(PVOID ctx){assert(ctx==(PVOID)1&&usb_live);stops++;}
static VOID N31UsbDestroy(PVOID ctx){assert(ctx==(PVOID)1&&usb_live&&!registered);usb_live=0;}

static BOOLEAN cplus_reset(PN31_ADAPTER a){(void)a;stops++;if(stop_fails)return FALSE;hardware=0;return TRUE;}
enum { RtlMacHardwareRelease=2 };
static BOOLEAN n31_mac_hardware(PN31_ADAPTER a,int phase){
 assert(phase==RtlMacHardwareRelease&&!hardware&&a->Backend==BK_CPLUS);
 a->PciManagementOwned=FALSE;return TRUE;
}
static BOOLEAN r8139_reset(PN31_ADAPTER a){return cplus_reset(a);}
static BOOLEAN cp_is25(PN31_ADAPTER a){(void)a;return TRUE;}
static VOID out32(ULONG p,ULONG v){assert(p==0xc000+RCP_IMR0_8125&&!v);}
static VOID out16(ULONG p,USHORT v){assert(p==0xc000+R8139_IMR&&!v);}
static VOID out8(ULONG p,UCHAR v){
 if(p==0xc000+R8139_CR||p==0xc000+NE_CR){hardware=0;stops++;}
 (void)v;
}
NDIS_STATUS NdisRegisterAdapter(PNDIS_HANDLE handle,NDIS_HANDLE mac,NDIS_HANDLE adapter,NDIS_HANDLE cfg,PVOID name,PVOID info){
 (void)cfg;(void)name;(void)info;assert(mac==g_mac&&!registered);
 if(stage==2)return NDIS_STATUS_FAILURE;
 registered=1;*handle=adapter;return NDIS_STATUS_SUCCESS;
}
NDIS_STATUS NdisDeregisterAdapter(NDIS_HANDLE handle){assert(handle&&registered&&(!hardware||stop_fails));registered=0;return NDIS_STATUS_SUCCESS;}
static VOID publish_mac(PN31_ADAPTER a,PNDIS_STRING name){(void)a;(void)name;}
static VOID addr_change(void){}static VOID filt_change(void){}static VOID close_action(void){}
#define cp(f) (f)
BOOLEAN EthCreateFilter(UINT count,ETH_ADDRESS_CHANGE change,ETH_FILTER_CHANGE filter,ETH_DEFERRED_CLOSE close,PUCHAR mac,PNDIS_SPIN_LOCK lock,PNDIS_HANDLE handle){
 (void)count;(void)change;(void)filter;(void)close;(void)mac;(void)lock;
 filters++;if(stage==4)return FALSE;*handle=(PVOID)0x55;return TRUE;
}
VOID NdisInitializeTimer(PNDIS_TIMER t,NDIS_TIMER_FUNCTION f,PVOID ctx){(void)t;(void)f;assert(ctx);}
static void n31_image_get(void){}
VOID NdisSetTimer(PNDIS_TIMER t,UINT ms){(void)t;assert(ms==100);timers++;}
static void reset(void){
 assert(!live&&!registered&&!hardware&&!usb_live&&!mappings);memset(allocations,0,sizeof(allocations));memset(adapters,0,sizeof(adapters));memset(&pollset,0,sizeof(pollset));
 n=alloc_calls=fail_alloc=stage=timers=filters=stops=stop_fails=0;
}
'''
tests=r'''
int main(void){
 unsigned b,k,count,total=0;
 for(b=0;b<4;b++){
  reset();backend=b;
  assert(n31_add(0,0,0)==NDIS_STATUS_SUCCESS&&n==1&&timers==1&&filters==1&&registered&&(hardware==(b!=BK_USB)));
  assert(adapters[0]->Running);count=alloc_calls;
  assert(count==(b==BK_USB?1:b==BK_CPLUS?3+RCP_NUM_TX_DESC+RCP_NUM_RX_DESC:2+N31_TXN));
  n31_stop_hw(adapters[0]);NdisDeregisterAdapter(adapters[0]);if(b==BK_USB)N31UsbDestroy(adapters[0]->UsbContext);n31_free_dma(adapters[0]);n31_unmap(adapters[0]);MmFreeContiguousMemory(adapters[0]);assert(!live&&!mappings);
  for(k=1;k<=count;k++){
   reset();fail_alloc=k;
   assert(n31_add(0,0,0)!=NDIS_STATUS_SUCCESS&&alloc_calls==k&&!live&&!n&&!timers&&!registered&&!hardware);total++;
  }
  for(k=1;k<=6;k++){
   if(k==6&&b!=BK_CPLUS)continue;
   if((b!=BK_USB&&k==5)||(b==BK_USB&&(k==1||k==3)))continue;
   reset();stage=k;
   assert(n31_add(0,0,0)!=NDIS_STATUS_SUCCESS&&!live&&!n&&!timers&&!registered&&!hardware);
   assert(stops==((k==3||k==4||k==6)?1:0));assert(!usb_live);total++;
  }
 }
 for(b=BK_8139;b<=BK_CPLUS;b++)for(k=3;k<=4;k++){
  unsigned j;reset();backend=b;stage=k;stop_fails=1;
  assert(n31_add(0,0,0)!=NDIS_STATUS_SUCCESS&&!registered&&!n&&!timers&&!mappings);
  assert(hardware&&stops==1&&live==alloc_calls-1&&!allocations[1]);
  /* Harness-only power-off: retained DMA can now be reclaimed safely. */
  hardware=0;for(j=2;j<512;j++)if(allocations[j])MmFreeContiguousMemory(allocations[j]);
  assert(!live);total++;
 }
 reset();n=N31_MAX;assert(n31_add(0,0,0)==NDIS_STATUS_RESOURCES&&!alloc_calls&&!live);
 printf("PASS: NT3.1 %u failed-start schedules, three PCI backends and the USB bridge, original DMA frees, stop/deregister before free, no failed-adapter publication\n",total);
}
'''
with tempfile.TemporaryDirectory() as d:
 c=Path(d)/'test.c';exe=Path(d)/'test'
 c.write_text(prefix+struct+shim+function('n31_free_dma')+function('n31_stop_hw')+function('n31_publish')+function('n31_add')+tests)
 subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-Wno-cast-function-type','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(src),str(c),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
