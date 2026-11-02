/*
 * init.c - MiniportInitialize / Halt / Reset / CheckForHang, PCI resource
 *          discovery and chip identification.
 * Copyright (C) 2026 ages2001.
 */
#include "rltklant.h"
#include "backend_usb.h"
#include "physeq.h"
#if defined(RTL_NDIS5)
#include "native_pci.h"
#endif

ULONG STDC NdisWritePciSlotInformation(NDIS_HANDLE, ULONG, ULONG, PVOID, ULONG);
VOID STDC KeInitializeEvent(PVOID, int, BOOLEAN);
NTSTATUS STDC KeDelayExecutionThread(int, BOOLEAN, PLARGE_INTEGER);

/* ---- QEMU 0xE9 debug-console trace (bring-up only; define RTL_DBG) ---- */
#ifdef RTL_DBG
static VOID dbg(const char *s){ while(*s) RtlOutByte(0xE9,(UCHAR)*s++); }
static VOID dbgh(ULONG v){ char b[11]; int i; b[0]='0'; b[1]='x';
    for(i=0;i<8;i++){ UCHAR n=(UCHAR)((v>>((7-i)*4))&0xF); b[2+i]=(char)(n<10?'0'+n:'A'+n-10);} b[10]=0; dbg(b); }
#else
#define dbg(s)
#define dbgh(v)
#endif

static RTL_FW_U32 PciPhyRead(void *p,RTL_FW_U16 port){return RtlRead32((PRLTK_ADAPTER)p,port);}
static void PciPhyWrite(void*p,RTL_FW_U16 port,RTL_FW_U32 value){RtlWrite32((PRLTK_ADAPTER)p,port,value);}
static int PciPhyUs(void*p,RTL_FW_U32 us){(void)p;NdisStallExecution(us);return 1;}
static int PciPhyMs(void*p,RTL_FW_U32 ms)
{
    LARGE_INTEGER interval;(void)p;
    if(ms>2000)return 0;
    interval.QuadPart=(ULONGLONG)(-(long long)ms*10000);
    return KeDelayExecutionThread(0,FALSE,&interval)>=0;
}
static int PciPhyCancelled(void*p){return ((PRLTK_ADAPTER)p)->Halting;}
static BOOLEAN InitializePciPhy(PRLTK_ADAPTER a)
{
    RTL_FW_BUS bus;RTL_PHY_REPORT report;ULONG txc;UINT i;
    if(a->Backend!=RTL_BACKEND_CPLUS)return TRUE;
    /* No interrupt, timer or protocol send owns this unpublished adapter.
       Stop the MAC before any sleeping firmware work or DMA allocation. */
    if(a->Is8125)RtlWrite32(a,RCP_IMR0_8125,0);else RtlWrite16(a,RCP_IMR,0);
    RtlWrite8(a,RCP_CR,RCP_CR_RST);
    for(i=0;i<1000;i++){
        UCHAR command=RtlRead8(a,RCP_CR);
        if(command==0xff)return FALSE;
        if(!(command&RCP_CR_RST))break;
        NdisStallExecution(10);
    }
    if(i==1000)return FALSE;
    txc=RtlRead32(a,RCP_TCR);if(txc==0xffffffffUL)return FALSE;
    NdisZeroMemory(&bus,sizeof(bus));bus.Context=a;
    bus.Read32=PciPhyRead;bus.Write32=PciPhyWrite;
    bus.DelayUs=PciPhyUs;bus.DelayMs=PciPhyMs;bus.Cancelled=PciPhyCancelled;
    RtlPciPhyConfigureByXid(&bus,(RTL_FW_U16)((txc>>20)&0xfff),!(a->Flags&RTL_F_FASTET),&report);
    dbg("PCI PHY result=");dbgh(report.Result);dbg(" commands=");dbgh(report.Firmware.Executed);dbg("\n");
    a->PhyPage=0;
    return report.Result==RtlFwOk;
}

/* CM_PARTIAL_RESOURCE_LIST (DDK layout, 4-byte packed) */
#pragma pack(push, 4)
typedef struct _RTL_CM_DESC {
    UCHAR  Type;
    UCHAR  ShareDisposition;
    USHORT Flags;
    union {
        struct { LARGE_INTEGER Start; ULONG Length; } Generic;
        struct { ULONG Level; ULONG Vector; ULONG_PTR Affinity; } Interrupt;
    } u;
} RTL_CM_DESC;
typedef struct _RTL_CM_LIST {
    USHORT Version;
    USHORT Revision;
    ULONG  Count;
    RTL_CM_DESC Desc[1];
} RTL_CM_LIST, *PRTL_CM_LIST;
#pragma pack(pop)

#define CmResourceTypePort       1
#define CmResourceTypeInterrupt  2
#define CmResourceTypeMemory     3


/* Bus/slots (device<<0 | function<<5) already bound by an earlier
 * adapter instance of this driver.  NDIS serializes MiniportInitialize, so
 * when several Realtek cards are present each instance claims the next
 * still-unclaimed supported slot instead of all picking the first one.
 * Only used on the NDIS 3.0 path; on NDIS 5 PnP hands each instance its
 * own slot. */
/* One bit per bus/device/function, not one bit per device on bus zero. */
static ULONG g_ClaimedSlots[2048];
static NDIS_SPIN_LOCK g_SlotLock;       /* guards g_ClaimedSlots on SMP (MPS) */
static BOOLEAN g_SlotLockReady = FALSE;
#define RTL_SLOT_TAG(bus,slot) (((bus) << 8) | (slot))

/* Called once from DriverEntry, before any MiniportInitialize, so the slot
   bitmap is safe when several adapters initialize concurrently on a
   multiprocessor (MPS) system. */
VOID RtlInitSlots(VOID)
{
    NdisAllocateSpinLock(&g_SlotLock);
    NdisZeroMemory(g_ClaimedSlots, sizeof(g_ClaimedSlots));
    g_SlotLockReady = TRUE;
}
/* These run with g_SlotLock already held by the caller (select + claim is
   one atomic region so two instances never take the same free slot). */
static BOOLEAN RtlSlotClaimed(ULONG bus, ULONG slot) {
    ULONG tag = RTL_SLOT_TAG(bus,slot);
    return (g_ClaimedSlots[tag >> 5] & (1UL << (tag & 31))) != 0;
}
static VOID RtlClaimSlot(ULONG bus, ULONG slot) {
    ULONG tag = RTL_SLOT_TAG(bus,slot);
    g_ClaimedSlots[tag >> 5] |= 1UL << (tag & 31);
}
/* a->PciSlot holds the encoded bus/slot + 1 (0 = none). */
VOID RtlReleaseSlot(PRLTK_ADAPTER a)
{
    if (a->PciSlot) {
        if (g_SlotLockReady) NdisAcquireSpinLock(&g_SlotLock);
        ULONG tag = a->PciSlot - 1;
        g_ClaimedSlots[tag >> 5] &= ~(1UL << (tag & 31));
        if (g_SlotLockReady) NdisReleaseSpinLock(&g_SlotLock);
        a->PciSlot = 0;
    }
}

/* The HAL accepts an explicit bus/slot without relying on the NDIS context
 * absent from some legacy OEM installations. 'slot' uses the Windows PCI
 * encoding: device in bits 0-4, function in bits 5-7. */
ULONG STDC HalGetBusDataByOffset(ULONG, ULONG, ULONG, PVOID, ULONG, ULONG);
ULONG STDC HalSetBusDataByOffset(ULONG, ULONG, ULONG, PVOID, ULONG, ULONG);
static ULONG PciConfigRead32(ULONG bus, ULONG slot, ULONG off)
{
    ULONG value = 0xFFFFFFFFUL;
    /* PCIConfiguration = 4. HAL serializes configuration cycles with other
     * drivers; a private CF8/CFC lock cannot do so on multiprocessor NT. */
    if (HalGetBusDataByOffset(4, bus, slot, &value, off, 4) != 4)
        return 0xFFFFFFFFUL;
    return value;
}
static VOID PciConfigWrite32(ULONG bus, ULONG slot, ULONG off, ULONG val)
{
    HalSetBusDataByOffset(4, bus, slot, &val, off, 4);
}
/* read BAR 'bar', return base in *pbase and length in *plen; *pio TRUE if I/O */
static VOID PciReadBar(ULONG bus, ULONG slot, ULONG bar, ULONG *pbase, ULONG *plen, BOOLEAN *pio)
{
    ULONG orig = PciConfigRead32(bus, slot, bar), mask;
    *pbase = *plen = 0; *pio = FALSE;
    if (orig == 0xFFFFFFFFUL) return;
    /* This legacy resource path currently maps below-4GB BARs only. */
    if (!(orig & 1) && (orig & 6) == 4 &&
        PciConfigRead32(bus,slot,bar+4) != 0) return;
    PciConfigWrite32(bus, slot, bar, 0xFFFFFFFFUL);
    mask = PciConfigRead32(bus, slot, bar);
    PciConfigWrite32(bus, slot, bar, orig);           /* restore */
    if (orig & 1) { *pio = TRUE;  *pbase = orig & ~0x3UL; *plen = (~(mask & ~0x3UL) + 1) & 0xFFFF; }
    else          { *pio = FALSE; *pbase = orig & ~0xFUL; *plen = ~(mask & ~0xFUL) + 1; }
}

static ULONG PciRead32(PRLTK_ADAPTER a, ULONG slot, ULONG off)
{
    ULONG v = 0xFFFFFFFFUL;
#if !defined(RTL_NDIS5)
    return PciConfigRead32(a->PciBus, slot, off);
#else
    UINT nativeValue=0xffffffffU;
    (void)slot;
    if (!RtlNativePciConfig(a->PciPdo,0,(USHORT)off,4,&nativeValue))
        return 0xFFFFFFFFUL;
    return nativeValue;
#endif
}

/* Is this vendor/device one we drive? */
static BOOLEAN RtlSupported(ULONG id)
{
    return ((id & 0xFFFF) == RTL_VENDOR_ID) && RtlFindPciId((USHORT)(id >> 16)) != NULL;
}

/* Caller holds g_SlotLock across selection and claim. */
static BOOLEAN RtlFindLegacySlot(PULONG selectedBus, PULONG selectedSlot)
{
    ULONG bus = *selectedBus, slot = *selectedSlot, i, function;
    if (bus <= 255 && slot <= 255 && !RtlSlotClaimed(bus,slot) &&
        RtlSupported(PciConfigRead32(bus,slot,0))) return TRUE;
    for (bus = 0; bus < 256; bus++) {
        for (i = 0; i < 32; i++) {
            ULONG id = PciConfigRead32(bus, i, 0);
            ULONG functions;
            if ((id & 0xFFFF) == 0xFFFF || !(id & 0xFFFF)) continue;
            functions = (PciConfigRead32(bus,i,0x0C) & 0x00800000UL) ? 8 : 1;
            for (function = 0; function < functions; function++) {
                ULONG candidate = i | (function << 5);
                if (RtlSlotClaimed(bus,candidate)) continue;
                id = PciConfigRead32(bus,candidate,0);
                if (RtlSupported(id)) {
                    *selectedBus = bus; *selectedSlot = candidate;
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}

/* Read the identity of the PCI function at 'slot'. */
static BOOLEAN RtlReadIdentity(PRLTK_ADAPTER a, ULONG slot)
{
    ULONG id = PciRead32(a, slot, PCI_CFG_VENDOR);
    ULONG cmd;
    if(!RtlSupported(id))return FALSE;
    a->PciDeviceId = (USHORT)(id >> 16);
    a->Revision    = (UCHAR)(PciRead32(a, slot, PCI_CFG_REVISION) & 0xFF);
    a->PciSubsys   = (USHORT)(PciRead32(a, slot, PCI_CFG_SUBSYS) >> 16);
    /* make sure I/O, memory decode and bus mastering are on */
    cmd = PciRead32(a, slot, PCI_CFG_COMMAND);
    if(cmd==0xffffffffUL)return FALSE;
    if ((cmd & 0x7) != 0x7) {
        cmd = (cmd & 0xFFFF) | 0x7;
#if defined(RTL_NDIS5)
        {UINT nativeCommand=(UINT)cmd;if(!RtlNativePciConfig(a->PciPdo,1,PCI_CFG_COMMAND,2,&nativeCommand))return FALSE;}
#else
        PciConfigWrite32(a->PciBus, slot, PCI_CFG_COMMAND, cmd);
#endif
    }
    cmd=PciRead32(a,slot,PCI_CFG_COMMAND);
    return cmd!=0xffffffffUL&&(cmd&7)==7;
}

static NDIS_STATUS RtlGetResources(PRLTK_ADAPTER a, PRTL_CM_LIST *plist,
                                   PRTL_CM_LIST *pfree, PUINT pfreelen)
{
    NDIS_STATUS st;
    UINT len = 0;
    *pfree = NULL; *pfreelen = 0;

#if defined(RTL_NDIS5)
    /* PnP owns the resources; configuration belongs to this function's
       bus PDO. NdisReadPciSlotInformation is a no-op on some NT5 x64 paths.
       Keep a real device reference through all passive configuration I/O. */
    {
        PVOID pdo=NULL;
        NdisMGetDeviceProperty(a->MiniportHandle,&pdo,NULL,NULL,NULL,NULL);
        a->PciPdo=RtlNativePciRetain(pdo);
        if(!a->PciPdo)return NDIS_STATUS_ADAPTER_NOT_FOUND;
        if(!RtlSupported(PciRead32(a,0,PCI_CFG_VENDOR)))return NDIS_STATUS_ADAPTER_NOT_FOUND;
    }
    if(!RtlReadIdentity(a, 0))return NDIS_STATUS_ADAPTER_NOT_FOUND;
    NdisMQueryAdapterResources(&st, a->WrapperConfig, NULL, &len);
    if (!len) return NDIS_STATUS_RESOURCES;
    *pfree = (PRTL_CM_LIST)RtlAlloc(len);
    if (!*pfree) return NDIS_STATUS_RESOURCES;
    *pfreelen = len;
    NdisMQueryAdapterResources(&st, a->WrapperConfig, *pfree, &len);
    if (st != NDIS_STATUS_SUCCESS) return st;
    *plist = *pfree;
    return NDIS_STATUS_SUCCESS;
#else
    /* NT 3.5x / 4.0: find our PCI slot, then let NDIS assign resources. */
    {
        NDIS_HANDLE cfg;
        PNDIS_CONFIGURATION_PARAMETER p;
        static WCHAR kSlot[] = L"SlotNumber";
        static WCHAR kBus[] = L"BusNumber";
        NDIS_STRING s;
        ULONG slot = 0xFFFFFFFFUL, bus = 0;

        s.Buffer = kSlot; s.Length = sizeof(kSlot) - 2; s.MaximumLength = sizeof(kSlot);
        NdisOpenConfiguration(&st, &cfg, a->WrapperConfig);
        if (st == NDIS_STATUS_SUCCESS) {
            NdisReadConfiguration(&st, &p, cfg, &s, NdisParameterInteger);
            if (st == NDIS_STATUS_SUCCESS) slot = p->ParameterData.IntegerData;
            s.Buffer = kBus; s.Length = sizeof(kBus)-2; s.MaximumLength = sizeof(kBus);
            NdisReadConfiguration(&st, &p, cfg, &s, NdisParameterInteger);
            if (st == NDIS_STATUS_SUCCESS) bus = p->ParameterData.IntegerData;
            NdisCloseConfiguration(cfg);
        }
        dbg("GR slot="); dbgh(slot); dbg("\n");
        /* Accept the registry slot only if it names a supported card that no
           earlier instance already took; otherwise scan for the next free
           supported slot so multiple Realtek cards never collide.  The whole
           select-and-claim runs under g_SlotLock so two instances
           initializing at once on an MPS box can't take the same slot. */
        if (g_SlotLockReady) NdisAcquireSpinLock(&g_SlotLock);
        if (!RtlFindLegacySlot(&bus,&slot)) slot = 0xFFFFFFFFUL;
        if (slot == 0xFFFFFFFFUL) {
            if (g_SlotLockReady) NdisReleaseSpinLock(&g_SlotLock);
            dbg("GR NOTFOUND\n"); return NDIS_STATUS_ADAPTER_NOT_FOUND;
        }
        RtlClaimSlot(bus,slot);
        a->PciBus = bus;
        a->PciSlot = RTL_SLOT_TAG(bus,slot) + 1;
        if (g_SlotLockReady) NdisReleaseSpinLock(&g_SlotLock);
        dbg("GR found bus="); dbgh(bus); dbg(" slot="); dbgh(slot); dbg("\n");
        if(!RtlReadIdentity(a, slot))return NDIS_STATUS_ADAPTER_NOT_FOUND;

        /* NdisMPciAssignResources needs the registry bus/slot that an OEM
           "do not detect" install never wrote, so it fails here.  Read the
           BARs and interrupt line from HAL config-space access instead. */
        {
            ULONG bar, base, len2, irq, cmd, n = 0;
            BOOLEAN isio;
            PRTL_CM_LIST L;

            /* Preserve command bits and never echo write-one-to-clear status. */
            cmd = PciConfigRead32(bus, slot, PCI_CFG_COMMAND);

            L = (PRTL_CM_LIST)RtlAlloc(sizeof(RTL_CM_LIST) + 3 * sizeof(RTL_CM_DESC));
            if (!L) return NDIS_STATUS_RESOURCES;
            NdisZeroMemory(L, sizeof(RTL_CM_LIST) + 3 * sizeof(RTL_CM_DESC));
            *pfree = L; *pfreelen = sizeof(RTL_CM_LIST) + 3 * sizeof(RTL_CM_DESC);

            PciConfigWrite32(bus, slot, PCI_CFG_COMMAND, (cmd & 0xFFFFUL) & ~7UL);
            for (bar = PCI_CFG_BAR0; bar <= PCI_CFG_BAR2; bar += 4) {
                ULONG encoding = PciConfigRead32(bus,slot,bar);
                PciReadBar(bus, slot, bar, &base, &len2, &isio);
                /* Upper halves are never independent BARs, including when
                 * the lower half could not be mapped by this legacy path. */
                if (!(encoding & 1) && (encoding & 6) == 4) bar += 4;
                if (!base || !len2) continue;
                L->Desc[n].Type = (UCHAR)(isio ? CmResourceTypePort : CmResourceTypeMemory);
                L->Desc[n].u.Generic.Start.u.LowPart = base;
                L->Desc[n].u.Generic.Start.u.HighPart = 0;
                L->Desc[n].u.Generic.Length = len2;
                dbg(isio ? "  BAR io=" : "  BAR mem="); dbgh(base); dbg(" len="); dbgh(len2); dbg("\n");
                n++;
            }
            PciConfigWrite32(bus, slot, PCI_CFG_COMMAND, (cmd & 0xFFFFUL) | 7);
            irq = PciConfigRead32(bus, slot, PCI_CFG_INTLINE) & 0xFF;
            dbg("  irq="); dbgh(irq); dbg("\n");
            L->Desc[n].Type = CmResourceTypeInterrupt;
            L->Desc[n].u.Interrupt.Level  = irq;   /* bus-relative; NDIS translates */
            L->Desc[n].u.Interrupt.Vector = irq;
            n++;
            L->Count = n;
            *plist = L;
            return NDIS_STATUS_SUCCESS;
        }
    }
#endif
}

NDIS_STATUS STDC
RtlInitialize(PNDIS_STATUS OpenErrorStatus, PUINT SelectedMediumIndex,
              PNDIS_MEDIUM MediumArray, UINT MediumArraySize,
              NDIS_HANDLE MiniportHandle, NDIS_HANDLE WrapperConfigContext)
{
    PRLTK_ADAPTER a;
    NDIS_STATUS st;
    PRTL_CM_LIST list = NULL, tofree = NULL;
    UINT freelen = 0, i;
    ULONG ioBase = 0, ioLen = 0, irqLevel = 0, irqVector = 0;
    NDIS_PHYSICAL_ADDRESS memBase;
    ULONG memLen = 0;
    BOOLEAN havePort = FALSE, haveMem = FALSE, haveIrq = FALSE;
    BOOLEAN backendStarted = FALSE;
    int usbConfig;
    const RTL_PCI_ID *pid;

    (void)OpenErrorStatus;
    dbg("\nRLTK RtlInitialize\n");
    for (i = 0; i < MediumArraySize; i++)
        if (MediumArray[i] == NdisMedium802_3) break;
    if (i == MediumArraySize) { dbg("no 802.3 medium\n"); return NDIS_STATUS_UNSUPPORTED_MEDIA; }
    *SelectedMediumIndex = i;

    a = (PRLTK_ADAPTER)RtlAlloc(sizeof(RLTK_ADAPTER));
    if (!a) return NDIS_STATUS_RESOURCES;
    NdisZeroMemory(a, sizeof(RLTK_ADAPTER));
    a->MiniportHandle = MiniportHandle;
    a->WrapperConfig  = WrapperConfigContext;
    memBase.QuadPart = 0;
    NdisAllocateSpinLock(&a->Lock);
    KeInitializeEvent(&a->LinkTimerStopped, 0, FALSE);

    usbConfig=RtlUsbReadConfig(a);
    if(usbConfig<0){st=NDIS_STATUS_INVALID_DATA;goto fail;}
    if(usbConfig){
#if defined(RTL_NDIS5)
        NdisMSetAttributesEx(MiniportHandle,(NDIS_HANDLE)a,0,0,NdisInterfaceInternal);
#else
        NdisMSetAttributes(MiniportHandle,(NDIS_HANDLE)a,FALSE,NdisInterfaceInternal);
#endif
        a->Ops=&RtlOpsUsb;
        if(!RtlStartBackend(a)){st=NDIS_STATUS_RESOURCES;goto fail;}
        backendStarted=TRUE;
        goto start_timer;
    }

#if defined(RTL_NDIS5)
    /* Use the extended form so NDIS treats us as a PnP bus-master miniport and
       sets up the bus/resource context that NdisMQueryAdapterResources and the
       config-space routines rely on (the legacy NdisMSetAttributes leaves that
       incomplete on XP/2003 x64). */
    NdisMSetAttributesEx(MiniportHandle, (NDIS_HANDLE)a, 0,
                         NDIS_ATTRIBUTE_BUS_MASTER, NdisInterfacePci);
#else
    NdisMSetAttributes(MiniportHandle, (NDIS_HANDLE)a, TRUE, NdisInterfacePci);
#endif

    st = RtlGetResources(a, &list, &tofree, &freelen);
    dbg("GetResources st="); dbgh(st); dbg("\n");
    if (st != NDIS_STATUS_SUCCESS) { goto fail; }

    /* parse the assigned resources first - needed to classify the part when
       the PCI config read is unavailable (x64 PnP) */
    for (i = 0; i < list->Count; i++) {
        RTL_CM_DESC *d = &list->Desc[i];
        switch (d->Type) {
        case CmResourceTypePort:
            if (!havePort) { ioBase = d->u.Generic.Start.u.LowPart; ioLen = d->u.Generic.Length; havePort = TRUE; }
            break;
        case CmResourceTypeMemory:
            if (!haveMem)  { memBase = d->u.Generic.Start; memLen = d->u.Generic.Length; haveMem = TRUE; }
            break;
        case CmResourceTypeInterrupt:
            if (!haveIrq)  { irqLevel = d->u.Interrupt.Level; irqVector = d->u.Interrupt.Vector; haveIrq = TRUE; }
            break;
        }
    }
    dbg("res port="); dbgh(havePort?ioBase:0); dbg(" mem="); dbgh(haveMem?memLen:0);
    dbg(" irq="); dbgh(haveIrq?irqVector:0xFFFFFFFF); dbg("\n");
    if (!haveIrq || (!havePort && !haveMem)) { dbg("missing res\n"); st = NDIS_STATUS_RESOURCES; goto fail; }

    /* classify the backend.  Normally the PCI device id selects it; when the
       config read returns nothing (x64 PnP), fall back to the resource profile
       (+ a TxConfig XID probe below):
         no memory window  -> NE2000 (RTL8029, I/O only)
         memory window     -> C+ / Gigabit family (exact chip from the XID)   */
    pid = RtlFindPciId(a->PciDeviceId);
    if (pid) {
        a->Backend      = pid->Backend;
        a->MaxSpeedMbit = pid->MaxSpeed;
    } else if (!haveMem) {
        a->Backend      = RTL_BACKEND_NE2000;
        a->PciDeviceId  = 0x8029;
        a->MaxSpeedMbit = RTL_SPEED_10;
    } else {
        a->Backend      = RTL_BACKEND_CPLUS;
        a->MaxSpeedMbit = 0;                    /* resolved from the XID below */
    }

    /* registers: MMIO for the C+ family (PCIe parts are MMIO-only),
       I/O ports for RTL8139 / RTL8029 */
    if (havePort && a->Backend != RTL_BACKEND_CPLUS) {
        if (ioLen < 0x20) ioLen = 0x100;
        st = NdisMRegisterIoPortRange(&a->IoPortVa, MiniportHandle, ioBase, ioLen);
        if (st != NDIS_STATUS_SUCCESS) { goto fail; }
        a->IoPortBase = (ULONG)(ULONG_PTR)a->IoPortVa;
        a->IoPortLen  = ioLen;
        a->IoPortPhys = ioBase;
        a->UseMmio = FALSE;
    } else if (haveMem) {
        st = NdisMMapIoSpace((PVOID *)&a->IoBase, MiniportHandle, memBase, memLen);
        if (st != NDIS_STATUS_SUCCESS) { goto fail; }
        a->MemLen = memLen;
        a->UseMmio = TRUE;
    } else {
        if (ioLen < 0x20) ioLen = 0x100;
        st = NdisMRegisterIoPortRange(&a->IoPortVa, MiniportHandle, ioBase, ioLen);
        if (st != NDIS_STATUS_SUCCESS) { goto fail; }
        a->IoPortBase = (ULONG)(ULONG_PTR)a->IoPortVa;
        a->IoPortLen  = ioLen;
        a->IoPortPhys = ioBase;
        a->UseMmio = FALSE;
    }

    /* backend + exact chip name / ceiling */
    switch (a->Backend) {
    case RTL_BACKEND_8139:  a->Ops = &RtlOps8139;  break;
    case RTL_BACKEND_CPLUS: a->Ops = &RtlOpsCplus; break;
    default:                a->Ops = &RtlOpsNe2000; break;
    }
    {
        ULONG txc = 0;
        if (a->Backend == RTL_BACKEND_8139)       txc = RtlRead32(a, R8139_TCR);
        else if (a->Backend == RTL_BACKEND_CPLUS) txc = RtlRead32(a, RCP_TCR);
        RtlResolveName(a, txc);
    }
    if (!a->MaxSpeedMbit) a->MaxSpeedMbit = RTL_SPEED_1000;   /* safety net */
    a->Is8125 = (a->Flags & RTL_F_8125REGS) ? TRUE : FALSE;

    RtlReadSettings(a);         /* SpeedDuplex, NetworkAddress */

    if(!InitializePciPhy(a)){st=NDIS_STATUS_FAILURE;goto fail;}

    /* bus-master DMA plumbing (needed before shared memory allocation) */
    if (a->Backend != RTL_BACKEND_NE2000) {
        st = NdisMAllocateMapRegisters(MiniportHandle, 0, TRUE, 2, 2048);
        if (st != NDIS_STATUS_SUCCESS) { goto fail; }
        a->MapRegs = TRUE;
    }

    dbg("backend="); dbgh(a->Backend); dbg(" did="); dbgh(a->PciDeviceId); dbg("\n");
    if (!RtlStartBackend(a)) { dbg("Ops->Init FAILED\n"); st = NDIS_STATUS_FAILURE; goto fail; }
    backendStarted = TRUE;
    dbg("Ops->Init ok\n");

    st = NdisMRegisterInterrupt(&a->Interrupt, MiniportHandle, irqVector, irqLevel,
                                TRUE, TRUE, NdisInterruptLevelSensitive);
    dbg("RegisterInterrupt st="); dbgh(st); dbg("\n");
    if (st != NDIS_STATUS_SUCCESS) { goto fail; }
    a->IntRegistered = TRUE;
    a->IrqVector=irqVector;a->IrqLevel=irqLevel;
    if (!RtlStartRecovery(a)) {st=NDIS_STATUS_RESOURCES;goto fail;}
    dbg("pre-EnableInts\n");
    a->Ops->EnableInts(a, TRUE);
    dbg("post-EnableInts\n");

    {
        ULONG spd = 0; BOOLEAN fd = FALSE;
        a->LinkUp = a->Ops->LinkState(a, &spd, &fd);
        a->LinkSpeedMbit = RtlCapSpeed(a, spd ? spd : a->MaxSpeedMbit);
        a->FullDuplex = fd;
    }
    dbg("post-LinkState\n");

start_timer:
    NdisMInitializeTimer(&a->LinkTimer, MiniportHandle, RtlLinkTimer, a);
    RtlResumeLinkTimer(a);
    dbg("post-Timer\n");

    RtlPublishName(a);          /* exact adapter name in the registry */
    dbg("post-PublishName\n");

    if (tofree) RtlFree(tofree, freelen);
    dbg("RLTK INIT DONE OK\n");
    return NDIS_STATUS_SUCCESS;

fail:
    dbg("RLTK INIT FAIL st="); dbgh(st); dbg("\n");
    RtlStopRecovery(a);
    if (a->IntRegistered) NdisMDeregisterInterrupt(&a->Interrupt);
    if (backendStarted) a->Ops->Halt(a);
    if (a->MapRegs) NdisMFreeMapRegisters(MiniportHandle);
    if (a->IoBase) NdisMUnmapIoSpace(MiniportHandle, a->IoBase, a->MemLen);
    if (a->IoPortVa) NdisMDeregisterIoPortRange(MiniportHandle, a->IoPortPhys, a->IoPortLen, a->IoPortVa);
#if defined(RTL_NDIS5)
    RtlNativePciRelease(a->PciPdo);a->PciPdo=NULL;
#endif
    if (tofree) RtlFree(tofree, freelen);
    RtlReleaseSlot(a);
    NdisFreeSpinLock(&a->Lock);
    RtlFree(a, sizeof(RLTK_ADAPTER));
    return st;
}

