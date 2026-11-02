/* Test-only Windows 2000 helper. Reset a named USBNT PDO through the actual
 * internal USB IOCTL. Not a production driver or a hardware certification. */
#define _WIN32_WINNT 0x0500
#define NTDDI_VERSION 0x05000000
typedef union {unsigned long ContextAsUlong;} SYSTEM_POWER_STATE_CONTEXT;
#if defined(_WIN64)
#define __builtin_ia32_sfence() __asm__ __volatile__("sfence" ::: "memory")
#endif
#include <ntddk.h>
#include <usbioctl.h>
#define RESET_TEST CTL_CODE(FILE_DEVICE_UNKNOWN,0x800,METHOD_BUFFERED,FILE_WRITE_ACCESS)
static UNICODE_STRING link;
static void trace(const char *s,NTSTATUS status)
{
    int i;while(*s)WRITE_PORT_UCHAR((PUCHAR)0xe9,(UCHAR)*s++);
    for(i=28;i>=0;i-=4)WRITE_PORT_UCHAR((PUCHAR)0xe9,"0123456789ABCDEF"[((ULONG)status>>i)&15]);
    WRITE_PORT_UCHAR((PUCHAR)0xe9,'\n');
}
static NTSTATUS NTAPI Dispatch(PDEVICE_OBJECT device,PIRP irp)
{
    PIO_STACK_LOCATION sp=IoGetCurrentIrpStackLocation(irp);
    NTSTATUS status=STATUS_INVALID_DEVICE_REQUEST;
    (void)device;
    if(sp->MajorFunction==IRP_MJ_CREATE||sp->MajorFunction==IRP_MJ_CLOSE)status=STATUS_SUCCESS;
    else if(sp->MajorFunction==IRP_MJ_DEVICE_CONTROL&&sp->Parameters.DeviceIoControl.IoControlCode==RESET_TEST){
        ULONG size=sp->Parameters.DeviceIoControl.InputBufferLength;
        WCHAR *buffer=irp->AssociatedIrp.SystemBuffer;
        if(size>=4&&size<=512&&!(size&1)&&buffer[size/2-1]==0){
            UNICODE_STRING name,expected,hub,hub20;PDEVICE_OBJECT target=NULL,top=NULL;PFILE_OBJECT file=NULL;
            RtlInitUnicodeString(&name,buffer);RtlInitUnicodeString(&expected,L"\\Driver\\USBNT");
            RtlInitUnicodeString(&hub,L"\\Driver\\USBHUB");RtlInitUnicodeString(&hub20,L"\\Driver\\USBHUB20");
            /* The file object retains the named base device. The returned
               top object may be the attached network function driver. */
            status=IoGetDeviceObjectPointer(&name,FILE_READ_DATA,&file,&top);
            trace("RESETTEST object=",status);
            if(NT_SUCCESS(status)){
                target=file->DeviceObject;
                if(target->Type!=IO_TYPE_DEVICE ||
                   (!RtlEqualUnicodeString(&target->DriverObject->DriverName,&expected,TRUE)&&
                    !RtlEqualUnicodeString(&target->DriverObject->DriverName,&hub,TRUE)&&
                    !RtlEqualUnicodeString(&target->DriverObject->DriverName,&hub20,TRUE)))status=STATUS_NOT_SUPPORTED;
                else{
                    KEVENT done;IO_STATUS_BLOCK ios;PIRP request;
                    KeInitializeEvent(&done,NotificationEvent,FALSE);
                    request=IoBuildDeviceIoControlRequest(IOCTL_INTERNAL_USB_RESET_PORT,target,NULL,0,NULL,0,TRUE,&done,&ios);
                    if(!request)status=STATUS_INSUFFICIENT_RESOURCES;
                    else{status=IoCallDriver(target,request);if(status==STATUS_PENDING){KeWaitForSingleObject(&done,Executive,KernelMode,FALSE,NULL);status=ios.Status;}trace("RESETTEST ioctl=",status);}
                }
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
    RtlInitUnicodeString(&name,L"\\Device\\RltkResetTest");RtlInitUnicodeString(&link,L"\\DosDevices\\RltkResetTest");
    status=IoCreateDevice(driver,0,&name,FILE_DEVICE_UNKNOWN,FILE_DEVICE_SECURE_OPEN,FALSE,&device);if(!NT_SUCCESS(status))return status;
    status=IoCreateSymbolicLink(&link,&name);if(!NT_SUCCESS(status)){IoDeleteDevice(device);return status;}
    for(i=0;i<=IRP_MJ_MAXIMUM_FUNCTION;i++)driver->MajorFunction[i]=Dispatch;
    driver->DriverUnload=Unload;device->Flags&=~DO_DEVICE_INITIALIZING;return STATUS_SUCCESS;
}
/* Keep an explicit absolute relocation even when x64 code is entirely RIP
   relative, so the shared driver-image validation also checks rebasing. */
static PVOID image_anchor __attribute__((used))=(PVOID)DriverEntry;
