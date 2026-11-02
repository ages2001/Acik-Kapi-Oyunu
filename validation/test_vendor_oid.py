"""Exercise the production miniport OID getter with distinct permanent/current MACs."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1];src=root/'RLTKLANT/src'
s=(src/'datapath.c').read_text();a=s.index('NDIS_STATUS STDC\nRtlQueryInformation');b=s.index('\nNDIS_STATUS STDC',a+1);query=s[a:b]
oids=sorted(set(re.findall(r'case (OID_\w+):',query)))
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ndisvendor.h"
typedef uint32_t ULONG,*PULONG,NDIS_OID;typedef uint16_t USHORT;typedef void*PVOID;typedef void*NDIS_HANDLE;typedef int NDIS_STATUS;
#define STDC
#define NDIS_STATUS_NOT_SUPPORTED 1
#define NDIS_STATUS_INVALID_LENGTH 2
#define NDIS_STATUS_SUCCESS 0
#define NdisHardwareStatusReady 0
#define NdisMedium802_3 0
#define RLTK_MAX_FRAME 1514
#define RLTK_HEADER_SIZE 14
#define RLTK_VERSION_MAJOR 1
#define RLTK_VERSION_MINOR 0
#define RLTK_MAX_MULTICAST 16
#define NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA 1
#define NDIS_MAC_OPTION_RECEIVE_SERIALIZED 2
#define NDIS_MAC_OPTION_TRANSFERS_NOT_PEND 4
#define NDIS_MAC_OPTION_NO_LOOPBACK 8
#define NdisMoveMemory memcpy
typedef struct {unsigned Lock;char FriendlyName[128];unsigned char PermanentAddr[6],CurrentAddr[6],McastList[16][6];
 ULONG LinkSpeedMbit,MaxSpeedMbit,PacketFilter,LinkUp,XmitOk,RcvOk,XmitErr,RcvErr,RcvNoBuf,McastCount,AlignErr,OneColl,MoreColl;} ADAPTER,*PRLTK_ADAPTER;
static ULONG RtlOids[1];
static void NdisAcquireSpinLock(unsigned*p){assert(!*p);*p=1;}
static void NdisReleaseSpinLock(unsigned*p){assert(*p);*p=0;}
'''
tests=r'''
int main(void){ADAPTER a={0};unsigned position,value,length,i,cases=0;ULONG written,needed;
 unsigned char result[12];
 for(position=0;position<3;position++)for(value=0;value<256;value++){
  a.PermanentAddr[0]=0x00;a.PermanentAddr[1]=0xe0;a.PermanentAddr[2]=0x4c;a.PermanentAddr[position]=value;
  memset(a.CurrentAddr,0xa5,6);a.CurrentAddr[0]=2;
  for(length=0;length<=9;length++){
   memset(result,0xcc,sizeof(result));written=needed=123;
   NDIS_STATUS status=RtlQueryInformation(&a,OID_GEN_VENDOR_ID,result+1,length,&written,&needed);
   if(length<4){assert(status==NDIS_STATUS_INVALID_LENGTH&&!written&&needed==4);for(i=0;i<sizeof(result);i++)assert(result[i]==0xcc);}
   else {assert(!status&&written==4&&!needed&&!memcmp(result+1,a.PermanentAddr,3)&&result[4]==0);
    assert(result[0]==0xcc);for(i=5;i<sizeof(result);i++)assert(result[i]==0xcc);}
   cases++;
  }
 }
 printf("PASS: %u production miniport vendor-OID queries; permanent/OEM bytes, independent configured MAC, exact four-byte output and short-buffer guards\n",cases);
}
'''
with tempfile.TemporaryDirectory(prefix='rltk-vendor-oid-') as td:
 p=Path(td)/'test.c';p.write_text(prefix+'\nenum {'+','.join(oids)+'};\n'+query+tests);out=Path(td)/'test'
 subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(src),str(p),'-o',str(out)],check=True)
 subprocess.run([str(out)],check=True,timeout=20)
