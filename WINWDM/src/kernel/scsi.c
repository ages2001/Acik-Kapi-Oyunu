#include <ntddk.h>
#include <srb.h>
#include "wdm_scsi.h"

#define NT31_SCSI_SIZE 64
#define NT35_SCSI_SIZE 76
#define NT5_SCSI_SIZE 80

C_ASSERT(FIELD_OFFSET(HW_INITIALIZATION_DATA, VendorIdLength) == 62);
C_ASSERT(FIELD_OFFSET(HW_INITIALIZATION_DATA, VendorId) == NT31_SCSI_SIZE);
C_ASSERT(FIELD_OFFSET(HW_INITIALIZATION_DATA, HwAdapterControl) == NT35_SCSI_SIZE);
C_ASSERT(sizeof(HW_INITIALIZATION_DATA) >= NT5_SCSI_SIZE);

static BOOLEAN usesLegacyBus(INTERFACE_TYPE bus)
{
    return bus == Internal || bus == Isa || bus == Eisa || bus == MicroChannel;
}

ULONG NTAPI WkScsiPortInitialize(PVOID driver, PVOID registryPath,
    PHW_INITIALIZATION_DATA initialization, PVOID context)
{
    HW_INITIALIZATION_DATA local = {0};
    const UCHAR *source = (const UCHAR *)initialization;
    UCHAR *destination = (UCHAR *)&local;
    ULONG size, selected, index;

    if (!initialization) return (ULONG)STATUS_INVALID_PARAMETER;
    size = initialization->HwInitializationDataSize;
    if (size != NT35_SCSI_SIZE && size != NT5_SCSI_SIZE)
        return ScsiPortInitialize(driver, registryPath, initialization, context);

    /* NT 5 adds an optional adapter-control callback. An active callback
     * requires that layout. PCI matching and port flags require NT 3.5. */
    if (size == NT5_SCSI_SIZE && initialization->HwAdapterControl)
        return ScsiPortInitialize(driver, registryPath, initialization, context);
    selected = NT35_SCSI_SIZE;
    if (usesLegacyBus(initialization->AdapterInterfaceType)) {
        selected = NT31_SCSI_SIZE;
        /* NT 3.1 reserves the last two bytes of its 64-byte structure. */
        for (index = FIELD_OFFSET(HW_INITIALIZATION_DATA, VendorIdLength);
             index < NT35_SCSI_SIZE; ++index) {
            if (source[index]) {
                selected = NT35_SCSI_SIZE;
                break;
            }
        }
    }
    if (size == selected)
        return ScsiPortInitialize(driver, registryPath, initialization, context);
    for (index = 0; index < selected; ++index) destination[index] = source[index];
    local.HwInitializationDataSize = selected;
    return ScsiPortInitialize(driver, registryPath, &local, context);
}
