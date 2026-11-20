/* Counted NT file names carry the pin class, a separator and binary create data. */
#define _KSDDK_
#include "nt_legacy.h"
#include <ks.h>

static const WCHAR pinName[] = KSSTRING_Pin L"\\";
#define PIN_PREFIX_BYTES (sizeof(pinName) - sizeof(WCHAR))
static const KSIDENTIFIER defaultInterface = {
    .Set = {0x1a8766a0,0x62ce,0x11cf,{0xa5,0xd6,0x28,0xdb,4,0xc1,0,0}},
    .Id = KSINTERFACE_STANDARD_STREAMING
};
static const KSIDENTIFIER defaultMedium = {
    .Set = {0x4747b320,0x62ce,0x11cf,{0xa5,0xd6,0x28,0xdb,4,0xc1,0,0}},
    .Id = KSMEDIUM_STANDARD_DEVIO
};
static const GUID wildcard = {0};

NTSTATUS NTAPI KsCreatePin(HANDLE filter, PKSPIN_CONNECT connect, ACCESS_MASK access, PHANDLE result)
{
    const KSDATAFORMAT *format;
    UNICODE_STRING name;
    OBJECT_ATTRIBUTES attributes;
    IO_STATUS_BLOCK io;
    ULONG bytes, limit = 0xfffe - PIN_PREFIX_BYTES - sizeof(*connect);
    NTSTATUS status;
    if (!result) return STATUS_INVALID_PARAMETER;
    *result = NULL;
    if (!filter || !connect) return STATUS_INVALID_PARAMETER;
    format = (const KSDATAFORMAT *)(connect + 1);
    if (format->FormatSize < sizeof(*format)) return STATUS_INVALID_BUFFER_SIZE;
    if (format->FormatSize > limit) return STATUS_NAME_TOO_LONG;
    if (format->Flags & KSDATAFORMAT_ATTRIBUTES) return STATUS_NOT_SUPPORTED;
    bytes = sizeof(*connect) + format->FormatSize;
    name.Length = name.MaximumLength = (USHORT)(PIN_PREFIX_BYTES + ((bytes + 1) & ~1UL));
    name.Buffer = ExAllocatePool(PagedPool, name.Length);
    if (!name.Buffer) return STATUS_INSUFFICIENT_RESOURCES;
    RtlCopyMemory(name.Buffer, pinName, PIN_PREFIX_BYTES);
    RtlCopyMemory((PUCHAR)name.Buffer + PIN_PREFIX_BYTES, connect, bytes);
    if (bytes & 1) ((PUCHAR)name.Buffer)[name.Length - 1] = 0;
    InitializeObjectAttributes(&attributes, &name, OBJ_CASE_INSENSITIVE, filter, NULL);
    status = ZwCreateFile(result, access, &attributes, &io, NULL, FILE_ATTRIBUTE_NORMAL, 0,
                          FILE_OPEN, 0, NULL, 0);
    ExFreePool(name.Buffer);
    return status;
}

static BOOLEAN supportsId(const KSIDENTIFIER *id, const KSIDENTIFIER *items, ULONG count,
                          const KSIDENTIFIER *fallback)
{
    ULONG i;
    if (!count) { items = fallback; count = 1; }
    if (!items || id->Flags) return FALSE;
    for (i = 0; i < count; ++i)
        if (id->Id == items[i].Id && IsEqualGUID(&id->Set, &items[i].Set)) return TRUE;
    return FALSE;
}

static NTSTATUS supportsFormat(const KSPIN_DESCRIPTOR *pin, const KSDATAFORMAT *format)
{
    ULONG i;
    if (format->Flags & KSDATAFORMAT_ATTRIBUTES) return STATUS_NOT_SUPPORTED;
    if (format->Reserved || (pin->DataRangesCount && !pin->DataRanges)) return STATUS_INVALID_PARAMETER;
    for (i = 0; i < pin->DataRangesCount; ++i) {
        const KSDATARANGE *range = pin->DataRanges[i];
        if (!range || range->FormatSize < sizeof(*range)) return STATUS_INVALID_PARAMETER;
        if (range->Flags & KSDATARANGE_REQUIRED_ATTRIBUTES) continue;
        if (IsEqualGUID(&range->MajorFormat, &wildcard)) return STATUS_SUCCESS;
        if (!IsEqualGUID(&range->MajorFormat, &format->MajorFormat)) continue;
        if (IsEqualGUID(&range->SubFormat, &wildcard)) return STATUS_SUCCESS;
        if (IsEqualGUID(&range->SubFormat, &format->SubFormat) &&
            IsEqualGUID(&range->Specifier, &format->Specifier)) return STATUS_SUCCESS;
    }
    return STATUS_NO_MATCH;
}

NTSTATUS NTAPI KsValidateConnectRequest(PIRP irp, ULONG count, const KSPIN_DESCRIPTOR *pins,
                                         PKSPIN_CONNECT *result)
{
    PIO_STACK_LOCATION stack;
    PFILE_OBJECT peer;
    UNICODE_STRING item, expected;
    const UCHAR *data;
    ULONG bytes;
    PKSPIN_CONNECT captured;
    PKSDATAFORMAT format;
    const KSPIN_DESCRIPTOR *pin;
    NTSTATUS status;
    if (!result) return STATUS_INVALID_PARAMETER;
    *result = NULL;
    if (!irp || !pins || !count) return STATUS_INVALID_PARAMETER;
    stack = IoGetCurrentIrpStackLocation(irp);
    if (stack->MajorFunction != IRP_MJ_CREATE || !stack->FileObject) return STATUS_INVALID_DEVICE_REQUEST;
    item = stack->FileObject->FileName;
    if (!item.Buffer || item.Length > item.MaximumLength || (item.Length & 1)) return STATUS_INVALID_BUFFER_SIZE;
    if (item.Length && item.Buffer[0] == L'\\') {
        ++item.Buffer;
        item.Length -= sizeof(WCHAR);
    }
    if (item.Length < PIN_PREFIX_BYTES) return STATUS_OBJECT_NAME_INVALID;
    bytes = item.Length - PIN_PREFIX_BYTES;
    item.Length = item.MaximumLength = PIN_PREFIX_BYTES;
    RtlInitUnicodeString(&expected, pinName);
    if (!RtlEqualUnicodeString(&item, &expected, TRUE)) return STATUS_OBJECT_NAME_INVALID;
    if (bytes < sizeof(*captured) + sizeof(*format)) return STATUS_INVALID_BUFFER_SIZE;
    /* The I/O manager captured this counted name for IRP_MJ_CREATE. The binary
     * tail can be only WCHAR-aligned. Copy it before accessing its fields. */
    data = (const UCHAR *)item.Buffer + PIN_PREFIX_BYTES;
    captured = ExAllocatePool(NonPagedPool, bytes);
    if (!captured) return STATUS_INSUFFICIENT_RESOURCES;
    RtlCopyMemory(captured, data, bytes);
    format = (PKSDATAFORMAT)(captured + 1);
    status = STATUS_INVALID_BUFFER_SIZE;
    if (format->FormatSize < sizeof(*format) || format->FormatSize > bytes - sizeof(*captured)) goto fail;
    status = STATUS_INVALID_PARAMETER;
    if (captured->PinId >= count) goto fail;
    pin = &pins[captured->PinId];
    status = STATUS_NO_MATCH;
    if (pin->Communication != KSPIN_COMMUNICATION_BOTH &&
        pin->Communication != (captured->PinToHandle ? KSPIN_COMMUNICATION_SOURCE : KSPIN_COMMUNICATION_SINK))
        goto fail;
    if (!supportsId(&captured->Interface, pin->Interfaces, pin->InterfacesCount, &defaultInterface) ||
        !supportsId(&captured->Medium, pin->Mediums, pin->MediumsCount, &defaultMedium)) goto fail;
    status = supportsFormat(pin, format);
    if (!NT_SUCCESS(status)) goto fail;
    if (captured->PinToHandle) {
        status = ObReferenceObjectByHandle(captured->PinToHandle, 0, *IoFileObjectType, irp->RequestorMode,
                                           (PVOID *)&peer, NULL);
        if (!NT_SUCCESS(status)) goto fail;
        ObDereferenceObject(peer);
    }
    *result = captured;
    return STATUS_SUCCESS;
fail:
    ExFreePool(captured);
    return status;
}
