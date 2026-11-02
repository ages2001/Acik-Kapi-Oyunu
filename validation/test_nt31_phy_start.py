"""Exercise actual NT3.1 pre-PHY reset: register widths, aliases and failures."""
from pathlib import Path
import re
import subprocess
import tempfile

src = Path(__file__).resolve().parents[1] / 'RLTKLANT/src'
text = (src / 'drvnt31.c').read_text()

def function(name):
    m = re.search(r'static \w+ ' + name + r'\([^;{]*\)\s*\{', text)
    assert m, name
    end, depth = m.end(), 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[m.start():end]

prefix = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "fwbus.h"
#include "physeq.h"
#include "rtlchip.h"
typedef uint32_t ULONG;typedef uint16_t USHORT;typedef uint8_t UCHAR;
typedef unsigned UINT;typedef int BOOLEAN;
#include "rltkhw.h"
#define TRUE 1
#define FALSE 0
#define BK_CPLUS 2
typedef struct {ULONG IoBase;USHORT DeviceId;unsigned Backend;} Adapter,*PN31_ADAPTER;
static unsigned expected_width,mask_writes,reset_writes,polls,stalls,uploads;
static unsigned stuck,removed,absent_tcr,upload_failure,expected_gmii;
static RTL_FW_U32 n31_phy_read(void*p,RTL_FW_U16 reg){(void)p;(void)reg;return 0;}
static void n31_phy_write(void*p,RTL_FW_U16 reg,RTL_FW_U32 value){(void)p;(void)reg;(void)value;}
static int n31_phy_us(void*p,RTL_FW_U32 us){(void)p;(void)us;return 1;}
static int n31_phy_ms(void*p,RTL_FW_U32 ms){(void)p;(void)ms;return 1;}
static int n31_phy_cancelled(void*p){(void)p;return 0;}
static void mask(ULONG reg,ULONG value,unsigned width){
 assert(value==0&&width==expected_width);
 assert(reg==0xc000+(width==32?RCP_IMR0_8125:RCP_IMR));mask_writes++;
}
static void out32(ULONG reg,ULONG value){mask(reg,value,32);}
static void out16(ULONG reg,USHORT value){mask(reg,value,16);}
static void out8(ULONG reg,UCHAR value){
 assert(mask_writes==1&&reg==0xc000+RCP_CR&&value==RCP_CR_RST);reset_writes++;
}
static UCHAR in8(ULONG reg){assert(reg==0xc000+RCP_CR);polls++;return removed?0xff:(stuck||polls<4)?RCP_CR_RST:0;}
static ULONG in32(ULONG reg){assert(reg==0xc000+RCP_TCR&&!stuck&&!removed);return absent_tcr?UINT32_MAX:0x60900700;}
static void KeStallExecutionProcessor(unsigned us){assert(us==10);stalls++;}
static void dbg(const char*s){(void)s;}static void dbgh(ULONG value){(void)value;}
enum RTL_FW_RESULT RtlPciPhyConfigureByXid(RTL_FW_BUS*b,RTL_FW_U16 xid,int gmii,RTL_PHY_REPORT*r){
 assert(mask_writes==1&&reset_writes==1&&polls==4&&stalls==3);
 assert(xid==0x609&&gmii==(int)expected_gmii);
 assert(b->Context&&b->Read32==n31_phy_read&&b->Write32==n31_phy_write);
 assert(b->DelayUs==n31_phy_us&&b->DelayMs==n31_phy_ms&&b->Cancelled==n31_phy_cancelled);
 assert(!b->Removed&&!b->PollLimit&&!b->MaxCommands);uploads++;
 memset(r,0,sizeof(*r));r->Result=upload_failure?RtlFwIoError:RtlFwOk;return r->Result;
}
'''
tests = r'''
int main(void){
 unsigned i,mode,cases=0;
 for(i=0;i<RTL_PCI_ID_COUNT;i++){
  const RTL_PCI_ID*p=&RtlPciIds[i];Adapter a={0xc000,p->DeviceId,p->Backend};
  if(p->Backend!=RTL_BACKEND_CPLUS)continue;
  expected_width=p->MaxSpeed>1000?32:16;expected_gmii=p->MaxSpeed>=1000;
  for(mode=0;mode<5;mode++){
   mask_writes=reset_writes=polls=stalls=uploads=0;
   stuck=mode==1;removed=mode==2;absent_tcr=mode==3;upload_failure=mode==4;
   assert(n31_initialize_phy(&a)==(mode==0));
   assert(mask_writes==1&&reset_writes==1);
   if(stuck)assert(polls==1000&&stalls==1000&&!uploads);
   else if(removed)assert(polls==1&&!stalls&&!uploads);
   else assert(polls==4&&stalls==3&&uploads==(mode!=3));
   cases++;
  }
 }
 printf("PASS: %u actual NT3.1 PHY-start schedules across all C+ PCI IDs; register widths/aliases, delayed reset, timeout, removal, failed read and upload\n",cases);
}
'''
with tempfile.TemporaryDirectory() as directory:
    c = Path(directory) / 'test.c'
    binary = Path(directory) / 'test'
    c.write_text(prefix + function('cp_is25') + '\n' + function('n31_initialize_phy') + tests)
    subprocess.run(['gcc', '-std=c99', '-Wall', '-Wextra', '-Werror',
                    '-Wno-unused-function', '-fsanitize=address,undefined',
                    '-I', str(src), str(c), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
