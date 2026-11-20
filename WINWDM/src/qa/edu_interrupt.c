/* The single-CPU EDU fixture owns PCI slot 5 and uses the PC's legacy PIC.
 * The interrupt handler acknowledges only the bits belonging to the test. */
#include <ntddk.h>
#include "wdm_interrupt.h"

typedef struct {
    PULONG Registers;
    volatile ULONG Handled;
    volatile ULONG LastCause;
} EDU_IRQ_CONTEXT;

static BOOLEAN NTAPI eduIsr(PKINTERRUPT interrupt, PVOID context)
{
    EDU_IRQ_CONTEXT *irq = context;
    ULONG cause = READ_REGISTER_ULONG(irq->Registers + 0x24 / 4) & 3;
    (void)interrupt;
    if (!cause) return FALSE;
    WRITE_REGISTER_ULONG(irq->Registers + 0x64 / 4, cause);
    irq->LastCause = cause;
    ++irq->Handled;
    return TRUE;
}

static NTSTATUS raiseAndWait(EDU_IRQ_CONTEXT *irq, ULONG cause)
{
    LARGE_INTEGER delay;
    ULONG expected = irq->Handled + 1;
    unsigned attempt;
    delay.QuadPart = -100000;
    WRITE_REGISTER_ULONG(irq->Registers + 0x60 / 4, cause);
    for (attempt = 0; attempt < 200 && irq->Handled < expected; ++attempt)
        KeDelayExecutionThread(KernelMode, FALSE, &delay);
    if (irq->Handled != expected || irq->LastCause != cause ||
        READ_REGISTER_ULONG(irq->Registers + 0x24 / 4) != 0)
        return STATUS_DEVICE_DATA_ERROR;
    return STATUS_SUCCESS;
}

NTSTATUS WkEduTestInterrupt(PDRIVER_OBJECT driver, PULONG registers, ULONG pciInterrupt,
                            void (*report)(const char *, ULONG))
{
    IO_CONNECT_INTERRUPT_PARAMETERS connect = {0};
    IO_DISCONNECT_INTERRUPT_PARAMETERS disconnect = {0};
    PIO_CONNECT_INTERRUPT_FULLY_SPECIFIED_PARAMETERS line = &connect.FullySpecified;
    PKINTERRUPT interrupt = NULL;
    PDEVICE_OBJECT device;
    EDU_IRQ_CONTEXT *irq;
    PCI_COMMON_CONFIG pci;
    KAFFINITY affinity = 0;
    KIRQL irql = 0;
    ULONG vector = 0, cause, pin = (pciInterrupt >> 8) & 255, number = pciInterrupt & 255;
    NTSTATUS status;
    if (pin != 1 || number < 3 || number >= 16) return STATUS_DEVICE_CONFIGURATION_ERROR;
    report("firmware IRQ ", number);
    if (HalGetBusData(PCIConfiguration, 0, 5, &pci, PCI_COMMON_HDR_LENGTH) == PCI_COMMON_HDR_LENGTH &&
        pci.VendorID == 0x1234 && pci.DeviceID == 0x11e8 &&
        pci.u.type0.InterruptPin == pin && pci.u.type0.InterruptLine != 0xff) {
        ULONG lineNumber = pci.u.type0.InterruptLine;
        report("PCI HAL IRQ ", lineNumber);
        vector = HalGetInterruptVector(PCIBus, 0, lineNumber, lineNumber, &irql, &affinity);
    }
    if (!vector)
        vector = HalGetInterruptVector(Isa, 0, number, number, &irql, &affinity);
    report("translated vector ", vector);
    if (!vector || !affinity) return STATUS_DEVICE_CONFIGURATION_ERROR;
    status = IoCreateDevice(driver, sizeof(*irq), NULL, FILE_DEVICE_UNKNOWN, 0, FALSE, &device);
    if (!NT_SUCCESS(status)) return status;
    irq = device->DeviceExtension;
    irq->Registers = registers;
    disconnect.Version = CONNECT_FULLY_SPECIFIED;
    connect.Version = CONNECT_MESSAGE_BASED;
    status = IoConnectInterruptEx(&connect);
    if (status != STATUS_NOT_SUPPORTED || connect.Version != CONNECT_FULLY_SPECIFIED) {
        status = STATUS_DEVICE_DATA_ERROR;
        goto finish;
    }
    line->PhysicalDeviceObject = device;
    line->InterruptObject = &interrupt;
    line->ServiceRoutine = eduIsr;
    line->ServiceContext = irq;
    line->ShareVector = TRUE;
    line->Vector = vector;
    line->Irql = line->SynchronizeIrql = irql;
    line->InterruptMode = LevelSensitive;
    if (IoConnectInterruptEx(&connect) != STATUS_INVALID_PARAMETER_10 || interrupt) {
        status = STATUS_DEVICE_DATA_ERROR;
        goto finish;
    }
    line->ProcessorEnableMask = affinity;
    for (cause = 1; cause <= 2; ++cause) {
        status = IoConnectInterruptEx(&connect);
        if (!NT_SUCCESS(status)) break;
        status = raiseAndWait(irq, cause);
        /* Lower the test line even on timeout before the ISR is disconnected. */
        WRITE_REGISTER_ULONG(registers + 0x64 / 4, cause);
        disconnect.ConnectionContext.InterruptObject = interrupt;
        IoDisconnectInterruptEx(&disconnect);
        interrupt = NULL;
        report("ISR acknowledged mask ", irq->LastCause);
        if (!NT_SUCCESS(status)) break;
    }
    report("handled interrupts ", irq->Handled);
finish:
    if (interrupt) {
        disconnect.ConnectionContext.InterruptObject = interrupt;
        IoDisconnectInterruptEx(&disconnect);
    }
    IoDeleteDevice(device);
    return status;
}
