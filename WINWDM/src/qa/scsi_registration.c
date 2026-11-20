/* Boot-time SCSIPORT ABI measurement for disposable x86 QEMU guests. The
 * miniport reports every adapter as absent and never claims a controller. */
#include <ntddk.h>
#include <srb.h>
#include "wdm_scsi.h"

#ifndef WK_SCSI_DESCRIPTOR_SIZE
#error Select an x86 registration layout.
#endif

C_ASSERT(FIELD_OFFSET(HW_INITIALIZATION_DATA, VendorId) == 64);
C_ASSERT(FIELD_OFFSET(HW_INITIALIZATION_DATA, HwAdapterControl) == 76);
C_ASSERT(WK_SCSI_DESCRIPTOR_SIZE == 64 || WK_SCSI_DESCRIPTOR_SIZE == 76 ||
         WK_SCSI_DESCRIPTOR_SIZE == 80);

static VOID logText(const char *text)
{
    while (*text) {
        __asm__ volatile ("outb %0, $0xe9" : : "a" ((UCHAR)*text));
        ++text;
    }
}

static VOID logHex(ULONG value)
{
    const char digits[] = "0123456789abcdef";
    char number[9];
    unsigned index;
    for (index = 8; index > 0; --index) {
        number[index - 1] = digits[value & 15];
        value >>= 4;
    }
    number[8] = 0;
    logText(number);
}

static ULONG NTAPI findAdapter(PVOID device, PVOID context, PVOID busInfo,
        PCHAR arguments, PPORT_CONFIGURATION_INFORMATION config, PBOOLEAN again)
{
    (void)device; (void)busInfo; (void)arguments; (void)config;
    ++*(PULONG)context;
    *again = FALSE;
    return SP_RETURN_NOT_FOUND;
}

static BOOLEAN NTAPI initialize(PVOID device)
{
    (void)device;
    return FALSE;
}

static BOOLEAN NTAPI startIo(PVOID device, PSCSI_REQUEST_BLOCK request)
{
    (void)device; (void)request;
    return FALSE;
}

static BOOLEAN NTAPI resetBus(PVOID device, ULONG path)
{
    (void)device; (void)path;
    return FALSE;
}

#ifdef WK_SCSI_CONTROL
static SCSI_ADAPTER_CONTROL_STATUS NTAPI adapterControl(PVOID device,
    SCSI_ADAPTER_CONTROL_TYPE type, PVOID parameters)
{
    (void)device; (void)type; (void)parameters;
    return ScsiAdapterControlUnsuccessful;
}
#endif

NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING path)
{
    HW_INITIALIZATION_DATA setup = {0};
    ULONG status, searches = 0;
    setup.HwInitializationDataSize = WK_SCSI_DESCRIPTOR_SIZE;
    setup.AdapterInterfaceType = Isa;
    setup.HwFindAdapter = findAdapter;
    setup.HwInitialize = initialize;
    setup.HwStartIo = startIo;
    setup.HwResetBus = resetBus;
    setup.DeviceExtensionSize = sizeof(ULONG);
#ifdef WK_SCSI_PCI
    setup.AdapterInterfaceType = PCIBus;
    setup.NumberOfAccessRanges = 1;
    setup.VendorIdLength = 4;
    setup.VendorId = "1000";
    setup.DeviceIdLength = 4;
    setup.DeviceId = "0001";
#endif
#ifdef WK_SCSI_CONTROL
    setup.HwAdapterControl = adapterControl;
#endif
#ifdef WK_SCSI_ADAPT
    status = WkScsiPortInitialize(driver, path, &setup, &searches);
    logText("WINWDM SDK adapter, ");
#else
    status = ScsiPortInitialize(driver, path, &setup, &searches);
#endif
    logText("WINWDM SCSIPORT size=0x");
    logHex(setup.HwInitializationDataSize);
    logText(" status=0x");
    logHex(status);
    logText(" find_calls=0x");
    logHex(searches);
    logText("\n");
    return (NTSTATUS)status;
}
