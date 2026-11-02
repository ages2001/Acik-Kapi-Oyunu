"""Exercise the production NT3.1 receive loop with hostile DMA headers."""
from pathlib import Path
import re, subprocess, tempfile
src=Path(__file__).resolve().parents[1]/'RLTKLANT/src'
s=(src/'drvnt31.c').read_text()
m=re.search(r'static VOID r8139_poll_rx\([^;{]*\)\s*\{',s)
end=m.end();depth=1
while depth:
    depth+=(s[end]=='{')-(s[end]=='}');end+=1
code=r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef uint32_t ULONG;typedef uint16_t USHORT;typedef uint8_t UCHAR,*PUCHAR;
typedef char* PCHAR;typedef unsigned UINT;typedef void VOID,*NDIS_HANDLE;
#include "rltkhw.h"
#define ETHLEN 14
#define MAXFRAME 1514
typedef struct {ULONG IoBase,RxOffset;PUCHAR RxRing;UCHAR LastFrame[1514];UINT LastLen;void*FilterDb;} Adapter,*PN31_ADAPTER;
static unsigned empty,reads,acks,recoveries,indications,expected,seed,persistent;
static unsigned in8(unsigned port){assert(port==R8139_CR);reads++;return empty;}
static unsigned in16(unsigned port){assert(port==R8139_ISR);return 1;}
static void out16(unsigned port,unsigned value){(void)value;if(port==R8139_CAPR){acks++;if(!persistent)empty=1;}else assert(port==R8139_ISR);}
static void n31_queue_rx_recovery(Adapter*a){(void)a;recoveries++;}
static void EthFilterIndicateReceive(void*f,void*a,char*ctx,UCHAR*h,unsigned hn,UCHAR*p,unsigned look,unsigned total){
 (void)a;assert(f&&ctx==(char*)h&&hn==14&&look==total&&look+14==expected&&p==h+14);
 for(unsigned i=0;i<expected;i++)assert(h[i]==(UCHAR)(i*29+seed));indications++;
}
'''
tests=r'''
int main(void){UCHAR ring[32768+16+2048];Adapter a={.RxRing=ring,.FilterDb=&ring};unsigned writer=0,wraps=0;
 for(unsigned n=0;n<10000;n++){
  unsigned len=60+(n*37)%1455;expected=len;seed=n;UCHAR*p=ring+writer;
  assert(a.RxOffset==writer);p[0]=1;p[1]=0;p[2]=(UCHAR)(len+4);p[3]=(UCHAR)((len+4)>>8);
  for(unsigned i=0;i<len;i++)p[4+i]=(UCHAR)(i*29+seed);
  if(writer+len+8>32768)wraps++;writer=((writer+len+11)&~3u)%32768;empty=0;
  r8139_poll_rx(&a);assert(a.RxOffset==writer&&empty&&!recoveries);
 }
 assert(indications==10000&&wraps>100);
 unsigned old=a.RxOffset,before=acks;
 for(unsigned i=0;i<5;i++){
  unsigned len=i==0?0:i==1?17:i==2?1519:i==3?65535:64;UCHAR*p=ring+old;
  p[0]=i==4?0:1;p[1]=0;p[2]=(UCHAR)len;p[3]=(UCHAR)(len>>8);empty=0;
  r8139_poll_rx(&a);assert(recoveries==i+1&&a.RxOffset==old&&acks==before&&indications==10000);
 }
 ring[old+2]=0xf0;ring[old+3]=0xff;r8139_poll_rx(&a);assert(recoveries==5&&acks==before);
 a.RxOffset=65535;r8139_poll_rx(&a);assert(recoveries==6&&acks==before);
 /* A hostile continuously nonempty producer cannot monopolize a DPC. */
 memset(ring,0,sizeof(ring));a.RxOffset=0;expected=60;seed=0;
 for(unsigned off=0;off<64*68;off+=68){ring[off]=1;ring[off+2]=64;for(unsigned i=0;i<60;i++)ring[off+4+i]=(UCHAR)(i*29);}
 persistent=1;empty=0;reads=0;r8139_poll_rx(&a);
 assert(reads==64&&acks==before+64&&indications==10064&&recoveries==6);
 printf("PASS: NT3.1 10000 exact payloads/%u ring wraps; five malformed headers, out-of-range offset and incomplete DMA; bounded 64-frame DPC; no untrusted pointer advance\n",wraps);
}
'''
with tempfile.TemporaryDirectory(prefix='rltk-nt31-rx-') as td:
    p=Path(td)/'test.c';exe=Path(td)/'test'
    p.write_text(code+s[m.start():end]+tests)
    subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-I',str(src),str(p),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
