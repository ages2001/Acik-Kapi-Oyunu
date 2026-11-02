/* Test-only USBNT EHCI controller power IRP helper; no actual ACPI sleep. Not a production driver or a hardware certification. */
#define _WIN32_WINNT 0x0500
#define NTDDI_VERSION 0x05000000
typedef union {unsigned long ContextAsUlong;} SYSTEM_POWER_STATE_CONTEXT;
#if defined(_WIN64)
#define __builtin_ia32_sfence() __asm__ __volatile__("sfence" ::: "memory")
#endif
#include <ntddk.h>
#include <usbioctl.h>
#define RESET_TEST CTL_CODE(FILE_DEVICE_UNKNOWN,0x800,METHOD_BUFFERED,FILE_WRITE_ACCESS)
#define SYSTEM_TEST CTL_CODE(FILE_DEVICE_UNKNOWN,0x801,METHOD_BUFFERED,FILE_WRITE_ACCESS)
#define RESUME_TEST CTL_CODE(FILE_DEVICE_UNKNOWN,0x802,METHOD_BUFFERED,FILE_WRITE_ACCESS)
static UNICODE_STRING link;
static void trace(const char *s,NTSTATUS status)
{
    int i;while(*s)WRITE_PORT_UCHAR((PUCHAR)0xe9,(UCHAR)*s++);
    for(i=28;i>=0;i-=4)WRITE_PORT_UCHAR((PUCHAR)0xe9,"0123456789ABCDEF"[((ULONG)status>>i)&15]);
    WRITE_PORT_UCHAR((PUCHAR)0xe9,'\n');
}

typedef struct {KEVENT Event;NTSTATUS Status;} POWER_DONE;
static NTSTATUS NTAPI PowerDone(PDEVICE_OBJECT dev,PIRP irp,PVOID arg)
{
    POWER_DONE *done=(POWER_DONE*)arg;(void)dev;
    done->Status=irp->IoStatus.Status;
    KeSetEvent(&done->Event,IO_NO_INCREMENT,FALSE);
    return STATUS_MORE_PROCESSING_REQUIRED;
}
static NTSTATUS FunctionPower(PDEVICE_OBJECT target,POWER_STATE_TYPE type, ULONG state, UCHAR minor)
{
    POWER_DONE done;PIRP request;PIO_STACK_LOCATION sp;KIRQL old;
    request=IoAllocateIrp(target->StackSize,FALSE);
    if(!request)return STATUS_INSUFFICIENT_RESOURCES;
    KeInitializeEvent(&done.Event,NotificationEvent,FALSE);
    done.Status=STATUS_NOT_SUPPORTED;request->IoStatus.Status=STATUS_NOT_SUPPORTED;
    sp=IoGetNextIrpStackLocation(request);
    sp->MajorFunction=IRP_MJ_POWER;sp->MinorFunction=minor;
    sp->Parameters.Power.Type=type;
    sp->Parameters.Power.State.DeviceState=state;
    IoSetCompletionRoutine(request,PowerDone,&done,TRUE,TRUE,TRUE);
    /* Exercise the deferred path, not only the caller's passive fast path. */
    KeRaiseIrql(DISPATCH_LEVEL,&old);
    PoCallDriver(target,request);
    KeLowerIrql(old);
    KeWaitForSingleObject(&done.Event,Executive,KernelMode,FALSE,NULL);
    IoFreeIrp(request);
    trace(minor==IRP_MN_QUERY_POWER?"SLPTEST Q3=":type==SystemPowerState?(state==PowerSystemSleeping3?"SLPTEST S3=":"SLPTEST S0="):(state==PowerDeviceD3?"SLPTEST D3=":"SLPTEST D0="),done.Status);
    return done.Status;
}
static NTSTATUS CyclePower(PDEVICE_OBJECT target, int mode)
{
    return FunctionPower(target, DevicePowerState,
                         mode == 2 ? PowerDeviceD0 : PowerDeviceD3,
                         mode == 0 ? IRP_MN_QUERY_POWER : IRP_MN_SET_POWER);
}

static NTSTATUS NTAPI Dispatch(PDEVICE_OBJECT device,PIRP irp)
{
    PIO_STACK_LOCATION sp=IoGetCurrentIrpStackLocation(irp);
    NTSTATUS status=STATUS_INVALID_DEVICE_REQUEST;
    (void)device;
    if(sp->MajorFunction==IRP_MJ_CREATE||sp->MajorFunction==IRP_MJ_CLOSE)status=STATUS_SUCCESS;
    else if(sp->MajorFunction==IRP_MJ_DEVICE_CONTROL&&(sp->Parameters.DeviceIoControl.IoControlCode==RESET_TEST||sp->Parameters.DeviceIoControl.IoControlCode==SYSTEM_TEST||sp->Parameters.DeviceIoControl.IoControlCode==RESUME_TEST)){
        ULONG size=sp->Parameters.DeviceIoControl.InputBufferLength;
        WCHAR *buffer=irp->AssociatedIrp.SystemBuffer;
        if(size>=4&&size<=512&&!(size&1)&&buffer[size/2-1]==0){
            UNICODE_STRING name,expected,hub,hub20;PDEVICE_OBJECT target=NULL,top=NULL;PFILE_OBJECT file=NULL;
            RtlInitUnicodeString(&name,buffer);RtlInitUnicodeString(&expected,L"\\Driver\\USBNT");
            RtlInitUnicodeString(&hub,L"\\Driver\\USBHUB");RtlInitUnicodeString(&hub20,L"\\Driver\\USBHUB20");
            /* The file object retains the named base device. The returned
               top object may be the attached network function driver. */
            status=IoGetDeviceObjectPointer(&name,FILE_READ_DATA,&file,&top);
            trace("SLPTEST object=",status);
            if(NT_SUCCESS(status)){
                target=IoGetAttachedDeviceReference(file->DeviceObject);
                if(target->Type!=IO_TYPE_DEVICE ||
                   !RtlEqualUnicodeString(&target->DriverObject->DriverName,&expected,TRUE))status=STATUS_NOT_SUPPORTED;
                else{
                    status=CyclePower(target,(sp->Parameters.DeviceIoControl.IoControlCode-RESET_TEST)/4);

                }
                ObDereferenceObject(target);
                ObDereferenceObject(file);
            }
        }else status=STATUS_INVALID_PARAMETER;
    }
    irp->IoStatus.Status=status;irp->IoStatus.Information=0;IoCompleteRequest(irp,IO_NO_INCREMENT);return status;
}
static VOID NTAPI Unload(PDRIVER_OBJECT driver){IoDeleteSymbolicLink(&link);IoDeleteDevice(driver->DeviceObject);}
NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT driver,PUNICODE_STRING registry)
{
    UNICODE_STRING name;PDEVICE_OBJECT device;NTSTATUS status;unsigned i;(void)registry;
    RtlInitUnicodeString(&name,L"\\Device\\RltkSleepTest");RtlInitUnicodeString(&link,L"\\DosDevices\\RltkSleepTest");
    status=IoCreateDevice(driver,0,&name,FILE_DEVICE_UNKNOWN,FILE_DEVICE_SECURE_OPEN,FALSE,&device);if(!NT_SUCCESS(status))return status;
    status=IoCreateSymbolicLink(&link,&name);if(!NT_SUCCESS(status)){IoDeleteDevice(device);return status;}
    for(i=0;i<=IRP_MJ_MAXIMUM_FUNCTION;i++)driver->MajorFunction[i]=Dispatch;
    driver->DriverUnload=Unload;device->Flags&=~DO_DEVICE_INITIALIZING;return STATUS_SUCCESS;
}
/* Keep an explicit absolute relocation even when x64 code is entirely RIP
   relative, so the shared driver-image validation also checks rebasing. */
static PVOID image_anchor __attribute__((used))=(PVOID)DriverEntry;
