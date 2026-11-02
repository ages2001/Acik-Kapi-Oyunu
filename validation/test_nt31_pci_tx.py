"""Exercise production NT3.1 C+ sends with delayed DMA and concurrent senders."""
from pathlib import Path
import re, subprocess, tempfile
src = Path(__file__).resolve().parents[1] / 'RLTKLANT/src'
text = (src / 'drvnt31.c').read_text()
def function(name):
    m = re.search(r'static \w+(?: STDC)? '+name+r'\([^;{]*\)\s*\{', text)
    assert m, name
    end = m.end(); depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}'); end += 1
    return text[m.start():end]
prefix = r'''
#define __stdcall
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include "ndisnt31.h"
#include "rltkhw.h"
#define MAXFRAME 1514
#define ETHLEN 14
#define N31_TXN 4
enum {BK_NE2000,BK_8139,BK_CPLUS,BK_USB};
typedef struct {
 PVOID UsbContext;ULONG TxLock,IoBase;UINT Backend,CpTxHead,TxCur;int Resetting,ResetFailed,RecoveryStop;
 RCP_DESC*TxDesc;UCHAR*CpTx[RCP_NUM_TX_DESC];ULONG CpTxP[RCP_NUM_TX_DESC];
 UCHAR*TxBuf[N31_TXN];
} Adapter,*PN31_ADAPTER;
typedef struct {PN31_ADAPTER Adapter;} Binding,*PN31_BINDING;
static Adapter adapter;static RCP_DESC ring[RCP_NUM_TX_DESC];
static UCHAR buffers[RCP_NUM_TX_DESC][RCP_RX_BUF_SIZE];
static unsigned bells,accepted,rejected,tsd[4];
static ULONG in32(ULONG reg){assert(adapter.TxLock&&reg>=R8139_TSD0&&reg<R8139_TSD0+16);return tsd[(reg-R8139_TSD0)/4];}
VOID KeAcquireSpinLock(PULONG l,PKIRQL old){while(__sync_lock_test_and_set(l,1))sched_yield();*old=0;}
VOID KeReleaseSpinLock(PULONG l,KIRQL old){assert(*l&&old==0);__sync_lock_release(l);}
VOID NdisQueryBuffer(PNDIS_BUFFER b,PVOID*va,PUINT n){*va=b->MappedSystemVa;*n=b->ByteCount;sched_yield();}
static int N31UsbSend(PVOID p,PUCHAR f,UINT n){(void)p;(void)f;(void)n;assert(0);return 0;}
static int cp_is25(PN31_ADAPTER a){(void)a;return 1;}
static void out32(ULONG reg,ULONG value){
 if(adapter.Backend==BK_8139){
  unsigned slot=(reg-R8139_TSD0)/4;assert(adapter.TxLock&&slot<4&&(tsd[slot]&R8139_TSD_OWN));
  assert((value&R8139_TSD_SIZE_MASK)==64);tsd[slot]=value&~R8139_TSD_OWN;bells++;return;
 }
 unsigned s=(adapter.CpTxHead-1)&(RCP_NUM_TX_DESC-1),i;
 assert(adapter.TxLock&&reg==RCP_TPPOLL_8125&&value==1);
 assert((ring[s].opts1&(RCP_DESC_OWN|RCP_DESC_FS|RCP_DESC_LS))==(RCP_DESC_OWN|RCP_DESC_FS|RCP_DESC_LS));
 assert((ring[s].opts1&RCP_DESC_FRAGLEN)==64&&ring[s].addr_lo==adapter.CpTxP[s]&&!ring[s].addr_hi&&!ring[s].opts2);
 for(i=1;i<64;i++)assert(buffers[s][i]==buffers[s][0]);
 bells++;sched_yield(); /* DMA deliberately retains OWN after the doorbell. */
}
static void out8(ULONG reg,ULONG value){(void)reg;(void)value;assert(0);}
'''
middle = r'''
static void hw_tx(PN31_ADAPTER a,UINT len){if(a->Backend==BK_8139)r8139_tx(a,len);else cplus_tx(a,len);}
'''
tests = r'''
static NDIS_STATUS send_byte(unsigned n){
 UCHAR frame[64];NDIS_BUFFER b={0};NDIS_PACKET p={0};Binding binding={&adapter};
 memset(frame,n,sizeof(frame));b.MappedSystemVa=frame;b.ByteCount=sizeof(frame);p.Private.Head=&b;
 return n31_send(&binding,&p);
}
static void*sender(void*arg){unsigned i;for(i=0;i<128;i++){
 NDIS_STATUS s=send_byte((unsigned)(ULONG_PTR)arg);
 if(s==NDIS_STATUS_SUCCESS)__sync_fetch_and_add(&accepted,1);
 else {assert(s==NDIS_STATUS_RESOURCES);__sync_fetch_and_add(&rejected,1);}
}return NULL;}
int main(void){unsigned round,i,j;pthread_t threads[8];
 for(round=0;round<100;round++){
  memset(&adapter,0,sizeof(adapter));memset(ring,0,sizeof(ring));memset(buffers,0xa5,sizeof(buffers));
  adapter.Backend=BK_CPLUS;adapter.TxDesc=ring;bells=accepted=rejected=0;
  for(i=0;i<RCP_NUM_TX_DESC;i++){adapter.CpTx[i]=buffers[i];adapter.CpTxP[i]=0x10000+i*0x1000;}
  for(i=0;i<8;i++)assert(!pthread_create(&threads[i],NULL,sender,(void*)(ULONG_PTR)(i+1)));
  for(i=0;i<8;i++)assert(!pthread_join(threads[i],NULL));
  assert(accepted==RCP_NUM_TX_DESC&&rejected==8*128-RCP_NUM_TX_DESC);
  assert(adapter.CpTxHead==RCP_NUM_TX_DESC&&bells==RCP_NUM_TX_DESC&&!adapter.TxLock);
  for(i=0;i<RCP_NUM_TX_DESC;i++){
   assert(buffers[i][0]>=1&&buffers[i][0]<=8);
   for(j=1;j<64;j++)assert(buffers[i][j]==buffers[i][0]);
  }
  /* Release exactly one hardware slot; it can be reused once, not twice. */
  ring[0].opts1&=~RCP_DESC_OWN;
  assert(send_byte(99)==NDIS_STATUS_SUCCESS&&adapter.CpTxHead==RCP_NUM_TX_DESC+1);
  assert(send_byte(100)==NDIS_STATUS_RESOURCES&&adapter.CpTxHead==RCP_NUM_TX_DESC+1);
  for(i=0;i<64;i++)assert(buffers[0][i]==99);
  ring[1].opts1&=~RCP_DESC_OWN;
  adapter.Resetting=1;assert(send_byte(100)==NDIS_STATUS_RESOURCES);adapter.Resetting=0;
  adapter.ResetFailed=1;assert(send_byte(100)==NDIS_STATUS_RESOURCES);adapter.ResetFailed=0;
  adapter.RecoveryStop=1;assert(send_byte(100)==NDIS_STATUS_RESOURCES);adapter.RecoveryStop=0;
  assert(adapter.CpTxHead==RCP_NUM_TX_DESC+1);
 }
 memset(&adapter,0,sizeof(adapter));adapter.Backend=BK_8139;bells=accepted=rejected=0;
 for(i=0;i<4;i++){adapter.TxBuf[i]=buffers[i];tsd[i]=R8139_TSD_OWN;}
 for(i=0;i<8;i++)assert(!pthread_create(&threads[i],NULL,sender,(void*)(ULONG_PTR)(i+1)));
 for(i=0;i<8;i++)assert(!pthread_join(threads[i],NULL));
 assert(accepted==4&&rejected==1020&&bells==4&&adapter.TxCur==4);
 UCHAR saved[64];memcpy(saved,buffers[0],64);
 assert(send_byte(77)==NDIS_STATUS_RESOURCES&&!memcmp(saved,buffers[0],64));
 tsd[0]=R8139_TSD_OWN|R8139_TSD_TOK;assert(send_byte(88)==NDIS_STATUS_SUCCESS&&adapter.TxCur==5);
 for(i=0;i<64;i++)assert(buffers[0][i]==88);
 tsd[1]=0xffffffffU;assert(send_byte(99)==NDIS_STATUS_RESOURCES&&adapter.TxCur==5);
 puts("PASS: RTL8139 four-slot delayed DMA/concurrent senders, single reuse, absent-device rejection; ");
 puts("PASS: NT3.1 C+ 100 concurrent schedules, delayed DMA ownership, full-ring backpressure and single-slot reuse");return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
    p=Path(d)/'test.c'; exe=Path(d)/'test'
    p.write_text(prefix+function('cplus_tx')+function('r8139_tx')+function('hw_txbuf')+middle+function('n31_send')+tests)
    subprocess.run(['gcc','-std=c99','-O2','-g','-Wall','-Wextra','-Werror','-pthread',
                    '-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(src),str(p),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
