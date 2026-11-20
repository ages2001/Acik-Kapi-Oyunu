#ifndef WINWDM_PCI_DEVICE_H
#define WINWDM_PCI_DEVICE_H
#include <ntddk.h>
#include "wdm_remove_lock.h"

typedef struct {
    PUCHAR port;
    ULONG length;
    BOOLEAN started;
    DEVICE_POWER_STATE power;
    IO_REMOVE_LOCK removeLock;
} WK_PCI_EXTENSION;

NTSTATUS NTAPI WkPciDispatch(PDEVICE_OBJECT device, PIRP irp);
NTSTATUS WkTestPci(PDEVICE_OBJECT control, PULONG count);
#endif
