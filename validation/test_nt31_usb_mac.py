"""Production NT3.1 MAC send/reset/RX routing with USB and PCI fixtures."""
from pathlib import Path
import re,subprocess,tempfile
src=Path(__file__).resolve().parents[1]/'RLTKLANT/src';text=(src/'drvnt31.c').read_text()
def function(name):
 m=re.search(r'(?:static )?\w+(?: STDC)? '+name+r'\([^;{]*\)\s*\{',text);assert m,name
 end=m.end();depth=1
 while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[m.start():end]
prefix=r'''
#define __stdcall
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "ndisnt31.h"
#define MAXFRAME 1514
#define ETHLEN 14
typedef struct {PVOID UsbContext,FilterDb;UCHAR LastFrame[1514];UINT LastLen;ULONG TxLock;} Adapter,*PN31_ADAPTER;
typedef struct {PN31_ADAPTER Adapter;} Binding,*PN31_BINDING;
static unsigned sent,len,reset,received,pci;static int room=1;
static UCHAR output[1514];
VOID KeAcquireSpinLock(PULONG l,PKIRQL old){assert(!*l);*l=1;*old=0;}
VOID KeReleaseSpinLock(PULONG l,KIRQL old){assert(*l&&old==0);*l=0;}
VOID NdisQueryBuffer(PNDIS_BUFFER b,PVOID*va,PUINT n){*va=b->MappedSystemVa;*n=b->ByteCount;}
static int N31UsbSend(PVOID p,PUCHAR frame,UINT n){assert(p==(PVOID)1);if(!room)return 0;memcpy(output,frame,n);sent++;len=n;return 1;}
static VOID N31UsbReset(PVOID p){assert(p==(PVOID)1);reset++;}
static BOOLEAN hw_init(PN31_ADAPTER a){assert(!a->UsbContext);pci++;return TRUE;}
static PUCHAR hw_txbuf(PN31_ADAPTER a){assert(!a->UsbContext&&a->TxLock);return room?output:NULL;}
static VOID hw_tx(PN31_ADAPTER a,UINT n){assert(!a->UsbContext);pci++;sent++;len=n;}
VOID EthFilterIndicateReceive(NDIS_HANDLE f,NDIS_HANDLE a,PCHAR dest,PVOID h,UINT hn,PVOID d,UINT look,UINT total){
 Adapter*owner=a;assert(f==(PVOID)2&&dest==(PCHAR)owner->LastFrame&&h==dest&&hn==14&&d==owner->LastFrame+14&&look==total);received++;
}
'''
tests=r'''
int main(void){
 Adapter a={.UsbContext=(PVOID)1,.FilterDb=(PVOID)2};Binding b={&a};NDIS_PACKET p={0};NDIS_BUFFER x={0},y={0};UCHAR frame[1514];unsigned n,i;
 for(i=0;i<sizeof(frame);i++)frame[i]=(UCHAR)(i*31+7);p.Private.Head=&x;x.Next=&y;
 for(n=14;n<=1514;n++){
  x.MappedSystemVa=frame;x.ByteCount=n/2;y.MappedSystemVa=frame+n/2;y.ByteCount=n-n/2;
  assert(n31_send(&b,&p)==NDIS_STATUS_SUCCESS);assert(len==(n<60?60:n)&&!memcmp(output,frame,n));
  for(i=n;i<len;i++)assert(!output[i]);
 }
 i=sent;x.ByteCount=1514;y.ByteCount=1;assert(n31_send(&b,&p)==NDIS_STATUS_FAILURE&&sent==i);
 x.ByteCount=14;x.MappedSystemVa=NULL;y.ByteCount=0;assert(n31_send(&b,&p)==NDIS_STATUS_FAILURE&&sent==i);
 x.MappedSystemVa=frame;x.ByteCount=13;assert(n31_send(&b,&p)==NDIS_STATUS_FAILURE&&sent==i);
 x.ByteCount=64;room=0;assert(n31_send(&b,&p)==NDIS_STATUS_RESOURCES&&sent==i);room=1;
 assert(n31_reset(&b)==NDIS_STATUS_SUCCESS&&reset==1&&!pci);
 N31UsbReceive(&a,frame,1514);assert(received==1&&a.LastLen==1514&&!memcmp(a.LastFrame,frame,1514));
 N31UsbReceive(&a,frame,13);N31UsbReceive(&a,frame,1515);assert(received==1);
 a.FilterDb=NULL;N31UsbReceive(&a,frame,64);assert(received==1&&a.LastLen==64);
 a.UsbContext=NULL;assert(n31_send(&b,&p)==NDIS_STATUS_SUCCESS&&pci==1&&len==64);
 room=0;i=sent;assert(n31_send(&b,&p)==NDIS_STATUS_RESOURCES&&sent==i&&!a.TxLock);room=1;
 assert(n31_reset(&b)==NDIS_STATUS_SUCCESS&&pci==2);
 puts("PASS: NT3.1 USB MAC 1501 split-buffer lengths, padding, malformed/oversize/resource failure, reset and RX indication; PCI dispatch retained");return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';exe=Path(d)/'test';p.write_text(prefix+'\n'.join(function(n) for n in ['n31_send','n31_reset','N31UsbReceive'])+tests)
 subprocess.run(['gcc','-std=c99','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(src),str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
