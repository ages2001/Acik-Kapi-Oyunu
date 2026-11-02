"""Production reset URB and TX stall policy, including ambiguous send errors."""
from pathlib import Path
import re,subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'RLTKLANT/src/native_urb.c').read_text()
def fn(name):
    m=re.search(r'(?:static )?(?:int|NTSTATUS) '+name+r'\([^;{]*\)\s*\{',s);assert m
    i=m.end();d=1
    while d:d+=(s[i]=='{')-(s[i]=='}');i+=1
    return s[m.start():i]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
typedef int32_t NTSTATUS;typedef uint16_t USHORT;typedef unsigned long ULONG;typedef void*USBD_PIPE_HANDLE;
#define FALSE 0
#define NT_SUCCESS(x) ((x)>=0)
#define RtlZeroMemory(p,n) memset(p,0,n)
#define STATUS_INVALID_PARAMETER -1
#define USBD_STATUS_STALL_PID 0xc0000004u
#define URB_FUNCTION_SYNC_RESET_PIPE_AND_CLEAR_STALL 30
#define URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER 9
#define URB_FUNCTION_VENDOR_DEVICE 23
#define USBD_TRANSFER_DIRECTION_IN 1
#define USBD_TRANSFER_DIRECTION_OUT 0
typedef struct {unsigned Length,Function,Status;} HDR;
struct _URB_PIPE_REQUEST {HDR Hdr;USBD_PIPE_HANDLE PipeHandle;};
struct _URB_BULK_OR_INTERRUPT_TRANSFER {HDR Hdr;USBD_PIPE_HANDLE PipeHandle;unsigned TransferFlags;void*TransferBuffer;ULONG TransferBufferLength;};
struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST {HDR Hdr;unsigned TransferFlags;void*TransferBuffer;ULONG TransferBufferLength;unsigned Request,Value,Index;};
typedef union {HDR UrbHeader;struct _URB_PIPE_REQUEST Pipe;} URB,*PURB;
typedef struct {USBD_PIPE_HANDLE Out;} RTL_URB_TARGET;
static unsigned submits,resets,stalls;static NTSTATUS sendstatus,resetstatus;static RTL_URB_TARGET*owner;
static NTSTATUS Submit(RTL_URB_TARGET*t,PURB u,ULONG ms,int closing){
 assert(t==owner&&ms==5000&&!closing);submits++;
 if(u->UrbHeader.Function==30){assert(u->UrbHeader.Length==sizeof(struct _URB_PIPE_REQUEST)&&u->Pipe.PipeHandle==owner->Out);resets++;return resetstatus;}
 assert(u->UrbHeader.Function==9||u->UrbHeader.Function==23);
 u->UrbHeader.Status=stalls?USBD_STATUS_STALL_PID:0xc0000001u;return sendstatus;
}
static NTSTATUS TakeRx(RTL_URB_TARGET*t,void*b,ULONG n,ULONG*done){(void)t;(void)b;(void)n;*done=0;return -20;}
'''
main=r'''
int main(void){RTL_URB_TARGET t;unsigned char bytes[100]={0},setup[8]={0x40,5,0,0,0,0,100,0};ULONG done;unsigned i,j;
 owner=&t;t.Out=&t;
 for(i=0;i<2;i++)for(j=0;j<2;j++){
  submits=resets=0;stalls=i;sendstatus=-10;resetstatus=j?-11:0;
  assert(RtlUrbTransfer(&t,1,2,NULL,bytes,100,&done)==-10&&!done);
  assert(resets==i&&submits==1+i); /* exactly one send; no automatic replay */
 }
 submits=resets=0;stalls=1;sendstatus=-10;
 assert(RtlUrbTransfer(&t,0,0,setup,bytes,100,&done)==-10&&!done&&!resets&&submits==1);
 submits=resets=0;stalls=0;sendstatus=0;
 assert(!RtlUrbTransfer(&t,1,2,NULL,bytes,100,&done)&&done==100&&!resets&&submits==1);
 puts("PASS: native RESET_PIPE ABI request, original TX failure preserved, no ambiguous frame resend, no bulk reset for control stalls");return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
    c=Path(d)/'test.c';exe=Path(d)/'test';c.write_text(prefix+fn('ResetPipe')+fn('RtlUrbTransfer')+main)
    subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
