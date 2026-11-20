/* Exercise the exported KS handlers from a separately linked native client. */
#include <ntddk.h>
#include <ks.h>

BOOLEAN NTAPI WkTestIntersection(PIRP irp);
NTSTATUS WkStartKsFilter(PDRIVER_OBJECT driver);

static const GUID pinCategory = {0x62011f10, 0x7422, 0x4189, {0x8a,1,2,3,4,5,6,7}};
static const GUID nodeType = {0x62011f11, 0x7422, 0x4189, {0x8a,1,2,3,4,5,6,7}};
static const KSPIN_INTERFACE streamInterface = {
    .Set = {0x1a8766a0,0x62ce,0x11cf,{0xa5,0xd6,0x28,0xdb,4,0xc1,0,0}}, .Id = KSINTERFACE_STANDARD_STREAMING
};
static const KSPIN_MEDIUM deviceMedium = {
    .Set = {0x4747b320,0x62ce,0x11cf,{0xa5,0xd6,0x28,0xdb,4,0xc1,0,0}}, .Id = KSMEDIUM_STANDARD_DEVIO
};
static const KSTOPOLOGY_CONNECTION connection = {KSFILTER_NODE, 1, 0, 0};

static void trace(const char *text)
{
    while (*text) WRITE_PORT_UCHAR((PUCHAR)0xe9, (UCHAR)*text++);
    WRITE_PORT_UCHAR((PUCHAR)0xe9, '\n');
}

static BOOLEAN equal(const VOID *left, const VOID *right, ULONG bytes)
{
    const UCHAR *a = left, *b = right;
    while (bytes--) if (*a++ != *b++) return FALSE;
    return TRUE;
}

static BOOLEAN run(PIRP irp)
{
    KSP_PIN request = {0};
    KSPIN_DESCRIPTOR pins[2] = {{0}};
    KSTOPOLOGY topology = {0};
    KSDATARANGE firstRange = {0};
    struct { KSDATARANGE Header; UCHAR Extra[8]; } extended = {0};
    PKSDATARANGE ranges[] = {&extended.Header, &firstRange};
    union { LONGLONG Alignment; UCHAR Bytes[256]; ULONG Value; KSMULTIPLE_ITEM Items; } output;
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
    ULONG bytes = sizeof(KSMULTIPLE_ITEM) + sizeof(extended) + sizeof(firstRange);
    pins[1].DataFlow = KSPIN_DATAFLOW_IN;
    pins[1].Communication = KSPIN_COMMUNICATION_SINK;
    pins[1].Category = &pinCategory;
    pins[1].InterfacesCount = pins[1].MediumsCount = 1;
    pins[1].Interfaces = &streamInterface;
    pins[1].Mediums = &deviceMedium;
    pins[1].DataRanges = ranges;
    pins[1].DataRangesCount = 2;
    firstRange.FormatSize = sizeof(firstRange);
    firstRange.MajorFormat = nodeType;
    extended.Header.FormatSize = sizeof(KSDATARANGE) + 3;
    extended.Extra[0] = 0x72;
    extended.Extra[2] = 0xbc;
    topology.CategoriesCount = topology.TopologyNodesCount = topology.TopologyConnectionsCount = 1;
    topology.Categories = &pinCategory;
    topology.TopologyNodes = &nodeType;
    topology.TopologyConnections = &connection;
    stack->MajorFunction = IRP_MJ_DEVICE_CONTROL;
    stack->Parameters.DeviceIoControl.IoControlCode = IOCTL_KS_PROPERTY;
    stack->Parameters.DeviceIoControl.InputBufferLength = sizeof(KSPROPERTY);
    stack->Parameters.DeviceIoControl.OutputBufferLength = sizeof(output);
    request.Property.Flags = KSPROPERTY_TYPE_GET;
    request.Property.Id = KSPROPERTY_PIN_CTYPES;
    request.PinId = 1;
#define PIN() KsPinPropertyHandler(irp, &request.Property, &output, 2, pins)
#define TOPOLOGY() KsTopologyPropertyHandler(irp, &request.Property, &output, &topology)
    if (PIN() != STATUS_SUCCESS || output.Value != 2 || irp->IoStatus.Information != sizeof(ULONG))
        return FALSE;
    stack->Parameters.DeviceIoControl.InputBufferLength = sizeof(request);
    request.Property.Id = KSPROPERTY_PIN_DATAFLOW;
    if (PIN() != STATUS_SUCCESS || output.Value != KSPIN_DATAFLOW_IN) return FALSE;
    request.Property.Id = KSPROPERTY_PIN_COMMUNICATION;
    if (PIN() != STATUS_SUCCESS || output.Value != KSPIN_COMMUNICATION_SINK) return FALSE;
    request.Property.Id = KSPROPERTY_PIN_CATEGORY;
    if (PIN() != STATUS_SUCCESS || !equal(&output, &pinCategory, sizeof(GUID))) return FALSE;
    request.Property.Id = KSPROPERTY_PIN_INTERFACES;
    if (PIN() != STATUS_SUCCESS || output.Items.Count != 1 ||
        output.Items.Size != sizeof(output.Items) + sizeof(streamInterface) ||
        !equal(&output.Items + 1, &streamInterface, sizeof(streamInterface))) return FALSE;
    request.Property.Id = KSPROPERTY_PIN_MEDIUMS;
    if (PIN() != STATUS_SUCCESS || output.Items.Count != 1 ||
        !equal(&output.Items + 1, &deviceMedium, sizeof(deviceMedium))) return FALSE;
    trace("KS pin count, dataflow, communication, category, interface and medium PASS");
    request.Property.Id = KSPROPERTY_PIN_DATARANGES;
    output.Value = 0xcccccccc;
    stack->Parameters.DeviceIoControl.OutputBufferLength = 0;
    if (PIN() != STATUS_BUFFER_OVERFLOW || irp->IoStatus.Information != bytes) return FALSE;
    stack->Parameters.DeviceIoControl.OutputBufferLength = bytes - 1;
    if (PIN() != STATUS_BUFFER_TOO_SMALL || output.Value != 0xcccccccc || irp->IoStatus.Information) return FALSE;
    stack->Parameters.DeviceIoControl.OutputBufferLength = bytes;
    RtlFillMemory(output.Bytes, sizeof(output), 0xcc);
    if (PIN() != STATUS_SUCCESS || irp->IoStatus.Information != bytes || output.Items.Size != bytes ||
        output.Items.Count != 2 || !equal(output.Bytes + sizeof(output.Items), &extended, sizeof(extended)) ||
        !equal(output.Bytes + sizeof(output.Items) + sizeof(extended), &firstRange, sizeof(firstRange)) ||
        output.Bytes[bytes] != 0xcc) return FALSE;
    trace("KS extended data ranges, length query, alignment, padding and short buffer PASS");
    stack->Parameters.DeviceIoControl.OutputBufferLength = sizeof(output);
    request.Property.Id = KSPROPERTY_TOPOLOGY_CATEGORIES;
    if (TOPOLOGY() != STATUS_SUCCESS || !equal(&output.Items + 1, &pinCategory, sizeof(GUID))) return FALSE;
    request.Property.Id = KSPROPERTY_TOPOLOGY_NODES;
    if (TOPOLOGY() != STATUS_SUCCESS || !equal(&output.Items + 1, &nodeType, sizeof(GUID))) return FALSE;
    request.Property.Id = KSPROPERTY_TOPOLOGY_CONNECTIONS;
    if (TOPOLOGY() != STATUS_SUCCESS || output.Items.Count != 1 ||
        !equal(&output.Items + 1, &connection, sizeof(connection))) return FALSE;
    trace("KS topology categories, node types and directed connections PASS");
    request.Property.Id = KSPROPERTY_PIN_DATARANGES;
    request.PinId = 0;
    if (PIN() != STATUS_SUCCESS || output.Items.Count || output.Items.Size != sizeof(KSMULTIPLE_ITEM)) return FALSE;
    request.PinId = 2;
    if (PIN() != STATUS_INVALID_PARAMETER || irp->IoStatus.Information) return FALSE;
    request.PinId = 1;
    firstRange.Flags = KSDATARANGE_ATTRIBUTES;
    if (PIN() != STATUS_NOT_SUPPORTED || irp->IoStatus.Information) return FALSE;
    firstRange.Flags = 0;
    firstRange.FormatSize = MAXULONG;
    if (PIN() != STATUS_INTEGER_OVERFLOW || irp->IoStatus.Information) return FALSE;
    request.Property.Id = KSPROPERTY_PIN_CINSTANCES;
    if (PIN() != STATUS_NOT_SUPPORTED || irp->IoStatus.Information) return FALSE;
    stack->Parameters.DeviceIoControl.InputBufferLength = sizeof(KSPROPERTY);
    if (PIN() != STATUS_INVALID_BUFFER_SIZE) return FALSE;
    if (irp->IoStatus.Status != STATUS_PENDING) return FALSE;
    trace("KS empty descriptors, invalid pin, oversized and unsupported ranges, IRP ownership PASS");
    if (!WkTestIntersection(irp)) return FALSE;
    trace("KS ordered PCM intersection, 48000 Hz stereo 16-bit format and callback ABI PASS");
    trace("KS wildcards, size query, short buffer, no match and invalid packed input PASS");
    return TRUE;
}

NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registryPath)
{
    PIRP irp = IoAllocateIrp(1, FALSE);
    BOOLEAN passed;
    (void)registryPath;
    if (!irp) return STATUS_INSUFFICIENT_RESOURCES;
    IoSetNextIrpStackLocation(irp);
    irp->IoStatus.Status = STATUS_PENDING;
    passed = run(irp);
    IoFreeIrp(irp);
    if (!passed) { trace("KS property client FAIL"); return STATUS_UNSUCCESSFUL; }
    return WkStartKsFilter(driver);
}
