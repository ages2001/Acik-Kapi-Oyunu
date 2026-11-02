"""Verify actual kernel-delay wrappers: signed NT intervals, zero and errors."""
from pathlib import Path
import re, subprocess, tempfile
src=Path(__file__).resolve().parents[1]/'RLTKLANT/src'
def extract(file,name):
    code=(src/file).read_text();m=re.search(r'static int '+name+r'\([^;{]*\)\s*\{',code);assert m
    pos=m.end();depth=1
    while depth:depth+=(code[pos]=='{')-(code[pos]=='}');pos+=1
    return code[m.start():pos]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef uint32_t RTL_FW_U32,ULONG;typedef uint64_t ULONGLONG;
typedef union {uint64_t QuadPart;} LARGE_INTEGER;
typedef struct {uint32_t LowPart;int32_t HighPart;} LARGE_INT;
#define FALSE 0
typedef void* PN31_ADAPTER;
static int cancelled;static unsigned stalls;
static int PciCancelled(void*p){(void)p;return cancelled;}
static int n31_phy_cancelled(void*p){(void)p;return cancelled;}
static void NdisStallExecution(unsigned us){assert(us<=50);stalls++;}
static void KeStallExecutionProcessor(unsigned us){NdisStallExecution(us);}
static int fail;static unsigned calls;static int64_t interval;
static int KeDelayExecutionThread(int mode,int alert,const void*p){
 const uint32_t*words=p;assert(!mode&&!alert);calls++;
 interval=(int64_t)(int32_t)words[1]*4294967296LL+words[0];return fail?-1:0;
}
'''
tests=r'''
int main(void){
 unsigned n,k;int(*waits[])(void*,RTL_FW_U32)={PciDelayMs,n31_phy_ms};
 for(k=0;k<2;k++){
  for(n=0;n<=2000;n++){
   calls=stalls=0;interval=0;fail=0;assert(waits[k](0,n));
   assert(interval==-(int64_t)n*10000&&calls==(n?1U:0U)&&stalls==(n?0U:1U));
   fail=1;calls=0;assert(waits[k](0,n)==(n==0));
  }
  cancelled=1;calls=stalls=0;assert(!waits[k](0,1)&&!calls&&!stalls);cancelled=0;
  calls=0;assert(!waits[k](0,2001)&&!waits[k](0,UINT32_MAX)&&!calls);
 }
 puts("PASS: both kernel wait wrappers, 0..2000 ms, signed 100 ns intervals, zero delay, failure and overflow guards");
}
'''
with tempfile.TemporaryDirectory(prefix='rtlk-fw-waits-') as tmp:
    code=Path(tmp)/'test.c';exe=Path(tmp)/'test'
    code.write_text(prefix+extract('pci_start.c','PciDelayUs')+extract('pci_start.c','PciDelayMs')+extract('drvnt31.c','n31_phy_us')+extract('drvnt31.c','n31_phy_ms')+tests)
    subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(code),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
