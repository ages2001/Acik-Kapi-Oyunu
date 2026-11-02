"""Fault-inject production pipe replacement after inbox-stack RESET_PORT.

The ABI shim is shared with the SELECT_CONFIGURATION builder test. The
production request, reconfiguration and carrier functions are extracted
verbatim. OS behavior is tested separately in the XP guest.
"""
from pathlib import Path
import ast,re,subprocess,tempfile
here=Path(__file__).resolve().parent
source=(here.parent/'RLTKLANT/src/native_urb.c').read_text()
tree=ast.parse((here/'test_native_configuration.py').read_text())
prefix=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='prefix' for t in n.targets))
def fn(name):
    m=re.search(r'(?:static )?(?:void|int|PURB) '+name+r'\([^;{]*\)\s*\{',source);assert m,name
    at=m.end();depth=1
    while depth:depth+=(source[at]=='{')-(source[at]=='}');at+=1
    return source[m.start():at]
shim=r'''
typedef int KIRQL;typedef unsigned char*PUCHAR;
typedef void* USBD_PIPE_HANDLE;
typedef USBD_PIPE_INFORMATION* PUSBD_PIPE_INFORMATION;
typedef struct {unsigned char bytes[18];} USB_DEVICE_DESCRIPTOR;
typedef struct {int Lock,Stopping,Reconfigure,ResetObserved,RxFault,RxAccept;ULONG RxEpoch,RxHead,RxCount;
 PUSB_CONFIGURATION_DESCRIPTOR ConfigurationData;ULONG InterfaceOffset;USB_DEVICE_DESCRIPTOR Identity;
 USBD_PIPE_HANDLE In,Out;void*Configuration;} RTL_URB_TARGET;
#define TRUE 1
#define FALSE 0
#define USB_DEVICE_DESCRIPTOR_TYPE 1
#define USBD_STATUS_INVALID_PIPE_HANDLE 0x80000600u
#define USBD_STATUS_INVALID_PARAMETER 0x80000300u
#define USBD_STATUS_DEVICE_GONE 0xc0007000u
#define UsbdPipeTypeBulk 2
#define NT_SUCCESS(s) ((s)>=0)
static unsigned mode,requests,frees;static int held;
static void KeAcquireSpinLock(int*lock,int*old){(void)lock;assert(!held);held=1;*old=0;}
static void KeReleaseSpinLock(int*lock,int old){(void)lock;assert(held&&!old);held=0;}
static int RtlUrbCancelled(RTL_URB_TARGET*t){return t->Stopping;}
static size_t RtlCompareMemory(const void*a,const void*b,size_t n){return memcmp(a,b,n)?0:n;}
static int Descriptor(RTL_URB_TARGET*t,int kind,void*p,size_t n){
 assert(!held&&t->Reconfigure&&kind==1&&n==sizeof(t->Identity));memcpy(p,&t->Identity,n);
 if(mode==2)((unsigned char*)p)[0]^=1;return mode==1?-1:0;
}
static void ExFreePool(void*p){assert(p);frees++;free(p);}
static int Submit(RTL_URB_TARGET*t,PURB urb,ULONG ms,int closing){
 PUSBD_INTERFACE_INFORMATION p=&urb->UrbSelectConfiguration.Interface;
 assert(!held&&!t->Stopping&&t->Reconfigure&&t->RxFault&&!t->RxAccept&&ms==5000&&!closing);requests++;
 p->Pipes[0].PipeType=p->Pipes[1].PipeType=UsbdPipeTypeBulk;
 p->Pipes[0].EndpointAddress=0x81;p->Pipes[1].EndpointAddress=0x02;
 p->Pipes[0].PipeHandle=(void*)0x300;p->Pipes[1].PipeHandle=(void*)0x400;
 urb->UrbSelectConfiguration.ConfigurationHandle=(void*)0x500;
 if(mode==5)p->NumberOfPipes++;if(mode==6)p->Length--;
 if(mode==7)p->Pipes[0].PipeHandle=NULL;if(mode==8)p->Pipes[1].PipeHandle=NULL;
 if(mode==9)t->Stopping=1;if(mode==10)p->Pipes[0].PipeType=1;
 return mode==4?-1:0;
}
'''
main=r'''
int main(void){
 struct {USB_CONFIGURATION_DESCRIPTOR config;USB_INTERFACE_DESCRIPTOR iface;} data;
 RTL_URB_TARGET t;unsigned stop,code,round;
 memset(&data,0,sizeof(data));data.iface.bNumEndpoints=2;
 for(round=0;round<100;round++)for(mode=0;mode<=10;mode++){
  memset(&t,0,sizeof(t));t.ConfigurationData=&data.config;t.InterfaceOffset=offsetof(__typeof__(data),iface);
  t.In=(void*)0x100;t.Out=(void*)0x200;t.Configuration=(void*)0x99;
  t.RxEpoch=7;t.RxCount=4;t.RxAccept=1;requests=frees=calls=0;fail=mode==3;
  if(round%3){
   RequestReconfiguration(&t,USBD_STATUS_INVALID_PARAMETER);assert(!t.Reconfigure);
   if(round%3==1)RequestReconfiguration(&t,USBD_STATUS_DEVICE_GONE);
   else RtlUrbObserveBusReset(&t);
   assert(t.ResetObserved&&!t.Reconfigure);
   RequestReconfiguration(&t,USBD_STATUS_INVALID_PARAMETER);
  }else RequestReconfiguration(&t,USBD_STATUS_INVALID_PIPE_HANDLE);
  assert(t.Reconfigure&&t.RxFault&&!t.RxAccept&&t.RxEpoch==8&&!t.RxCount);
  assert(!RtlUrbSetCarrier(&t,1));
  if(!mode){assert(ReconfigurePipes(&t));assert(t.In==(void*)0x300&&t.Out==(void*)0x400&&t.Configuration==(void*)0x500);
   assert(!t.Reconfigure&&!t.RxFault&&t.RxEpoch==9&&RtlUrbSetCarrier(&t,1));}
  else{assert(!ReconfigurePipes(&t));assert(t.Reconfigure&&t.RxFault&&!t.RxAccept&&t.In==(void*)0x100&&t.Out==(void*)0x200);}
  assert(frees==(mode!=1&&mode!=2&&mode!=3));assert(!held);
 }
 for(stop=0;stop<2;stop++)for(code=0;code<2;code++){
  memset(&t,0,sizeof(t));t.Stopping=stop;RequestReconfiguration(&t,code?USBD_STATUS_INVALID_PIPE_HANDLE:0xc0007000u);
  assert(t.Reconfigure==(!stop&&code));assert(!ReconfigurePipes(&t));
 }
 puts("PASS: 1100 production pipe reconfiguration schedules; identity/allocation/submit/shape/handle/stop failures never publish partial handles; carrier gated; no old frame replay");
}
'''
with tempfile.TemporaryDirectory() as td:
    p=Path(td)/'test.c';exe=Path(td)/'test'
    p.write_text(prefix+shim+fn('SelectConfiguration')+fn('RequestReconfiguration')+fn('RtlUrbObserveBusReset')+fn('ReconfigurePipes')+fn('RtlUrbSetCarrier')+main)
    subprocess.run(['gcc','-std=gnu99','-Wall','-Wextra','-Wno-misleading-indentation','-Werror','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
