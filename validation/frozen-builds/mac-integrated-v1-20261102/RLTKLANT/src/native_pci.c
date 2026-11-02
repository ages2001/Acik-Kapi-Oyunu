/* Use the owning PCI bus's PnP configuration IRPs. In particular, do not
 * guess an x64 BDF or use global CF8/CFC accesses from a PnP miniport.
 * This file is built against public NT5 DDK layouts, like native_urb.c. */
#define _WIN32_WINNT 0x0500
#define NTDDI_VERSION 0x05000000
typedef union {unsigned long ContextAsUlong;} SYSTEM_POWER_STATE_CONTEXT;
#if defined(_WIN64)
#define __builtin_ia32_sfence() __asm__ __volatile__("sfence" ::: "memory")
#endif
#include <ntddk.h>
#include "native_pci.h"

typedef struct {
    KEVENT Event;
    NTSTATUS Status;
    ULONG_PTR Done;
} RTL_PCI_WAIT;

void *RtlNativePciRetain(void *pdo)
{
    if(!pdo||KeGetCurrentIrql()!=PASSIVE_LEVEL)return NULL;
    ObReferenceObject(pdo);return pdo;
}

void RtlNativePciRelease(void *pdo)
{
    if(pdo)ObDereferenceObject(pdo);
}

static NTSTATUS NTAPI PciCompleted(PDEVICE_OBJECT device,PIRP irp,PVOID context)
{
    RTL_PCI_WAIT *wait=context;(void)device;
    wait->Status=irp->IoStatus.Status;wait->Done=irp->IoStatus.Information;
    KeSetEvent(&wait->Event,IO_NO_INCREMENT,FALSE);
    return STATUS_MORE_PROCESSING_REQUIRED;
}

int RtlNativePciConfig(void *object,int write,unsigned short offset,
                       unsigned width,unsigned int *value)
{
    PDEVICE_OBJECT pdo=object;PIRP irp;PIO_STACK_LOCATION stack;
    RTL_PCI_WAIT wait;LARGE_INTEGER timeout;NTSTATUS status;
    if(!pdo||!value||(write!=0&&write!=1)||
       (width!=1&&width!=2&&width!=4)||offset%width||offset>256-width||
       KeGetCurrentIrql()!=PASSIVE_LEVEL)return 0;
    irp=IoAllocateIrp(pdo->StackSize,FALSE);if(!irp)return 0;
    if(!write)*value=0;
    KeInitializeEvent(&wait.Event,NotificationEvent,FALSE);
    wait.Status=STATUS_UNSUCCESSFUL;wait.Done=0;
    stack=IoGetNextIrpStackLocation(irp);
    stack->MajorFunction=IRP_MJ_PNP;
    stack->MinorFunction=write?IRP_MN_WRITE_CONFIG:IRP_MN_READ_CONFIG;
    stack->Parameters.ReadWriteConfig.WhichSpace=PCI_WHICHSPACE_CONFIG;
    stack->Parameters.ReadWriteConfig.Buffer=value;
    stack->Parameters.ReadWriteConfig.Offset=offset;
    stack->Parameters.ReadWriteConfig.Length=width;
    irp->IoStatus.Status=STATUS_NOT_SUPPORTED;
    irp->IoStatus.Information=0;
    IoSetCompletionRoutine(irp,PciCompleted,&wait,TRUE,TRUE,TRUE);
    IoCallDriver(pdo,irp);
    timeout.QuadPart=-5000000LL; /* 500 ms before cancellation. */
    status=KeWaitForSingleObject(&wait.Event,Executive,KernelMode,FALSE,&timeout);
    if(status!=STATUS_SUCCESS){
        IoCancelIrp(irp);
        /* The lower driver still owns the IRP and stack-backed buffer until
           cancellation completes. A timeout is never permission to free it. */
        KeWaitForSingleObject(&wait.Event,Executive,KernelMode,FALSE,NULL);
    }
    IoFreeIrp(irp);
    return status==STATUS_SUCCESS&&NT_SUCCESS(wait.Status)&&wait.Done==width;
}

int RtlNativePciFunction(void *object,unsigned char *function)
{
    ULONG address=0xffffffffUL,length=0;NTSTATUS status;
    if(!object||!function||KeGetCurrentIrql()!=PASSIVE_LEVEL)return 0;
    status=IoGetDeviceProperty(object,DevicePropertyAddress,sizeof(address),&address,&length);
    if(!NT_SUCCESS(status)||length!=sizeof(address)||(address&0xffff)>7||
       (address>>16)>31)return 0;
    *function=(unsigned char)address;return 1;
}
