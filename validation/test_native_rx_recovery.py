"""A cancelled receive must recover carrier without waiting for a packet."""
from pathlib import Path
import re,subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'RLTKLANT/src/native_urb.c').read_text()
m=re.search(r'static int RecoverReceive\([^;{]*\)\s*\{',s);assert m
end=m.end();d=1
while d:d+=(s[end]=='{')-(s[end]=='}');end+=1
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef uint32_t ULONG;
typedef int BOOLEAN;
#define FALSE 0
#define TRUE 1
#define NT_SUCCESS(s) ((s)>=0)
#define USBD_STATUS_STALL_PID 0xc0000004u
#define USBD_STATUS_CANCELED 0xc0010000u
typedef struct Target {struct Target*RxLane;void*In;int Stopping,RxFault;} RTL_URB_TARGET;
static RTL_URB_TARGET *owner;static unsigned resets;static int result;
static int RtlUrbCancelled(RTL_URB_TARGET*t){return t->Stopping;}
static int ResetPipe(RTL_URB_TARGET*t,void*pipe){assert(t==owner->RxLane&&pipe==owner->In);resets++;return result;}
static void ReceiveFault(RTL_URB_TARGET*t,int fault){assert(t==owner&&!fault);t->RxFault=fault;}
'''
main=r'''
int main(void){ULONG codes[]={USBD_STATUS_STALL_PID,USBD_STATUS_CANCELED,0xc0000001u,0xc0007000u};RTL_URB_TARGET t,lane;unsigned i,stop,fail;BOOLEAN pending;int ready;
 owner=&t;t.RxLane=&lane;t.In=&lane;
 for(i=0;i<4;i++)for(stop=0;stop<2;stop++)for(fail=0;fail<2;fail++){
  t.Stopping=stop;t.RxFault=1;resets=0;result=fail?-1:0;pending=FALSE;ready=RecoverReceive(&t,codes[i],&pending);
  assert(resets==(!stop&&i<2));assert(t.RxFault==!(i<2&&!stop&&!fail));
  assert(pending==(i<2&&(stop||fail))&&ready==!pending);
 }
 for(i=0;i<2;i++){
  t.Stopping=0;t.RxFault=1;resets=0;result=-1;pending=FALSE;
  assert(!RecoverReceive(&t,codes[i],&pending)&&pending&&resets==1);
  for(fail=0;fail<100;fail++){assert(!RecoverReceive(&t,0,&pending)&&pending&&t.RxFault);}
  assert(resets==101);result=0;
  assert(RecoverReceive(&t,0,&pending)&&!pending&&!t.RxFault&&resets==102);
  assert(RecoverReceive(&t,0,&pending)&&resets==102);
 }
 puts("PASS: 16 receive recovery schedules plus two 100-failure retry sequences; pending cancellation/stall recovery survives lower-device power rejection, ACK clears fault without RX traffic, stopping never publishes recovery");return 0;}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'test.c';exe=Path(td)/'test';p.write_text(prefix+s[m.start():end]+main)
 subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
