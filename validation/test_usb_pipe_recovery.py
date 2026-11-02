"""Production USBNT clear-halt/reset URBs must propagate both failure layers."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]/'USBNT/src'
def fn(file,name):
    s=(root/file).read_text();m=re.search(r'(?:static )?\w+ '+name+r'\([^;{]*\)\s*\{',s);assert m
    i=m.end();depth=1
    while depth:depth+=(s[i]=='{')-(s[i]=='}');i+=1
    return s[m.start():i]
prefix=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
typedef unsigned short USHORT;typedef int LONG,USBD_STATUS,NTSTATUS;
#define USB_OK 0
#define USB_ERR_STALL -1
#define USB_ERR_IO -2
#define USB_ERR_TIMEOUT -3
#define USB_ERR_ABORTED -4
#define USB_ERR_NODEV -5
#define USB_ERR_NOMEM -6
#define USB_ERR_PARAM -7
#define USB_ERR_BABBLE -8
#define USB_RT_OUT 0
#define USB_RT_STD 0
#define USB_RT_ENDPOINT 2
#define USB_REQ_CLEAR_FEATURE 1
#define USB_FEATURE_ENDPOINT_HALT 0
#define USB_EP_ISOCH 1
#define URB_FUNCTION_ABORT_PIPE 2
#define URB_FUNCTION_SYNC_RESET_PIPE_AND_CLEAR_STALL 30
#define URB_FUNCTION_SYNC_RESET_PIPE 48
#define USBD_STATUS_INVALID_PIPE_HANDLE -20
#define STATUS_INVALID_PARAMETER -21
typedef struct USB_PIPE USB_PIPE;
typedef struct {int(*PipeReset)(USB_PIPE*);} HCD_OPS;
typedef struct {HCD_OPS*Ops;} USB_HC;
typedef struct {USB_HC*Hc;} USB_DEV;
struct USB_PIPE {USB_DEV*Dev;void*Cur;int Toggle,Halted,Endpoint,DirIn;};
typedef struct {USB_PIPE P;int Type;} UD_PIPE;
typedef struct {UD_PIPE*Pipe;} UD_FUNC;
typedef struct {struct {USHORT Function;USBD_STATUS Status;} UrbHeader;struct {void*PipeHandle;} UrbPipeRequest;} URB;
typedef struct {UD_FUNC*F;URB*Urb;NTSTATUS St;} UD_JOB;
static int sequence,control_at,reset_at,cancel_at,abort_at,control_result,reset_result;
static int Reset(USB_PIPE*p){(void)p;reset_at=++sequence;return reset_result;}
static void UsbCancelPipe(USB_PIPE*p){p->Cur=NULL;cancel_at=++sequence;}
static int UsbControl(USB_DEV*d,int type,int req,int value,USHORT index,int len,void*data,int timeout,void*done){
 assert(d&&type==2&&req==1&&!value&&(index==0x81||index==2)&&!len&&!data&&timeout==1000&&!done);
 control_at=++sequence;return control_result;
}
static UD_PIPE*PipeOf(UD_FUNC*f,void*h){return h==f->Pipe?f->Pipe:NULL;}
static void AbortPipe(UD_PIPE*p){(void)p;abort_at=++sequence;}
static USBD_STATUS UsbdStatus(LONG s,UD_FUNC*f){assert(f);return s;}
static NTSTATUS NtStatusOf(USBD_STATUS s){return s;}
'''
main=r'''
int main(void){HCD_OPS ops={Reset};USB_HC hc={&ops};USB_DEV dev={&hc};UD_PIPE pipe;
 UD_FUNC f={&pipe};URB u;UD_JOB j={&f,&u,99};int error,mode;
 for(mode=0;mode<3;mode++)for(error=0;error>=-8;error--){
  memset(&pipe,0,sizeof(pipe));memset(&u,0,sizeof(u));pipe.P.Dev=&dev;
  pipe.P.Endpoint=1;pipe.P.DirIn=1;pipe.P.Toggle=pipe.P.Halted=1;
  u.UrbHeader.Function=mode==2?URB_FUNCTION_SYNC_RESET_PIPE:URB_FUNCTION_SYNC_RESET_PIPE_AND_CLEAR_STALL;
  u.UrbPipeRequest.PipeHandle=&pipe;
  sequence=control_at=reset_at=cancel_at=abort_at=0;
  control_result=mode==0?error:0;reset_result=mode==0?0:error;
  PipeRequest(&j);assert(j.St==error&&u.UrbHeader.Status==error&&abort_at==1);
  if(mode==0&&error)assert(!reset_at&&pipe.P.Toggle&&pipe.P.Halted);
  if(mode==1||(!mode&&!error))assert(reset_at>control_at&&control_at>abort_at);
  if(mode==2)assert(!control_at&&reset_at>abort_at);
  assert(pipe.P.Toggle==!!error&&pipe.P.Halted==!!error);
 }
 sequence=control_at=reset_at=cancel_at=abort_at=0;control_result=reset_result=0;
 pipe.P.Cur=&pipe;assert(UsbClearHalt(&pipe.P)==0&&cancel_at>control_at&&reset_at>cancel_at);
 sequence=control_at=reset_at=cancel_at=abort_at=0;
 u.UrbHeader.Function=URB_FUNCTION_ABORT_PIPE;PipeRequest(&j);
 assert(!j.St&&abort_at&&!control_at&&!reset_at);
 u.UrbPipeRequest.PipeHandle=NULL;PipeRequest(&j);
 assert(j.St==STATUS_INVALID_PARAMETER&&u.UrbHeader.Status==USBD_STATUS_INVALID_PIPE_HANDLE);
 puts("PASS: 27 clear/reset fault combinations, control-before-DATA0 ordering, no false success or halt clearing, abort and invalid handles");return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
    c=Path(d)/'test.c';exe=Path(d)/'test'
    c.write_text(prefix+fn('usbcore.c','UsbResetPipeToggle')+fn('usbcore.c','UsbClearHalt')+fn('usbdi.c','PipeRequest')+main)
    subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
