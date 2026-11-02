"""Exercise production miniport flatten/send with fragmented and invalid MDLs."""
from pathlib import Path
import re, subprocess, tempfile

root=Path(__file__).resolve().parents[1]
text=(root/'RLTKLANT/src/datapath.c').read_text()
def extract(name):
    m=re.search(r'^(?:static UINT|NDIS_STATUS STDC) '+name+r'\([^;{]*\)\s*\{',text,re.M);assert m
    end=m.end();depth=1
    while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[m.start():end]
prefix=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef unsigned UINT;typedef unsigned char UCHAR,*PUCHAR;typedef void* PVOID;typedef void* NDIS_HANDLE;typedef int NDIS_STATUS;
#define STDC
#define TRUE 1
#define RLTK_MAX_FRAME 1514
#define RLTK_HEADER_SIZE 14
#define NDIS_STATUS_INVALID_DATA 1
#define NDIS_STATUS_RESOURCES 2
#define NDIS_STATUS_SUCCESS 0
#define NdisZeroMemory memset_zero
#define NdisMoveMemory memcpy
static void memset_zero(void*p,unsigned n){memset(p,0,n);}
typedef struct Buffer{struct Buffer*Next;void*Address;unsigned Length;}Buffer,*PNDIS_BUFFER;
typedef struct{struct{PNDIS_BUFFER Head;}Private;}Packet,*PNDIS_PACKET;
typedef struct Adapter Adapter,*PRLTK_ADAPTER;
typedef struct{int(*Transmit)(Adapter*,unsigned char*,unsigned);}Ops;
struct Adapter{Ops*Ops;unsigned XmitErr,XmitOk,TxStalled,Halting,ResetPending,ResetFailed,RxRecoveryNeeded,Lock;};
static void NdisAcquireSpinLock(unsigned*l){assert(!*l);*l=1;}
static void NdisReleaseSpinLock(unsigned*l){assert(*l);*l=0;}
static void NdisQueryBuffer(Buffer*b,void**p,unsigned*n){*p=b->Address;*n=b->Length;}
static Buffer*RtlBufNext(Buffer*b){return b->Next;}
static unsigned calls,written,busy;static unsigned char received[1514];
static int transmit(Adapter*a,unsigned char*p,unsigned n){(void)a;calls++;assert(n<=sizeof(received));memcpy(received,p,n);written=n;return !busy;}
'''
tests=r'''
int main(void){
 unsigned split,i;unsigned char bytes[1515];Ops ops={transmit};Adapter a={.Ops=&ops};
 for(i=0;i<sizeof(bytes);i++)bytes[i]=(unsigned char)(i*29+7);
 for(split=0;split<=1514;split++){
  Buffer tail={0,bytes+split,1514-split},zero={&tail,0,0},head={&zero,bytes,split};Packet p={{&head}};
  calls=0;assert(RtlSend(&a,&p,0)==NDIS_STATUS_SUCCESS&&calls==1&&written==1514&&!memcmp(received,bytes,1514));
 }
 for(split=0;split<=1515;split++){
  Buffer tail={0,bytes+split,1515-split},head={&tail,bytes,split};Packet p={{&head}};
  calls=0;assert(RtlSend(&a,&p,0)==NDIS_STATUS_INVALID_DATA&&calls==0);
 }
 for(i=0;i<14;i++){
  Buffer b={0,bytes,i};Packet p={{&b}};calls=0;assert(RtlSend(&a,&p,0)==NDIS_STATUS_INVALID_DATA&&!calls);
 }
 for(i=14;i<60;i++){
  Buffer b={0,bytes,i};Packet p={{&b}};calls=0;assert(RtlSend(&a,&p,0)==NDIS_STATUS_SUCCESS&&calls==1&&written==60&&!memcmp(received,bytes,i));
  for(unsigned n=i;n<60;n++)assert(!received[n]);
 }
 {Buffer bad={0,0,1},head={&bad,bytes,100};Packet p={{&head}};calls=0;assert(RtlSend(&a,&p,0)==NDIS_STATUS_INVALID_DATA&&!calls);}
 {Buffer empty={0,0,0},head={&empty,bytes,1514};Packet p={{&head}};calls=0;assert(RtlSend(&a,&p,0)==NDIS_STATUS_SUCCESS&&calls==1&&written==1514);}
 {Buffer b={0,bytes,60};Packet p={{&b}};unsigned old=a.XmitOk;busy=1;calls=0;assert(RtlSend(&a,&p,0)==NDIS_STATUS_RESOURCES&&calls==1&&a.TxStalled&&a.XmitOk==old);}
 {Buffer b={0,bytes,60};Packet p={{&b}};busy=0;
  for(i=1;i<16;i++){a.Halting=i&1;a.ResetPending=i&2;a.ResetFailed=i&4;a.RxRecoveryNeeded=i&8;calls=0;
   assert(RtlSend(&a,&p,0)==NDIS_STATUS_RESOURCES&&!calls&&!a.Lock);}}
 printf("PASS: 1515 exact-frame splits, 1516 overlength splits, short/null MDLs, zero-length tails, deterministic padding and backend backpressure\n");
}
'''
with tempfile.TemporaryDirectory(prefix='rltk-send-check-') as td:
    p=Path(td)/'test.c';p.write_text(prefix+extract('RtlFlatten')+'\n'+extract('RtlSend')+tests)
    exe=Path(td)/'test'
    subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(p),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
