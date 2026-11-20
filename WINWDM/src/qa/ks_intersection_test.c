/* A separate KS client with a stereo PCM pin and a minidriver range callback. */
#include <ntddk.h>
#include <ks.h>
#include <windef.h>
#include <tchar.h>
#define NOBITMAP
#include <mmreg.h>
#include <ksmedia.h>

static const GUID audio = {0x73647561,0x0000,0x0010,{0x80,0,0,0xaa,0,0x38,0x9b,0x71}};
static const GUID pcm = {WAVE_FORMAT_PCM,0x0000,0x0010,{0x80,0,0,0xaa,0,0x38,0x9b,0x71}};
static const GUID waveFormat = {0x05589f81,0xc356,0x11ce,{0xbf,1,0,0xaa,0,0x55,0x59,0x5a}};
static ULONG visits;

static NTSTATUS NTAPI pcmIntersection(PIRP irp, PKSP_PIN pin, PKSDATARANGE range, PVOID data)
{
    const KSDATARANGE_AUDIO *request = (const KSDATARANGE_AUDIO *)range;
    PKSDATAFORMAT_WAVEFORMATEX format = data;
    ULONG capacity = IoGetCurrentIrpStackLocation(irp)->Parameters.DeviceIoControl.OutputBufferLength;
    ++visits;
    if (pin->PinId != 1 || range->FormatSize < sizeof(*request) ||
        request->MaximumChannels < 2 || request->MinimumBitsPerSample > 16 ||
        request->MaximumBitsPerSample < 16 || request->MinimumSampleFrequency > 48000 ||
        request->MaximumSampleFrequency < 48000) return STATUS_NO_MATCH;
    if (!capacity) {
        irp->IoStatus.Information = sizeof(*format);
        return STATUS_BUFFER_OVERFLOW;
    }
    if (capacity < sizeof(*format)) return STATUS_BUFFER_TOO_SMALL;
    RtlZeroMemory(format, sizeof(*format));
    format->DataFormat.FormatSize = sizeof(*format);
    format->DataFormat.SampleSize = 4;
    format->DataFormat.MajorFormat = audio;
    format->DataFormat.SubFormat = pcm;
    format->DataFormat.Specifier = waveFormat;
    format->WaveFormatEx.wFormatTag = WAVE_FORMAT_PCM;
    format->WaveFormatEx.nSamplesPerSec = 48000;
    format->WaveFormatEx.nChannels = 2;
    format->WaveFormatEx.wBitsPerSample = 16;
    format->WaveFormatEx.nBlockAlign = 4;
    format->WaveFormatEx.nAvgBytesPerSec = 192000;
    irp->IoStatus.Information = sizeof(*format);
    return STATUS_SUCCESS;
}

BOOLEAN NTAPI WkTestIntersection(PIRP irp)
{
    struct {
        KSP_PIN pin;
        KSMULTIPLE_ITEM list;
        KSDATARANGE_AUDIO candidates[4];
    } query = {0};
    KSDATARANGE_AUDIO available = {0};
    KSDATAFORMAT_WAVEFORMATEX output;
    PKSDATARANGE ranges[] = { &available.DataRange };
    KSPIN_DESCRIPTOR descriptors[2] = {{0}};
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
    ULONG i;
    NTSTATUS status;
    available.DataRange.FormatSize = sizeof(available);
    available.DataRange.MajorFormat = audio;
    available.DataRange.SubFormat = pcm;
    available.DataRange.Specifier = waveFormat;
    available.MinimumSampleFrequency = available.MaximumSampleFrequency = 48000;
    available.MinimumBitsPerSample = available.MaximumBitsPerSample = 16;
    available.MaximumChannels = 2;
    descriptors[1].DataRangesCount = 1;
    descriptors[1].DataRanges = ranges;
    query.pin.Property.Id = KSPROPERTY_PIN_DATAINTERSECTION;
    query.pin.Property.Flags = KSPROPERTY_TYPE_GET;
    query.pin.PinId = 1;
    query.list.Size = sizeof(query) - sizeof(query.pin);
    query.list.Count = 4;
    for (i = 0; i < 4; ++i) query.candidates[i] = available;
    /* First GUID mismatch, rejected rate, caller wildcard, unused exact match. */
    query.candidates[0].DataRange.SubFormat.Data1 = WAVE_FORMAT_ALAW;
    query.candidates[1].MinimumSampleFrequency = query.candidates[1].MaximumSampleFrequency = 44100;
    RtlZeroMemory(&query.candidates[2].DataRange.SubFormat, sizeof(GUID));
    stack->Parameters.DeviceIoControl.InputBufferLength = sizeof(query);
    stack->Parameters.DeviceIoControl.OutputBufferLength = sizeof(output);
#define INTERSECT(out) KsPinDataIntersection(irp, &query.pin, (out), 2, descriptors, \
                          (PFNKSINTERSECTHANDLER)(void (*)(void))pcmIntersection)
    visits = 0;
    status = INTERSECT(&output);
    if (status != STATUS_SUCCESS || visits != 2 || irp->IoStatus.Information != sizeof(output) ||
        output.WaveFormatEx.wFormatTag != WAVE_FORMAT_PCM || output.WaveFormatEx.nChannels != 2 ||
        output.WaveFormatEx.nSamplesPerSec != 48000 || output.WaveFormatEx.wBitsPerSample != 16 ||
        output.WaveFormatEx.nBlockAlign != 4 || output.WaveFormatEx.nAvgBytesPerSec != 192000)
        return FALSE;
    stack->Parameters.DeviceIoControl.OutputBufferLength = 0;
    visits = 0;
    if (INTERSECT(NULL) != STATUS_BUFFER_OVERFLOW || visits != 2 ||
        irp->IoStatus.Information != sizeof(output)) return FALSE;
    stack->Parameters.DeviceIoControl.OutputBufferLength = sizeof(output) - 1;
    output.WaveFormatEx.nSamplesPerSec = 123;
    if (INTERSECT(&output) != STATUS_BUFFER_TOO_SMALL || irp->IoStatus.Information ||
        output.WaveFormatEx.nSamplesPerSec != 123) return FALSE;
    stack->Parameters.DeviceIoControl.OutputBufferLength = sizeof(output);
    query.list.Count = 1;
    visits = 0;
    if (INTERSECT(&output) != STATUS_NO_MATCH || visits || irp->IoStatus.Information) return FALSE;
    query.candidates[0] = available;
    query.candidates[0].DataRange.FormatSize = sizeof(KSDATARANGE) - 1;
    if (INTERSECT(&output) != STATUS_INVALID_BUFFER_SIZE || visits) return FALSE;
    query.candidates[0].DataRange.FormatSize = MAXULONG;
    if (INTERSECT(&output) != STATUS_INVALID_BUFFER_SIZE || visits) return FALSE;
    query.candidates[0] = available;
    query.candidates[0].DataRange.Flags = KSDATARANGE_ATTRIBUTES;
    if (INTERSECT(&output) != STATUS_NOT_SUPPORTED || visits) return FALSE;
    query.candidates[0] = available;
    query.list.Size = sizeof(query.list) + sizeof(KSDATARANGE) - 1;
    if (INTERSECT(&output) != STATUS_INVALID_BUFFER_SIZE || visits) return FALSE;
    query.list.Size = sizeof(query) - sizeof(query.pin);
    query.pin.Reserved = 1;
    if (INTERSECT(&output) != STATUS_INVALID_PARAMETER || visits) return FALSE;
    query.pin.Reserved = 0;
    /* The descriptor may advertise a wildcard as well. */
    RtlZeroMemory(&available.DataRange.Specifier, sizeof(GUID));
    return INTERSECT(&output) == STATUS_SUCCESS && visits == 1 && irp->IoStatus.Status == STATUS_PENDING;
}
