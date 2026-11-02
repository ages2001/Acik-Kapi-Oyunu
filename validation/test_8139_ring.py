"""Production 8139 ring configuration, wrap payloads, and malformed RX at DPC."""
from pathlib import Path
import re, subprocess, tempfile
src = Path(__file__).resolve().parents[1]/'RLTKLANT/src'
def extract(file, name):
    s=(src/file).read_text();m=re.search(r'static VOID '+name+r'\([^;{]*\)\s*\{',s);assert m
    end=m.end();depth=1
    while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
    return s[m.start():end]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint32_t ULONG;typedef uint16_t USHORT;typedef uint8_t UCHAR,*PUCHAR;
typedef unsigned UINT;typedef void VOID;
#define TRUE 1
#define RLTK_HEADER_SIZE 14
#define RLTK_MAX_FRAME 1514
#define NDIS_PACKET_TYPE_DIRECTED 1
#define NDIS_PACKET_TYPE_MULTICAST 2
#define NDIS_PACKET_TYPE_ALL_MULTICAST 4
#define NDIS_PACKET_TYPE_BROADCAST 8
#define NDIS_PACKET_TYPE_PROMISCUOUS 32
#include "rltkhw.h"
typedef struct {unsigned PacketFilter,Filter,McastCount,IoBase;UCHAR McastList[32][6];
 UCHAR*RxRing;unsigned RxOffset,RcvErr,RxRecoveryNeeded;} Adapter,*PRLTK_ADAPTER,*PN31_ADAPTER;
static unsigned regs[256],empty=1,indications,completions,expected_len,seed;
static unsigned RtlCrc32Bits(UCHAR*p){(void)p;return 0;}
static void RtlWrite32(Adapter*a,unsigned r,unsigned v){(void)a;regs[r]=v;}
static void out32(unsigned r,unsigned v){regs[r]=v;}
static unsigned RtlRead8(Adapter*a,unsigned r){(void)a;assert(r==R8139_CR);return empty?1:0;}
static void RtlWrite16(Adapter*a,unsigned r,unsigned v){assert(r==R8139_CAPR);assert(v==(unsigned)(USHORT)(a->RxOffset-16));empty=1;}
static void RtlIndicateFrame(Adapter*a,UCHAR*p,unsigned n){(void)a;assert(n==expected_len);
 for(unsigned i=0;i<n;i++)assert(p[i]==(UCHAR)(i*29+seed));indications++;}
static void RtlIndicateComplete(Adapter*a){(void)a;completions++;}
'''
main=r'''
int main(void){
 UCHAR ring[32768+16+1536];Adapter a={.RxRing=ring};unsigned writer=0,wraps=0;
 R39SetRxMode(&a);assert((regs[0x44]&0x1800)==0x1000&&(regs[0x44]&0x80));
 r8139_setfilter(&a);assert((regs[0x44]&0x1800)==0x1000&&(regs[0x44]&0x80));
 assert(R8139_RXBUF_LEN==32768);
 for(unsigned n=0;n<10000;n++){
  unsigned len=60+(n*37)%1455;expected_len=len;seed=n;
  assert(a.RxOffset==writer);UCHAR*p=ring+writer;
  p[0]=1;p[1]=0;p[2]=(UCHAR)(len+4);p[3]=(UCHAR)((len+4)>>8);
  for(unsigned i=0;i<len;i++)p[4+i]=(UCHAR)(i*29+seed);
  if(writer+len+8>32768)wraps++;
  writer=((writer+len+8+3)&~3u)%32768;empty=0;
  R39Receive(&a);assert(empty&&a.RxOffset==writer&&!a.RxRecoveryNeeded);
 }
 assert(indications==10000&&wraps>100);
 unsigned before=indications,old=a.RxOffset;
 for(unsigned bad=0;bad<4;bad++){
  UCHAR*p=ring+old;unsigned len=bad==0?0:bad==1?17:bad==2?1519:64;
  p[0]=bad==3?0:1;p[1]=0;p[2]=(UCHAR)len;p[3]=(UCHAR)(len>>8);
  a.RxRecoveryNeeded=0;empty=0;R39Receive(&a);
  assert(a.RxRecoveryNeeded&&a.RxOffset==old&&indications==before);
 }
 ring[old+2]=0xf0;ring[old+3]=0xff;a.RxRecoveryNeeded=0;empty=0;
 R39Receive(&a);assert(!a.RxRecoveryNeeded&&indications==before);
 printf("PASS: 10000 exact payloads, %u ring wraps, both driver configurations select 32 KiB/linear slack; four malformed headers defer reset without DMA APIs; incomplete DMA waits\n",wraps);
}
'''
with tempfile.TemporaryDirectory(prefix='rltk-8139-ring-') as td:
    p=Path(td)/'test.c';exe=Path(td)/'test'
    p.write_text(prefix+extract('backend_8139.c','R39SetRxMode')+extract('drvnt31.c','r8139_setfilter')+extract('backend_8139.c','R39Receive')+main)
    subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-I',str(src),str(p),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
