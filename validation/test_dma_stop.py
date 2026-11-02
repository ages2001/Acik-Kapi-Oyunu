"""Fault-inject production C+ reset completion before any DMA free."""
from pathlib import Path
import re,subprocess,tempfile
src=Path(__file__).resolve().parents[1]/'RLTKLANT/src'
text=(src/'backend_cplus.c').read_text()
def fn(name):
    text=(src/('backend_8139.c' if name.startswith('R39') else 'backend_cplus.c')).read_text()
    m=re.search(r'static \w+ '+name+r'\([^;{]*\)\s*\{',text);assert m
    end=m.end();depth=1
    while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[m.start():end]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
typedef uint32_t ULONG;typedef uint16_t USHORT;typedef uint8_t UCHAR;
typedef unsigned UINT;typedef int BOOLEAN;typedef void VOID;
#define TRUE 1
#define FALSE 0
#include "rltkhw.h"
#define RX_ALLOC (R8139_RXBUF_LEN+16+1536)
#define TX_ALLOC 1536
#define CP_TX_RING_BYTES (RCP_NUM_TX_DESC*sizeof(RCP_DESC))
#define CP_RX_RING_BYTES (RCP_NUM_RX_DESC*sizeof(RCP_DESC))
typedef struct {void*MiniportHandle;PRCP_DESC TxDesc,RxDesc;unsigned TxDescPhys,RxDescPhys;
 void*CpTxBuf[RCP_NUM_TX_DESC],*CpRxBuf[RCP_NUM_RX_DESC];
 unsigned CpTxBufPhys[RCP_NUM_TX_DESC],CpRxBufPhys[RCP_NUM_RX_DESC];
 void*RxRing,*TxBuf[R8139_TX_DESCS];unsigned RxRingPhys,TxBufPhys[R8139_TX_DESCS];
} Adapter,*PRLTK_ADAPTER;
static unsigned reads,waits,release_after,mode,freed,live,resets,masked,confirmed;
static void CpImr(Adapter*a,ULONG n){(void)a;assert(!n);masked++;}
static void RtlWrite16(Adapter*a,unsigned reg,USHORT value){(void)a;assert(reg==R8139_IMR&&!value);masked++;}
static void RtlWrite8(Adapter*a,unsigned reg,UCHAR value){
 (void)a;assert(masked&&reg==RCP_CR&&value==RCP_CR_RST);resets++;reads=waits=confirmed=0;
}
static UCHAR RtlRead8(Adapter*a,unsigned reg){
 (void)a;assert(reg==RCP_CR&&resets);reads++;
 if(mode==1)return 0xff;
 if(reads<=release_after)return RCP_CR_RST;
 if(mode==2)return RCP_CR_TE;
 if(mode==3)return RCP_CR_RE;
 confirmed=1;return 0;
}
static void NdisStallExecution(unsigned us){assert(us==10);waits++;}
static void NdisMFreeSharedMemory(void*h,unsigned size,int cached,void*p,unsigned physical){
 (void)h;(void)size;(void)cached;(void)physical;assert(confirmed&&p&&live);free(p);freed++;live--;
}
static void *alloc(void){void*p=malloc(64);assert(p);live++;return p;}
static void seed(Adapter*a,unsigned count,unsigned backend){unsigned i;memset(a,0,sizeof(*a));
 if(backend){if(!count--)return;a->RxRing=alloc();for(i=0;i<R8139_TX_DESCS;i++){if(!count--)return;a->TxBuf[i]=alloc();}return;}
 if(count--){a->TxDesc=alloc();}else return;
 if(count--){a->RxDesc=alloc();}else return;
 for(i=0;i<RCP_NUM_TX_DESC;i++){if(!count--)return;a->CpTxBuf[i]=alloc();}
 for(i=0;i<RCP_NUM_RX_DESC;i++){if(!count--)return;a->CpRxBuf[i]=alloc();}
}
'''
main=r'''
int main(void){Adapter a;unsigned count,m,delay,backend,tests=0;
 for(backend=0;backend<2;backend++)for(count=0;count<=(backend?1+R8139_TX_DESCS:2+RCP_NUM_TX_DESC+RCP_NUM_RX_DESC);count++){
  for(m=0;m<4;m++)for(delay=0;delay<3;delay++){
   seed(&a,count,backend);mode=m;release_after=delay==0?0:delay==1?999:1000;freed=masked=0;
   if(backend)R39Halt(&a);else CpHalt(&a);
   if(!mode&&release_after<1000){assert(freed==count&&!live&&reads==release_after+1);}
   else{assert(!freed&&live==count);assert(reads==(mode==1?1:1000));
    mode=release_after=0;if(backend)R39Halt(&a);else CpHalt(&a);assert(freed==count&&!live);
   }
   assert(!a.TxDesc&&!a.RxDesc);tests++;
  }
 }
 printf("PASS: %u partial/full DMA allocations across delayed reset, timeout, absent bus, live TX/RX and later recovery; no free before acknowledgement\n",tests);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as td:
    p=Path(td)/'test.c';exe=Path(td)/'test'
    p.write_text(prefix+fn('CpReset')+fn('CpFreeDma')+fn('CpHalt')+fn('R39Reset')+fn('R39FreeDma')+fn('R39Halt')+main)
    subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(src),str(p),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
