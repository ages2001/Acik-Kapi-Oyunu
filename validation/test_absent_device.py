"""Compile unchanged production register routines against fault-injected I/O.

Function extraction avoids linking unrelated NDIS allocation/packet routines.
This checks register contracts, not OS PnP or real hardware removal.
"""
from pathlib import Path
import os, re, subprocess, tempfile

src = Path(__file__).resolve().parents[1] / 'RLTKLANT/src'
groups = {
    'backend_cplus.c': ['CpImr', 'CpIsrRead', 'CpIsrAck', 'CpIsr', 'CpLinkState'],
    'backend_8139.c': ['R39Reset', 'R39Isr', 'R39LinkState'],
    'backend_ne2000.c': ['NeReset', 'NeIsr', 'NeLinkState'],
}
functions = []
for filename, names in groups.items():
    code = (src / filename).read_text()
    for name in names:
        match = re.search(r'static \w+ '+name+r'\([^;{]*\)\s*\{', code)
        assert match, name
        pos, depth = match.end(), 1
        while depth:
            depth += (code[pos] == '{') - (code[pos] == '}')
            pos += 1
        functions.append(code[match.start():pos])

prefix = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint32_t ULONG, *PULONG;
typedef uint16_t USHORT;
typedef uint8_t UCHAR;
typedef unsigned UINT;
typedef int BOOLEAN, *PBOOLEAN;
#define VOID void
#define TRUE 1
#define FALSE 0
#include "rltkhw.h"
typedef struct { int Is8125; ULONG PendingIsr; } Adapter, *PRLTK_ADAPTER;
static ULONG regs[256];
static unsigned writes,stalls;
static int absent,complete_after;
static UCHAR RtlRead8(PRLTK_ADAPTER a, UINT r) {
 (void)a;
 if(absent)return 255;
 if(complete_after >= 0 && stalls >= (unsigned)complete_after) {
  if(r==R8139_CR)return 0;
  if(r==NE_P0_ISR)return NE_ISR_RST;
 }
 return (UCHAR)regs[r];
}
static USHORT RtlRead16(PRLTK_ADAPTER a,UINT r){(void)a;return absent?65535:(USHORT)regs[r];}
static ULONG RtlRead32(PRLTK_ADAPTER a,UINT r){(void)a;return absent?UINT32_MAX:regs[r];}
/* Count writes without changing scripted read responses. */
static void RtlWrite8(PRLTK_ADAPTER a,UINT r,UCHAR v){(void)a;(void)r;(void)v;writes++;}
static void RtlWrite16(PRLTK_ADAPTER a,UINT r,USHORT v){(void)a;(void)r;(void)v;writes++;}
static void RtlWrite32(PRLTK_ADAPTER a,UINT r,ULONG v){(void)a;(void)r;(void)v;writes++;}
static void NdisStallExecution(UINT us){assert(us==10);stalls++;}
static void clear(void){memset(regs,0,sizeof(regs));writes=stalls=0;absent=0;complete_after=-1;}
'''
tests = r'''
int main(void) {
 Adapter a={0};ULONG speed;BOOLEAN duplex;int newer;
 for(newer=0;newer<2;newer++) {
  a.Is8125=newer;a.PendingIsr=0;clear();absent=1;
  assert(!CpIsr(&a));assert(!writes && !a.PendingIsr);
  speed=123;duplex=1;assert(!CpLinkState(&a,&speed,&duplex));assert(!speed&&!duplex);
  clear();assert(!CpIsr(&a));assert(!writes);
  regs[newer?RCP_ISR0_8125:RCP_ISR]=RCP_INT_ROK;
  assert(CpIsr(&a));assert(writes==2 && a.PendingIsr==RCP_INT_ROK);
  regs[RCP_PHYSTATUS]=RCP_PHYS_LINKSTS|RCP_PHYS_FULLDUP|(newer?RCP_PHYS_10000M:RCP_PHYS_1000M);
  assert(CpLinkState(&a,&speed,&duplex));assert(speed==(newer?10000:1000)&&duplex);
 }
 clear();a.PendingIsr=0;absent=1;
 assert(!R39Isr(&a)&&!NeIsr(&a));assert(!writes&&!a.PendingIsr);
 assert(!R39LinkState(&a,&speed,&duplex)&&!speed&&!duplex);
 assert(!NeLinkState(&a,&speed,&duplex)&&!speed&&!duplex);
 assert(!R39Reset(&a));assert(stalls==0);
 assert(!NeReset(&a));assert(stalls==0);
 clear();regs[R8139_CR]=R8139_CR_RST;
 assert(!R39Reset(&a));assert(stalls==1000);
 clear();assert(!NeReset(&a));assert(stalls==1000);
 clear();regs[R8139_CR]=R8139_CR_RST;complete_after=7;
 assert(R39Reset(&a));assert(stalls==7);
 clear();complete_after=7;assert(NeReset(&a));assert(stalls==7);
 clear();regs[NE_CR]=NE_CR_STA|NE_CR_RD2;regs[NE_P0_ISR]=NE_ISR_PRX;
 assert(NeIsr(&a));assert(a.PendingIsr==NE_ISR_PRX);
 puts("PASS: production C+/8139/8029 absent-device ISR/link checks and reset timeout/recovery");
 return 0;
}
'''
with tempfile.TemporaryDirectory() as td:
    td = Path(td); unit = td/'faults.c'; binary = td/'faults'
    unit.write_text(prefix+'\n'.join(functions)+tests)
    subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer','-I'+str(src),str(unit),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1'})
