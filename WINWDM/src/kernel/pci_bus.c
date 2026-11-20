/* An explicit test request opens the QEMU device on the root PCI bus. The
 * private bus translates its assigned BAR and supplies PnP/power IRPs to the
 * function driver, including on kernels without native PnP dispatch entries. */
#include "pci_device.h"
#include "protocol.h"
#include "queue.h"
#define WK_QEMU_TEST_ID 0x00051b36UL
static volatile ULONG occupied;

#ifdef WK_VMM
extern KIRQL WkMaskInterrupts(void);
extern void WkRestoreInterrupts(KIRQL irql);
#else
/* NT 3.1's PC HAL exposes HalGetBusData but has no PCI bus handler.
 * The pre-XP DDK declares KeNumberProcessors through the import pointer. */
extern const CCHAR * const processorCount __asm__("__imp__KeNumberProcessors");
#endif

static ULONG config(unsigned slot, unsigned reg)
{
    ULONG saved, value, addr=0x80000000UL | (slot<<11) | reg;
#ifdef WK_VMM
    KIRQL irql=WkMaskInterrupts();
#else
    ULONG flags;
    __asm__ volatile ("pushfl; popl %0; cli" : "=r"(flags) : : "memory", "cc");
#endif
    __asm__ volatile ("inl %%dx,%%eax":"=a"(saved):"d"(0xcf8));
    __asm__ volatile ("outl %%eax,%%dx"::"a"(addr),"d"(0xcf8));
    __asm__ volatile ("inl %%dx,%%eax":"=a"(value):"d"(0xcfc));
    __asm__ volatile ("outl %%eax,%%dx"::"a"(saved),"d"(0xcf8));
#ifdef WK_VMM
    WkRestoreInterrupts(irql);
#else
    __asm__ volatile ("pushl %0; popfl" : : "r"(flags) : "memory", "cc");
#endif
    return value;
}

#ifdef WK_VMM
UCHAR NTAPI READ_PORT_UCHAR(PUCHAR port)
{
    UCHAR value;
    __asm__ volatile ("inb %%dx,%%al":"=a"(value):"d"((USHORT)(ULONG_PTR)port));
    return value;
}

VOID NTAPI WRITE_PORT_UCHAR(PUCHAR port,UCHAR value)
{
    __asm__ volatile ("outb %%al,%%dx"::"a"(value),"d"((USHORT)(ULONG_PTR)port));
}
#endif

static NTSTATUS findPorts(PCM_RESOURCE_LIST resources)
{
    unsigned slot;
#ifndef WK_VMM
    ULONG rootId;
    BOOLEAN legacyPorts=HalGetBusData(PCIConfiguration,0,0,&rootId,sizeof(rootId))==0;
    /* No PCI HAL lock is available on this path. Masking interrupts serializes
     * the saved selector and data cycle on a uniprocessor PC. */
    if (legacyPorts && *processorCount!=1) return STATUS_NOT_SUPPORTED;
#endif
    for (slot=0;slot<32;++slot) {
        ULONG bar, command;
        PHYSICAL_ADDRESS translated;
        PCM_PARTIAL_RESOURCE_DESCRIPTOR item;
#ifdef WK_VMM
        if (config(slot,0)!=WK_QEMU_TEST_ID) continue;
        command=config(slot,4);
        bar=config(slot,0x14);
#else
        ULONG header[6], space=1;
        PCI_SLOT_NUMBER address;
        PHYSICAL_ADDRESS bus;
        if (legacyPorts) {
            if (config(slot,0)!=WK_QEMU_TEST_ID) continue;
            command=config(slot,4);
            bar=config(slot,0x14);
        } else {
            address.u.AsULONG=0;
            address.u.bits.DeviceNumber=slot;
            if (HalGetBusData(PCIConfiguration,0,address.u.AsULONG,header,sizeof(header))!=sizeof(header) ||
                header[0]!=WK_QEMU_TEST_ID) continue;
            command=header[1];
            bar=header[5];
        }
#endif
        if (!(command&1) || !(bar&1) || (bar&~3UL)==0 || (bar&~3UL)>0xff00)
            return STATUS_DEVICE_CONFIGURATION_ERROR;
#ifdef WK_VMM
        translated.QuadPart=bar&~3UL;
#else
        bus.QuadPart=bar&~3UL;
        if (!HalTranslateBusAddress(legacyPorts ? Isa : PCIBus,0,bus,&space,&translated) || space!=1)
            return STATUS_DEVICE_CONFIGURATION_ERROR;
#endif
        resources->Count=1;
        resources->List[0].InterfaceType=PCIBus;
        resources->List[0].BusNumber=0;
        resources->List[0].PartialResourceList.Version=1;
        resources->List[0].PartialResourceList.Revision=1;
        resources->List[0].PartialResourceList.Count=1;
        item=resources->List[0].PartialResourceList.PartialDescriptors;
        item->Type=CmResourceTypePort;
        item->ShareDisposition=CmResourceShareDeviceExclusive;
        item->Flags=CM_RESOURCE_PORT_IO;
        item->u.Port.Start=translated;
        item->u.Port.Length=256;
        return STATUS_SUCCESS;
    }
    return STATUS_NO_SUCH_DEVICE;
}

static NTSTATUS NTAPI completed(PDEVICE_OBJECT device,PIRP irp,PVOID context)
{
    (void)device;
    *(PIO_STATUS_BLOCK)context=irp->IoStatus;
    return STATUS_MORE_PROCESSING_REQUIRED;
}

static NTSTATUS send(PDEVICE_OBJECT device,UCHAR major,UCHAR minor,
                     PCM_RESOURCE_LIST resources,DEVICE_POWER_STATE power,PULONG count)
{
    struct { IRP irp; IO_STACK_LOCATION frame; } packet;
    PIO_STACK_LOCATION stack;
    IO_STATUS_BLOCK result;
    NTSTATUS status;
    C_ASSERT(FIELD_OFFSET(typeof(packet),frame)==sizeof(IRP));
    result.Status=STATUS_PENDING;
    result.Information=0;
    IoInitializeIrp(&packet.irp,IoSizeOfIrp(1),1);
    stack=IoGetNextIrpStackLocation(&packet.irp);
    stack->MajorFunction=major;
    stack->MinorFunction=minor;
    stack->DeviceObject=device;
    if (major==IRP_MJ_PNP && minor==IRP_MN_START_DEVICE) {
        stack->Parameters.StartDevice.AllocatedResourcesTranslated=resources;
    } else if (major==IRP_MJ_POWER) {
        stack->Parameters.Power.Type=DevicePowerState;
        stack->Parameters.Power.State.DeviceState=power;
    } else if (major==IRP_MJ_DEVICE_CONTROL) {
        stack->Parameters.DeviceIoControl.IoControlCode=WK_IOCTL_PCI;
        stack->Parameters.DeviceIoControl.OutputBufferLength=sizeof(*count);
        packet.irp.AssociatedIrp.SystemBuffer=count;
        packet.irp.Flags=IRP_BUFFERED_IO;
    }
    IoSetCompletionRoutine(&packet.irp,completed,&result,TRUE,TRUE,TRUE);
    IoSetNextIrpStackLocation(&packet.irp);
    status=WkPciDispatch(device,&packet.irp);
    if (result.Status!=status || (major==IRP_MJ_DEVICE_CONTROL && NT_SUCCESS(status) &&
        result.Information!=sizeof(*count))) return STATUS_DEVICE_DATA_ERROR;
    return status;
}

NTSTATUS WkTestPci(PDEVICE_OBJECT control,PULONG count)
{
    CM_RESOURCE_LIST resources={0};
    PDEVICE_OBJECT device;
    WK_PCI_EXTENSION *state;
    NTSTATUS status;
    ULONG value=0;
    if (__atomic_exchange_n(&occupied,1,__ATOMIC_ACQUIRE))
        return STATUS_DEVICE_BUSY;
    status=findPorts(&resources);
    if (!NT_SUCCESS(status)) goto done;
    status=IoCreateDevice(control->DriverObject,sizeof(WK_PCI_EXTENSION),NULL,FILE_DEVICE_UNKNOWN,0,FALSE,&device);
    if (!NT_SUCCESS(status)) goto done;
    state=device->DeviceExtension;
    IoInitializeRemoveLock(&state->removeLock,0,0,0);
    device->Flags &= ~DO_DEVICE_INITIALIZING;
    status=send(device,IRP_MJ_PNP,IRP_MN_START_DEVICE,&resources,0,NULL);
    if (!NT_SUCCESS(status)) goto remove;
    if (send(device,IRP_MJ_DEVICE_CONTROL,0,NULL,0,&value)!=STATUS_SUCCESS || value!=1 ||
        send(device,IRP_MJ_POWER,IRP_MN_SET_POWER,NULL,PowerDeviceD3,NULL)!=STATUS_SUCCESS ||
        send(device,IRP_MJ_DEVICE_CONTROL,0,NULL,0,&value)!=STATUS_DEVICE_NOT_READY ||
        send(device,IRP_MJ_POWER,IRP_MN_SET_POWER,NULL,PowerDeviceD0,NULL)!=STATUS_SUCCESS ||
        send(device,IRP_MJ_DEVICE_CONTROL,0,NULL,0,&value)!=STATUS_SUCCESS || value!=2 ||
        send(device,IRP_MJ_PNP,IRP_MN_STOP_DEVICE,NULL,0,NULL)!=STATUS_SUCCESS ||
        send(device,IRP_MJ_DEVICE_CONTROL,0,NULL,0,&value)!=STATUS_DEVICE_NOT_READY ||
        send(device,IRP_MJ_PNP,IRP_MN_START_DEVICE,&resources,0,NULL)!=STATUS_SUCCESS ||
        WkQueueRemovalProbe(control,&state->removeLock)!=STATUS_PENDING)
        status=STATUS_DEVICE_DATA_ERROR;
remove:
    if (send(device,IRP_MJ_PNP,IRP_MN_REMOVE_DEVICE,NULL,0,NULL)!=STATUS_SUCCESS ||
        send(device,IRP_MJ_DEVICE_CONTROL,0,NULL,0,&value)!=STATUS_DELETE_PENDING)
        status=STATUS_DEVICE_DATA_ERROR;
    IoDeleteDevice(device);
    if (NT_SUCCESS(status)) {
        if (WkRemovalProbePassed()) *count=value;
        else status=STATUS_DEVICE_DATA_ERROR;
    }
done:
    __atomic_store_n(&occupied,0,__ATOMIC_RELEASE);
    return status;
}
