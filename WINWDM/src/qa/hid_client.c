/* Use an unchanged HIDPARSE.SYS to decode reports captured from QEMU's keyboard. */
#include <ntddk.h>
#include <hidusage.h>
#include <hidpi.h>

/* Public x86 hidpddi.h records absent from the MinGW-w64 12 DDK. */
typedef struct {
    USAGE UsagePage, Usage;
    UCHAR CollectionNumber, Reserved[15];
    USHORT InputLength, OutputLength, FeatureLength, PreparsedDataLength;
    PHIDP_PREPARSED_DATA PreparsedData;
} COLLECTION;
typedef struct {
    COLLECTION *Collections;
    ULONG CollectionCount;
    PVOID ReportIDs;
    ULONG ReportIDCount, Debug[8];
} DEVICE_DESCRIPTION_HID;
NTSTATUS NTAPI HidP_GetCollectionDescription(PUCHAR, ULONG, POOL_TYPE, DEVICE_DESCRIPTION_HID *);
VOID NTAPI HidP_FreeCollectionDescription(DEVICE_DESCRIPTION_HID *);
C_ASSERT(sizeof(COLLECTION) == 32);
C_ASSERT(sizeof(DEVICE_DESCRIPTION_HID) == 48);

static void message(const char *text)
{
    while (*text) WRITE_PORT_UCHAR((PUCHAR)0xe9, (UCHAR)*text++);
    WRITE_PORT_UCHAR((PUCHAR)0xe9, '\n');
}

static NTSTATUS readFixture(UCHAR *buffer, ULONG capacity, PULONG bytes)
{
    UNICODE_STRING path;
    OBJECT_ATTRIBUTES attributes;
    IO_STATUS_BLOCK io;
    HANDLE file;
    NTSTATUS status;
    RtlInitUnicodeString(&path, L"\\DosDevices\\C:\\DMAQA\\HIDWIRE.BIN");
    InitializeObjectAttributes(&attributes, &path, OBJ_CASE_INSENSITIVE, NULL, NULL);
    status = ZwOpenFile(&file, FILE_READ_DATA | SYNCHRONIZE, &attributes, &io,
                        FILE_SHARE_READ, FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE);
    if (!NT_SUCCESS(status)) return status;
    status = ZwReadFile(file, NULL, NULL, NULL, &io, buffer, capacity, NULL, NULL);
    if (NT_SUCCESS(status)) *bytes = (ULONG)io.Information;
    ZwClose(file);
    return status;
}

static NTSTATUS decode(UCHAR *wire, ULONG length)
{
    DEVICE_DESCRIPTION_HID device = {0};
    HIDP_CAPS caps;
    ULONG size, index;
    NTSTATUS status;
    if (length < 26) return STATUS_INVALID_BUFFER_SIZE;
    size = wire[0] | ((ULONG)wire[1] << 8);
    if (!size || size != length - 26) return STATUS_INVALID_BUFFER_SIZE;
    status = HidP_GetCollectionDescription(wire + 2, size, NonPagedPool, &device);
    if (!NT_SUCCESS(status)) return status;
    status = STATUS_DEVICE_DATA_ERROR;
    if (device.CollectionCount != 1 || !device.Collections ||
        HidP_GetCaps(device.Collections[0].PreparsedData, &caps) != HIDP_STATUS_SUCCESS ||
        caps.UsagePage != 1 || caps.Usage != 6 || caps.InputReportByteLength != 9) goto done;
    message("HID keyboard collection 01/06, nine-byte parser report PASS");
    for (index = 0; index < 3; ++index) {
        CHAR report[9] = {0};
        USAGE usages[16];
        ULONG count = sizeof(usages) / sizeof(usages[0]);
        ULONG expected = index == 2 ? 0 : index + 1;
        RtlCopyMemory(report + 1, wire + 2 + size + index * 8, 8);
        if (HidP_GetUsages(HidP_Input, 7, 0, usages, &count, device.Collections[0].PreparsedData,
                           report, sizeof(report)) != HIDP_STATUS_SUCCESS || count != expected) goto done;
        if ((index == 0 && usages[0] != 13) ||
            (index == 1 && !((usages[0] == 13 && usages[1] == 14) ||
                             (usages[0] == 14 && usages[1] == 13)))) goto done;
    }
    message("HID input usages: J=0d, J+K=0d/0e, released=empty PASS");
    status = STATUS_SUCCESS;
done:
    HidP_FreeCollectionDescription(&device);
    return status;
}

static VOID NTAPI unload(PDRIVER_OBJECT driver) { (void)driver; }

NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING path)
{
    UCHAR wire[512];
    ULONG length = 0;
    NTSTATUS status = readFixture(wire, sizeof(wire), &length);
    (void)path;
    if (NT_SUCCESS(status)) status = decode(wire, length);
    if (!NT_SUCCESS(status)) { message("HID class client FAIL"); return status; }
    driver->DriverUnload = unload;
    return STATUS_SUCCESS;
}
