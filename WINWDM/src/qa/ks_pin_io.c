/* Independent KS filter and kernel-mode streaming client for the native NT tests. */
#include "../kernel/nt_legacy.h"
#include <ks.h>
#include <windef.h>
#include <tchar.h>
#define NOBITMAP
#include <mmreg.h>
#include <ksmedia.h>
#include "ks_pin_test.h"

#define PACKET_BYTES 512
typedef struct { ULONG Id; KSSTATE State; } PIN;
typedef struct { KSEMAPHORE TestLock; ULONG Used; UCHAR Data[PACKET_BYTES]; } FILTER;
static const GUID audio = {0x73647561,0,0x10,{0x80,0,0,0xaa,0,0x38,0x9b,0x71}};
static const GUID pcm = {1,0,0x10,{0x80,0,0,0xaa,0,0x38,0x9b,0x71}};
static const GUID wave = {0x05589f81,0xc356,0x11ce,{0xbf,1,0,0xaa,0,0x55,0x59,0x5a}};
static const GUID stream = {0x1a8766a0,0x62ce,0x11cf,{0xa5,0xd6,0x28,0xdb,4,0xc1,0,0}};
static const GUID medium = {0x4747b320,0x62ce,0x11cf,{0xa5,0xd6,0x28,0xdb,4,0xc1,0,0}};
static const GUID connection = {0x1d58c920,0xac9b,0x11cf,{0xa5,0xd6,0x28,0xdb,4,0xc1,0,0}};
static const WCHAR name[] = L"\\Device\\WkKsPinQa", link[] = L"\\DosDevices\\WkKsPinQa";

static VOID trace(const char *text)
{
    while (*text) WRITE_PORT_UCHAR((PUCHAR)0xe9, (UCHAR)*text++);
    WRITE_PORT_UCHAR((PUCHAR)0xe9, '\n');
}

static NTSTATUS create(PIRP irp, PFILE_OBJECT file)
{
    KSDATARANGE range = {0};
    PKSDATARANGE ranges[] = {&range};
    KSPIN_DESCRIPTOR pins[2] = {{0}};
    PKSPIN_CONNECT captured;
    PKSDATAFORMAT_WAVEFORMATEX selected;
    NTSTATUS status;
    PIN *pin;
    if (!file->FileName.Length && !file->RelatedFileObject) return STATUS_SUCCESS;
    range.FormatSize = sizeof(range);
    range.MajorFormat = audio;
    range.SubFormat = pcm;
    range.Specifier = wave;
    pins[0].Communication = pins[1].Communication = KSPIN_COMMUNICATION_SINK;
    pins[0].DataFlow = KSPIN_DATAFLOW_IN;
    pins[1].DataFlow = KSPIN_DATAFLOW_OUT;
    pins[0].DataRanges = pins[1].DataRanges = ranges;
    pins[0].DataRangesCount = pins[1].DataRangesCount = 1;
    status = KsValidateConnectRequest(irp, 2, pins, &captured);
    if (!NT_SUCCESS(status)) return status;
    selected = (PKSDATAFORMAT_WAVEFORMATEX)(captured + 1);
    status = STATUS_NO_MATCH;
    if (selected->DataFormat.FormatSize != sizeof(*selected) ||
        selected->WaveFormatEx.wFormatTag != WAVE_FORMAT_PCM ||
        selected->WaveFormatEx.nChannels != 2 || selected->WaveFormatEx.nSamplesPerSec != 48000 ||
        selected->WaveFormatEx.wBitsPerSample != 16 || selected->WaveFormatEx.nBlockAlign != 4 ||
        selected->WaveFormatEx.nAvgBytesPerSec != 192000 || selected->WaveFormatEx.cbSize) goto done;
    pin = ExAllocatePool(NonPagedPool, sizeof(*pin));
    if (!pin) { status = STATUS_INSUFFICIENT_RESOURCES; goto done; }
    pin->Id = captured->PinId;
    pin->State = KSSTATE_STOP;
    file->FsContext = pin;
    status = STATUS_SUCCESS;
done:
    ExFreePool(captured);
    return status;
}

/* METHOD_NEITHER is used by the KS stream controls. This isolated filter accepts
 * packet buffers only from its own kernel client. User-mode opens exercise the
 * counted create data captured by the NT I/O manager. */
static NTSTATUS pinControl(FILTER *filter, PIN *pin, PIRP irp, PIO_STACK_LOCATION stack)
{
    ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;
    ULONG output = stack->Parameters.DeviceIoControl.OutputBufferLength;
    PKSSTREAM_HEADER header = irp->UserBuffer;
    if (irp->RequestorMode != KernelMode) return STATUS_ACCESS_DENIED;
    if (code == IOCTL_KS_PROPERTY) {
        const KSPROPERTY *property = stack->Parameters.DeviceIoControl.Type3InputBuffer;
        KSSTATE *state = irp->UserBuffer;
        if (!property || !state || stack->Parameters.DeviceIoControl.InputBufferLength != sizeof(*property) ||
            output != sizeof(*state) || !IsEqualGUID(&property->Set, &connection) ||
            property->Id != KSPROPERTY_CONNECTION_STATE || property->Flags != KSPROPERTY_TYPE_SET)
            return STATUS_INVALID_PARAMETER;
        if (*state < KSSTATE_STOP || *state > KSSTATE_RUN) return STATUS_INVALID_PARAMETER;
        pin->State = *state;
        return STATUS_SUCCESS;
    }
    if (code != (pin->Id ? IOCTL_KS_READ_STREAM : IOCTL_KS_WRITE_STREAM)) return STATUS_INVALID_DEVICE_REQUEST;
    if (pin->State != KSSTATE_RUN) return STATUS_INVALID_DEVICE_STATE;
    if (output != sizeof(*header) || !header || header->Size != sizeof(*header) || !header->Data ||
        header->OptionsFlags || header->FrameExtent > PACKET_BYTES) return STATUS_INVALID_PARAMETER;
    if (code == IOCTL_KS_WRITE_STREAM) {
        if (!header->DataUsed || header->DataUsed > header->FrameExtent || (header->DataUsed & 3))
            return STATUS_INVALID_BUFFER_SIZE;
        RtlCopyMemory(filter->Data, header->Data, header->DataUsed);
        filter->Used = header->DataUsed;
    } else {
        if (!filter->Used || header->FrameExtent < filter->Used) return STATUS_BUFFER_TOO_SMALL;
        RtlCopyMemory(header->Data, filter->Data, filter->Used);
        header->DataUsed = filter->Used;
        filter->Used = 0;
    }
    irp->IoStatus.Information = sizeof(*header);
    return STATUS_SUCCESS;
}

static NTSTATUS control(HANDLE pin, ULONG code, PVOID input, ULONG inBytes, PVOID data, ULONG bytes)
{
    IO_STATUS_BLOCK io;
    NTSTATUS status = ZwDeviceIoControlFile(pin, NULL, NULL, NULL, &io, code, input, inBytes, data, bytes);
    if (status != STATUS_PENDING) return status;
    status = NtWaitForSingleObject(pin, FALSE, NULL);
    return NT_SUCCESS(status) ? io.Status : status;
}

static NTSTATUS roundTrip(void)
{
    struct { KSPIN_CONNECT Connect; KSDATAFORMAT_WAVEFORMATEX Format; } request = {0};
    KSPROPERTY property = {0};
    KSSTREAM_HEADER header = {0};
    UCHAR sent[PACKET_BYTES], received[PACKET_BYTES];
    UNICODE_STRING path;
    OBJECT_ATTRIBUTES attributes;
    IO_STATUS_BLOCK io;
    HANDLE root, input = NULL, output = NULL;
    ULONG access = GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE, i;
    NTSTATUS status;
    KSSTATE state;
    C_ASSERT(FIELD_OFFSET(__typeof__(request), Format) == sizeof(KSPIN_CONNECT));
    RtlInitUnicodeString(&path, name);
    InitializeObjectAttributes(&attributes, &path, OBJ_CASE_INSENSITIVE, NULL, NULL);
    status = ZwCreateFile(&root, access, &attributes, &io, NULL, FILE_ATTRIBUTE_NORMAL,
                          FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_OPEN, 0, NULL, 0);
    if (!NT_SUCCESS(status)) return status;
    request.Connect.Interface.Set = stream;
    request.Connect.Interface.Id = KSINTERFACE_STANDARD_STREAMING;
    request.Connect.Medium.Set = medium;
    request.Connect.Priority.PriorityClass = KSPRIORITY_NORMAL;
    request.Connect.Priority.PrioritySubClass = 1;
    request.Format.DataFormat.FormatSize = sizeof(request.Format);
    request.Format.DataFormat.MajorFormat = audio;
    request.Format.DataFormat.SubFormat = pcm;
    request.Format.DataFormat.Specifier = wave;
    request.Format.DataFormat.SampleSize = 4;
    request.Format.WaveFormatEx.wFormatTag = WAVE_FORMAT_PCM;
    request.Format.WaveFormatEx.nChannels = 2;
    request.Format.WaveFormatEx.nBlockAlign = 4;
    request.Format.WaveFormatEx.wBitsPerSample = 16;
    request.Format.WaveFormatEx.nSamplesPerSec = 44100;
    request.Format.WaveFormatEx.nAvgBytesPerSec = 192000;
    status = KsCreatePin(root, &request.Connect, access, &input);
    if (status != STATUS_NO_MATCH || input) { status = STATUS_DEVICE_DATA_ERROR; goto done; }
    request.Format.WaveFormatEx.nSamplesPerSec = 48000;
    request.Connect.PinId = 2;
    status = KsCreatePin(root, &request.Connect, access, &input);
    if (status != STATUS_INVALID_PARAMETER || input) { status = STATUS_DEVICE_DATA_ERROR; goto done; }
    request.Connect.PinId = 0;
    status = KsCreatePin(root, &request.Connect, access, &input);
    if (!NT_SUCCESS(status)) goto done;
    request.Connect.PinId = 1;
    status = KsCreatePin(root, &request.Connect, access, &output);
    if (!NT_SUCCESS(status)) goto done;
    trace("KS native pin handles: render and capture at 48000 Hz stereo PASS");
    header.Size = sizeof(header);
    header.Data = sent;
    header.FrameExtent = header.DataUsed = sizeof(sent);
    for (i = 0; i < sizeof(sent); ++i) sent[i] = (UCHAR)((i * 73) ^ (i >> 3));
    status = control(input, IOCTL_KS_WRITE_STREAM, NULL, 0, &header, sizeof(header));
    if (status != STATUS_INVALID_DEVICE_STATE) { status = STATUS_DEVICE_DATA_ERROR; goto done; }
    property.Set = connection;
    property.Id = KSPROPERTY_CONNECTION_STATE;
    property.Flags = KSPROPERTY_TYPE_SET;
    for (state = KSSTATE_ACQUIRE; state <= KSSTATE_RUN; ++state) {
        status = control(input, IOCTL_KS_PROPERTY, &property, sizeof(property), &state, sizeof(state));
        if (!NT_SUCCESS(status)) goto done;
        status = control(output, IOCTL_KS_PROPERTY, &property, sizeof(property), &state, sizeof(state));
        if (!NT_SUCCESS(status)) goto done;
    }
    status = control(input, IOCTL_KS_WRITE_STREAM, NULL, 0, &header, sizeof(header));
    if (!NT_SUCCESS(status)) goto done;
    header.DataUsed = 0;
    header.Data = received;
    status = control(output, IOCTL_KS_READ_STREAM, NULL, 0, &header, sizeof(header));
    if (!NT_SUCCESS(status)) goto done;
    if (header.DataUsed != sizeof(sent) || RtlCompareMemory(sent, received, sizeof(sent)) != sizeof(sent))
        status = STATUS_DEVICE_DATA_ERROR;
    if (NT_SUCCESS(status)) trace("KS state transitions, WRITE_STREAM and READ_STREAM: 512 matching PCM bytes PASS");
done:
    if (output) ZwClose(output);
    if (input) ZwClose(input);
    ZwClose(root);
    return status;
}

static NTSTATUS NTAPI dispatch(PDEVICE_OBJECT device, PIRP irp)
{
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
    PFILE_OBJECT file = stack->FileObject;
    FILTER *filter = device->DeviceExtension;
    NTSTATUS status = STATUS_SUCCESS;
    irp->IoStatus.Information = 0;
    switch (stack->MajorFunction) {
    case IRP_MJ_CREATE:
        status = create(irp, file);
        break;
    case IRP_MJ_CLOSE:
        if (file->FsContext) ExFreePool(file->FsContext);
        file->FsContext = NULL;
        break;
    case IRP_MJ_CLEANUP:
        break;
    case IRP_MJ_DEVICE_CONTROL:
        if (file->FsContext) status = pinControl(filter, file->FsContext, irp, stack);
        else if (stack->Parameters.DeviceIoControl.IoControlCode == WK_KS_ROUNDTRIP) {
            KeWaitForSingleObject(&filter->TestLock, Executive, KernelMode, FALSE, NULL);
            filter->Used = 0;
            status = roundTrip();
            KeReleaseSemaphore(&filter->TestLock, IO_NO_INCREMENT, 1, FALSE);
        } else status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
    }
    irp->IoStatus.Status = status;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}

static VOID NTAPI removeFilter(PDRIVER_OBJECT driver)
{
    UNICODE_STRING alias;
    RtlInitUnicodeString(&alias, link);
    IoDeleteSymbolicLink(&alias);
    IoDeleteDevice(driver->DeviceObject);
}

NTSTATUS WkStartKsFilter(PDRIVER_OBJECT driver)
{
    UNICODE_STRING path, alias;
    PDEVICE_OBJECT device;
    FILTER *filter;
    NTSTATUS status;
    RtlInitUnicodeString(&path, name);
    RtlInitUnicodeString(&alias, link);
    status = IoCreateDevice(driver, sizeof(FILTER), &path, FILE_DEVICE_KS, 0, FALSE, &device);
    if (!NT_SUCCESS(status)) return status;
    filter = device->DeviceExtension;
    KeInitializeSemaphore(&filter->TestLock, 1, 1);
    driver->MajorFunction[IRP_MJ_CREATE] = driver->MajorFunction[IRP_MJ_CLOSE] = dispatch;
    driver->MajorFunction[IRP_MJ_CLEANUP] = driver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = dispatch;
    driver->DriverUnload = removeFilter;
    status = IoCreateSymbolicLink(&alias, &path);
    if (!NT_SUCCESS(status)) { IoDeleteDevice(device); return status; }
    device->Flags &= ~DO_DEVICE_INITIALIZING;
    return STATUS_SUCCESS;
}
