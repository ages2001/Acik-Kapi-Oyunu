"""Production USBDI reset orchestration, sibling rundown and failure propagation."""
from pathlib import Path
import re,subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'USBNT/src/usbdi.c').read_text()
m=re.search(r'static void ResetPort\([^;{]*\)\s*\{',s);assert m
end=m.end();depth=1
while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
prefix=r'''
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef unsigned char UCHAR;typedef int BOOLEAN;typedef int KIRQL;
#define TRUE 1
#define FALSE 0
#define USB_OK 0
#define USB_RT_OUT 0
#define USB_RT_STD 0
#define USB_RT_DEVICE 0
#define USB_REQ_SET_CONFIGURATION 9
#define STATUS_NOT_SUPPORTED -20
#define STATUS_DEVICE_BUSY -21
#define USBD_STATUS_CANCELED -10
#define UD_PIPES 3
#define CONTAINING_RECORD(p,t,f) ((t*)((char*)(p)-offsetof(t,f)))
typedef struct List {struct List*Flink,*Blink;} LIST_ENTRY;
static void InitializeListHead(LIST_ENTRY*l){l->Flink=l->Blink=l;}
static int IsListEmpty(LIST_ENTRY*l){return l->Flink==l;}
static LIST_ENTRY*RemoveHeadList(LIST_ENTRY*l){LIST_ENTRY*n=l->Flink;l->Flink=n->Flink;n->Flink->Blink=l;return n;}
typedef struct Pipe {int Opened,Toggle,Halted,IsoNext,Resource,Active,Id;} USB_PIPE;
typedef struct {int(*PipeOpen)(USB_PIPE*);void(*PipeClose)(USB_PIPE*);} OPS;
typedef struct {OPS*Ops;} HC;
typedef struct {int NativeIf,IsHub,Index,ConfigValue;USB_PIPE Ep0;HC*Hc;} USB_DEV;
typedef struct {int Used;USB_PIPE P;} UD_PIPE;
typedef struct {void*F;UCHAR Alt,Num;} IFACE;
typedef struct Func {struct Func*Next;USB_DEV*Dev;int Dead,Resetting,ResetFailed,Config;UD_PIPE Pipe[UD_PIPES];IFACE Iface[32];} UD_FUNC;
typedef struct {LIST_ENTRY Link;} UD_REQ;
typedef struct {UD_FUNC*F;int St;} UD_JOB;
static UD_FUNC*Funcs;static struct {LIST_ENTRY Wait;int Reset,Retry;} Ep0[1];
static int UdLock,Kick,locked,fail,step,physical,configuration,alts,cancelled,aborted;
static int client_busy,client_gated,client_result;
static int native_mask,native_paused,native_result,native_restore_fail;
static int msc_mask,msc_paused,msc_result,msc_restore_fail;
static int crw_mask,crw_paused,crw_result,crw_restore_fail;
static int uvc_mask,uvc_paused,uvc_result,uvc_restore_fail;
static int audio_mask,audio_paused,audio_result,audio_restore_fail;
static int AudioResetInterfaces(USB_DEV*d){(void)d;return audio_mask;}
static void AudioBeginDeviceReset(USB_DEV*d){(void)d;assert(client_gated);audio_paused=1;}
static int AudioRestoreDevice(USB_DEV*d){(void)d;assert(audio_paused);return audio_restore_fail?-35:0;}
static void AudioEndDeviceReset(USB_DEV*d,int r){(void)d;assert(audio_paused);audio_result=r;audio_paused=0;}
static int UvcResetInterfaces(USB_DEV*d){(void)d;return uvc_mask;}
static void UvcBeginDeviceReset(USB_DEV*d){(void)d;assert(client_gated);uvc_paused=1;}
static int UvcRestoreDevice(USB_DEV*d){(void)d;assert(uvc_paused);return uvc_restore_fail?-34:0;}
static void UvcEndDeviceReset(USB_DEV*d,int r){(void)d;assert(uvc_paused);uvc_result=r;uvc_paused=0;}
static int RtsxUsbResetInterfaces(USB_DEV*d){(void)d;return crw_mask;}
static void RtsxUsbBeginDeviceReset(USB_DEV*d){(void)d;assert(client_gated);crw_paused=1;}
static int RtsxUsbRestoreDevice(USB_DEV*d){(void)d;assert(crw_paused);return crw_restore_fail?-33:0;}
static void RtsxUsbEndDeviceReset(USB_DEV*d,int r){(void)d;assert(crw_paused);crw_result=r;crw_paused=0;}
static int MscResetInterfaces(USB_DEV*d){(void)d;return msc_mask;}
static void MscBeginDeviceReset(USB_DEV*d){(void)d;assert(client_gated);msc_paused=1;}
static int MscRestoreDevice(USB_DEV*d){(void)d;assert(msc_paused);return msc_restore_fail?-32:0;}
static void MscEndDeviceReset(USB_DEV*d,int r){(void)d;assert(msc_paused);msc_result=r;msc_paused=0;}
static int HidResetInterfaces(USB_DEV*d){(void)d;return native_mask;}
static void HidBeginDeviceReset(USB_DEV*d){(void)d;assert(client_gated);native_paused=1;}
static int HidRestoreDevice(USB_DEV*d){(void)d;assert(native_paused);return native_restore_fail?-31:0;}
static void HidEndDeviceReset(USB_DEV*d,int r){(void)d;assert(native_paused);native_result=r;native_paused=0;}
static int CliBeginDeviceReset(USB_DEV*d){(void)d;assert(!client_gated);if(client_busy)return 0;client_gated=1;return 1;}
static void CliEndDeviceReset(USB_DEV*d,int result){(void)d;assert(client_gated);client_gated=0;client_result=result;}
static void KeAcquireSpinLock(int*l,KIRQL*old){assert(l==&UdLock&&!locked);locked=1;*old=0;}
static void KeReleaseSpinLock(int*l,KIRQL old){assert(l==&UdLock&&locked&&!old);locked=0;}
static int check(void){return ++step==fail?-30:0;}
static void gates(void){UD_FUNC*f;for(f=Funcs;f;f=f->Next)if(!f->Dead)assert(f->Resetting);}
static void AbortPipe(UD_PIPE*p){gates();assert(p->Used);p->P.Active=0;aborted++;}
static void close_pipe(USB_PIPE*p){assert(!locked);p->Opened=p->Resource=0;}
static void UsbClosePipe(USB_PIPE*p){if(p->Opened)close_pipe(p);}
static void Finish(UD_REQ*r,int status,LIST_ENTRY*done){(void)r;(void)done;assert(locked&&status==-10);cancelled++;}
static void CompleteList(LIST_ENTRY*done){(void)done;assert(!locked);}
static void UsbCancelPipe(USB_PIPE*p){gates();p->Active=0;}
static int UsbResetDevice(USB_DEV*d){UD_FUNC*f;int i;gates();assert(!d->Ep0.Active);
 for(f=Funcs;f;f=f->Next)if(!f->Dead)for(i=0;i<UD_PIPES;i++)if(f->Pipe[i].Used)assert(!f->Pipe[i].P.Opened&&!f->Pipe[i].P.Active&&!f->Pipe[i].P.Resource);
 assert(client_gated&&native_paused&&msc_paused&&crw_paused&&uvc_paused&&audio_paused);physical++;return check();}
static int UsbControl(USB_DEV*d,int rt,int req,int config,int index,int len,void*b,int timeout,void*done){
 (void)d;(void)rt;(void)index;(void)len;(void)b;(void)done;assert(physical&&req==9&&config==2&&timeout==1000);configuration++;return check();}
static int UsbSetInterface(USB_DEV*d,UCHAR num,UCHAR alt){(void)d;assert(configuration&&num<2&&alt==num+1);alts++;return check();}
static int open_pipe(USB_PIPE*p){assert(configuration&&alts==2&&!p->Resource&&!p->Toggle&&!p->Halted&&!p->IsoNext);p->Resource=1;return check();}
static void OsWakeWorker(void){assert(!locked&&Kick);}
static int UsbdStatus(int r,UD_FUNC*f){(void)f;return r;}
static int NtStatusOf(int r){return r;}
'''
main=r'''
int main(void){OPS ops={open_pipe,close_pipe};HC hc={&ops};USB_DEV dev;UD_FUNC funcs[3];UD_JOB job;UD_REQ request;int i,j,fault,cases=0;
 for(fault=0;fault<=8;fault++){
  memset(&dev,0,sizeof(dev));memset(funcs,0,sizeof(funcs));dev.Hc=&hc;dev.Ep0.Active=1;
  Funcs=funcs;for(i=0;i<3;i++){funcs[i].Dev=&dev;funcs[i].Next=i<2?&funcs[i+1]:0;funcs[i].Config=2;
   funcs[i].Iface[i].F=&funcs[i];funcs[i].Iface[i].Num=i;funcs[i].Iface[i].Alt=i+1;
   for(j=0;j<2;j++){USB_PIPE*p=&funcs[i].Pipe[j].P;funcs[i].Pipe[j].Used=1;p->Id=i*3+j;p->Opened=p->Resource=p->Active=p->Toggle=p->Halted=p->IsoNext=1;}}
  funcs[2].Dead=1;InitializeListHead(&Ep0[0].Wait);request.Link.Flink=request.Link.Blink=&Ep0[0].Wait;Ep0[0].Wait.Flink=Ep0[0].Wait.Blink=&request.Link;
  Ep0[0].Reset=Ep0[0].Retry=1;job.F=funcs;fail=fault;step=physical=configuration=alts=cancelled=aborted=Kick=0;
  ResetPort(&job);cases++;assert(job.St==(fault?-30:0)&&cancelled==1&&aborted==4&&!locked);
  assert(!client_gated&&client_result==job.St&&native_result==job.St&&msc_result==job.St&&crw_result==job.St&&uvc_result==job.St&&audio_result==job.St&&!native_paused&&!msc_paused&&!crw_paused&&!uvc_paused&&!audio_paused);
  assert(!Ep0[0].Reset&&!Ep0[0].Retry&&IsListEmpty(&Ep0[0].Wait));
  for(i=0;i<2;i++){assert(!funcs[i].Resetting&&funcs[i].ResetFailed==(fault!=0));for(j=0;j<2;j++){
   USB_PIPE*p=&funcs[i].Pipe[j].P;assert(funcs[i].Pipe[j].Used&&p->Id==i*3+j&&!p->Active);assert(p->Opened==!fault&&p->Resource==!fault);}}
  assert(funcs[2].Pipe[0].P.Active&&funcs[2].Pipe[0].P.Opened&&!funcs[2].Resetting);
  /* A second reset can recover the same public handles after any failure. */
  fail=step=physical=configuration=alts=cancelled=aborted=0;ResetPort(&job);assert(!job.St&&!funcs[0].ResetFailed&&!funcs[1].ResetFailed);
 }
 client_busy=1;step=0;ResetPort(&job);assert(job.St==STATUS_DEVICE_BUSY&&!step&&!client_gated);client_busy=0;
 dev.NativeIf=1;native_mask=1;fail=step=physical=configuration=alts=cancelled=aborted=0;ResetPort(&job);assert(!job.St);
 native_restore_fail=1;step=physical=configuration=alts=cancelled=aborted=0;ResetPort(&job);assert(job.St==-31&&native_result==-31&&funcs[0].ResetFailed);
 native_restore_fail=0;step=physical=configuration=alts=cancelled=aborted=0;ResetPort(&job);assert(!job.St&&!funcs[0].ResetFailed);
 dev.NativeIf=3;msc_mask=2;step=physical=configuration=alts=cancelled=aborted=0;ResetPort(&job);assert(!job.St);
 msc_restore_fail=1;step=physical=configuration=alts=cancelled=aborted=0;ResetPort(&job);assert(job.St==-32&&native_result==-32&&msc_result==-32&&funcs[0].ResetFailed);
 msc_restore_fail=0;step=physical=configuration=alts=cancelled=aborted=0;ResetPort(&job);assert(!job.St&&!funcs[0].ResetFailed);
 dev.NativeIf=7;crw_mask=4;step=physical=configuration=alts=cancelled=aborted=0;ResetPort(&job);assert(!job.St);
 crw_restore_fail=1;step=physical=configuration=alts=cancelled=aborted=0;ResetPort(&job);assert(job.St==-33&&native_result==-33&&msc_result==-33&&crw_result==-33&&funcs[0].ResetFailed);
 crw_restore_fail=0;step=physical=configuration=alts=cancelled=aborted=0;ResetPort(&job);assert(!job.St&&!funcs[0].ResetFailed);
 dev.NativeIf=15;uvc_mask=8;step=physical=configuration=alts=cancelled=aborted=0;ResetPort(&job);assert(!job.St);
 uvc_restore_fail=1;step=physical=configuration=alts=cancelled=aborted=0;ResetPort(&job);assert(job.St==-34&&native_result==-34&&msc_result==-34&&crw_result==-34&&uvc_result==-34&&funcs[0].ResetFailed);
 uvc_restore_fail=0;step=physical=configuration=alts=cancelled=aborted=0;ResetPort(&job);assert(!job.St&&!funcs[0].ResetFailed);
 dev.NativeIf=31;audio_mask=16;step=physical=configuration=alts=cancelled=aborted=0;ResetPort(&job);assert(!job.St);
 audio_restore_fail=1;step=physical=configuration=alts=cancelled=aborted=0;ResetPort(&job);assert(job.St==-35&&native_result==-35&&msc_result==-35&&crw_result==-35&&uvc_result==-35&&audio_result==-35&&funcs[0].ResetFailed);
 audio_restore_fail=0;step=physical=configuration=alts=cancelled=aborted=0;ResetPort(&job);assert(!job.St&&!funcs[0].ResetFailed);
 native_mask=msc_mask=crw_mask=uvc_mask=audio_mask=0;
 dev.NativeIf=1;step=0;ResetPort(&job);assert(job.St==STATUS_NOT_SUPPORTED&&!step);dev.NativeIf=0;dev.IsHub=1;ResetPort(&job);assert(job.St==STATUS_NOT_SUPPORTED&&!step);
 printf("PASS: %d sibling-function physical resets and retry recovery, every restore failure, unchanged handles, dead sibling and shared native-HID/MSC/card-reader/video/audio restore/failure with unsupported-owner isolation\n",cases);return 0;}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'test.c';exe=Path(td)/'test';p.write_text(prefix+s[m.start():end]+main)
 subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
