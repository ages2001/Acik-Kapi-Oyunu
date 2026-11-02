"""Production port reset: address-zero sequencing, identity and failure cleanup.
Host-controller calls are mocks; this does not establish electrical behavior.
"""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]/'USBNT/src'
def function(file,name):
 s=(root/file).read_text();m=re.search(r'(?:static )?(?:int|void) '+name+r'\([^;{]*\)\s*\{',s);assert m
 i=m.end();depth=1
 while depth:depth+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[m.start():i]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t UCHAR;typedef uint32_t ULONG;
#define USB_OK 0
#define USB_PORT_HANDOFF 1
#define USB_ERR_NODEV -1
#define USB_ERR_IO -2
#define USB_ERR_TIMEOUT -3
#define USB_RT_OUT 0
#define USB_RT_STD 0
#define USB_RT_DEVICE 0
#define USB_REQ_SET_ADDRESS 5
#define USB_DT_DEVICE 1
typedef struct Dev USB_DEV;
typedef struct Pipe {int Opened,Toggle,Halted;void *Cur,*Tail,*HcPriv;USB_DEV*Dev;} USB_PIPE;
typedef struct Ops {int(*PortReset)(void*,int,UCHAR*);void(*PortClearChange)(void*,int);
 int(*DevInit)(USB_DEV*);void(*DevFree)(USB_DEV*);int(*PipeOpen)(USB_PIPE*);void(*PipeClose)(USB_PIPE*);
 int(*DevSetAddress)(USB_DEV*);int(*DevUpdate)(USB_DEV*);} HCD_OPS;
typedef struct {HCD_OPS*Ops;int Running,Removed;} HC;
struct Dev {HC*Hc;USB_DEV*Parent;int Gone,Port;UCHAR Address,Speed,DevDesc[18];USB_PIPE Ep0;};
static int fail,step,resource,pipe_resource,reset_count,hub,changed,short_read,wrong_speed,addressed;
static int check(void){return ++step==fail?USB_ERR_TIMEOUT:USB_OK;}
static void OsSleepMs(int ms){assert(ms>0);}
static void close_pipe(USB_PIPE*p){pipe_resource=0;p->HcPriv=0;}
static void UsbClosePipe(USB_PIPE*p){if(p->Opened){close_pipe(p);p->Opened=0;}}
static void free_device(USB_DEV*d){assert(!d->Ep0.Opened&&!pipe_resource);resource=0;}
static int reset_port(void*hc,int port,UCHAR*speed){(void)hc;assert(port==1&&!resource&&!pipe_resource);reset_count++;*speed=wrong_speed?3:2;return check();}
static int HubResetChild(USB_DEV*d,UCHAR*s){assert(hub&&d->Parent);return reset_port(d->Hc,d->Port,s);}
static void clear_change(void*hc,int port){(void)hc;assert(port==1&&!hub);}
static int init_device(USB_DEV*d){assert(reset_count&&d->Address==0&&!resource);resource=1;return check();}
static int open_pipe(USB_PIPE*p){assert(resource&&!pipe_resource);pipe_resource=1;p->HcPriv=p;return check();}
static int assign(USB_DEV*d){int r;assert(d->Address==0&&pipe_resource);r=check();if(!r){d->Address=19;addressed=1;}return r;}
static int UsbControl(USB_DEV*d,int rt,int req,int address,int index,int len,void*b,int timeout,void*done){
 int r;(void)rt;(void)index;(void)len;(void)b;(void)done;assert(d->Address==0&&address==7&&req==5&&timeout==1000&&pipe_resource);r=check();if(!r)addressed=1;return r;}
static int update(USB_DEV*d){assert(addressed&&d->Address);return check();}
static int UsbGetDescriptor(USB_DEV*d,int type,int index,int lang,UCHAR*b,int len,ULONG*got){
 int r;(void)index;(void)lang;assert(type==1&&len==18&&addressed);r=check();memcpy(b,d->DevDesc,18);if(changed)b[10]^=1;*got=short_read?8:18;return r;}
'''
main=r'''
int main(void){HCD_OPS ops={reset_port,clear_change,init_device,free_device,open_pipe,close_pipe,0,update};HC hc={&ops,1,0};USB_DEV d;int mode,fault,variant,result,cases=0;
 for(mode=0;mode<4;mode++)for(variant=0;variant<4;variant++)for(fault=0;fault<=6;fault++){
  memset(&d,0,sizeof(d));d.Hc=&hc;d.Address=7;d.Speed=2;d.Port=1;d.Ep0.Opened=1;d.Ep0.Dev=&d;
  hub=mode&1;d.Parent=hub?&d:0;ops.DevSetAddress=mode&2?assign:0;
  fail=fault;step=reset_count=addressed=0;resource=pipe_resource=1;
  changed=variant==1;short_read=variant==2;wrong_speed=variant==3;
  result=UsbResetDevice(&d);cases++;
  if(!fault&&!variant){assert(!result&&resource&&pipe_resource&&d.Ep0.Opened&&d.Address==(mode&2?19:7));}
  else{assert(result!=USB_OK&&!resource&&!pipe_resource&&!d.Ep0.Opened);if(!(mode&2))assert(d.Address==7);}
 }
 memset(&d,0,sizeof(d));d.Hc=&hc;d.Gone=1;step=0;assert(UsbResetDevice(&d)==USB_ERR_NODEV&&!step);
 printf("PASS: %d production physical-reset schedules, root/hub, legacy/xHCI address, every operation failure, speed/identity/short-descriptor rejection\n",cases);return 0;}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'test.c';exe=Path(td)/'test';p.write_text(prefix+function('usbcore.c','UsbResetDevice')+main)
 subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
