"""Production NT5 PCI IRP ownership, bounds and cancellation contracts.

Public DDK structure layout is checked by x86/x64 compilation separately.
This dispatcher checks behavior, including lower-driver completion ownership.
"""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[1]
code=(root/'RLTKLANT/src/native_pci.c').read_text()
code=code.replace('#include <ntddk.h>','').replace('#include "native_pci.h"','')
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef uint32_t ULONG;typedef uintptr_t ULONG_PTR;typedef int32_t NTSTATUS;typedef void*PVOID;
typedef struct{int signalled;}KEVENT;
typedef struct{int64_t QuadPart;}LARGE_INTEGER;
typedef struct{int StackSize;}DEVICE_OBJECT,*PDEVICE_OBJECT;
#define NTAPI
#define TRUE 1
#define FALSE 0
#define PASSIVE_LEVEL 0
#define NotificationEvent 0
#define IO_NO_INCREMENT 0
#define Executive 0
#define KernelMode 0
#define STATUS_SUCCESS 0
#define STATUS_UNSUCCESSFUL (-1)
#define STATUS_NOT_SUPPORTED (-2)
#define STATUS_MORE_PROCESSING_REQUIRED (-3)
#define STATUS_TIMEOUT 258
#define NT_SUCCESS(s) ((s)>=0)
#define IRP_MJ_PNP 27
#define IRP_MN_READ_CONFIG 15
#define IRP_MN_WRITE_CONFIG 16
#define PCI_WHICHSPACE_CONFIG 0
#define DevicePropertyAddress 1
typedef struct{unsigned MajorFunction,MinorFunction;struct{struct{unsigned WhichSpace,Offset,Length;void*Buffer;}ReadWriteConfig;}Parameters;}STACK,*PIO_STACK_LOCATION;
typedef struct IRP IRP,*PIRP;
struct IRP{struct{NTSTATUS Status;ULONG_PTR Information;}IoStatus;STACK Stack;NTSTATUS(*Complete)(PDEVICE_OBJECT,PIRP,void*);void*Context;int completed;};
static IRP*active;static unsigned irql,alloc_fail,mode,calls,cancelled,freed,waits;static ULONG property_address,property_length=4;static int property_fail;
static unsigned char config[256];
static unsigned refs;
static void ObReferenceObject(void*p){assert(p);refs++;}
static void ObDereferenceObject(void*p){assert(p&&refs);refs--;}
static unsigned KeGetCurrentIrql(void){return irql;}
static PIRP IoAllocateIrp(int depth,int charge){assert(depth==2&&!charge&&!active);return alloc_fail?0:calloc(1,sizeof(IRP));}
static PIO_STACK_LOCATION IoGetNextIrpStackLocation(PIRP i){return &i->Stack;}
static void KeInitializeEvent(KEVENT*e,int kind,int initial){assert(!kind&&!initial);e->signalled=0;}
static void KeSetEvent(KEVENT*e,int boost,int wait){assert(!boost&&!wait);e->signalled=1;}
static void IoSetCompletionRoutine(PIRP i,NTSTATUS(*fn)(PDEVICE_OBJECT,PIRP,void*),void*ctx,int a,int b,int d){assert(a&&b&&d);i->Complete=fn;i->Context=ctx;}
static void complete(void){
 IRP*i=active;STACK*s=&i->Stack;assert(i&&!i->completed);
 if(mode==5){i->IoStatus.Status=STATUS_NOT_SUPPORTED;i->IoStatus.Information=0;}
 else{
  unsigned n=s->Parameters.ReadWriteConfig.Length-(mode==4);
  unsigned off=s->Parameters.ReadWriteConfig.Offset;void*b=s->Parameters.ReadWriteConfig.Buffer;
  if(s->MinorFunction==IRP_MN_READ_CONFIG)memcpy(b,config+off,n);else memcpy(config+off,b,n);
  i->IoStatus.Status=STATUS_SUCCESS;i->IoStatus.Information=n;
 }
 i->completed=1;assert(i->Complete(0,i,i->Context)==STATUS_MORE_PROCESSING_REQUIRED);
}
static void IoCallDriver(PDEVICE_OBJECT p,PIRP i){assert(p&&p->StackSize==2&&!active);active=i;calls++;
 assert(i->Stack.MajorFunction==IRP_MJ_PNP&&!i->Stack.Parameters.ReadWriteConfig.WhichSpace);
 assert(i->Stack.MinorFunction==IRP_MN_READ_CONFIG||i->Stack.MinorFunction==IRP_MN_WRITE_CONFIG);
 assert(i->Stack.Parameters.ReadWriteConfig.Offset+i->Stack.Parameters.ReadWriteConfig.Length<=256);
 if(mode==0||mode==4||mode==5)complete();
}
static NTSTATUS KeWaitForSingleObject(KEVENT*e,int reason,int kernel,int alert,LARGE_INTEGER*timeout){
 assert(!reason&&!kernel&&!alert&&active);waits++;
 if(timeout){assert(timeout->QuadPart==-5000000LL);if(mode==2||mode==3)return STATUS_TIMEOUT;if(mode==6)return STATUS_UNSUCCESSFUL;}
 else assert(cancelled);
 if(!e->signalled)complete();
 assert(e->signalled);return STATUS_SUCCESS;
}
static void IoCancelIrp(PIRP i){assert(i==active);cancelled++;if(mode==3)complete();}
static void IoFreeIrp(PIRP i){assert(i==active&&i->completed);active=0;freed++;free(i);}
static NTSTATUS IoGetDeviceProperty(void*p,int property,unsigned bytes,void*buffer,ULONG*length){assert(p&&property==DevicePropertyAddress&&bytes==4);*(ULONG*)buffer=property_address;*length=property_length;return property_fail?-1:0;}
'''
tests=r'''
int main(void){
 DEVICE_OBJECT dev={2};unsigned width,off,write,value,count=0;unsigned char function;
 assert(!RtlNativePciRetain(0));assert(RtlNativePciRetain(&dev)==&dev&&refs==1);RtlNativePciRelease(&dev);RtlNativePciRelease(0);assert(!refs);
 for(mode=0;mode<7;mode++)for(write=0;write<2;write++)for(width=1;width<=4;width*=2)for(off=0;off+width<=256;off+=width){
  unsigned before=calls;int success=mode==0||mode==1;value=0x89abcdefU;waits=cancelled=0;
  memset(config,0x5a,sizeof(config));
  assert(RtlNativePciConfig(&dev,write,off,width,&value)==success);assert(calls==before+1&&!active&&freed==calls);
  if(success){if(write){unsigned actual=0;memcpy(&actual,config+off,width);assert(actual==(0x89abcdefU & (0xffffffffU>>(32-8*width))));}
   else assert(value==(0x5a5a5a5aU & (0xffffffffU>>(32-8*width))));}
  assert(cancelled==((mode==2||mode==3||mode==6)?1U:0U));count++;
 }
 mode=0;
 for(width=0;width<8;width++)for(off=0;off<260;off++)if((width!=1&&width!=2&&width!=4)||off%width||off>256-width){
  unsigned before=calls;assert(!RtlNativePciConfig(&dev,0,off,width,&value)&&calls==before);}
 alloc_fail=1;assert(!RtlNativePciConfig(&dev,0,0,4,&value));alloc_fail=0;
 irql=2;assert(!RtlNativePciConfig(&dev,0,0,4,&value));irql=0;
 assert(!RtlNativePciConfig(0,0,0,4,&value)&&!RtlNativePciConfig(&dev,0,0,4,0));
 for(off=0;off<32;off++)for(width=0;width<8;width++){property_address=(off<<16)|width;assert(RtlNativePciFunction(&dev,&function)&&function==width);}
 property_address=8;assert(!RtlNativePciFunction(&dev,&function));property_address=32<<16;assert(!RtlNativePciFunction(&dev,&function));
 property_address=0;property_length=2;assert(!RtlNativePciFunction(&dev,&function));property_length=4;property_fail=1;assert(!RtlNativePciFunction(&dev,&function));
 printf("PASS: %u PCI config schedules: sync/pending, short/error, timeout/late/racing cancellation; bounds/IRQL/allocation/function guards; no free before completion\n",count);
}
'''
with tempfile.TemporaryDirectory(prefix='rltk-native-pci-') as td:
    p=Path(td)/'test.c';p.write_text(prefix+code+tests);exe=Path(td)/'test'
    subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(p),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
