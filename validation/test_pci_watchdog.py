"""Production descriptor-progress watchdog: stalls versus legitimate inactivity."""
from pathlib import Path
import re,subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'RLTKLANT/src/lifecycle.c').read_text()
m=re.search(r'BOOLEAN STDC RtlCheckForHang\([^;{]*\)\s*\{',s);assert m
end=m.end();depth=1
while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define STDC
#define TRUE 1
#define FALSE 0
#define RTL_BACKEND_CPLUS 2
#define RTL_BACKEND_8139 1
typedef int BOOLEAN;typedef void*NDIS_HANDLE;
typedef struct {int Lock,Backend,UsbContext,Halting,ResetPending,ResetFailed,LinkUp,RxRecoveryNeeded;
 uint32_t CpTxHead,CpTxTail,TxProgress;unsigned char TxNoProgressChecks;} Adapter,*PRLTK_ADAPTER;
static void NdisAcquireSpinLock(int*p){assert(!*p);*p=1;}
static void NdisReleaseSpinLock(int*p){assert(*p);*p=0;}
'''
test=r'''
int main(void){Adapter a;unsigned mask,i,hangs=0;memset(&a,0,sizeof(a));a.Backend=2;a.LinkUp=1;a.CpTxTail=1;
 for(i=0;i<3000;i++){if(RtlCheckForHang(&a))hangs++;assert(!a.Lock);}assert(hangs==1000);
 for(mask=1;mask<64;mask++){
  a.UsbContext=mask&1;a.Halting=mask&2;a.ResetPending=mask&4;a.ResetFailed=mask&8;a.LinkUp=!(mask&16);a.Backend=(mask&32)?0:2;
  for(i=0;i<10;i++)assert(!RtlCheckForHang(&a)&&!a.TxNoProgressChecks);
 }
 memset(&a,0,sizeof(a));a.Backend=2;a.LinkUp=1;
 for(i=0;i<1000;i++)assert(!RtlCheckForHang(&a));
 for(i=0;i<1000;i++){a.CpTxTail++;a.CpTxHead++;assert(!RtlCheckForHang(&a));}
 a.CpTxTail+=2;assert(!RtlCheckForHang(&a));assert(!RtlCheckForHang(&a));
 a.CpTxHead++;assert(!RtlCheckForHang(&a)&&!a.TxNoProgressChecks);
 assert(!RtlCheckForHang(&a));assert(!RtlCheckForHang(&a));assert(RtlCheckForHang(&a));
 a.CpTxHead=a.CpTxTail=a.TxProgress=UINT32_MAX;assert(!RtlCheckForHang(&a));
 a.CpTxTail=0;assert(!RtlCheckForHang(&a));assert(!RtlCheckForHang(&a));assert(RtlCheckForHang(&a));
 a.CpTxHead=0;assert(!RtlCheckForHang(&a));
 memset(&a,0,sizeof(a));a.Backend=1;
 assert(!RtlCheckForHang(&a));a.RxRecoveryNeeded=1;assert(RtlCheckForHang(&a));
 for(mask=1;mask<8;mask++){a.Halting=mask&1;a.ResetPending=mask&2;a.ResetFailed=mask&4;assert(!RtlCheckForHang(&a));}
 puts("PASS: 1000 stalled-engine detections; lifecycle/link/backend exclusions; idle/progress/counter wrap; RTL8139 malformed RX requires NDIS reset and honors stop/pending/failure gates");
}
'''
with tempfile.TemporaryDirectory(prefix='rltk-watchdog-') as td:
 p=Path(td)/'test.c';p.write_text(prefix+s[m.start():end]+test);exe=Path(td)/'test'
 subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
