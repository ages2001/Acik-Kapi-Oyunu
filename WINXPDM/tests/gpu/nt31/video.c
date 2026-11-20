/* XP-style PCI miniport probe for the installed Windows NT 3.1 video port.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "../../../src/compat/nt31/video.h"
typedef ULONG board_word;
typedef PULONG board_regs;
#define board_read VideoPortReadRegisterUlong
#define board_write VideoPortWriteRegisterUlong
#define board_delay() VideoPortStallExecution(10)
#include "../windows/board_ops.h"
#include "../windows/board_aux.h"

typedef VOID (NTAPI *STOP_TIMER)(PVOID);
typedef struct {
    ULONG signature;
    KEVENT dpc_done, timer_done;
    ULONG dpcs, ticks;
    STOP_TIMER stop;
    PHYSICAL_ADDRESS aux_page;
    ULONG first_visits, second_visits;
    BOOLEAN nvidia, child_error;
} TEST_EXTENSION;
static TEST_EXTENSION *found;
static ULONG context_cookie;
static ULONG requested_id;

static void msg(const char *s)
{
    while (*s) __asm__ volatile("outb %0,$0xe9" : : "a"((UCHAR)*s++));
}

static void hex(ULONG value)
{
    int shift;
    for (shift = 28; shift >= 0; shift -= 4) {
        UCHAR c = "0123456789abcdef"[(value >> shift) & 15];
        __asm__ volatile("outb %0,$0xe9" : : "a"(c));
    }
}

static void field(const char *s, ULONG value)
{
    msg(s);
    hex(value);
}

static BOOLEAN config_space(PVOID ext, PULONG id, PULONG vga)
{
    ULONG n;
    UCHAR saved = 0, trial, readback = 0, part[5], header[8], boundary[2] = {0, 0xcd};
    if (VideoPortGetBusData(ext, PCIConfiguration, 0, header, 0, 8) != 8 ||
        VideoPortGetBusData(ext, PCIConfiguration, 0, part, 1, 5) != 5 ||
        VideoPortGetBusData(ext, PCIConfiguration, 0, boundary, 255, 2) != 1 ||
        boundary[1] != 0xcd || VideoPortGetBusData(ext, Cmos, 0, boundary, 0, 1))
        return FALSE;
    for (n = 0; n < 5; ++n) if (part[n] != header[n+1]) return FALSE;
    RtlCopyMemory(id, header, sizeof(*id));
    if (VideoPortGetBusData(ext, PCIConfiguration, 0, &saved, 0x3c, 1) != 1) return FALSE;
    trial = saved ^ 1;
    n = VideoPortSetBusData(ext, PCIConfiguration, 0, &trial, 0x3c, 1);
    n += VideoPortGetBusData(ext, PCIConfiguration, 0, &readback, 0x3c, 1);
    n += VideoPortSetBusData(ext, PCIConfiguration, 0, &saved, 0x3c, 1);
    if (n != 3 || readback != trial ||
        VideoPortGetBusData(ext, PCIConfiguration, 0, &readback, 0x3c, 1) != 1 ||
        readback != saved) return FALSE;
    return !VideoPortGetVgaStatus(ext, vga) && *vga <= 1;
}

static BOOLEAN resource_ranges(PVOID ext, PVIDEO_ACCESS_RANGE ranges, PULONG slot)
{
    UCHAR before[40], after[40];
    ULONG i;
    if (VideoPortGetBusData(ext, PCIConfiguration, 0, before, 4, sizeof(before)) != sizeof(before) ||
        VideoPortGetAccessRanges(ext, 0, NULL, 1, ranges, NULL, NULL, slot) != ERROR_MORE_DATA ||
        VideoPortGetAccessRanges(ext, 0, NULL, 6, ranges, NULL, NULL, slot) != NO_ERROR ||
        VideoPortGetBusData(ext, PCIConfiguration, 0, after, 4, sizeof(after)) != sizeof(after))
        return FALSE;
    for (i = 0; i < sizeof(before); ++i) if (before[i] != after[i]) return FALSE;
    return ranges[0].RangeLength && ranges[1].RangeLength && ranges[2].RangeLength &&
           ranges[3].RangeLength && !ranges[4].RangeLength && !ranges[5].RangeLength;
}

static BOOLEAN nv_window(PVOID ext, PHYSICAL_ADDRESS mmio, PHYSICAL_ADDRESS fb, PULONG regs)
{
    PULONG window, page, counter;

    BOOLEAN ok = FALSE;
    mmio.QuadPart += 0x700000;
    window = VideoPortGetDeviceBase(ext, mmio, 4096, 0);
    if (!window) return FALSE;
    fb.QuadPart += 0x100000;
    page = VideoPortGetDeviceBase(ext, fb, 4096, 0);
    if (!page) goto done_window;
    mmio.QuadPart -= 0xf0000; /* PDISPLAY register page at MMIO + 0x610000. */
    counter = VideoPortGetDeviceBase(ext, mmio, 4096, 0);
    if (!counter) goto done_page;
    ok = board_pramin(regs + 0x1700 / 4, window, page, counter + 0x18 / 4);
    VideoPortFreeDeviceBase(ext, counter);
done_page:
    VideoPortFreeDeviceBase(ext, page);
done_window:
    VideoPortFreeDeviceBase(ext, window);
    return ok;
}

static BOOLEAN amd_dma(PVOID ext, PHYSICAL_ADDRESS fb, PULONG regs)
{
    PULONG page, saved;
    BOOLEAN ok = FALSE;
    fb.QuadPart += 0x400000;
    page = VideoPortGetDeviceBase(ext, fb, 4096, 0);
    if (!page) return FALSE;
    saved = ExAllocatePool(NonPagedPool, 1024);
    if (saved) {
        ok = board_sdma(regs + 0x4b80 / 4, page, saved);
        ExFreePool(saved);
    }
    VideoPortFreeDeviceBase(ext, page);
    return ok;
}

static BOOLEAN mmio(PVOID ext, ULONG id, PVIDEO_ACCESS_RANGE r)
{
    static const struct { USHORT device, chip; } nvidia[] = {
        {0x1402,0x126},{0x1401,0x126},{0x13c2,0x124},{0x13c0,0x124},{0x17c8,0x120},
        {0x1c81,0x137},{0x1c82,0x137},{0x1b83,0x134},{0x1c03,0x136},{0x1b81,0x134},
        {0x1b82,0x134},{0x1b80,0x134},{0x1b06,0x132}
    };
    BOOLEAN nv = (USHORT)id == 0x10de, ok = FALSE;
    ULONG reg = nv ? 0 : 3, vram = nv ? 1 : 0, ports = nv ? 3 : 2;
    PULONG regs, aperture;
    ULONG i, identification, expect = 0, saved, value, restore, mirror;
    if (!nv && (USHORT)id != 0x1002) return FALSE;
    if (r[reg].RangeLength != (nv ? 0x1000000UL : 0x100000UL) ||
        r[vram].RangeLength != 0x10000000UL || !r[ports].RangeInIoSpace ||
        r[ports].RangeLength != (nv ? 128UL : 256UL)) return FALSE;
    regs = VideoPortGetDeviceBase(ext, r[reg].RangeStart, nv ? 0x89000 : 0x5000, 0);
    if (!regs) return FALSE;
    aperture = VideoPortGetDeviceBase(ext, r[vram].RangeStart, 4096, 0);
    if (!aperture) goto out;
    identification = VideoPortReadRegisterUlong(regs + (nv ? 0 : 0xde3));
    field(" identity=", identification);
    if (nv) {
        for (i = 0; i < RTL_NUMBER_OF(nvidia); ++i)
            if (nvidia[i].device == (id >> 16)) expect = nvidia[i].chip;
        mirror = VideoPortReadRegisterUlong(regs + 0x88000 / 4);
        if (!expect || (identification >> 20) != expect || (identification & 0xff) != 0xa1 ||
            mirror != id || !nv_window(ext, r[reg].RangeStart, r[vram].RangeStart, regs)) goto unmap;
    } else if (identification != r[vram].RangeLength >> 20 ||
               !amd_dma(ext, r[vram].RangeStart, regs)) goto unmap;
    saved = VideoPortReadRegisterUlong(aperture + 16);
    VideoPortWriteRegisterUlong(aperture + 16, saved ^ 0xa55a3cc3UL);
    value = VideoPortReadRegisterUlong(aperture + 16);
    if (!nv) {
        ULONG old_index = VideoPortReadRegisterUlong(regs);
        ULONG old_hi = VideoPortReadRegisterUlong(regs + 6);
        VideoPortWriteRegisterUlong(regs + 6, 0);
        VideoPortWriteRegisterUlong(regs, 0x80000040);
        mirror = VideoPortReadRegisterUlong(regs + 1);
        VideoPortWriteRegisterUlong(regs, old_index);
        VideoPortWriteRegisterUlong(regs + 6, old_hi);
        if (mirror != value) value = saved;
    }
    VideoPortWriteRegisterUlong(aperture + 16, saved);
    restore = VideoPortReadRegisterUlong(aperture + 16);
    ok = value == (saved ^ 0xa55a3cc3UL) && restore == saved;
unmap:
    VideoPortFreeDeviceBase(ext, aperture);
out:
    VideoPortFreeDeviceBase(ext, regs);
    return ok;
}

static VOID NTAPI check_dpc(PVOID ext, PVOID cookie)
{
    TEST_EXTENSION *t = ext;
    if (ext == found && cookie == &context_cookie && VideoPortGetCurrentIrql() == DISPATCH_LEVEL)
        ++t->dpcs;
    KeSetEvent(&t->dpc_done, IO_NO_INCREMENT, FALSE);
}

static VOID NTAPI tick(PVOID ext)
{
    TEST_EXTENSION *t = ext;
    if (ext == found && t->signature == 0x31585057 && VideoPortGetCurrentIrql() == DISPATCH_LEVEL)
        ++t->ticks;
    if (t->ticks >= 2) KeSetEvent(&t->timer_done, IO_NO_INCREMENT, FALSE);
}

typedef VP_STATUS (NTAPI *REG_CALLBACK)(PVOID, PVOID, PWSTR, PVOID, ULONG);
extern VP_STATUS NTAPI get_registry(PVOID, PWSTR, UCHAR, REG_CALLBACK, PVOID)
    __asm__("_VideoPortGetRegistryParameters@20");

static VP_STATUS NTAPI registry_value(PVOID ext, PVOID ctx, PWSTR name, PVOID buf, ULONG length)
{
    ULONG received;
    UNREFERENCED_PARAMETER(name);
    if (ctx != ext || length != sizeof(received)) return ERROR_INVALID_PARAMETER;
    RtlCopyMemory(&received, buf, sizeof(received));
    ((TEST_EXTENSION *)ext)->signature = received;
    return NO_ERROR;
}

static VP_STATUS NTAPI find(PVOID ext, PVOID ctx, PWSTR args, PVIDEO_PORT_CONFIG_INFO c,
                            PUCHAR again)
{
    TEST_EXTENSION *t = ext;
    VIDEO_ACCESS_RANGE ranges[6];
    ULONG id, vga, slot;
    UNREFERENCED_PARAMETER(args);
    *again = FALSE;
    if (ctx != &context_cookie || !c->DriverRegistryPath || *c->DriverRegistryPath != L'\\' ||
        c->Length != sizeof(*c) || c->AdapterInterfaceType != PCIBus ||
        !c->VideoPortGetProcAddress || !config_space(ext, &id, &vga))
        { msg("PCI config FAIL\n"); return ERROR_DEV_NOT_EXIST; }
    field("PCI id=", id); field(" bus=", c->SystemIoBusNumber); field(" VGA route=", vga);
    if (!resource_ranges(ext, ranges, &slot))
        { msg(" PCI resources FAIL\n"); return ERROR_DEV_NOT_EXIST; }
    field(" slot=", slot);
    t->nvidia = (USHORT)id == 0x10de;
    t->aux_page = ranges[t->nvidia ? 0 : 3].RangeStart;
    t->aux_page.QuadPart += t->nvidia ? 0xd000 : 0x12000;
    if (!mmio(ext, id, ranges)) { msg(" MMIO/VRAM FAIL\n"); return ERROR_DEV_NOT_EXIST; }
    msg(" PCI config, BAR restore, register map and VRAM PASS\n");
    if (t->nvidia) msg("PRAMIN/BAR1 alias and running display clock PASS\n");
    else msg("NBIO VRAM alias, SDMA7 wrapped fill/copy, 64-bit fence and pointer writeback PASS\n");
    {
        PULONG page = VideoPortGetDeviceBase(t, t->aux_page, 4096, 0);
        BOOLEAN ok;
        if (!page) return ERROR_DEV_NOT_EXIST;
        ok = board_two_sinks(page + (t->nvidia ? 0x930/4 : 0xdc8/4), t->nvidia);
        VideoPortFreeDeviceBase(t, page);
        if (!ok) { msg("AUX discovery FAIL\n"); return ERROR_DEV_NOT_EXIST; }
        msg("Native AUX: two DPCD caps, four EDID blocks, two timings and absent sink PASS\n");
    }
    t->stop = c->VideoPortGetProcAddress(ext, (PUCHAR)"VideoPortStopTimer");
    if (!t->stop || !c->VideoPortGetProcAddress(ext, (PUCHAR)"VideoPortGetCurrentIrql") ||
        !c->VideoPortGetProcAddress(ext, (PUCHAR)"VideoPortStallExecution") ||
        c->VideoPortGetProcAddress(ext, (PUCHAR)"MissingEntry"))
        return ERROR_INVALID_FUNCTION;
    {
        ULONG expected = 0x31585057;
        if (VideoPortSetRegistryParameters(ext, L"WinxpdmProbeCookie", &expected, sizeof(expected)) ||
            get_registry(ext, L"WinxpdmProbeCookie", FALSE, registry_value, ext) ||
            t->signature != expected) { msg("Registry callback FAIL\n"); return ERROR_INVALID_DATA; }
        msg("Native registry round trip and miniport callback extension PASS\n");
    }
    KeInitializeEvent(&t->dpc_done, NotificationEvent, FALSE);
    KeInitializeEvent(&t->timer_done, NotificationEvent, FALSE);
    found = ext;
    requested_id = id;
    return NO_ERROR;
}

static BOOLEAN NTAPI initialize(PVOID ext)
{
    return ext == found;
}

static VP_STATUS NTAPI child(PVOID ext, PVIDEO_CHILD_ENUM_INFO info, PVIDEO_CHILD_TYPE type,
                             PUCHAR descriptor, PULONG uid, PULONG unused)
{
    TEST_EXTENSION *t = ext;
    ULONG n = info->ChildIndex, *visits = info->ChildHwDeviceExtension;
    PULONG page, aux;
    UCHAR revision;
    VP_STATUS result = VIDEO_ENUM_INVALID_DEVICE;
    if (KeGetCurrentIrql() != PASSIVE_LEVEL || t != found || info->Size != sizeof(*info) ||
        info->ACPIHwId || info->ChildDescriptorSize < 256 || !visits) {
        t->child_error = TRUE;
        return ERROR_INVALID_PARAMETER;
    }
    *unused = 0;
    if (n > 3) return VIDEO_ENUM_NO_MORE_DEVICES;
    if (n == 2) return VIDEO_ENUM_INVALID_DEVICE;
    page = VideoPortGetDeviceBase(t, t->aux_page, 4096, 0);
    if (!page) return ERROR_INVALID_FUNCTION;
    aux = page + (t->nvidia ? 0x930/4 : 0xdc8/4) + (n == 3 ? (t->nvidia ? 0x50/4 : 0x70/4) : 0);
    if (board_aux_transfer(aux, t->nvidia, 9, 0, &revision, 1)) {
        if (!board_aux_edid(aux, t->nvidia, descriptor, 0) || !board_aux_edid(aux, t->nvidia, descriptor + 128, 1)) {
            t->child_error = TRUE;
            result = ERROR_INVALID_DATA;
        } else {
            *uid = 0x100 + descriptor[12];
            *type = Monitor;
            if (n == 1) t->first_visits = ++*visits;
            else t->second_visits = ++*visits;
            result = VIDEO_ENUM_MORE_DEVICES;
        }
    }
    VideoPortFreeDeviceBase(t, page);
    return result;
}

static VP_STATUS NTAPI check_snapshot(PVOID ext, PVOID ctx, PWSTR name, PVOID bytes, ULONG size)
{
    WXP_CHILD_LIST *list = bytes;
    ULONG n = *(ULONG *)ctx, i;
    UNREFERENCED_PARAMETER(ext);
    UNREFERENCED_PARAMETER(name);
    if (size != FIELD_OFFSET(WXP_CHILD_LIST, child) + n * sizeof(WXP_CHILD_RECORD) ||
        list->version != 1 || list->count != n) return ERROR_INVALID_DATA;
    for (i = 0; i < n; ++i) {
        WXP_CHILD_RECORD *r = &list->child[i];
        ULONG k, base = 0, extra = 0;
        if (r->index != 1 + 2*i || r->type != Monitor || r->uid != 0x101 + i ||
            r->descriptor[12] != i + 1 || r->descriptor[128] != 2) return ERROR_INVALID_DATA;
        for (k = 0; k < 128; ++k) { base += r->descriptor[k]; extra += r->descriptor[k+128]; }
        if ((base | extra) & 255) return ERROR_INVALID_DATA;
    }
    return NO_ERROR;
}

static BOOLEAN snapshot(TEST_EXTENSION *t, ULONG count)
{
    ULONG i;
    LARGE_INTEGER delay;
    delay.QuadPart = -100000;
    for (i = 0; i < 500 && !t->child_error; ++i) {
        if (get_registry(t, L"WinxpdmChildren", FALSE, check_snapshot, &count) == NO_ERROR) return TRUE;
        KeDelayExecutionThread(KernelMode, FALSE, &delay);
    }
    return FALSE;
}

static BOOLEAN changed(TEST_EXTENSION *t, BOOLEAN connected)
{
    LARGE_INTEGER delay;
    KIRQL old;
    ULONG i;
    PULONG page = VideoPortGetDeviceBase(t, t->aux_page, 4096, 0);
    VP_STATUS first, second;
    UCHAR version;
    if (!page) return FALSE;
    delay.QuadPart = -100000;
    for (i = 0; i < 1200; ++i) {
        PULONG aux = page + (t->nvidia ? (0x930+0x50)/4 : (0xdc8+0x70)/4);
        if (board_aux_transfer(aux, t->nvidia, 9, 0, &version, 1) == connected) break;
        KeDelayExecutionThread(KernelMode, FALSE, &delay);
    }
    VideoPortFreeDeviceBase(t, page);
    if (i == 1200) return FALSE;
    old = KfRaiseIrql(DISPATCH_LEVEL);
    first = VideoPortEnumerateChildren(t, NULL);
    second = VideoPortEnumerateChildren(t, NULL);
    KfLowerIrql(old);
    return !first && !second && snapshot(t, connected ? 2 : 1);
}

static BOOLEAN children(TEST_EXTENSION *t)
{
    if (!snapshot(t, 2)) return FALSE;
    if (t->first_visits != 1 || t->second_visits != 1) return FALSE;
    msg("AUX waiting for sink 2 disconnect\n");
    if (!changed(t, FALSE) || t->first_visits != 2) return FALSE;
    msg("AUX waiting for sink 2 reconnect\n");
    if (!changed(t, TRUE) || t->first_visits != 3 || t->second_visits != 1) return FALSE;
    msg("Native child enumeration: two monitors, unplug, reconnect, stable extensions and EDID PASS\n");
    return TRUE;
}

static BOOLEAN NTAPI start(PVOID ext, PVIDEO_REQUEST_PACKET packet)
{
    UNREFERENCED_PARAMETER(ext);
    packet->StatusBlock->Status = ERROR_INVALID_FUNCTION;
    return TRUE;
}

static BOOLEAN asynchronous(TEST_EXTENSION *t)
{
    LARGE_INTEGER wait;
    KIRQL old;
    ULONG stopped;
    BOOLEAN first, duplicate, timer_ok;
    wait.QuadPart = -50000000;
    old = KfRaiseIrql(DISPATCH_LEVEL);
    first = VideoPortQueueDpc(t, check_dpc, &context_cookie);
    duplicate = VideoPortQueueDpc(t, check_dpc, t);
    KfLowerIrql(old);
    if (!first || duplicate || KeWaitForSingleObject(&t->dpc_done, Executive, KernelMode,
                                                      FALSE, &wait) != STATUS_SUCCESS ||
        t->dpcs != 1) return FALSE;
    msg("Deferred callback, context, DISPATCH_LEVEL and duplicate rejection PASS\n");
    VideoPortStartTimer(t);
    VideoPortStartTimer(t);
    timer_ok = KeWaitForSingleObject(&t->timer_done, Executive, KernelMode, FALSE, &wait) == STATUS_SUCCESS;
    t->stop(t);
    stopped = t->ticks;
    wait.QuadPart = -12500000;
    KeDelayExecutionThread(KernelMode, FALSE, &wait);
    if (!timer_ok || stopped != t->ticks || stopped < 2) return FALSE;
    KeResetEvent(&t->timer_done);
    VideoPortStartTimer(t);
    wait.QuadPart = -40000000;
    timer_ok = KeWaitForSingleObject(&t->timer_done, Executive, KernelMode, FALSE, &wait) == STATUS_SUCCESS;
    t->stop(t);
    return timer_ok && t->ticks == stopped + 1;
}

/* Write only the probe's private service key. The miniport's enumerator must
   independently find this exact function despite the extra QEMU VGA adapter. */
static BOOLEAN select_board(PUNICODE_STRING key, ULONG id)
{
    OBJECT_ATTRIBUTES attrs;
    UNICODE_STRING name;
    HANDLE handle;
    NTSTATUS status;
    InitializeObjectAttributes(&attrs, key, OBJ_CASE_INSENSITIVE, NULL, NULL);
    if (!NT_SUCCESS(ZwOpenKey(&handle, KEY_SET_VALUE, &attrs))) return FALSE;
    RtlInitUnicodeString(&name, L"WinxpdmPciId");
    status = ZwSetValueKey(handle, &name, 0, REG_DWORD, &id, sizeof(id));
    ZwClose(handle);
    return NT_SUCCESS(status);
}

NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING key)
{
    VIDEO_HW_INITIALIZATION_DATA d = {0};
    NTSTATUS status;
    ULONG id = 0, bus, slot = 6;
    msg("NT 3.1 XP PCI video-port probe loaded\n");
    d.HwInitDataSize = SIZE_OF_WXP_VIDEO_HW_INITIALIZATION_DATA;
    d.AdapterInterfaceType = PCIBus;
    d.HwDeviceExtensionSize = sizeof(TEST_EXTENSION);
    d.HwFindAdapter = find;
    d.HwInitialize = initialize;
    d.HwStartIO = start;
    d.HwTimer = tick;
    d.HwGetVideoChildDescriptor = child;
    d.HwChildDeviceExtensionSize = sizeof(ULONG);
    if (!select_board(key, 0xfffefffeUL) ||
        VideoPortInitialize(driver, key, &d, &context_cookie) != (ULONG)STATUS_DEVICE_DOES_NOT_EXIST) {
        msg("Absent board selection FAIL\n");
        return STATUS_UNSUCCESSFUL;
    }
    for (bus = 0; bus < 2; ++bus, slot = 0)
        if (HalGetBusDataByOffset(PCIConfiguration, bus, slot, &id, 0, sizeof(id)) == sizeof(id) &&
            ((USHORT)id == 0x10de || (USHORT)id == 0x1002)) break;
    if (bus == 2 || !select_board(key, id)) return STATUS_NO_SUCH_DEVICE;
    status = VideoPortInitialize(driver, key, &d, &context_cookie);
    if (!NT_SUCCESS(status) || !found) {
        field("VideoPortInitialize failed ", status); msg("\n");
        return NT_SUCCESS(status) ? STATUS_NO_SUCH_DEVICE : status;
    }
    /* Once registered, the native video port owns the callbacks and extension. */
    if (!children(found)) msg("Child enumeration FAIL\n");
    else if (!asynchronous(found)) msg("DPC/timer FAIL\n");
    else { field("Periodic timer, cancellation and restart PASS id=", requested_id); msg("\n"); }
    return STATUS_SUCCESS;
}
