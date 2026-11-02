"""Exercise production PCI PHY handshakes and configuration failure propagation."""
from pathlib import Path
import re,subprocess,tempfile
src=Path(__file__).resolve().parents[1]/'RLTKLANT/src'
def function(text,name):
 m=re.search(r'(?:static )?\w+ '+name+r'\([^;{]*\)\s*\{',text);assert m,name
 pos=m.end();depth=1
 while depth:depth+=(text[pos]=='{')-(text[pos]=='}');pos+=1
 return text[m.start():pos]
b=(src/'backend_cplus.c').read_text();p=(src/'phy.c').read_text()
funcs='\n'.join(function(b,n) for n in ['RtlCpPhyOcpRead','RtlCpPhyOcpWrite','CpPhyAddress','CpLegacyMdio','RtlCpMdioRead','RtlCpMdioWrite'])
funcs+='\n'+'\n'.join(x for x in p.splitlines() if x.startswith('#define'))
funcs+='\n'+'\n'.join(function(p,n) for n in ['RtlAdvert','RtlConfigurePhy'])
defs='\n'.join(x for x in (src/'rltklant.h').read_text().splitlines() if x.startswith('#define RLTK_SD'))
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
typedef uint32_t ULONG;typedef uint16_t USHORT;typedef unsigned UINT;typedef int BOOLEAN;
#define TRUE 1
#define FALSE 0
#define VOID void
#include "rltkhw.h"
#include "rtlchip.h"
typedef struct { ULONG Flags,Is8125;USHORT PhyPage;UINT Backend,SpeedDuplex,MaxSpeedMbit; } Adapter,*PRLTK_ADAPTER;
static USHORT words[32768];static ULONG gate,cmd,port;
static unsigned transactions,fail_at,stalls,reads,delay_reads,gate_writes;
static int absent;
static void RtlWrite32(Adapter *a,ULONG reg,ULONG value){
 (void)a;
 if(reg==0xd0){gate=value;gate_writes++;return;}
 assert(reg==0xb8||reg==RCP_PHYAR);port=reg;cmd=value;transactions++;reads=0;
 if(value&0x80000000)words[(value>>16)&(reg==0xb8?0x7fff:31)]=(USHORT)value;
}
static ULONG RtlRead32(Adapter *a,ULONG reg){
 unsigned idx;(void)a;if(absent)return UINT32_MAX;
 if(reg==0xd0)return gate;
 assert(reg==port);reads++;
 if(transactions==fail_at||reads<=delay_reads)return cmd&0x80000000;
 idx=(cmd>>16)&(reg==0xb8?0x7fff:31);
 return (cmd&0x80000000)?cmd&0x7fffffff:(0x80000000|(idx<<16)|words[idx]);
}
static void NdisStallExecution(unsigned us){assert(us==20);stalls++;}
static void setup(void){memset(words,0,sizeof(words));gate=0x12360000;cmd=port=0;transactions=fail_at=stalls=reads=delay_reads=gate_writes=0;absent=0;}
'''
tests=r'''
int main(void){
 Adapter a={0};USHORT value;unsigned t,k;
 a.Backend=RTL_BACKEND_CPLUS;a.MaxSpeedMbit=10000;a.Is8125=1;
 setup();words[0xa5d4/2]=0xffff;
 assert(RtlCpPhyOcpRead(&a,0xa5d4,&value)&&value==0xffff);
 assert(!RtlCpPhyOcpRead(&a,0xa5d5,&value)&&transactions==1);
 setup();absent=1;assert(!RtlCpPhyOcpRead(&a,0xa5d4,&value)&&!stalls);
 assert(!RtlCpPhyOcpWrite(&a,0xa5d4,1));
 setup();fail_at=1;assert(!RtlCpPhyOcpWrite(&a,0xa5d4,1)&&stalls==2000);
 setup();fail_at=1;assert(!RtlCpPhyOcpRead(&a,0xa5d4,&value)&&stalls==2000);
 setup();delay_reads=7;assert(RtlCpPhyOcpWrite(&a,0xa5d4,0x0180)&&stalls==7);
 assert(RtlCpPhyOcpRead(&a,0xa5d4,&value)&&value==0x0180&&stalls==14);
 assert(RtlCpMdioWrite(&a,31,0xa43));assert(RtlCpMdioRead(&a,31,&value)&&value==0xa43);
 assert(RtlCpMdioWrite(&a,15,0x1234)&&words[0xa42e/2]==0x1234);
 assert(RtlCpMdioRead(&a,15,&value)&&value==0x1234);
 assert(!RtlCpMdioWrite(&a,32,1)&&!RtlCpMdioRead(&a,32,&value));
 assert(RtlCpMdioWrite(&a,31,0xa40));
 assert(RtlCpMdioRead(&a,31,&value)&&value==0);
 for(k=0;k<31;k++){
  assert(RtlCpMdioWrite(&a,k,(USHORT)(0x100+k)));
  assert(words[(0xa400+k*2)/2]==0x100+k);
  assert(RtlCpMdioRead(&a,k,&value)&&value==0x100+k);
 }
 /* Exhaust all 12-bit hardware pages and all non-selector MII registers. */
 for(k=0;k<4096;k++)for(t=0;t<31;t++){
  int expected=k&&k!=0xa40 ? (int)(k<<4)+(int)t*2-32 : 0xa400+(int)t*2;
  assert(RtlCpMdioWrite(&a,31,(USHORT)k));
  if(expected<0||expected>0xfffe){
   assert(!RtlCpMdioWrite(&a,t,0x4321)&&!RtlCpMdioRead(&a,t,&value));continue;
  }
  assert(RtlCpMdioWrite(&a,t,0x4321));
  assert(words[expected/2]==0x4321);
  assert(RtlCpMdioRead(&a,t,&value)&&value==0x4321);
 }
 assert(!RtlCpMdioWrite(&a,31,0x1000)&&a.PhyPage==0xfff);
 a.PhyPage=0;
 for(t=0;t<3;t++){
  a.Is8125=t==2;a.Flags=t==1?RTL_F_PHY_DP:0;
  setup();delay_reads=3;words[9]=0x1200;
  assert(RtlCpMdioRead(&a,9,&value));assert(stalls>=3);
  if(t==1)assert(gate==0x12360000&&gate_writes==2);
  setup();fail_at=1;assert(!RtlCpMdioWrite(&a,4,1));
  if(t==1)assert(gate==0x12360000&&gate_writes==2);
  setup();absent=1;assert(!RtlCpMdioRead(&a,9,&value));
 }
 a.Is8125=1;a.Flags=0;setup();assert(RtlConfigurePhy(&a));
 assert(words[0xa400/2]==0x1200&&words[(0xa400+8)/2]==0x05e1);
 assert(words[(0xa400+18)/2]==0x0200&&words[0xa5d4/2]==0x1180);
 t=transactions;assert(t==6);
 for(k=1;k<=t;k++){setup();fail_at=k;assert(!RtlConfigurePhy(&a)&&transactions==k);assert(words[0xa400/2]==(k==6?0x1200:0));}
 setup();words[(0xa400+18)/2]=0xffff;assert(!RtlConfigurePhy(&a)&&transactions==2);
 setup();words[0xa5d4/2]=0xffff;assert(!RtlConfigurePhy(&a)&&transactions==4);
 puts("PASS: MDIO/OCP timeout and removal, valid 0xffff data, DP gate restore, page validation and PHY configuration error propagation");
}
'''
with tempfile.TemporaryDirectory() as d:
 c=Path(d)/'test.c';exe=Path(d)/'test';c.write_text(prefix+defs+'\n'+funcs+tests)
 subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(src),str(c),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
