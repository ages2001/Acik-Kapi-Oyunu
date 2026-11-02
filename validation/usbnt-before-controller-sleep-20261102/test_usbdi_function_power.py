"""Exercise production function-power rundown and dispatch with sibling queues.

Host-controller completions are mocked; this does not test system sleep or
electrical port suspend. Run with ASan/UBSan enabled.
"""
from pathlib import Path
import re
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / 'USBNT/src/usbdi.c').read_text()

def function(name):
    match = re.search(r'(?:static )?(?:void|NTSTATUS) ' + name + r'\([^;{]*\)\s*\{', source)
    assert match, name
    end, depth = match.end(), 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end]

prefix = r'''
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
typedef unsigned long ULONG; typedef int NTSTATUS, KIRQL, BOOLEAN, DEVICE_POWER_STATE;
#define TRUE 1
#define FALSE 0
#define STATUS_SUCCESS 0
#define STATUS_UNSUCCESSFUL -1
#define STATUS_DEVICE_NOT_READY -2
#define STATUS_NO_SUCH_DEVICE -3
#define STATUS_NOT_SUPPORTED -4
#define STATUS_INVALID_PARAMETER -5
#define STATUS_INSUFFICIENT_RESOURCES -6
#define STATUS_PENDING 1
#define USBD_STATUS_CANCELED -7
#define USBD_STATUS_DEVICE_GONE -8
#define USBD_STATUS_XACT_ERROR -9
#define PowerDeviceD0 1
#define PowerDeviceD3 4
#define PowerSystemUnspecified 0
#define PowerSystemWorking 1
#define PowerSystemMaximum 7
#define DevicePowerState 1
#define SystemPowerState 0
#define IRP_MN_SET_POWER 2
#define IRP_MN_QUERY_POWER 3
#define IRP_MN_WAIT_WAKE 0
#define UD_PIPES 2
#define UD_JOB_POWER ((ULONG)-1)
#define IOCTL_INTERNAL_USB_RESET_PORT 20
#define URB_FUNCTION_SELECT_CONFIGURATION 0
#define URB_FUNCTION_SELECT_INTERFACE 1
#define IO_NO_INCREMENT 0
#define CONTAINING_RECORD(p,t,f) ((t*)((char*)(p)-offsetof(t,f)))
typedef struct List {struct List *Flink,*Blink;} LIST_ENTRY,*PLIST_ENTRY;
static void InitializeListHead(LIST_ENTRY*l){l->Flink=l->Blink=l;}
static void InsertTailList(LIST_ENTRY*h,LIST_ENTRY*l){l->Flink=h;l->Blink=h->Blink;h->Blink->Flink=l;h->Blink=l;}
static void RemoveEntryList(LIST_ENTRY*l){l->Blink->Flink=l->Flink;l->Flink->Blink=l->Blink;}
typedef union {int DeviceState,SystemState;} POWER_STATE;
typedef struct {int MinorFunction;struct {struct {int Type;POWER_STATE State;} Power;} Parameters;} STACK,*PIO_STACK_LOCATION;
typedef struct {STACK Stack;struct {int Status;} IoStatus;int completed,next;} IRP,*PIRP;
static PIO_STACK_LOCATION IoGetCurrentIrpStackLocation(PIRP irp){return &irp->Stack;}
typedef struct {int owner,active,opened,toggle;} USB_PIPE;
typedef struct {int Index;USB_PIPE Ep0;} USB_DEV;
typedef struct {int Used;USB_PIPE P;} UD_PIPE;
typedef struct Func {struct Func*Next;USB_DEV*Dev;int Dead,Suspended,Resetting,ResetFailed;void*Pdo;UD_PIPE Pipe[UD_PIPES];} UD_FUNC;
typedef struct {LIST_ENTRY Link;UD_FUNC*F;int finished;} UD_REQ;
typedef struct {struct {int Status,Function;} UrbHeader;} URB,*PURB;
typedef struct {UD_FUNC*F;PIRP Irp;PURB Urb;ULONG Code;NTSTATUS St;} UD_JOB;
static UD_FUNC*Funcs;static struct {LIST_ENTRY Wait;UD_REQ*Active;} Ep0[1];
static int UdLock,locked,Kick,aborts,ep0cancels,finished,power_updates,power_state,hardware_jobs;
static void KeAcquireSpinLock(int*l,KIRQL*irql){assert(l==&UdLock&&!locked);locked=1;*irql=0;}
static void KeReleaseSpinLock(int*l,KIRQL irql){assert(l==&UdLock&&locked&&!irql);locked=0;}
static void Finish(UD_REQ*r,int status,LIST_ENTRY*done){(void)done;assert(locked&&status==USBD_STATUS_CANCELED&&!r->finished);r->finished=1;finished++;}
static void CompleteList(LIST_ENTRY*done){(void)done;assert(!locked);}
static void gated(void){UD_FUNC*f;for(f=Funcs;f;f=f->Next)if(!f->Dead)assert(f->Resetting);}
static void AbortPipe(UD_PIPE*p){assert(!locked&&p->Used&&p->P.owner==0);gated();p->P.active=0;aborts++;}
static void UsbCancelPipe(USB_PIPE*p){assert(!locked&&p==&Funcs->Dev->Ep0);gated();assert(!Ep0[0].Active||Ep0[0].Active->F==Funcs);p->active=0;Ep0[0].Active=NULL;ep0cancels++;}
static void OsWakeWorker(void){assert(!locked&&Kick);}
static void PoSetPowerState(void*pdo,int type,POWER_STATE state){assert(pdo==Funcs&&type==DevicePowerState&&!locked);power_state=state.DeviceState;power_updates++;}
static void PoStartNextPowerIrp(PIRP irp){assert(!irp->next&&!irp->completed);irp->next++;}
static void IoCompleteRequest(PIRP irp,int boost){assert(!boost&&irp->next==1&&!irp->completed);irp->completed++;}
static void ResetPort(UD_JOB*j){(void)j;hardware_jobs++;}
static void SelectConfig(UD_JOB*j){(void)j;hardware_jobs++;}
static void SelectIface(UD_JOB*j){(void)j;hardware_jobs++;}
static void PipeRequest(UD_JOB*j){(void)j;hardware_jobs++;}
static NTSTATUS Job(UD_FUNC*,PIRP,PURB,ULONG);
'''

job = r'''
/* Dispatch test executes the real worker body. Async IRP allocation is not
 * represented by this host mock; it is covered by the WDM build/guest tests. */
static NTSTATUS Job(UD_FUNC*f,PIRP irp,PURB urb,ULONG code){UD_JOB j={f,irp,urb,code,99};JobW(&j);irp->IoStatus.Status=j.St;PoStartNextPowerIrp(irp);IoCompleteRequest(irp,0);return j.St;}
'''

main = r'''
static IRP power(int minor,int type,int state){IRP irp;memset(&irp,0,sizeof(irp));irp.Stack.MinorFunction=minor;irp.Stack.Parameters.Power.Type=type;irp.Stack.Parameters.Power.State.DeviceState=state;return irp;}
int main(void){USB_DEV dev;UD_FUNC f[3];UD_REQ requests[3],active;IRP irp;URB urb;UD_JOB j;int owner,i,k,cases=0;
 for(owner=0;owner<2;owner++){
  memset(&dev,0,sizeof(dev));memset(f,0,sizeof(f));memset(requests,0,sizeof(requests));memset(&active,0,sizeof(active));
  Funcs=f;for(i=0;i<3;i++){f[i].Dev=&dev;f[i].Pdo=&f[i];f[i].Next=i<2?&f[i+1]:NULL;
   for(k=0;k<UD_PIPES;k++){f[i].Pipe[k].Used=1;f[i].Pipe[k].P.owner=i;f[i].Pipe[k].P.active=f[i].Pipe[k].P.opened=f[i].Pipe[k].P.toggle=1;}}
  f[2].Dead=1;InitializeListHead(&Ep0[0].Wait);for(i=0;i<3;i++){requests[i].F=&f[i==1?1:0];InsertTailList(&Ep0[0].Wait,&requests[i].Link);}
  active.F=&f[owner];Ep0[0].Active=&active;dev.Ep0.active=1;
  aborts=ep0cancels=finished=power_updates=hardware_jobs=0;
  irp=power(IRP_MN_SET_POWER,DevicePowerState,PowerDeviceD3);assert(UsbdiPower(f,&irp)==STATUS_SUCCESS);
  assert(irp.completed==1&&irp.next==1&&power_state==PowerDeviceD3&&power_updates==1);
  assert(f[0].Suspended&&!f[1].Suspended&&!f[2].Suspended);
  assert(!f[0].Resetting&&!f[1].Resetting&&!f[2].Resetting&&aborts==2&&finished==2);
  assert(ep0cancels==!owner&&dev.Ep0.active==owner);
  assert(Ep0[0].Wait.Flink==&requests[1].Link&&requests[1].Link.Flink==&Ep0[0].Wait&&!requests[1].finished);
  for(i=0;i<3;i++)for(k=0;k<UD_PIPES;k++){assert(f[i].Pipe[k].P.opened&&f[i].Pipe[k].P.toggle);assert(f[i].Pipe[k].P.active==(i!=0));}
  memset(&urb,0,sizeof(urb));j.F=f;j.Irp=&irp;j.Urb=&urb;j.Code=0;JobW(&j);assert(j.St==STATUS_DEVICE_NOT_READY&&!hardware_jobs&&urb.UrbHeader.Status==USBD_STATUS_XACT_ERROR);
  j.Urb=NULL;j.Code=IOCTL_INTERNAL_USB_RESET_PORT;JobW(&j);assert(j.St==STATUS_DEVICE_NOT_READY&&!hardware_jobs);
  f[0].ResetFailed=1;irp=power(IRP_MN_SET_POWER,DevicePowerState,PowerDeviceD0);assert(UsbdiPower(f,&irp)==STATUS_SUCCESS&&!f[0].Suspended&&f[0].ResetFailed);
  /* D0 must permit the recovery request, without pretending reset succeeded. */
  JobW(&j);assert(hardware_jobs==1);f[0].ResetFailed=0;
  assert(power_state==PowerDeviceD0&&power_updates==2&&aborts==2&&finished==2);
  for(i=0;i<6;i++)if(i!=PowerDeviceD0&&i!=PowerDeviceD3){irp=power(IRP_MN_SET_POWER,DevicePowerState,i);assert(UsbdiPower(f,&irp)==STATUS_NOT_SUPPORTED&&irp.completed==1);cases++;}
  irp=power(IRP_MN_QUERY_POWER,DevicePowerState,PowerDeviceD3);assert(UsbdiPower(f,&irp)==STATUS_SUCCESS&&!f[0].Suspended&&power_updates==2);
  irp=power(IRP_MN_WAIT_WAKE,DevicePowerState,PowerDeviceD3);assert(UsbdiPower(f,&irp)==STATUS_NOT_SUPPORTED);
  irp=power(IRP_MN_SET_POWER,SystemPowerState,PowerSystemWorking);assert(UsbdiPower(f,&irp)==STATUS_SUCCESS&&power_updates==2);
  irp=power(IRP_MN_SET_POWER,SystemPowerState,PowerSystemMaximum);assert(UsbdiPower(f,&irp)==STATUS_INVALID_PARAMETER);
  f[0].Dead=1;irp=power(IRP_MN_SET_POWER,DevicePowerState,PowerDeviceD0);assert(UsbdiPower(f,&irp)==STATUS_NO_SUCH_DEVICE&&power_updates==2);
  cases+=8;
 }
 printf("PASS: %d production function-power cases; own/shared EP0, queued/active rundown, sibling isolation, preserved contexts/toggles, D0 reset recovery, invalid states, removal and IRP completion\n",cases);return 0;}
'''

with tempfile.TemporaryDirectory() as temp:
    path, exe = Path(temp)/'power.c', Path(temp)/'power'
    path.write_text(prefix + function('SetFunctionPower') + function('JobW') + job + function('UsbdiPower') + main)
    subprocess.run(['gcc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer', str(path), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
