#include "wdm_interrupt.h"

NTSTATUS NTAPI IoConnectInterruptEx(PIO_CONNECT_INTERRUPT_PARAMETERS parameters)
{
    PIO_CONNECT_INTERRUPT_FULLY_SPECIFIED_PARAMETERS line;
    if (!parameters) return STATUS_INVALID_PARAMETER;
    if (parameters->Version != CONNECT_FULLY_SPECIFIED) {
        parameters->Version = CONNECT_FULLY_SPECIFIED;
        return STATUS_NOT_SUPPORTED;
    }
    line = &parameters->FullySpecified;
    if (!line->InterruptObject) return STATUS_INVALID_PARAMETER;
    *line->InterruptObject = NULL;
    if (!line->PhysicalDeviceObject || !line->ServiceRoutine ||
        line->Irql <= DISPATCH_LEVEL || line->Irql > HIGH_LEVEL ||
        line->SynchronizeIrql < line->Irql || line->SynchronizeIrql > HIGH_LEVEL ||
        (line->InterruptMode != LevelSensitive && line->InterruptMode != Latched))
        return STATUS_INVALID_PARAMETER;
    if (!line->ProcessorEnableMask) return STATUS_INVALID_PARAMETER_10;
    return IoConnectInterrupt(line->InterruptObject, line->ServiceRoutine, line->ServiceContext,
        line->SpinLock, line->Vector, line->Irql, line->SynchronizeIrql,
        line->InterruptMode, line->ShareVector, line->ProcessorEnableMask, line->FloatingSave);
}

VOID NTAPI IoDisconnectInterruptEx(PIO_DISCONNECT_INTERRUPT_PARAMETERS parameters)
{
    if (parameters && parameters->Version == CONNECT_FULLY_SPECIFIED &&
        parameters->ConnectionContext.InterruptObject)
        IoDisconnectInterrupt(parameters->ConnectionContext.InterruptObject);
}
