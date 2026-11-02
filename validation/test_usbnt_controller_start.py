"""Exercise the actual START_DEVICE case: failed hardware must not publish a bus."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'USBNT/src/wdmmain.c').read_text();start=s.index('    case IRP_MN_START_DEVICE:',s.index('static NTSTATUS HcPnp('));end=s.index('    case IRP_MN_QUERY_DEVICE_RELATIONS:',start);body=s[start:end]
code=r'''
#include <assert.h>
#include <stdio.h>
typedef int BOOLEAN,NTSTATUS;
#define TRUE 1
#define FALSE 0
#define USB_OK 0
#define STATUS_SUCCESS 0
#define STATUS_IO_DEVICE_ERROR -10
#define NT_SUCCESS(s) ((s)>=0)
#define IRP_MN_START_DEVICE 0
#define LOG_INFO 0
#define UsbLog(...) ((void)0)
typedef struct {int Claimed,Started,Sleeping,Bus,Dev,Fn;void*Lower,*Pdo;} WDM_EXT;
typedef struct {WDM_EXT*X;int Gone,Settle,Result;} HC_ARG;
static int FirstStart,lower_status,worker_result,worker_calls,pci_valid,published,named,completed;
static int WdmForwardSync(void*lower,void*irp){(void)lower;(void)irp;return lower_status;}
static int WdmComplete(void*irp,int status,int info){(void)irp;assert(!info);completed++;return status;}
static int PciAddress(void*p,int*b,int*d,int*f){(void)p;*b=1;*d=2;*f=3;return pci_valid;}
static int OsTimeMs(void){return 5;}
static void HcStartW(void*p){HC_ARG*a=p;assert(a->Settle&&a->X);a->Result=worker_result;worker_calls++;}
static void WdmRunInWorker(void(*fn)(void*),void*p){fn(p);}
static void WdmBusOnController(WDM_EXT*x){assert(x->Started);published++;}
static void WdmTreeHcStarted(WDM_EXT*x){assert(x->Started);named++;}
static NTSTATUS start_case(WDM_EXT*x){HC_ARG a={x,FALSE,FALSE,99};NTSTATUS st;void*Irp=NULL;switch(0){
'''+body+r'''
}return -99;}
int main(void){WDM_EXT x;int fail,claimed,cases=0;
for(fail=0;fail<4;fail++)for(claimed=0;claimed<2;claimed++){
 x=(WDM_EXT){0};x.Claimed=claimed;x.Started=claimed;x.Sleeping=1;
 lower_status=0;worker_result=-fail;pci_valid=1;worker_calls=published=named=completed=0;
 assert(start_case(&x)==((fail&&!claimed)?STATUS_IO_DEVICE_ERROR:0));assert(completed==1);
 if(claimed){assert(!worker_calls&&!published&&!named&&x.Started);}
 else{assert(worker_calls==1);assert(x.Started==!fail&&published==!fail&&named==!fail&&x.Sleeping==!!fail);}
 cases++;
}
x=(WDM_EXT){0};lower_status=-20;worker_calls=published=named=completed=0;assert(start_case(&x)==-20&&completed==1&&!worker_calls&&!published&&!named&&!x.Started);cases++;
x=(WDM_EXT){0};lower_status=0;pci_valid=0;worker_calls=published=named=completed=FirstStart=0;assert(start_case(&x)==0&&completed==1&&!worker_calls&&!published&&!named&&x.Bus==255&&FirstStart);cases++;
printf("PASS: %d actual START_DEVICE cases; lower failures, failed controller start, claimed controller and ACPI path\n",cases);return 0;}
'''
with tempfile.TemporaryDirectory() as temp:
 p=Path(temp)/'start.c';exe=Path(temp)/'start';p.write_text(code)
 subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
