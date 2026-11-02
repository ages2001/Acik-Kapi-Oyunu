"""Exercise actual HCD teardown with live-DMA/endpoint guards and partial allocations."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]/'USBNT/src'
def extract(s,name):
 m=re.search(r'static (?:int|void) '+name+r'\([^;{]*\)\s*\{',s);assert m,name
 end=m.end();depth=1
 while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[m.start():end]
common=r'''
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
typedef uint32_t ULONG;typedef uint16_t USHORT;typedef unsigned char UCHAR;
#define USB_OK 0
#define USB_ERR_IO -1
#define USB_ERR_BUSY -2
#define USB_PAGE_SIZE 4096
#define EOP_USBSTS 1
#define XOP_USBSTS 1
#define OR_CONTROL 1
#define UR_USBSTS 1
#define STS_HALTED 0x20
#define STS_HCH 0x20
#define CTL_HCFS_MASK 0xc0
#define CTL_HCFS_RESET 0
#define RING_MAX_SEGS 4
#define SEG_TRBS 256
#define EVENT_TRBS 256
#define XHCI_MAX_SLOTS 8
static ULONG reg;
#define OpRd(h,r) (reg)
#define Rd(h,r) (reg)
#define Rd16(h,r) ((USHORT)reg)
typedef struct {void*Priv;int Running;} USB_HC;
static int blocks;
static void*allocate(size_t size){size_t*p=calloc(1,size+sizeof(size_t));assert(p);*p=size;blocks++;return p+1;}
static void release(void*ptr,size_t size){size_t*p;if(!ptr)return;p=(size_t*)ptr-1;assert(blocks>0&&(!size||*p==size));blocks--;free(p);}
static void OsFree(void*p){release(p,0);}
static void UsbDmaFree(void*p,ULONG s){release(p,s);}
static void OsDmaFree(void*p,ULONG s){release(p,s);}
'''
parts={
'Uhci':(r'''
typedef struct {void*Next,*Qh;} UQH;
typedef struct {void*IsoList;UQH Skel[3];void*Frames,*IsoFirst;} UHC;
#define UHCP(h) ((UHC*)(h)->Priv)
''',r'''UHC*p=allocate(sizeof(*p));h.Priv=p;for(i=0;i<3;i++)if(full||i<iteration%4)p->Skel[i].Qh=allocate(32);if(full||iteration%2)p->Frames=allocate(4096);if(full||iteration%3)p->IsoFirst=allocate(1024*sizeof(void*));''',r'''p->IsoList=p;assert(UhciRelease(&h)==USB_ERR_BUSY&&blocks==before);p->IsoList=NULL;p->Skel[1].Next=p;assert(UhciRelease(&h)==USB_ERR_BUSY&&blocks==before);p->Skel[1].Next=NULL;''','',0x20),
'Ohci':(r'''
typedef struct {void*Next,*Ed;} OED;
typedef struct {void*IsoList;OED Head[4];void*Hcca;} OHC;
#define OHCP(h) ((OHC*)(h)->Priv)
''',r'''OHC*p=allocate(sizeof(*p));h.Priv=p;for(i=0;i<4;i++)if(full||i<iteration%5)p->Head[i].Ed=allocate(32);if(full||iteration%2)p->Hcca=allocate(256);''',r'''p->IsoList=p;assert(OhciRelease(&h)==USB_ERR_BUSY&&blocks==before);p->IsoList=NULL;p->Head[2].Next=p;assert(OhciRelease(&h)==USB_ERR_BUSY&&blocks==before);p->Head[2].Next=NULL;''','',0),
'Ehci':(r'''
#define QH_SIZE 128
#define TD_SIZE 64
typedef struct {void*Next,*Qh,*Td[4];UCHAR NTd;} EQH;
typedef struct {void*IsoList;EQH AsyncHead,IntHead;void*Frames,*IsoFirst;} EHC;
#define EHCP(h) ((EHC*)(h)->Priv)
''',r'''EHC*p=allocate(sizeof(*p));h.Priv=p;if(full||iteration%2){p->AsyncHead.Qh=allocate(128);p->AsyncHead.NTd=2;for(i=0;i<2;i++)p->AsyncHead.Td[i]=allocate(64);}if(full||iteration%3)p->IntHead.Qh=allocate(128);if(full||iteration%2)p->Frames=allocate(4096);if(full||iteration%4)p->IsoFirst=allocate(1024*sizeof(void*));''',r'''p->IsoList=p;assert(EhciRelease(&h)==USB_ERR_BUSY&&blocks==before);p->IsoList=NULL;p->IntHead.Next=p;assert(EhciRelease(&h)==USB_ERR_BUSY&&blocks==before);p->IntHead.Next=NULL;''','QhFree',0x20),
'Xhci':(r'''
typedef struct {void*Seg[4],*Owner,*Off;ULONG NSeg;} XRING;
typedef struct {ULONG CmdBusy,ScratchCount;void*Slots[9];XRING Cmd;void*Evt,*Erst,*Dcbaa,*ScratchArr;void**ScratchBuf;} XHC;
#define XHCP(h) ((XHC*)(h)->Priv)
''',r'''XHC*p=allocate(sizeof(*p));h.Priv=p;for(i=0;i<4;i++)if(full||i<iteration%5)p->Cmd.Seg[i]=allocate(4096);if(full||iteration%2){p->Cmd.Owner=allocate(40);p->Cmd.Off=allocate(40);}if(full||iteration%2)p->Evt=allocate(4096);if(full||iteration%3)p->Erst=allocate(64);if(full||iteration%4)p->Dcbaa=allocate(2048);p->ScratchCount=iteration==1?600:3;if(full||iteration%2){unsigned size=p->ScratchCount*8;p->ScratchArr=allocate(size<64?64:size);}if(full||iteration%3){p->ScratchBuf=allocate(p->ScratchCount*sizeof(void*));for(i=0;i<(int)p->ScratchCount;i++)if(full||i<iteration%4)p->ScratchBuf[i]=allocate(4096);}''',r'''p->CmdBusy=1;assert(XhciRelease(&h)==USB_ERR_BUSY&&blocks==before);p->CmdBusy=0;p->Slots[2]=p;assert(XhciRelease(&h)==USB_ERR_BUSY&&blocks==before);p->Slots[2]=NULL;''','RingFree',0x20)}
with tempfile.TemporaryDirectory() as temp:
 for name,(types,setup,guard,helper,halted) in parts.items():
  source=(root/('hc'+name[0].lower()+'hci.c')).read_text();actual=(extract(source,helper) if helper else '')+extract(source,name+'Release')
  main='''int main(void){USB_HC h={0};int iteration,i,before,full;
assert(NAMERelease(&h)==USB_OK);
for(iteration=0;iteration<1000;iteration++){full=iteration%2;SETUP
before=blocks;reg=HALTED;h.Running=1;assert(NAMERelease(&h)==USB_ERR_IO&&blocks==before&&h.Priv==p);h.Running=0;
reg=0xffffffff;assert(NAMERelease(&h)==USB_ERR_IO&&blocks==before&&h.Priv==p);
reg=BADREG;assert(NAMERelease(&h)==USB_ERR_IO&&blocks==before&&h.Priv==p);
reg=HALTED;GUARD
assert(NAMERelease(&h)==USB_OK&&!h.Priv&&!blocks);assert(NAMERelease(&h)==USB_OK&&!blocks);
}
printf("PASS: NAME 1000 full/partial cleanup cycles, halted-DMA and live-endpoint guards\\n");return 0;}'''
  for k,val in [('SETUP',setup),('GUARD',guard),('HALTED',str(halted)),('BADREG',str(0x80 if name=='Ohci' else 0)),('NAME',name)]:main=main.replace(k,val)
  p=Path(temp)/(name+'.c');exe=Path(temp)/name;p.write_text(common+types+actual+main)
  subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(p),'-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True)
