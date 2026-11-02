"""Fault-inject actual controller power continuations with checked IRQL/lifetimes.

The host harness mocks the NT power manager and lower PCI stack. It tests
ordering/error propagation, not an ACPI sleep implementation or hardware.
"""
from pathlib import Path
import subprocess, tempfile
source=(Path(__file__).resolve().parents[1]/'USBNT/src/wdmmain.c').read_text()
start=source.index('typedef struct _HC_POWER')
end=source.index('/* ------------------------------------------------------------------ */\n/* Dispatch',start)
production=source[start:end]
prefix=r'''
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
typedef int NTSTATUS,BOOLEAN,DEVICE_POWER_STATE,SYSTEM_POWER_STATE,KEVENT;
typedef unsigned char UCHAR;typedef void VOID,*PVOID;
#define NTAPI
#define TRUE 1
#define FALSE 0
#define STATUS_SUCCESS 0
#define STATUS_PENDING 0x103
#define STATUS_MORE_PROCESSING_REQUIRED -100
#define STATUS_NO_SUCH_DEVICE -1
#define STATUS_IO_DEVICE_ERROR -2
#define STATUS_INVALID_PARAMETER -3
#define STATUS_NOT_SUPPORTED -4
#define STATUS_INSUFFICIENT_RESOURCES -5
#define NT_SUCCESS(s) ((s)>=0)
#define PowerDeviceD0 1
#define PowerDeviceD3 4
#define PowerSystemUnspecified 0
#define PowerSystemWorking 1
#define PowerSystemMaximum 7
#define SystemPowerState 0
#define DevicePowerState 1
#define IRP_MN_SET_POWER 2
#define IRP_MN_QUERY_POWER 3
#define EXT_HC 10
#define NotificationEvent 0
#define Executive 0
#define KernelMode 0
#define IO_NO_INCREMENT 0
#define DelayedWorkQueue 0
#define USB_OK 0
#define LOG_ERR 1
#define UsbLog(...) ((void)0)
typedef union {int DeviceState,SystemState;} POWER_STATE;
typedef struct {int Status;unsigned long Information;} IO_STATUS_BLOCK,*PIO_STATUS_BLOCK;
typedef struct {int MinorFunction;struct {struct {int Type;POWER_STATE State;} Power;} Parameters;} STACK,*PIO_STACK_LOCATION;
typedef struct Device {int unused;} DEVICE,*PDEVICE_OBJECT;
typedef struct Irp IRP,*PIRP;
typedef NTSTATUS (*COMPLETE)(PDEVICE_OBJECT,PIRP,PVOID);
struct Irp {STACK Stack;IO_STATUS_BLOCK IoStatus;int PendingReturned,marked,next,completed;COMPLETE completion;void*context;};
typedef struct {struct {int Kind;PDEVICE_OBJECT Self;} Nt;PDEVICE_OBJECT Pdo,Lower;int PowerLock,Started,Sleeping,Removed,Gone;} WDM_EXT;
typedef struct {WDM_EXT*X;int Gone,Settle,Result;} HC_ARG;
typedef struct Work {PDEVICE_OBJECT dev;int queued;} WORK,*PIO_WORKITEM;
static int irql,lower_status,lower_pending,start_result,stop_calls,start_calls,lower_calls,power_updates;
static int alloc_fail,work_fail,allocs,works,request_status,request_inline,request_completion;
static int requested_state,requested_minor,requests,sequence[32],sequence_count;
static void (*queued)(PDEVICE_OBJECT,PVOID);static void*queued_context;static PDEVICE_OBJECT queued_dev;
typedef void (*POWER_DONE)(PDEVICE_OBJECT,UCHAR,POWER_STATE,PVOID,PIO_STATUS_BLOCK);
static POWER_DONE requested_done;static void*requested_context;
static void record(int n){assert(sequence_count<32);sequence[sequence_count++]=n;}
static void*OsAlloc(size_t n){void*p;if(alloc_fail)return NULL;p=calloc(1,n);assert(p);allocs++;return p;}
static void OsFree(void*p){assert(p&&allocs>0);allocs--;free(p);}
static PIO_STACK_LOCATION IoGetCurrentIrpStackLocation(PIRP p){return &p->Stack;}
static void IoCopyCurrentIrpStackLocationToNext(PIRP p){(void)p;}
static void IoSkipCurrentIrpStackLocation(PIRP p){(void)p;}
static void IoSetCompletionRoutine(PIRP p,COMPLETE c,void*x,int a,int b,int d){assert(a&&b&&d);p->completion=c;p->context=x;}
static void PoStartNextPowerIrp(PIRP p){assert(!p->next&&!p->completed);p->next++;}
static void IoCompleteRequest(PIRP p,int boost){assert(!boost&&!p->completed&&p->next);p->completed++;record(5);}
static NTSTATUS WdmComplete(PIRP p,NTSTATUS s,int info){p->IoStatus.Status=s;p->IoStatus.Information=info;IoCompleteRequest(p,0);return s;}
static NTSTATUS IoAcquireRemoveLock(int*l,PIRP p){(void)p;if(*l<0)return STATUS_NO_SUCH_DEVICE;(*l)++;return 0;}
static void IoReleaseRemoveLock(int*l,PIRP p){(void)p;assert(*l>0);(*l)--;}
static void IoMarkIrpPending(PIRP p){p->marked=1;}
static void KeInitializeEvent(KEVENT*e,int type,int state){assert(type==0&&!state);*e=0;}
static void KeWaitForSingleObject(KEVENT*e,int a,int b,int c,void*d){assert(!irql&&*e&&!a&&!b&&!c&&!d);}
static NTSTATUS SyncDone(PDEVICE_OBJECT d,PIRP p,void*c){(void)d;(void)p;*(KEVENT*)c=1;return STATUS_MORE_PROCESSING_REQUIRED;}
static NTSTATUS PoCallDriver(PDEVICE_OBJECT d,PIRP p){NTSTATUS c=0;assert(d);lower_calls++;record(2);p->IoStatus.Status=lower_status;p->PendingReturned=lower_pending;
 if(p->completion)c=p->completion(d,p,p->context);if(c!=STATUS_MORE_PROCESSING_REQUIRED)IoCompleteRequest(p,0);return lower_pending?STATUS_PENDING:lower_status;}
static PIO_WORKITEM IoAllocateWorkItem(PDEVICE_OBJECT d){PIO_WORKITEM w;if(work_fail)return NULL;w=calloc(1,sizeof(*w));assert(w);w->dev=d;works++;return w;}
static void IoFreeWorkItem(PIO_WORKITEM w){assert(w&&works);works--;free(w);}
static void IoQueueWorkItem(PIO_WORKITEM w,void(*fn)(PDEVICE_OBJECT,PVOID),int queue,void*c){assert(w&&!queued&&!queue);w->queued=1;queued=fn;queued_context=c;queued_dev=w->dev;}
static void HcStopW(void*c){HC_ARG*a=c;assert(!irql&&!a->Gone);stop_calls++;record(1);}
static void HcStartW(void*c){HC_ARG*a=c;assert(!irql&&a->Settle);start_calls++;a->Result=start_result;record(3);}
static void WdmRunInWorker(void(*f)(void*),void*c){assert(!irql);f(c);}
static void PoSetPowerState(PDEVICE_OBJECT d,int type,POWER_STATE s){assert(!irql&&d&&type==DevicePowerState&&(s.DeviceState==1||s.DeviceState==4));power_updates++;record(4);}
static NTSTATUS PoRequestPowerIrp(PDEVICE_OBJECT d,UCHAR minor,POWER_STATE s,POWER_DONE done,void*c,void*out){IO_STATUS_BLOCK status={request_completion,0};assert(d&&!out);requests++;requested_state=s.DeviceState;requested_minor=minor;record(6);
 if(NT_SUCCESS(request_status)){if(request_inline)done(d,minor,s,c,&status);else{requested_done=done;requested_context=c;}}return request_status;}
'''
main=r'''
static DEVICE self,lower,pdo;
static WDM_EXT x;
static IRP p;
static int cases;
static void reset(int type,int state,int minor){assert(!allocs&&!works&&!queued&&!requested_done);memset(&x,0,sizeof(x));memset(&p,0,sizeof(p));x.Nt.Kind=EXT_HC;x.Nt.Self=&self;x.Pdo=&pdo;x.Lower=&lower;x.Started=1;
 p.Stack.MinorFunction=minor;p.Stack.Parameters.Power.Type=type;p.Stack.Parameters.Power.State.DeviceState=state;
 irql=2;lower_status=lower_pending=start_result=stop_calls=start_calls=lower_calls=power_updates=0;
 alloc_fail=work_fail=request_inline=request_completion=requests=sequence_count=0;request_status=STATUS_PENDING;cases++;}
static void work(void){void(*f)(PDEVICE_OBJECT,PVOID)=queued;assert(f&&x.PowerLock==1&&p.marked&&!p.completed);queued=NULL;irql=0;f(queued_dev,queued_context);irql=2;}
static void finish(int status){assert(p.completed==1&&p.next==1&&p.IoStatus.Status==status&&!x.PowerLock&&!allocs&&!works);}
static void device_done(int status){IO_STATUS_BLOCK s={status,0};POWER_STATE ps;POWER_DONE f=requested_done;void*c=requested_context;assert(f&&x.PowerLock==1&&!p.completed);requested_done=NULL;ps.DeviceState=requested_state;f(&pdo,requested_minor,ps,c,&s);}
int main(void){int pending,state,minor,bad;
 for(pending=0;pending<2;pending++){
  reset(DevicePowerState,PowerDeviceD3,IRP_MN_SET_POWER);lower_pending=pending;assert(FdoPower(&x,&p)==STATUS_PENDING&&!stop_calls&&!lower_calls);work();finish(0);assert(x.Sleeping&&stop_calls==1&&!start_calls&&power_updates==1&&sequence[0]==1&&sequence[1]==2);
  reset(DevicePowerState,PowerDeviceD0,IRP_MN_SET_POWER);x.Sleeping=1;lower_pending=pending;assert(FdoPower(&x,&p)==STATUS_PENDING);work();finish(0);assert(!x.Sleeping&&start_calls==1&&sequence[0]==2&&sequence[1]==3);
  reset(DevicePowerState,PowerDeviceD0,IRP_MN_SET_POWER);x.Sleeping=1;lower_status=-90;lower_pending=pending;assert(FdoPower(&x,&p)==STATUS_PENDING);work();finish(-90);assert(x.Sleeping&&!start_calls&&!power_updates);
  reset(DevicePowerState,PowerDeviceD0,IRP_MN_SET_POWER);x.Sleeping=1;start_result=-1;assert(FdoPower(&x,&p)==STATUS_PENDING);work();finish(STATUS_IO_DEVICE_ERROR);assert(x.Sleeping&&!power_updates);
  reset(DevicePowerState,PowerDeviceD3,IRP_MN_SET_POWER);lower_status=-91;lower_pending=pending;assert(FdoPower(&x,&p)==STATUS_PENDING);work();finish(-91);assert(!x.Sleeping&&start_calls==1&&stop_calls==1&&!power_updates);
  reset(DevicePowerState,PowerDeviceD3,IRP_MN_SET_POWER);lower_status=-91;start_result=-1;assert(FdoPower(&x,&p)==STATUS_PENDING);work();finish(-91);assert(x.Sleeping&&!power_updates);
 }
 for(state=PowerDeviceD0;state<=PowerDeviceD3;state+=3){
  reset(DevicePowerState,state,IRP_MN_SET_POWER);alloc_fail=1;assert(FdoPower(&x,&p)==STATUS_INSUFFICIENT_RESOURCES);finish(STATUS_INSUFFICIENT_RESOURCES);assert(!lower_calls&&!stop_calls);
  reset(DevicePowerState,state,IRP_MN_SET_POWER);work_fail=1;assert(FdoPower(&x,&p)==STATUS_INSUFFICIENT_RESOURCES);finish(STATUS_INSUFFICIENT_RESOURCES);assert(!lower_calls);
  reset(DevicePowerState,state,IRP_MN_SET_POWER);assert(FdoPower(&x,&p)==STATUS_PENDING);x.Removed=1;work();finish(STATUS_NO_SUCH_DEVICE);assert(!lower_calls&&!stop_calls&&!start_calls);
  reset(DevicePowerState,state,IRP_MN_QUERY_POWER);assert(FdoPower(&x,&p)==0);finish(0);assert(!stop_calls&&!start_calls&&lower_calls==1);
 }
 for(minor=IRP_MN_SET_POWER;minor<=IRP_MN_QUERY_POWER;minor++)for(state=1;state<PowerSystemMaximum;state++){
  reset(SystemPowerState,state,minor);assert(FdoPower(&x,&p)==STATUS_PENDING);assert(requests==1&&requested_minor==minor&&requested_state==(state==1?1:4)&&!queued&&!start_calls&&!stop_calls);device_done(0);finish(0);
  reset(SystemPowerState,state,minor);request_inline=1;request_completion=-92;assert(FdoPower(&x,&p)==STATUS_PENDING);finish(-92);
  reset(SystemPowerState,state,minor);lower_status=-93;assert(FdoPower(&x,&p)==STATUS_PENDING);finish(-93);assert(!requests);
  reset(SystemPowerState,state,minor);request_status=-94;assert(FdoPower(&x,&p)==STATUS_PENDING);finish(-94);assert(requests==1);
 }
 for(bad=0;bad<7;bad++){if(bad==1||bad==4)continue;reset(DevicePowerState,bad,IRP_MN_SET_POWER);assert(FdoPower(&x,&p)==STATUS_NOT_SUPPORTED);finish(STATUS_NOT_SUPPORTED);assert(!lower_calls);}
 reset(SystemPowerState,PowerSystemMaximum,IRP_MN_SET_POWER);assert(FdoPower(&x,&p)==STATUS_INVALID_PARAMETER);finish(STATUS_INVALID_PARAMETER);
 reset(SystemPowerState,0,IRP_MN_SET_POWER);assert(FdoPower(&x,&p)==STATUS_INVALID_PARAMETER);finish(STATUS_INVALID_PARAMETER);
 reset(17,0,IRP_MN_SET_POWER);assert(FdoPower(&x,&p)==STATUS_INVALID_PARAMETER);finish(STATUS_INVALID_PARAMETER);
 reset(DevicePowerState,1,IRP_MN_SET_POWER);x.Gone=1;assert(FdoPower(&x,&p)==STATUS_NO_SUCH_DEVICE);finish(STATUS_NO_SUCH_DEVICE);assert(!lower_calls);
 reset(DevicePowerState,1,IRP_MN_SET_POWER);x.Started=0;lower_pending=1;assert(FdoPower(&x,&p)==STATUS_PENDING);finish(0);assert(!start_calls&&p.marked);
 printf("PASS: %d production controller power cases: DISPATCH deferral, lower failure, failed resume/rollback, allocation failure, system-to-device mapping, inline callbacks, removal lifetime, exact completion\n",cases);return 0;}
'''
with tempfile.TemporaryDirectory() as temp:
    p=Path(temp)/'test.c';exe=Path(temp)/'test'
    p.write_text(prefix+production+main)
    subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(p),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
