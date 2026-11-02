/* Test-only NDIS 3 protocol: request real NdisReset on explicitly named
 * RLTKLANT adapters. It neither patches NDIS nor calls miniport code directly. */
#define _WIN32_WINNT 0x0500
#define NTDDI_VERSION 0x05000000
typedef union {unsigned long ContextAsUlong;} SYSTEM_POWER_STATE_CONTEXT;
#if defined(_WIN64)
#define __builtin_ia32_sfence() __asm__ __volatile__("sfence" ::: "memory")
#endif
#include "ndis3_protocol_test.h"
#define RESET_TEST CTL_CODE(FILE_DEVICE_UNKNOWN,0x801,METHOD_BUFFERED,FILE_WRITE_ACCESS)
typedef struct {ULONG Count;WCHAR Adapter[256];} REQUEST;
typedef struct {ULONG Open,Reset,Close,Completed,Pending;} RESULT;
typedef struct {KEVENT Event;NDIS_STATUS Status;ULONG Completions;} BINDING;
static NDIS_HANDLE protocol;
static KEVENT gate;
static UNICODE_STRING link;

static VOID NTAPI Complete(NDIS_HANDLE context,NDIS_STATUS status)
{BINDING*b=context;b->Status=status;b->Completions++;KeSetEvent(&b->Event,0,FALSE);}
static VOID NTAPI OpenComplete(NDIS_HANDLE context,NDIS_STATUS status,NDIS_STATUS error)
{(void)error;Complete(context,status);}
static VOID NTAPI RequestComplete(NDIS_HANDLE c,PNDIS_REQUEST r,NDIS_STATUS s)
{(void)r;Complete(c,s);}
static VOID NTAPI SendComplete(NDIS_HANDLE c,PNDIS_PACKET p,NDIS_STATUS s)
{(void)c;(void)p;(void)s;}
static VOID NTAPI TransferComplete(NDIS_HANDLE c,PNDIS_PACKET p,NDIS_STATUS s,UINT n)
{(void)c;(void)p;(void)s;(void)n;}
static NDIS_STATUS NTAPI Receive(NDIS_HANDLE c,NDIS_HANDLE r,PVOID h,UINT hn,PVOID l,UINT ln,UINT n)
{(void)c;(void)r;(void)h;(void)hn;(void)l;(void)ln;(void)n;return NDIS_STATUS_NOT_ACCEPTED;}
static VOID NTAPI Notify(NDIS_HANDLE c){(void)c;}
static VOID NTAPI Status(NDIS_HANDLE c,NDIS_STATUS s,PVOID p,UINT n)
{(void)c;(void)s;(void)p;(void)n;}
static NDIS_STATUS Wait(BINDING*b,NDIS_STATUS status)
{
    /* The binding/stack remain alive until NDIS completes its operation. */
    if(status==NDIS_STATUS_PENDING){KeWaitForSingleObject(&b->Event,Executive,KernelMode,FALSE,NULL);status=b->Status;}
    return status;
}
static NTSTATUS Reset(const REQUEST*input,RESULT*out)
{
    BINDING b;NDIS_HANDLE adapter=NULL;NDIS_STATUS status,error;NDIS_MEDIUM medium=NdisMedium802_3;
    UNICODE_STRING name,prefix;UINT selected;ULONG i;NDIS_REQUEST query;ULONG vendor=0;
    RtlZeroMemory(out,sizeof(*out));out->Open=out->Reset=out->Close=NDIS_STATUS_FAILURE;
    if(!input->Count||input->Count>100||input->Adapter[255])return STATUS_INVALID_PARAMETER;
    RtlInitUnicodeString(&name,input->Adapter);RtlInitUnicodeString(&prefix,L"\\Device\\");
    if(!RtlPrefixUnicodeString(&prefix,&name,TRUE))return STATUS_INVALID_PARAMETER;
    KeInitializeEvent(&b.Event,NotificationEvent,FALSE);b.Status=NDIS_STATUS_FAILURE;b.Completions=0;
    NdisOpenAdapter(&status,&error,&adapter,&selected,&medium,1,protocol,&b,&name,0,NULL);
    out->Open=Wait(&b,status);if(out->Open!=NDIS_STATUS_SUCCESS)return STATUS_SUCCESS;
    /* Reject a different vendor before requesting a destructive soft reset. */
    RtlZeroMemory(&query,sizeof(query));query.RequestType=NdisRequestQueryInformation;
    query.DATA.QUERY_INFORMATION.Oid=OID_GEN_VENDOR_ID;
    query.DATA.QUERY_INFORMATION.InformationBuffer=&vendor;
    query.DATA.QUERY_INFORMATION.InformationBufferLength=sizeof(vendor);
    KeClearEvent(&b.Event);NdisRequest(&status,adapter,&query);status=Wait(&b,status);
    if(status==NDIS_STATUS_SUCCESS&&vendor==0x0000e04c){
        for(i=0;i<input->Count;i++){
            KeClearEvent(&b.Event);NdisReset(&status,adapter);
            if(status==NDIS_STATUS_PENDING)out->Pending++;
            out->Reset=Wait(&b,status);if(out->Reset!=NDIS_STATUS_SUCCESS)break;
            out->Completed++;
        }
    }else out->Reset=NDIS_STATUS_NOT_SUPPORTED;
    KeClearEvent(&b.Event);NdisCloseAdapter(&status,adapter);out->Close=Wait(&b,status);
    return STATUS_SUCCESS;
}
static NTSTATUS NTAPI Dispatch(PDEVICE_OBJECT device,PIRP irp)
{
    PIO_STACK_LOCATION stack=IoGetCurrentIrpStackLocation(irp);NTSTATUS status=STATUS_INVALID_DEVICE_REQUEST;
    ULONG done=0;(void)device;
    if(stack->MajorFunction==IRP_MJ_CREATE||stack->MajorFunction==IRP_MJ_CLOSE)status=STATUS_SUCCESS;
    else if(stack->MajorFunction==IRP_MJ_DEVICE_CONTROL&&stack->Parameters.DeviceIoControl.IoControlCode==RESET_TEST&&
            stack->Parameters.DeviceIoControl.InputBufferLength==sizeof(REQUEST)&&
            stack->Parameters.DeviceIoControl.OutputBufferLength>=sizeof(RESULT)&&KeGetCurrentIrql()==PASSIVE_LEVEL){
        REQUEST request;RESULT result;RtlCopyMemory(&request,irp->AssociatedIrp.SystemBuffer,sizeof(request));
        KeWaitForSingleObject(&gate,Executive,KernelMode,FALSE,NULL);
        status=Reset(&request,&result);KeSetEvent(&gate,0,FALSE);
        if(NT_SUCCESS(status)){RtlCopyMemory(irp->AssociatedIrp.SystemBuffer,&result,sizeof(result));done=sizeof(result);}
    }
    irp->IoStatus.Status=status;irp->IoStatus.Information=done;IoCompleteRequest(irp,0);return status;
}
static VOID NTAPI Unload(PDRIVER_OBJECT driver)
{NDIS_STATUS status;IoDeleteSymbolicLink(&link);IoDeleteDevice(driver->DeviceObject);NdisDeregisterProtocol(&status,protocol);}
NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT driver,PUNICODE_STRING registry)
{
    NDIS30_PROTOCOL_CHARACTERISTICS c;NDIS_STATUS status;NTSTATUS result;UNICODE_STRING name;PDEVICE_OBJECT device;unsigned i;(void)registry;
    RtlZeroMemory(&c,sizeof(c));c.MajorNdisVersion=3;
    c.OpenAdapterCompleteHandler=OpenComplete;c.CloseAdapterCompleteHandler=Complete;
    c.SendCompleteHandler=SendComplete;c.TransferDataCompleteHandler=TransferComplete;
    c.ResetCompleteHandler=Complete;c.RequestCompleteHandler=RequestComplete;
    c.ReceiveHandler=Receive;c.ReceiveCompleteHandler=Notify;c.StatusHandler=Status;c.StatusCompleteHandler=Notify;
    RtlInitUnicodeString(&c.Name,L"RltkPciResetTest");
    NdisRegisterProtocol(&status,&protocol,(PNDIS_PROTOCOL_CHARACTERISTICS)&c,sizeof(c));
    if(status!=NDIS_STATUS_SUCCESS)return STATUS_UNSUCCESSFUL;
    RtlInitUnicodeString(&name,L"\\Device\\RltkPciResetTest");RtlInitUnicodeString(&link,L"\\DosDevices\\RltkPciResetTest");
    result=IoCreateDevice(driver,0,&name,FILE_DEVICE_UNKNOWN,0,FALSE,&device);
    if(!NT_SUCCESS(result)){NdisDeregisterProtocol(&status,protocol);return result;}
    result=IoCreateSymbolicLink(&link,&name);
    if(!NT_SUCCESS(result)){IoDeleteDevice(device);NdisDeregisterProtocol(&status,protocol);return result;}
    KeInitializeEvent(&gate,SynchronizationEvent,TRUE);
    for(i=0;i<=IRP_MJ_MAXIMUM_FUNCTION;i++)driver->MajorFunction[i]=Dispatch;
    driver->DriverUnload=Unload;device->Flags&=~DO_DEVICE_INITIALIZING;return STATUS_SUCCESS;
}
static PVOID image_anchor __attribute__((used))=(PVOID)DriverEntry;
