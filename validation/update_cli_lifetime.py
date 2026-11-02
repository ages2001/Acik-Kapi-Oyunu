from pathlib import Path
p=Path('/workspace/rltklant-work/USBNT/src/ntcli.c');s=p.read_text().replace('#include "ntdrv.h"', '#ifdef USBNT_CLI_HOST_TEST\n#include "ntcli_test_shim.h"\n#else\n#include "ntdrv.h"').replace('#include "usbntcli.h"', '#include "usbntcli.h"\n#endif',1)
s=s.replace('static PDEVICE_OBJECT DevDevice;', '#ifndef USBNT_CLI_HOST_TEST\nstatic PDEVICE_OBJECT DevDevice;\n#endif')
s=s.replace('static UCHAR     g_CliUsed[UC_MAX_CLIENTS];', '''static UCHAR     g_CliUsed[UC_MAX_CLIENTS]; /* 0 free, 1 live, 2 unregistering */
static ULONG     g_Callbacks[UC_MAX_CLIENTS];
/* IDs are opaque attachment tokens, never a reusable array index. */
static ULONG     g_Ids[USB_MAX_DEVICES];
static ULONG     g_NextId;
static UCHAR     g_Active[USB_MAX_DEVICES];''')
a=s.index('static USB_DEV *DevFromId');b=s.index('static void FillInfo',a)
s=s[:a]+'''/* One synchronous request at a time per device. This also protects the
 * pipe cache and the shared endpoint-zero setup buffer from client races.
 * Never hold the spin lock while waiting for a transfer or a callback. */
static int AcquireDevice(ULONG Id, USB_DEV **Device)
{
    ULONG i;
    OS_IRQL irql = OsLock();
    int result = UC_ERR_NODEV;
    *Device = NULL;
    for (i = 0; Id && i < USB_MAX_DEVICES; i++) {
        USB_DEV *d = &UsbDevs[i];
        if (g_Ids[i] != Id || !d->InUse || d->Gone || !d->Ready) continue;
        if (g_Active[i]) { result = UC_ERR_BUSY; break; }
        g_Active[i] = 1;
        *Device = d;
        result = UC_OK;
        break;
    }
    OsUnlock(irql);
    return result;
}
static void ReleaseDevice(USB_DEV *Device)
{
    OS_IRQL irql = OsLock();
    g_Active[Device->Index] = 0;
    OsUnlock(irql);
}
static void NotifyClient(int Slot, const UC_DEVINFO *Info, ULONG RemovedId)
{
    UC_CLIENT client;
    OS_IRQL irql = OsLock();
    if (g_CliUsed[Slot] != 1) { OsUnlock(irql); return; }
    client = g_Cli[Slot];
    g_Callbacks[Slot]++;
    OsUnlock(irql);
    if (Info) {
        if (client.Arrival) client.Arrival(client.Context, Info);
    } else if (client.Removal) client.Removal(client.Context, RemovedId);
    irql = OsLock();
    g_Callbacks[Slot]--;
    OsUnlock(irql);
}

'''+s[b:]
s=s.replace('o->Id        = (ULONG)d->Index + 1;', 'o->Id        = g_Ids[d->Index];')
s=s.replace('            g_Cli[slot].Arrival(g_Cli[slot].Context, &info);', '            if (info.Id) NotifyClient(slot, &info, 0);')
a=s.index('static void UC_CALL CliUnregister');b=s.index('static const USBNT_DEV_IF DevIf',a)
s=s[:a]+'''/* Unregister is a rundown barrier. It must not be called by a callback
 * of the same client: that callback is itself part of the rundown set. */
static void UC_CALL CliUnregister(const UC_CLIENT *Client)
{
    OS_IRQL irql;
    int i, slot = -1;
    if (!Client) return;
    irql = OsLock();
    for (i = 0; i < UC_MAX_CLIENTS; i++)
        if (g_CliUsed[i] == 1 && g_Cli[i].Context == Client->Context &&
            g_Cli[i].Arrival == Client->Arrival) {
            g_CliUsed[i] = 2;
            slot = i;
            break;
        }
    OsUnlock(irql);
    if (slot < 0) return;
    for (;;) {
        irql = OsLock();
        if (!g_Callbacks[slot]) {
            UsbMemSet(&g_Cli[slot], 0, sizeof(g_Cli[slot]));
            g_CliUsed[slot] = 0;
            OsUnlock(irql);
            return;
        }
        OsUnlock(irql);
        OsSleepMs(1);
    }
}

static int UC_CALL CliGetInfo(ULONG Id, UC_DEVINFO *Info)
{
    USB_DEV *d;
    int r;
    if (!Info) return UC_ERR_PARAM;
    r = AcquireDevice(Id, &d);
    if (r != UC_OK) return r;
    FillInfo(d, Info);
    ReleaseDevice(d);
    return UC_OK;
}

static int UC_CALL CliGetDescriptor(ULONG Id, UCHAR Type, UCHAR Index,
                                    void *Buf, ULONG Len, ULONG *Actual)
{
    USB_DEV *d;
    int r;
    if (Actual) *Actual = 0;
    if (!Buf || !Len || Len > 0xFFFF) return UC_ERR_PARAM;
    r = AcquireDevice(Id, &d);
    if (r != UC_OK) return r;
    r = UsbGetDescriptor(d, Type, Index, 0, Buf, (USHORT)Len, Actual);
    ReleaseDevice(d);
    return (r == USB_OK) ? UC_OK : UC_ERR_IO;
}

static int UC_CALL CliControl(ULONG Id, UCHAR ReqType, UCHAR Req, USHORT Value,
                              USHORT Index, void *Buf, USHORT Len,
                              ULONG TimeoutMs, ULONG *Actual)
{
    USB_DEV *d;
    int r;
    if (Actual) *Actual = 0;
    if (Len && !Buf) return UC_ERR_PARAM;
    r = AcquireDevice(Id, &d);
    if (r != UC_OK) return r;
    r = UsbControl(d, ReqType, Req, Value, Index, Len, Buf,
                   TimeoutMs ? TimeoutMs : 5000, Actual);
    ReleaseDevice(d);
    return (r == USB_OK) ? UC_OK : UC_ERR_IO;
}

static int UC_CALL CliTransfer(ULONG Id, UCHAR Endpoint, void *Buf, ULONG Len,
                               ULONG TimeoutMs, ULONG *Actual)
{
    USB_DEV *d;
    USB_PIPE *p;
    void *dma;
    ULONG phys, got = 0;
    int r;
    if (Actual) *Actual = 0;
    if ((Len && !Buf) || !(Endpoint & 15) || (Endpoint & 0x70)) return UC_ERR_PARAM;
    r = AcquireDevice(Id, &d);
    if (r != UC_OK) return r;
    p = GetPipe(d, Endpoint);
    if (!p) { ReleaseDevice(d); return UC_ERR_IO; }
    dma = OsDmaAlloc(Len ? Len : 1, &phys);
    if (!dma) { ReleaseDevice(d); return UC_ERR_NOMEM; }
    if (!(Endpoint & 0x80) && Len) UsbMemCpy(dma, Buf, Len);
    r = UsbTransfer(p, dma, phys, Len, TimeoutMs ? TimeoutMs : 5000, &got);
    if (got > Len) { got = 0; r = UC_ERR_IO; }
    if ((Endpoint & 0x80) && got) UsbMemCpy(Buf, dma, got);
    OsDmaFree(dma, Len ? Len : 1);
    if (Actual) *Actual = got;
    ReleaseDevice(d);
    return (r == USB_OK) ? UC_OK : UC_ERR_IO;
}

/* Version 1 has no client identity parameter, so this is advisory only. */
static int UC_CALL CliClaim(ULONG Id, int Claim)
{
    USB_DEV *d;
    int r = AcquireDevice(Id, &d);
    if (r != UC_OK) return r;
    g_Claimed[d->Index] = (UCHAR)(Claim ? 1 : 0);
    ReleaseDevice(d);
    return UC_OK;
}

'''+s[b:]
a=s.index('void CliArrival(');b=s.index('NTSTATUS NtCliCreate',a)
s=s[:a]+'''void CliArrival(USB_DEV *Dev)
{
    UC_DEVINFO info;
    OS_IRQL irql;
    int i;
    if (!Dev) return;
    irql = OsLock();
    /* Exhaustion fails closed rather than reusing a stale 32-bit handle. */
    if (g_NextId == 0xFFFFFFFFUL) { OsUnlock(irql); return; }
    g_Ids[Dev->Index] = ++g_NextId;
    FillInfo(Dev, &info);
    OsUnlock(irql);
    for (i = 0; i < UC_MAX_CLIENTS; i++) NotifyClient(i, &info, 0);
}

void CliDetach(USB_DEV *Dev)
{
    ULONG id;
    OS_IRQL irql;
    int i;
    if (!Dev) return;
    irql = OsLock();
    id = g_Ids[Dev->Index];
    g_Ids[Dev->Index] = 0; /* stop new requests before notifying clients */
    OsUnlock(irql);
    if (id) for (i = 0; i < UC_MAX_CLIENTS; i++) NotifyClient(i, NULL, id);
    /* Existing synchronous calls own the pipe and DMA until they return.
     * Do not race UsbClosePipe or DevRelease against their completion.
     * Current requests finish or time out; asynchronous cancellation is a
     * separate API requirement, not provided by this v1 rundown barrier. */
    for (;;) {
        UCHAR active;
        irql = OsLock();
        active = g_Active[Dev->Index];
        OsUnlock(irql);
        if (!active) break;
        OsSleepMs(1);
    }
    FreePipes(Dev->Index);
    g_Claimed[Dev->Index] = 0;
}

#ifndef USBNT_CLI_HOST_TEST
'''+s[b:]+ '\n#endif /* USBNT_CLI_HOST_TEST */\n'
p.write_text(s)
for p in (Path('/workspace/rltklant-work/USBNT/src/usbntcli.h'),Path('/workspace/rltklant-work/RLTKLANT/USBNT/src/usbntcli.h')):
 s=p.read_text().replace('#define UC_ERR_IO           (-8)','#define UC_ERR_IO           (-8)\n#define UC_ERR_BUSY         (-9)')
 s=s.replace('    /* claim (1) or release (0): a claimed device is skipped by GetInfo\n       enumeration for other clients; optional, drives nothing by itself */','    /* Version 1 claim/release is advisory; it does not identify an owner. */')
 s=s.replace('/* client callbacks, called in the USB worker context (PASSIVE_LEVEL) */','/* Callbacks run at PASSIVE_LEVEL (worker or Register replay). Keep them\n * nonblocking. Never call Unregister from this client\'s own callback.\n * Unregister waits for callbacks already in progress before returning. */')
 p.write_text(s)
