/* VMM adapter for the kit's software control device.
 * Compile the consumer against the same x86 WDM declarations as the NT build.
 * Only the adapter owns IRPs handed to the driver's dispatch routines. */
#include <ntddk.h>
#include "protocol.h"
#include "queue.h"
#include "resident.h"
#include "vmm_event.h"
#include "wdm_remove_lock.h"

extern NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT,PUNICODE_STRING);
static DRIVER_OBJECT driver;
typedef struct _DEVICE_RECORD {
    struct _DEVICE_RECORD *next;
    PDEVICE_OBJECT lower;
    BOOLEAN deleting;
    DEVICE_OBJECT object;
} DEVICE_RECORD;

static DEVICE_RECORD *devices;
static ULONG activeRequests;
static IO_REMOVE_LOCK lifetime;
static BOOLEAN initialized;
static PDEVICE_OBJECT endpoint;
static BOOLEAN linked;
extern KIRQL WkMaskInterrupts(void);
extern void WkRestoreInterrupts(KIRQL previous);

static const WCHAR deviceName[]=L"\\Device\\WdmKit";
static const WCHAR linkName[]=L"\\DosDevices\\WdmKit";

static void clear(void *address, ULONG bytes)
{
    UCHAR *p=address;
    while (bytes--) *p++=0;
}

void NTAPI RtlInitUnicodeString(PUNICODE_STRING result, PCWSTR source)
{
    USHORT chars=0;
    if (source)
        while (chars<0x7ffe && source[chars]) ++chars;
    result->Buffer=(PWSTR)source;
    result->Length=chars*sizeof(WCHAR);
    result->MaximumLength=source ? result->Length+sizeof(WCHAR) : 0;
}

/* VMM identifies a static VxD by its descriptor name. Only the two names
 * actually routed by this descriptor are available on the VMM backend. */
static BOOLEAN equalName(PUNICODE_STRING name,const WCHAR *expected)
{
    USHORT i;
    if (!name || !name->Buffer || name->Length%2) return FALSE;
    for (i=0;i<name->Length/2;++i) {
        WCHAR a=name->Buffer[i], b=expected[i];
        if (!b) return FALSE;
        if (a>='a' && a<='z') a-=32;
        if (b>='a' && b<='z') b-=32;
        if (a!=b) return FALSE;
    }
    return !expected[i];
}

static DEVICE_RECORD *recordOf(PDEVICE_OBJECT object)
{
    DEVICE_RECORD *record;
    for (record=devices; record; record=record->next)
        if (&record->object==object)
            return record;
    return NULL;
}

NTSTATUS NTAPI IoCreateDevice(PDRIVER_OBJECT owner,ULONG extension,
    PUNICODE_STRING name,DEVICE_TYPE type,ULONG characteristics,
    BOOLEAN exclusive,PDEVICE_OBJECT *result)
{
    DEVICE_RECORD *record;
    PDEVICE_OBJECT device;
    ULONG header=(sizeof(*record)+7U) & ~7U;
    KIRQL previous;
    if (!owner || (name && !equalName(name,deviceName)) || exclusive)
        return STATUS_NOT_SUPPORTED;
    if (extension>0xffffU-sizeof(DEVICE_OBJECT))
        return STATUS_INVALID_PARAMETER;
    record=WkResidentAllocate(header+extension);
    if (!record)
        return STATUS_INSUFFICIENT_RESOURCES;
    clear(record,header+extension);
    device=&record->object;
    device->Type=IO_TYPE_DEVICE;
    device->Size=(USHORT)(sizeof(*device)+extension);
    device->DriverObject=owner;
    device->DeviceType=type;
    device->Characteristics=characteristics;
    device->Flags=DO_DEVICE_INITIALIZING;
    device->StackSize=1;
    device->DeviceExtension=extension ? (PUCHAR)record+header : NULL;
    previous=WkMaskInterrupts();
    if (name && endpoint) {
        WkRestoreInterrupts(previous);
        WkResidentFree(record);
        return STATUS_OBJECT_NAME_COLLISION;
    }
    record->next=devices;
    devices=record;
    device->NextDevice=owner->DeviceObject;
    owner->DeviceObject=device;
    if (name) endpoint=device;
    *result=device;
    WkRestoreInterrupts(previous);
    return STATUS_SUCCESS;
}

/* Device deletion can coincide with a pending request or filter detachment.
 * Keep every member of the captured dispatch path resident until the last
 * external request finishes. Return system pages outside the list lock. */
static void collectDeleted(void)
{
    DEVICE_RECORD **item, *retired=NULL;
    KIRQL previous=WkMaskInterrupts();
    if (!activeRequests) {
        item=&devices;
        while (*item) {
            DEVICE_RECORD *record=*item;
            PDEVICE_OBJECT object=&record->object;
            if (record->deleting && !record->lower && !object->AttachedDevice) {
                PDEVICE_OBJECT *link=&object->DriverObject->DeviceObject;
                while (*link && *link!=object) link=&(*link)->NextDevice;
                if (*link) *link=object->NextDevice;
                if (object==endpoint) endpoint=NULL;
                *item=record->next;
                record->next=retired;
                retired=record;
            } else {
                item=&record->next;
            }
        }
    }
    WkRestoreInterrupts(previous);
    while (retired) {
        DEVICE_RECORD *next=retired->next;
        WkResidentFree(retired);
        retired=next;
    }
}

VOID NTAPI IoDeleteDevice(PDEVICE_OBJECT object)
{
    KIRQL previous=WkMaskInterrupts();
    DEVICE_RECORD *record=recordOf(object);
    if (record) record->deleting=TRUE;
    WkRestoreInterrupts(previous);
    collectDeleted();
}

PDEVICE_OBJECT NTAPI IoGetAttachedDevice(PDEVICE_OBJECT device)
{
    while (device->AttachedDevice)
        device=device->AttachedDevice;
    return device;
}

PDEVICE_OBJECT NTAPI IoAttachDeviceToDeviceStack(PDEVICE_OBJECT source,PDEVICE_OBJECT target)
{
    KIRQL previous=WkMaskInterrupts();
    DEVICE_RECORD *upper=recordOf(source), *lower=recordOf(target);
    PDEVICE_OBJECT top=NULL;
    if (upper && !upper->deleting && !upper->lower && !source->AttachedDevice && lower && !lower->deleting) {
        top=IoGetAttachedDevice(target);
        if (top==source || (top->Flags & DO_DEVICE_INITIALIZING) ||
            top->StackSize>=126 || recordOf(top)->deleting) top=NULL;
        if (top) {
            source->StackSize=top->StackSize+1;
            source->AlignmentRequirement=top->AlignmentRequirement;
            source->SectorSize=top->SectorSize;
            upper->lower=top;
            top->AttachedDevice=source;
        }
    }
    WkRestoreInterrupts(previous);
    return top;
}

NTSTATUS NTAPI IoAttachDeviceByPointer(PDEVICE_OBJECT source,PDEVICE_OBJECT target)
{
    return IoAttachDeviceToDeviceStack(source,target) ? STATUS_SUCCESS : STATUS_NO_SUCH_DEVICE;
}

VOID NTAPI IoDetachDevice(PDEVICE_OBJECT lower)
{
    KIRQL previous=WkMaskInterrupts();
    DEVICE_RECORD *removed=recordOf(lower) ? recordOf(lower->AttachedDevice) : NULL;
    if (removed) {
        PDEVICE_OBJECT above=removed->object.AttachedDevice;
        lower->AttachedDevice=above;
        if (above) recordOf(above)->lower=lower;
        removed->object.AttachedDevice=NULL;
        removed->lower=NULL;
        removed->object.StackSize=1;
        while (above) {
            --above->StackSize;
            above=above->AttachedDevice;
        }
    }
    WkRestoreInterrupts(previous);
    collectDeleted();
}

NTSTATUS NTAPI IoCreateSymbolicLink(PUNICODE_STRING link,PUNICODE_STRING target)
{
    if (!equalName(link,linkName) || !equalName(target,deviceName))
        return STATUS_NOT_SUPPORTED;
    if (!endpoint) return STATUS_NO_SUCH_DEVICE;
    if (linked) return STATUS_OBJECT_NAME_COLLISION;
    linked=TRUE;
    return STATUS_SUCCESS;
}

NTSTATUS NTAPI IoDeleteSymbolicLink(PUNICODE_STRING link)
{
    if (!linked || !equalName(link,linkName)) return STATUS_OBJECT_NAME_NOT_FOUND;
    linked=FALSE;
    return STATUS_SUCCESS;
}

VOID NTAPI IoInitializeIrp(PIRP irp, USHORT size, CCHAR count)
{
    clear(irp,size);
    irp->Type=IO_TYPE_IRP;
    irp->Size=size;
    irp->StackCount=count;
    irp->CurrentLocation=count+1;
    irp->Tail.Overlay.CurrentStackLocation=(PIO_STACK_LOCATION)(irp+1)+count;
    InitializeListHead(&irp->ThreadListEntry);
}

PIRP NTAPI IoAllocateIrp(CCHAR count, BOOLEAN chargeQuota)
{
    PIRP irp;
    USHORT bytes;
    if (count < 1 || count == 127 || chargeQuota)
        return NULL;
    bytes = IoSizeOfIrp(count);
    irp = WkResidentAllocate(bytes);
    if (irp)
        IoInitializeIrp(irp, bytes, count);
    return irp;
}

VOID NTAPI IoFreeIrp(PIRP irp)
{
    WkResidentFree(irp);
}

NTSTATUS FASTCALL IofCallDriver(PDEVICE_OBJECT target, PIRP irp)
{
    PIO_STACK_LOCATION stack;
    PDRIVER_DISPATCH dispatch;
    if (!target || !target->DriverObject || irp->CurrentLocation<=1)
        return STATUS_INVALID_DEVICE_STATE;
    IoSetNextIrpStackLocation(irp);
    stack=IoGetCurrentIrpStackLocation(irp);
    stack->DeviceObject=target;
    dispatch=stack->MajorFunction<=IRP_MJ_MAXIMUM_FUNCTION ?
        target->DriverObject->MajorFunction[stack->MajorFunction] : NULL;
    if (dispatch)
        return dispatch(target,irp);
    irp->IoStatus.Status=STATUS_INVALID_DEVICE_REQUEST;
    irp->IoStatus.Information=0;
    IoCompleteRequest(irp,IO_NO_INCREMENT);
    return STATUS_INVALID_DEVICE_REQUEST;
}

VOID FASTCALL IofCompleteRequest(PIRP irp,CCHAR boost)
{
    (void)boost;
    while (irp->CurrentLocation<=irp->StackCount) {
        PIO_STACK_LOCATION stack=IoGetCurrentIrpStackLocation(irp);
        UCHAR flags=stack->Control;
        BOOLEAN invoke=(irp->Cancel && (flags&SL_INVOKE_ON_CANCEL)) ||
            (flags&(NT_SUCCESS(irp->IoStatus.Status) ? SL_INVOKE_ON_SUCCESS : SL_INVOKE_ON_ERROR));
        PDEVICE_OBJECT upper;
        irp->PendingReturned=!!(flags&SL_PENDING_RETURNED);
        ++irp->CurrentLocation;
        ++irp->Tail.Overlay.CurrentStackLocation;
        upper=irp->CurrentLocation<=irp->StackCount ?
            IoGetCurrentIrpStackLocation(irp)->DeviceObject : NULL;
        if (invoke && stack->CompletionRoutine) {
            if (stack->CompletionRoutine(upper,irp,stack->Context)==STATUS_MORE_PROCESSING_REQUIRED)
                return;
        } else if (irp->PendingReturned && upper) {
            IoMarkIrpPending(irp);
        }
    }
    if (irp->UserIosb)
        *irp->UserIosb=irp->IoStatus;
}

typedef struct {
    KEVENT done;
    IO_STATUS_BLOCK result;
    ULONG reply;
} VMM_CALL;

static NTSTATUS NTAPI completeCall(PDEVICE_OBJECT device,PIRP irp,PVOID context)
{
    VMM_CALL *call=context;
    (void)device;
    call->result=irp->IoStatus;
    WkSignalVmmEvent(&call->done);
    return STATUS_MORE_PROCESSING_REQUIRED;
}

static PDEVICE_OBJECT acquireEndpoint(void)
{
    PDEVICE_OBJECT top=NULL;
    KIRQL previous=WkMaskInterrupts();
    if (linked && endpoint && !recordOf(endpoint)->deleting) {
        top=IoGetAttachedDevice(endpoint);
        if (recordOf(top)->deleting) top=NULL;
        else ++activeRequests;
    }
    WkRestoreInterrupts(previous);
    return top;
}

static NTSTATUS submit(UCHAR major,ULONG code,PULONG output)
{
    PDEVICE_OBJECT top;
    PIRP irp;
    VMM_CALL *call;
    PIO_STACK_LOCATION stack;
    NTSTATUS status=STATUS_INSUFFICIENT_RESOURCES;
    KIRQL previous;
    if (!initialized || !NT_SUCCESS(IoAcquireRemoveLock(&lifetime,&top)))
        return STATUS_DELETE_PENDING;
    top=acquireEndpoint();
    if (!top) {
        IoReleaseRemoveLock(&lifetime,&top);
        return STATUS_NO_SUCH_DEVICE;
    }
    irp=IoAllocateIrp(top->StackSize,FALSE);
    call=WkResidentAllocate(sizeof(*call));
    if (!irp || !call) goto done;
    WkInitVmmEvent(&call->done);
    call->result.Status=STATUS_PENDING;
    call->result.Information=0;
    call->reply=0;
    stack=IoGetNextIrpStackLocation(irp);
    stack->MajorFunction=major;
    if (major==IRP_MJ_DEVICE_CONTROL) {
        stack->Parameters.DeviceIoControl.IoControlCode=code;
        stack->Parameters.DeviceIoControl.OutputBufferLength=sizeof(call->reply);
        irp->Flags=IRP_BUFFERED_IO;
        irp->AssociatedIrp.SystemBuffer=&call->reply;
    }
    IoSetCompletionRoutine(irp,completeCall,call,TRUE,TRUE,TRUE);
    status=IoCallDriver(top,irp);
    if (status==STATUS_PENDING)
        WkWaitVmmEvent(&call->done);
    else if (code==WK_IOCTL_DEFERRED && NT_SUCCESS(status)) {
        status=STATUS_DEVICE_DATA_ERROR;
        goto done;
    }
    if (!NT_SUCCESS(status)) goto done;
    status=call->result.Status;
    if (status==STATUS_PENDING) status=STATUS_DEVICE_DATA_ERROR;
    if (NT_SUCCESS(status) && output) {
        if (call->result.Information!=sizeof(*output)) status=STATUS_DEVICE_DATA_ERROR;
        else *output=call->reply;
    }
done:
    if (irp) IoFreeIrp(irp);
    WkResidentFree(call);
    previous=WkMaskInterrupts();
    --activeRequests;
    WkRestoreInterrupts(previous);
    collectDeleted();
    IoReleaseRemoveLock(&lifetime,&top);
    return status;
}

int WkVxdOpen(void) { return NT_SUCCESS(submit(IRP_MJ_CREATE,0,NULL)); }
int WkVxdClose(void) { return NT_SUCCESS(submit(IRP_MJ_CLOSE,0,NULL)); }
int WkVxdStack(void)
{
    ULONG value=0;
    return NT_SUCCESS(submit(IRP_MJ_DEVICE_CONTROL,WK_IOCTL_STACK,&value)) &&
           value==(WK_STACK_BASE|WK_STACK_FILTER);
}
int WkVxdPci(void)
{
    ULONG count=0;
    return NT_SUCCESS(submit(IRP_MJ_DEVICE_CONTROL,WK_IOCTL_PCI,&count)) && count==2;
}
int WkVxdQuery(void)
{
    ULONG value=0;
    return NT_SUCCESS(submit(IRP_MJ_DEVICE_CONTROL,WK_IOCTL,&value)) &&
           value==WK_SIGNATURE && WkVxdStack();
}
int WkVxdDeferred(void)
{
    ULONG value=0;
    return NT_SUCCESS(submit(IRP_MJ_DEVICE_CONTROL,WK_IOCTL_DEFERRED,&value)) &&
           value==WK_SIGNATURE;
}
void WkVxdStop(void)
{
    if (!initialized || !NT_SUCCESS(IoAcquireRemoveLock(&lifetime,&driver))) return;
    IoReleaseRemoveLockAndWait(&lifetime,&driver);
    if (endpoint && driver.DriverUnload) driver.DriverUnload(&driver);
    initialized=FALSE;
    WkStopResidentPool();
}
int WkVxdStart(void)
{
    UNICODE_STRING path;
    if (!WkStartResidentPool()) return 0;
    IoInitializeRemoveLock(&lifetime,0,0,0);
    initialized=TRUE;
    clear(&driver,sizeof(driver));
    driver.Type=IO_TYPE_DRIVER;
    driver.Size=sizeof(driver);
    RtlInitUnicodeString(&path,NULL);
    if (!NT_SUCCESS(DriverEntry(&driver,&path))) {
        initialized=FALSE;
        WkStopResidentPool();
        return 0;
    }
    /* The asynchronous boot probe is completed by the scheduled VMM event. */
    if (WkVxdOpen() && WkVxdClose()) return 1;
    WkVxdStop();
    return 0;
}
