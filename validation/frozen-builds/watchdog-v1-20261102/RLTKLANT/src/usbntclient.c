/* USBNT client lifecycle. Callbacks only publish work; USB control requests
 * run on a dedicated system thread. This is the transport bring-up layer;
 * NDIS USB adapter presentation is a separate integration requirement. */
#include "rltklant.h"
#include "backend_usb.h"
#include "usbnic.h"
#include "usbcli.h"
#include "usbntcli.h"

#ifdef RTL_DBG
static VOID logtext(const char *s) { while (*s) RtlOutByte(0xe9, (UCHAR)*s++); }
static VOID loghex(ULONG value) { int i; for (i=28;i>=0;i-=4) { UCHAR n=(UCHAR)((value>>i)&15); RtlOutByte(0xe9,(UCHAR)(n<10?'0'+n:'A'+n-10)); } }
#else
#define logtext(s)
#define loghex(v)
#endif

/* Public NT DRIVER_OBJECT prefix, identical on NT 3.x through XP. Native
 * pointer alignment gives DriverUnload offsets 0x34 (x86), 0x68 (x64). */
typedef VOID (STDC *RLT_UNLOAD)(PVOID);
typedef struct {
    SHORT Type, Size;
    PVOID DeviceObject;
    ULONG Flags;
    PVOID DriverStart;
    ULONG DriverSize;
    PVOID DriverSection, DriverExtension;
    UNICODE_STRING DriverName;
    PVOID HardwareDatabase, FastIoDispatch, DriverInit, DriverStartIo;
    RLT_UNLOAD DriverUnload;
} RLT_DRIVER_PREFIX;
typedef char RltUnloadOffsetChecked[
    __builtin_offsetof(RLT_DRIVER_PREFIX, DriverUnload) == (sizeof(PVOID)==8 ? 0x68 : 0x34) ? 1 : -1];
typedef union { ULONG_PTR Alignment; UCHAR Storage[64]; } RLT_EVENT;
VOID STDC KeInitializeEvent(PVOID, int, BOOLEAN);
LONG STDC KeSetEvent(PVOID, LONG, BOOLEAN);
#if defined(RTL_NT31)
/* NT3.1 exports ResetEvent and NtWaitForSingleObject, not the later names. */
LONG STDC KeResetEvent(PVOID);
#define KeClearEvent(p) ((void)KeResetEvent(p))
#define ZwWaitForSingleObject NtWaitForSingleObject
#else
VOID STDC KeClearEvent(PVOID);
#endif
NTSTATUS STDC KeWaitForSingleObject(PVOID, int, int, BOOLEAN, PLARGE_INTEGER);
NTSTATUS STDC ObReferenceObjectByHandle(PVOID, ULONG, PVOID, int, PVOID *, PVOID);
NTSTATUS STDC PsCreateSystemThread(PVOID *, ULONG, PVOID, PVOID, PVOID,
                                   VOID (STDC *)(PVOID), PVOID);
VOID STDC PsTerminateSystemThread(NTSTATUS);
NTSTATUS STDC ZwWaitForSingleObject(PVOID, BOOLEAN, PLARGE_INTEGER);
NTSTATUS STDC ZwClose(PVOID);
NTSTATUS STDC KeDelayExecutionThread(int,BOOLEAN,PLARGE_INTEGER);
static VOID WaitWake(PVOID event)
{
    LARGE_INTEGER timeout;
    timeout.QuadPart = (ULONGLONG)-100000LL; /* ten milliseconds */
    KeWaitForSingleObject(event, 0, 0, FALSE, &timeout);
}

#define USB_CLIENT_SLOTS 64
static struct {
    UC_DEVINFO Info;
    BOOLEAN Present, Dirty;
    RTL_USBNT_TARGET Target;
    USBNIC Nic;                    /* only the system thread accesses it */
    BOOLEAN Carrier, CarrierKnown; /* diagnostic state; not NDIS indication */
    NDIS_STATUS LinkStatus;
} g_Devices[USB_CLIENT_SLOTS];
static NDIS_SPIN_LOCK g_Lock;
static RLT_EVENT g_Wake;
static BOOLEAN g_Stop, g_Registered;
static PVOID g_Thread;
static PFILE_OBJECT g_File;
static const USBNT_DEV_IF *g_Interface;
static RLT_UNLOAD g_NdisUnload;
static UC_CLIENT g_Client;

static int HardwareCancelled(void*context)
{
    RTL_USBNT_TARGET*t=context;UC_DEVINFO info;BOOLEAN stop;
    NdisAcquireSpinLock(&g_Lock);stop=g_Stop;NdisReleaseSpinLock(&g_Lock);
    return stop||!t||!t->If||t->If->GetInfo(t->Id,&info)!=UC_OK;
}
static int HardwareWait(void*context,unsigned ms)
{
    LARGE_INTEGER delay;
    if(HardwareCancelled(context))return 0;
    delay.QuadPart=-(long long)ms*10000;
    return KeDelayExecutionThread(0,FALSE,&delay)==0&&!HardwareCancelled(context);
}

static void UC_CALL ClientArrival(void *ctx, const UC_DEVINFO *info)
{
    int i, slot = -1;
    (void)ctx;
    if (!info || !RtlUsbMatch(info->Vid, info->Pid)) return;
    NdisAcquireSpinLock(&g_Lock);
    if (!g_Stop) {
        for (i=0;i<USB_CLIENT_SLOTS;i++)
            if (g_Devices[i].Present && g_Devices[i].Info.Id == info->Id) { slot=i; break; }
        if (slot < 0) for (i=0;i<USB_CLIENT_SLOTS;i++)
            if (!g_Devices[i].Present) { slot=i; break; }
        if (slot >= 0) {
            g_Devices[slot].Info = *info;
            g_Devices[slot].Present = TRUE;
            g_Devices[slot].Dirty = TRUE;
        }
    }
    NdisReleaseSpinLock(&g_Lock);
    if (slot >= 0) KeSetEvent(&g_Wake, 0, FALSE);
}

static void UC_CALL ClientRemoval(void *ctx, ULONG id)
{
    int i;
    (void)ctx;
    NdisAcquireSpinLock(&g_Lock);
    for (i=0;i<USB_CLIENT_SLOTS;i++) {
        if (g_Devices[i].Present && g_Devices[i].Info.Id == id) {
            g_Devices[i].Present = FALSE;
            g_Devices[i].Dirty = TRUE;
        }
    }
    NdisReleaseSpinLock(&g_Lock);
    KeSetEvent(&g_Wake, 0, FALSE);
}

static VOID STDC ClientThread(PVOID context)
{
    int i;
    (void)context;
    for (;;) {
        BOOLEAN stopping;
        WaitWake(&g_Wake);
        KeClearEvent(&g_Wake);
        NdisAcquireSpinLock(&g_Lock);
        stopping = g_Stop;
        NdisReleaseSpinLock(&g_Lock);
        for (i=0;i<USB_CLIENT_SLOTS;i++) {
            UC_DEVINFO info;
            BOOLEAN present, dirty;
            USBNT_BIND bind;
            USBNT_OSVER os;
            NDIS_STATUS status;
            USBNIC_PLATFORM platform;
            NdisAcquireSpinLock(&g_Lock);
            present = g_Devices[i].Present && !stopping;
            dirty = g_Devices[i].Dirty || stopping;
            g_Devices[i].Dirty = FALSE;
            info = g_Devices[i].Info;
            NdisReleaseSpinLock(&g_Lock);
            if (dirty && (g_Devices[i].Nic.Nic.Running||g_Devices[i].Nic.HardwareReady)) {
                RtlUsbDisconnected(g_Devices[i].Nic.Nic.Mac);
                UsbNicDetach(&g_Devices[i].Nic);
                logtext("UNT-DOWN id=");loghex(g_Devices[i].Target.Id);logtext("\n");
            }
            if (!present) continue;
            if (dirty) {
                g_Devices[i].CarrierKnown = FALSE;
                /* Revalidate after deferred notification, before any register I/O. */
                if (g_Interface->GetInfo(info.Id, &info) != UC_OK) continue;
                RtlUsbNtBind(&bind, &g_Devices[i].Target, g_Interface, info.Id);
                os.Major = 4; os.Minor = 0; /* this path explicitly uses USBNT */
                platform.BusSpeed=info.Speed==UC_SPEED_FULL?2:info.Speed==UC_SPEED_HIGH?3:info.Speed==UC_SPEED_SUPER?5:0;
                platform.WaitMs=HardwareWait;platform.Cancelled=HardwareCancelled;platform.Context=&g_Devices[i].Target;
                status = UsbNicAttachEx(&g_Devices[i].Nic, os, UsbHcUnknown,
                                     info.Vid, info.Pid, bind.DevObject, bind.Submit,&platform);
                logtext(status == NDIS_STATUS_SUCCESS ? "UNT-UP id=" : "UNT-FAIL id=");
                loghex(info.Id);logtext(" pid=");loghex(info.Pid);
                logtext(" status=");loghex((ULONG)status);logtext("\n");
            }
            if (g_Devices[i].Nic.Nic.Running||g_Devices[i].Nic.HardwareReady) {
                BOOLEAN carrier = FALSE;
                status = UsbNicReadLink(&g_Devices[i].Nic, &carrier);
                if (!g_Devices[i].CarrierKnown ||
                    carrier != g_Devices[i].Carrier ||
                    status != g_Devices[i].LinkStatus) {
                    logtext("UNT-LINK id=");loghex(g_Devices[i].Target.Id);
                    logtext(" up=");loghex(carrier);
                    logtext(" status=");loghex((ULONG)status);logtext("\n");
                }
                g_Devices[i].CarrierKnown = TRUE;
                g_Devices[i].Carrier = carrier;
                g_Devices[i].LinkStatus = status;
                RtlUsbService(&g_Devices[i].Nic,g_Devices[i].Target.Id);
            }
        }
        if (stopping) break;
    }
    PsTerminateSystemThread(0);
}

static VOID ClientStop(VOID)
{
    if (!g_Thread) return;
    if (g_Registered) {
        g_Interface->Unregister(&g_Client); /* callback rundown barrier */
        g_Registered = FALSE;
    }
    NdisAcquireSpinLock(&g_Lock);
    g_Stop = TRUE;
    NdisReleaseSpinLock(&g_Lock);
    KeSetEvent(&g_Wake, 0, FALSE);
    /* Wait for thread termination, not a flag set before its final return. */
    KeWaitForSingleObject(g_Thread, 0, 0, FALSE, NULL);
    ObDereferenceObject(g_Thread);
    g_Thread = NULL;
    NdisFreeSpinLock(&g_Lock);
    if (g_File) { ObDereferenceObject(g_File); g_File = NULL; }
    g_Interface = NULL;
}

static VOID STDC ClientUnload(PVOID driver)
{
    RLT_UNLOAD original = g_NdisUnload;
    ClientStop();
    ((RLT_DRIVER_PREFIX *)driver)->DriverUnload = original;
    if (original) original(driver);
}

VOID RtlUsbNtClientStart(PVOID driver)
{
    static WCHAR name[] = L"\\Device\\UsbNtDev";
    UNICODE_STRING path;
    PDEVICE_OBJECT device = NULL;
    USBNT_DEV_EXT *ext;
    RLT_DRIVER_PREFIX *object = (RLT_DRIVER_PREFIX *)driver;
    NTSTATUS status;
    PVOID threadHandle = NULL;
    path.Buffer=name;path.Length=sizeof(name)-2;path.MaximumLength=sizeof(name);
    if (!object->DriverUnload) { logtext("UNT: no NDIS unload handler\n"); return; }
    status = IoGetDeviceObjectPointer(&path, FILE_READ_DATA, &g_File, &device);
    if (!NT_SUCCESS(status) || !device) { logtext("USBNT not present (no USB stack)\n"); return; }
    ext = (USBNT_DEV_EXT *)device->DeviceExtension;
    if (!ext || ext->Magic != USBNT_DEV_EXT_MAGIC || !ext->If ||
        ext->If->Version != USBNT_DEV_IF_VERSION || ext->If->Size < sizeof(USBNT_DEV_IF)) {
        ObDereferenceObject(g_File);g_File=NULL;return;
    }
    g_Interface=ext->If;
    NdisAllocateSpinLock(&g_Lock);
    KeInitializeEvent(&g_Wake, 0, FALSE);
    g_Stop=FALSE;
    status=PsCreateSystemThread(&threadHandle,0x00100000,NULL,NULL,NULL,ClientThread,NULL);
    if (!NT_SUCCESS(status)) {
        g_Thread=NULL;NdisFreeSpinLock(&g_Lock);ObDereferenceObject(g_File);
        g_File=NULL;g_Interface=NULL;return;
    }
    /* A process-relative handle cannot be used later from an arbitrary
     * unload context on old NT. Retain the referenced thread object instead. */
    status=ObReferenceObjectByHandle(threadHandle,0x00100000,NULL,0,&g_Thread,NULL);
    if (!NT_SUCCESS(status)) {
        NdisAcquireSpinLock(&g_Lock);g_Stop=TRUE;NdisReleaseSpinLock(&g_Lock);
        KeSetEvent(&g_Wake,0,FALSE);
        ZwWaitForSingleObject(threadHandle,FALSE,NULL);ZwClose(threadHandle);
        g_Thread=NULL;NdisFreeSpinLock(&g_Lock);ObDereferenceObject(g_File);
        g_File=NULL;g_Interface=NULL;return;
    }
    ZwClose(threadHandle);
    g_Client.Context=&g_Client;g_Client.Arrival=ClientArrival;g_Client.Removal=ClientRemoval;
    if (g_Interface->Register(&g_Client) != UC_OK) { ClientStop(); return; }
    g_Registered=TRUE;
    g_NdisUnload=object->DriverUnload;
    object->DriverUnload=ClientUnload;
    logtext("UNT: deferred USB worker registered\n");
}
