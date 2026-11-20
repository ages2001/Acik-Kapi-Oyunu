/* Match the data ranges in a captured KSPROPERTY_PIN_DATAINTERSECTION request. */
#define _KSDDK_
#include "nt_legacy.h"
#include <ks.h>

/* The x86 DDK uses stdcall by default. The MinGW ks.h typedef omits it. */
typedef NTSTATUS (NTAPI *X86_INTERSECT)(PIRP, PKSP_PIN, PKSDATARANGE, PVOID);

static BOOLEAN sameGuid(const GUID *left, const GUID *right)
{
    const UCHAR *a = (const UCHAR *)left, *b = (const UCHAR *)right;
    ULONG i;
    for (i = 0; i < sizeof(GUID); ++i)
        if (a[i] != b[i]) return FALSE;
    return TRUE;
}

static BOOLEAN matchGuid(const GUID *requested, const GUID *available)
{
    static const GUID wildcard = {0};
    return sameGuid(requested, available) || sameGuid(requested, &wildcard) ||
           sameGuid(available, &wildcard);
}

NTSTATUS NTAPI KsPinDataIntersection(PIRP irp, PKSP_PIN pin, PVOID output,
                                     ULONG count, const KSPIN_DESCRIPTOR *descriptors,
                                     PFNKSINTERSECTHANDLER intersect)
{
    PIO_STACK_LOCATION stack;
    const KSPIN_DESCRIPTOR *factory;
    const KSMULTIPLE_ITEM *list;
    const UCHAR *cursor;
    ULONG input, remaining, i, j;
    if (!irp) return STATUS_INVALID_PARAMETER;
    irp->IoStatus.Information = 0;
    stack = IoGetCurrentIrpStackLocation(irp);
    input = stack->Parameters.DeviceIoControl.InputBufferLength;
    if (!pin || !descriptors || !intersect) return STATUS_INVALID_PARAMETER;
    if (input < sizeof(*pin) + sizeof(*list)) return STATUS_INVALID_BUFFER_SIZE;
    if (pin->PinId >= count || pin->Reserved) return STATUS_INVALID_PARAMETER;
    if (pin->Property.Id != KSPROPERTY_PIN_DATAINTERSECTION ||
        !(pin->Property.Flags & KSPROPERTY_TYPE_GET)) return STATUS_NOT_SUPPORTED;
    if (!output && stack->Parameters.DeviceIoControl.OutputBufferLength) return STATUS_INVALID_PARAMETER;
    list = (const KSMULTIPLE_ITEM *)(pin + 1);
    if (list->Size < sizeof(*list) || list->Size > input - sizeof(*pin)) return STATUS_INVALID_BUFFER_SIZE;
    factory = &descriptors[pin->PinId];
    if (factory->DataRangesCount && !factory->DataRanges) return STATUS_INVALID_PARAMETER;
    cursor = (const UCHAR *)(list + 1);
    remaining = list->Size - sizeof(*list);
    for (i = 0; i < list->Count; ++i) {
        PKSDATARANGE requested = (PKSDATARANGE)cursor;
        ULONG size, pad;
        if (remaining < sizeof(*requested)) return STATUS_INVALID_BUFFER_SIZE;
        size = requested->FormatSize;
        if (size < sizeof(*requested) || size > remaining) return STATUS_INVALID_BUFFER_SIZE;
        if (requested->Flags & (KSDATARANGE_ATTRIBUTES | KSDATARANGE_REQUIRED_ATTRIBUTES))
            return STATUS_NOT_SUPPORTED;
        for (j = 0; j < factory->DataRangesCount; ++j) {
            const KSDATARANGE *available = factory->DataRanges[j];
            NTSTATUS status;
            if (!available || available->FormatSize < sizeof(*available)) return STATUS_INVALID_PARAMETER;
            if (!matchGuid(&requested->MajorFormat, &available->MajorFormat) ||
                !matchGuid(&requested->SubFormat, &available->SubFormat) ||
                !matchGuid(&requested->Specifier, &available->Specifier)) continue;
            if (available->Flags & (KSDATARANGE_ATTRIBUTES | KSDATARANGE_REQUIRED_ATTRIBUTES))
                return STATUS_NOT_SUPPORTED;
            status = ((X86_INTERSECT)(void (*)(void))intersect)(irp, pin, requested, output);
            if (status != STATUS_NO_MATCH) return status;
            irp->IoStatus.Information = 0;
        }
        if (i + 1 < list->Count) {
            pad = (0UL - size) & 7;
            if (pad > remaining - size) return STATUS_INVALID_BUFFER_SIZE;
            cursor += size + pad;
            remaining -= size + pad;
        }
    }
    return STATUS_NO_MATCH;
}
