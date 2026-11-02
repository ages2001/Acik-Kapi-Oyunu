"""Exercise production NT3.1 protocol binding/filter ownership and close paths.
The fake filter models the NT3.1 exported contract, including close during RX.
It is not an emulation or replacement for the guest NDIS filter package.
"""
from pathlib import Path
import re, subprocess, tempfile
src=Path(__file__).resolve().parents[1]/'RLTKLANT/src'
text=(src/'drvnt31.c').read_text()
def function(name):
 m=re.search(r'(?:static )?\w+(?: STDC)?\s+'+name+r'\([^;{]*\)\s*\{',text);assert m,name
 end=m.end();depth=1
 while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[m.start():end]
structs=text[text.index('typedef struct _N31_ADAPTER'):text.index('#include "nt31poll.h"')]
prefix=r'''
#define __stdcall
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ndisnt31.h"
#include "ndisvendor.h"
#include "rltkhw.h"
#define MAXFRAME 1514
#define N31_TXN 4
#define dbg(x) ((void)0)
#define dbgh(x) ((void)0)
'''
stubs=r'''
typedef struct {PN31_BINDING Owner;NDIS_HANDLE Ndis;UINT Filter,References,Multicast;int Closing;} FakeOpen;
static FakeOpen* opens[32];static unsigned allocated,completed,hwupdates;static int fail_alloc,fail_open;
static PN31_ADAPTER locked;static NDIS_HANDLE last_completed;
static void hw_setfilter(PN31_ADAPTER a){assert(locked==a);hwupdates++;}
static const char*rtl_name_for(USHORT id){(void)id;return "Realtek";}
static UINT rtl_strlen(const char*s){return strlen(s);}
static void N31UsbName(PVOID p,char*s,UINT n){(void)p;assert(n>=8);strcpy(s,"USB NIC");}
VOID KeAcquireSpinLock(PULONG lock,PKIRQL old){assert(!locked);locked=(PN31_ADAPTER)((char*)lock-__builtin_offsetof(N31_ADAPTER,Lock));*old=2;}
VOID KeReleaseSpinLock(PULONG lock,KIRQL old){assert(locked&&(void*)lock==(void*)&locked->Lock&&old==2);locked=NULL;}
NDIS_STATUS NdisAllocateMemory(PVOID*out,UINT n,UINT flags,NDIS_PHYSICAL_ADDRESS max){(void)flags;(void)max;if(fail_alloc)return NDIS_STATUS_RESOURCES;*out=calloc(1,n);assert(*out);allocated++;return 0;}
VOID NdisFreeMemory(PVOID p,UINT n,UINT flags){(void)n;(void)flags;assert(p&&allocated);allocated--;free(p);}
VOID NdisCompleteCloseAdapter(NDIS_HANDLE h,NDIS_STATUS s){assert(!locked&&s==0);last_completed=h;completed++;}
BOOLEAN EthNoteFilterOpenAdapter(NDIS_HANDLE db,NDIS_HANDLE owner,NDIS_HANDLE ndis,PNDIS_HANDLE handle){
 unsigned i;assert(locked==db);if(fail_open)return FALSE;
 for(i=0;i<32;i++)if(!opens[i]){FakeOpen*f=calloc(1,sizeof(*f));assert(f);f->Owner=owner;f->Ndis=ndis;f->References=1;opens[i]=f;*handle=f;return TRUE;}return FALSE;
}
static NDIS_STATUS STDC filt_change(UINT,UINT,NDIS_HANDLE,PNDIS_REQUEST,BOOLEAN);
NDIS_STATUS EthFilterAdjust(NDIS_HANDLE db,NDIS_HANDLE handle,PNDIS_REQUEST r,UINT filter,BOOLEAN set){
 FakeOpen*f=handle;UINT all=0,i,old=((PN31_ADAPTER)db)->Filter;assert(locked==db&&f&&!f->Closing);f->Filter=filter;
 for(i=0;i<32;i++)if(opens[i]&&opens[i]->Owner->Adapter==db)all|=opens[i]->Filter;
 return filt_change(old,all,f->Owner,r,set);
}
NDIS_STATUS EthChangeFilterAddresses(NDIS_HANDLE db,NDIS_HANDLE handle,PNDIS_REQUEST r,UINT count,PVOID addresses,BOOLEAN set){
 FakeOpen*f=handle;(void)r;(void)addresses;(void)set;assert(locked==db&&count<=16);f->Multicast=count;return 0;
}
static void drop_filter(FakeOpen*f){unsigned i;for(i=0;i<32;i++)if(opens[i]==f){opens[i]=NULL;free(f);return;}assert(0);}
NDIS_STATUS EthDeleteFilterOpenAdapter(NDIS_HANDLE db,NDIS_HANDLE handle,PNDIS_REQUEST r){
 FakeOpen*f=handle;assert(locked==db);EthFilterAdjust(db,handle,r,0,FALSE);f->Closing=1;
 if(--f->References)return NDIS_STATUS_CLOSING_INDICATING;drop_filter(f);return 0;
}
'''
tests=r'''
static NDIS_STATUS set_filter(PN31_BINDING b,ULONG value,UINT length){NDIS_REQUEST r={0};r.RequestType=NdisRequestSetInformation;r.DATA.SET_INFORMATION.Oid=OID_GEN_CURRENT_PACKET_FILTER;r.DATA.SET_INFORMATION.InformationBuffer=&value;r.DATA.SET_INFORMATION.InformationBufferLength=length;return n31_request(b,&r);}
int main(void){
 N31_ADAPTER a={0};NDIS_MEDIUM medium=NdisMedium802_3;UINT selected;NDIS_STATUS error;NDIS_HANDLE x,y;unsigned i;
 a.FilterDb=&a;
 for(i=0;i<200;i++){
  assert(n31_open(&error,&x,&selected,&medium,1,(PVOID)1,&a,0,NULL)==0&&error==0);
  assert(n31_open(&error,&y,&selected,&medium,1,(PVOID)2,&a,0,NULL)==0&&x!=y&&allocated==2);
  PN31_BINDING b=x,c=y;FakeOpen*f=b->FilterHandle,*g=c->FilterHandle;
  /* The OID carries the permanent address bytes, not the PCI vendor ID.
     Verify the actual request path, byte order, and short-buffer guard. */
  {UCHAR value[4]={0xcc,0xcc,0xcc,0xcc};NDIS_REQUEST r={0};
   a.Mac[0]=0x00;a.Mac[1]=0xe0;a.Mac[2]=(UCHAR)i;
   r.RequestType=NdisRequestQueryInformation;r.DATA.QUERY_INFORMATION.Oid=OID_GEN_VENDOR_ID;
   r.DATA.QUERY_INFORMATION.InformationBuffer=value;r.DATA.QUERY_INFORMATION.InformationBufferLength=3;
   assert(n31_request(b,&r)==NDIS_STATUS_BUFFER_TOO_SHORT&&r.DATA.QUERY_INFORMATION.BytesNeeded==4&&value[0]==0xcc);
   r.DATA.QUERY_INFORMATION.InformationBufferLength=4;
   assert(!n31_request(b,&r)&&r.DATA.QUERY_INFORMATION.BytesWritten==4);
   assert(value[0]==0&&value[1]==0xe0&&value[2]==(UCHAR)i&&value[3]==0);
  }
  assert(set_filter(b,NDIS_PACKET_TYPE_DIRECTED,4)==0);
  assert(set_filter(c,NDIS_PACKET_TYPE_BROADCAST,4)==0&&a.Filter==9&&f->Filter==1&&g->Filter==8);
  assert(set_filter(b,0,3)==NDIS_STATUS_INVALID_LENGTH&&f->Filter==1);
  assert(set_filter(b,0x80000000UL,4)==NDIS_STATUS_NOT_SUPPORTED&&f->Filter==1);
  {UCHAR addr[6]={1,0,0x5e,0,0,1};NDIS_REQUEST r={0};r.RequestType=NdisRequestSetInformation;r.DATA.SET_INFORMATION.Oid=OID_802_3_MULTICAST_LIST;r.DATA.SET_INFORMATION.InformationBuffer=addr;r.DATA.SET_INFORMATION.InformationBufferLength=6;
   assert(n31_request(b,&r)==0&&f->Multicast==1&&g->Multicast==0&&r.DATA.SET_INFORMATION.BytesRead==6);
   r.DATA.SET_INFORMATION.InformationBufferLength=5;assert(n31_request(b,&r)==NDIS_STATUS_INVALID_LENGTH&&f->Multicast==1);
  }
  /* A close from inside a receive callback must leave the binding alive
     until Eth's final reference returns, without touching the other open. */
  f->References++;assert(n31_close(b)==NDIS_STATUS_PENDING&&allocated==2&&a.Filter==8);
  n31_filter_lock(&a);assert(!--f->References);close_action(f->Owner);drop_filter(f);n31_filter_unlock(&a);
  assert(allocated==1&&completed==i+1&&last_completed==(PVOID)1&&g->Filter==8);
  assert(set_filter(c,NDIS_PACKET_TYPE_DIRECTED|NDIS_PACKET_TYPE_BROADCAST,4)==0);
  assert(n31_close(c)==0&&allocated==0&&!a.Filter);
 }
 fail_alloc=1;assert(n31_open(&error,&x,&selected,&medium,1,(PVOID)1,&a,0,NULL)==NDIS_STATUS_RESOURCES&&!x&&!allocated);fail_alloc=0;
 fail_open=1;assert(n31_open(&error,&x,&selected,&medium,1,(PVOID)1,&a,0,NULL)==NDIS_STATUS_RESOURCES&&!x&&!allocated);fail_open=0;
 /* BOOLEAN is a one-byte ABI return; a failure cannot inherit high EAX bits. */
 assert(sizeof(BOOLEAN)==1);
 puts("PASS: NT3.1 200 dual-protocol open/filter/deferred-close schedules, independent multicast, allocation/filter failure cleanup, lock ownership and BOOLEAN ABI");return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';exe=Path(d)/'test'
 p.write_text(prefix+structs+stubs+'\n'.join(function(n) for n in ['filt_change','n31_filter_lock','n31_filter_unlock','close_action','n31_open','n31_close','put','n31_request'])+tests)
 subprocess.run(['gcc','-std=c99','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(src),str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
