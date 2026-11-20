/* Register an XP PCI miniport through the native NT 3.1 video port.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "video.h"

typedef struct {
    WXP_PCI_ADDRESS pci;
    PVIDEO_HW_INITIALIZE initialize;
    PVIDEO_HW_INTERRUPT interrupt;
    PVIDEO_HW_START_IO start_io;
    PVIDEO_HW_TIMER tick;
    KDPC dpc, timer_dpc;
    KTIMER timer;
    WXP_CHILD_PORT children;
    BOOLEAN ticking, finding, registered;
} ADAPTER;

typedef struct {
    PVIDEO_HW_INITIALIZATION_DATA xp;
    PUNICODE_STRING key;
    PVOID context;
    WXP_PCI_ADDRESS pci;
    ADAPTER *adapter;
    BOOLEAN tried;
} REGISTRATION;

C_ASSERT(sizeof(ADAPTER) % 8 == 0);
C_ASSERT(sizeof(KDPC) >= 32);
C_ASSERT(sizeof(KTIMER) >= 40);
C_ASSERT(FIELD_OFFSET(VIDEO_HW_INITIALIZATION_DATA, HwResetHw) == 0x20);

static ADAPTER *adapter(PVOID extension)
{
    return (ADAPTER *)extension - 1;
}

ULONG NTAPI WxpVideoBusRead(PVOID ext, BUS_DATA_TYPE type, ULONG slot, PVOID buf,
                            ULONG offset, ULONG length)
{
    ADAPTER *a = adapter(ext);
    UNREFERENCED_PARAMETER(slot); /* PCI is addressed through the selected function on XP. */
    return HalGetBusDataByOffset(type, a->pci.bus, a->pci.slot, buf, offset, length);
}

ULONG NTAPI WxpVideoBusWrite(PVOID ext, BUS_DATA_TYPE type, ULONG slot, PVOID buf,
                             ULONG offset, ULONG length)
{
    ADAPTER *a = adapter(ext);
    UNREFERENCED_PARAMETER(slot);
    return HalSetBusDataByOffset(type, a->pci.bus, a->pci.slot, buf, offset, length);
}

VP_STATUS NTAPI WxpVgaStatus(PVOID ext, PULONG status)
{
    ADAPTER *a = adapter(ext);
    USHORT command = 0;
    if (!status) return ERROR_INVALID_PARAMETER;
    *status = 0;
    if (WxpVideoBusRead(ext, PCIConfiguration, 0, &command, 4, sizeof(command)) != sizeof(command))
        return ERROR_DEV_NOT_EXIST;
    if (a->pci.vga_route && (command & 3) == 3) *status = DEVICE_VGA_ENABLED;
    return NO_ERROR;
}

VP_STATUS NTAPI WxpAccessRanges(PVOID ext, ULONG requested, PIO_RESOURCE_DESCRIPTOR request,
                                ULONG capacity, PVIDEO_ACCESS_RANGE result, PVOID vendor,
                                PVOID device, PULONG slot)
{
    ADAPTER *a = adapter(ext);
    VIDEO_ACCESS_RANGE ranges[6] = {0};
    ULONG count = 0, i;
    VP_STATUS status;
    UNREFERENCED_PARAMETER(request);
    UNREFERENCED_PARAMETER(vendor);
    UNREFERENCED_PARAMETER(device);
    if (!a->finding) return ERROR_INVALID_FUNCTION;
    if (requested) return ERROR_INVALID_FUNCTION; /* No ISA resource allocator on this path. */
    if (!capacity) return VideoPortVerifyAccessRanges(a, 0, NULL);
    if (!result) return ERROR_INVALID_PARAMETER;
    status = wxp_pci_ranges(&a->pci, ranges, &count);
    if (status) return status;
    if (count > capacity) return ERROR_MORE_DATA;
    status = VideoPortVerifyAccessRanges(a, count, ranges);
    if (status) return status;
    for (i = 0; i < capacity; ++i) {
        if (i < count) result[i] = ranges[i];
        else RtlZeroMemory(&result[i], sizeof(*result));
    }
    if (slot) *slot = a->pci.slot;
    return NO_ERROR;
}

static VOID NTAPI dispatch_dpc(PKDPC dpc, PVOID a, PVOID callback, PVOID context)
{
    UNREFERENCED_PARAMETER(dpc);
    ((PMINIPORT_DPC_ROUTINE)callback)((ADAPTER *)a + 1, context);
}

BOOLEAN NTAPI WxpQueueDpc(PVOID ext, PMINIPORT_DPC_ROUTINE callback, PVOID context)
{
    ADAPTER *a = adapter(ext);
    return a->registered && callback && KeInsertQueueDpc(&a->dpc, callback, context);
}

static VOID arm_timer(ADAPTER *a)
{
    LARGE_INTEGER due;
    due.QuadPart = -10000000;
    KeSetTimer(&a->timer, due, &a->timer_dpc);
}

static VOID NTAPI timer_dpc(PKDPC dpc, PVOID context, PVOID arg1, PVOID arg2)
{
    ADAPTER *a = context;
    UNREFERENCED_PARAMETER(dpc);
    UNREFERENCED_PARAMETER(arg1);
    UNREFERENCED_PARAMETER(arg2);
    if (a->ticking) {
        a->tick(a + 1);
        if (a->ticking) arm_timer(a);
    }
}

VOID NTAPI WxpStartTimer(PVOID ext)
{
    ADAPTER *a = adapter(ext);
    KIRQL old;
    if (!a->tick) return;
    old = KfRaiseIrql(DISPATCH_LEVEL);
    if (!a->ticking) {
        a->ticking = TRUE;
        if (a->registered) arm_timer(a);
    }
    KfLowerIrql(old);
}

VOID NTAPI WxpStopTimer(PVOID ext)
{
    ADAPTER *a = adapter(ext);
    KIRQL old = KfRaiseIrql(DISPATCH_LEVEL);
    a->ticking = FALSE;
    KeCancelTimer(&a->timer);
    KeRemoveQueueDpc(&a->timer_dpc);
    KfLowerIrql(old);
}

static PVOID NTAPI resolve_proc(PVOID ext, PUCHAR name)
{
    extern const struct { const char *name; PVOID function; } wxp_video_entries[];
    ULONG i;
    UNREFERENCED_PARAMETER(ext);
    if (!name) return NULL;
    for (i = 0; wxp_video_entries[i].name; ++i) {
        const char *s = wxp_video_entries[i].name;
        const UCHAR *t = name;
        while (*s && (UCHAR)*s == *t) { ++s; ++t; }
        if (!*s && !*t) return wxp_video_entries[i].function;
    }
    return NULL;
}

static VP_STATUS NTAPI find_adapter(PVOID hw, PVOID ctx, PWSTR args,
                                    PVIDEO_PORT_CONFIG_INFO native, PUCHAR again)
{
    ADAPTER *a = hw;
    REGISTRATION *r = ctx;
    VIDEO_PORT_CONFIG_INFO cfg = {0};
    ULONG bytes = min(native->Length, sizeof(cfg));
    PWSTR path;
    UCHAR repeat = FALSE, irq[2];
    VP_STATUS status;
    *again = FALSE;
    if (r->tried) return ERROR_DEV_NOT_EXIST;
    r->tried = TRUE;
    a->pci = r->pci;
    a->initialize = r->xp->HwInitialize;
    a->start_io = r->xp->HwStartIO;
    a->interrupt = r->xp->HwInterrupt;
    a->tick = r->xp->HwTimer;
    wxp_children_init(&a->children, a + 1, r->xp);
    KeInitializeDpc(&a->dpc, dispatch_dpc, a);
    KeInitializeDpc(&a->timer_dpc, timer_dpc, a);
    KeInitializeTimer(&a->timer);
    path = (PWSTR)((PUCHAR)(a + 1) + ((r->xp->HwDeviceExtensionSize + 1) & ~1UL));
    RtlCopyMemory(path, r->key->Buffer, r->key->Length);
    path[r->key->Length / sizeof(WCHAR)] = 0;
    RtlCopyMemory(&cfg, native, bytes);
    cfg.Length = sizeof(cfg);
    cfg.SystemIoBusNumber = a->pci.bus;
    cfg.AdapterInterfaceType = PCIBus;
    cfg.VideoPortGetProcAddress = resolve_proc;
    cfg.DriverRegistryPath = path;
    if (a->interrupt && WxpVideoBusRead(a + 1, PCIConfiguration, 0, irq, 0x3c, 2) == 2 &&
        irq[1] && irq[0] && irq[0] < 16) {
        cfg.BusInterruptLevel = cfg.BusInterruptVector = irq[0];
        cfg.InterruptMode = LevelSensitive;
        cfg.InterruptShareable = TRUE;
    }
    a->finding = TRUE;
    status = r->xp->HwFindAdapter(a + 1, r->context, args, &cfg, &repeat);
    a->finding = FALSE;
    if (status != NO_ERROR) {
        KeRemoveQueueDpc(&a->dpc);
        WxpStopTimer(a + 1);
        VideoPortVerifyAccessRanges(a, 0, NULL);
        return status;
    }
    cfg.Length = native->Length;
    cfg.SystemIoBusNumber = native->SystemIoBusNumber;
    cfg.AdapterInterfaceType = native->AdapterInterfaceType;
    RtlCopyMemory(native, &cfg, bytes);
    r->adapter = a;
    return NO_ERROR;
}

static BOOLEAN NTAPI initialize_adapter(PVOID hw)
{
    ADAPTER *a = hw;
    return a->initialize(a + 1);
}

static BOOLEAN NTAPI start_io(PVOID hw, PVIDEO_REQUEST_PACKET request)
{
    ADAPTER *a = hw;
    return a->start_io(a + 1, request);
}

static BOOLEAN NTAPI interrupt(PVOID hw)
{
    ADAPTER *a = hw;
    return a->interrupt(a + 1);
}

static ULONG selected_id(PUNICODE_STRING path)
{
    OBJECT_ATTRIBUTES attrs;
    UNICODE_STRING value_name;
    HANDLE key;
    ULONG length, id = 0x10de;
    union { KEY_VALUE_PARTIAL_INFORMATION info; ULONG aligned[4]; } value;
    InitializeObjectAttributes(&attrs, path, OBJ_CASE_INSENSITIVE, NULL, NULL);
    if (!NT_SUCCESS(ZwOpenKey(&key, KEY_QUERY_VALUE, &attrs))) return id;
    RtlInitUnicodeString(&value_name, L"WinxpdmPciId");
    if (NT_SUCCESS(ZwQueryValueKey(key, &value_name, KeyValuePartialInformation,
                                   &value, sizeof(value), &length)) &&
        value.info.Type == REG_DWORD && value.info.DataLength == sizeof(id))
        RtlCopyMemory(&id, value.info.Data, sizeof(id));
    ZwClose(key);
    return id;
}

ULONG NTAPI WxpVideoInitialize(PVOID driver, PUNICODE_STRING key,
                               PVIDEO_HW_INITIALIZATION_DATA xp, PVOID context)
{
    VIDEO_HW_INITIALIZATION_DATA native = {0};
    REGISTRATION r = {0};
    ULONG overhead, status;
    if (!xp || !key || !key->Buffer ||
        (xp->HwInitDataSize != SIZE_OF_W2K_VIDEO_HW_INITIALIZATION_DATA &&
         xp->HwInitDataSize != SIZE_OF_WXP_VIDEO_HW_INITIALIZATION_DATA))
        return STATUS_REVISION_MISMATCH;
    if (xp->AdapterInterfaceType != PCIBus) return STATUS_DEVICE_DOES_NOT_EXIST;
    if (!xp->HwFindAdapter || !xp->HwInitialize || !xp->HwStartIO) return STATUS_INVALID_PARAMETER;
    overhead = sizeof(ADAPTER) + key->Length + 3;
    if (xp->HwDeviceExtensionSize > MAXULONG - overhead) return STATUS_INVALID_PARAMETER;
    if (!wxp_find_display(selected_id(key), &r.pci)) return STATUS_DEVICE_DOES_NOT_EXIST;
    r.xp = xp;
    r.key = key;
    r.context = context;
    native.HwInitDataSize = FIELD_OFFSET(VIDEO_HW_INITIALIZATION_DATA, HwResetHw);
    native.AdapterInterfaceType = Isa; /* NT 3.1 has no PCI bus enumerator. */
    native.HwDeviceExtensionSize = xp->HwDeviceExtensionSize + overhead;
    native.StartingDeviceNumber = xp->StartingDeviceNumber;
    native.HwFindAdapter = find_adapter;
    native.HwInitialize = initialize_adapter;
    native.HwStartIO = start_io;
    native.HwInterrupt = xp->HwInterrupt ? interrupt : NULL;
    status = VideoPortInitialize(driver, key, &native, &r);
    if (NT_SUCCESS((NTSTATUS)status) && r.adapter) {
        KIRQL old = KfRaiseIrql(DISPATCH_LEVEL);
        r.adapter->registered = TRUE;
        if (r.adapter->ticking) arm_timer(r.adapter);
        KfLowerIrql(old);
        wxp_children_request(&r.adapter->children, TRUE);
    }
    return status;
}

VP_STATUS NTAPI WxpVideoChildren(PVOID ext, PVOID reserved)
{
    if (!ext || reserved) return ERROR_INVALID_PARAMETER;
    wxp_children_request(&adapter(ext)->children, FALSE);
    return NO_ERROR;
}

PVOID NTAPI WxpDeviceBase(PVOID ext, PHYSICAL_ADDRESS address, ULONG size, UCHAR io)
{
    return VideoPortGetDeviceBase(adapter(ext), address, size, io);
}

VOID NTAPI WxpFreeBase(PVOID ext, PVOID p)
{
    VideoPortFreeDeviceBase(adapter(ext), p);
}

VP_STATUS NTAPI WxpInt10(PVOID ext, PVIDEO_X86_BIOS_ARGUMENTS regs)
{
    return VideoPortInt10(adapter(ext), regs);
}

VP_STATUS NTAPI WxpMap(PVOID ext, PHYSICAL_ADDRESS addr, PULONG len, PULONG io, PVOID *p)
{
    return VideoPortMapMemory(adapter(ext), addr, len, io, p);
}

VP_STATUS NTAPI WxpUnmap(PVOID ext, PVOID p, HANDLE process)
{
    return VideoPortUnmapMemory(adapter(ext), p, process);
}

VP_STATUS NTAPI WxpTrappedPorts(PVOID ext, ULONG n, PVIDEO_ACCESS_RANGE ranges)
{
    return VideoPortSetTrappedEmulatorPorts(adapter(ext), n, ranges);
}

VP_STATUS NTAPI WxpSetRegistry(PVOID ext, PWSTR name, PVOID buf, ULONG length)
{
    return VideoPortSetRegistryParameters(adapter(ext), name, buf, length);
}

/* Both installed video ports propagate the query callback's VP_STATUS. */
typedef VP_STATUS (NTAPI *REG_CALLBACK)(PVOID, PVOID, PWSTR, PVOID, ULONG);
typedef struct { REG_CALLBACK fn; PVOID context; } REG_QUERY;
extern VP_STATUS NTAPI native_get_registry(PVOID, PWSTR, UCHAR, REG_CALLBACK, PVOID)
    __asm__("_VideoPortGetRegistryParameters@20");

static VP_STATUS NTAPI registry_callback(PVOID hw, PVOID ctx, PWSTR name, PVOID data, ULONG length)
{
    REG_QUERY *query = ctx;
    return query->fn((ADAPTER *)hw + 1, query->context, name, data, length);
}

VP_STATUS NTAPI WxpGetRegistry(PVOID ext, PWSTR name, UCHAR file, REG_CALLBACK fn, PVOID ctx)
{
    REG_QUERY query = {fn, ctx};
    return native_get_registry(adapter(ext), name, file, registry_callback, &query);
}
