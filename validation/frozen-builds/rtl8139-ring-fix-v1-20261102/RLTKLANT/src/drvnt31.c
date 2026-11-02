/*
 * driver31.c - Windows NT 3.1 build of RLTKLANT (NDIS 3.0 MAC model).
 *
 * NT 3.1 has no NDIS miniport model and no PCI NDIS API, so this is a full
 * NDIS 3.0 MAC driver that finds the card by direct PCI config access
 * (0xCF8/0xCFC), uses the Eth* filter package for receive routing, and gets
 * bus-master DMA buffers from MmAllocateContiguousMemory / MmGetPhysicalAddress
 * (physical == bus address on x86 PCI).  All Realtek register models are
 * covered: NE2000 (RTL8029), 8139 (RTL8129/8139/810x) and C+ (RTL8169..8127).
 *
 * Receive is polled from an NDIS timer (NT 3.1 interrupt wiring is avoided for
 * portability); transmit is synchronous.  A QEMU 0xE9 debug-console trace is
 * compiled in (RTL_DBG) for bring-up.
 *
 * Copyright (C) 2026 ages2001.
 */
#include "ndisnt31.h"
#include "rltkhw.h"
#include "rtlchip.h"
#include "nt31usb.h"
#include "physeq.h"
#include "macseq.h"
#include "nt31ndisabi.h"
#include "ndisvendor.h"

#define ETHLEN    14
#define MAXFRAME  1514
#define N31_TXN   4

enum { BK_NE2000=0, BK_8139=1, BK_CPLUS=2, BK_USB=3 };

typedef struct _N31_ADAPTER {
    NDIS_HANDLE  NdisAdapterHandle;
    NDIS_HANDLE  MacHandle;
    NDIS_HANDLE  FilterDb;            /* from EthCreateFilter              */
    NDIS_SPIN_LOCK Lock;
    NDIS_INTERRUPT Interrupt;
    ULONG        IoBase;
    PVOID        MappedBase;          /* NDIS PortOffset target            */
    ULONG        MmioLength;          /* owned noncached PCI mapping       */
    ULONG        Irq;
    ULONG        Bdf;                 /* packed PCI bus<<16|dev<<8|fn       */
    UINT         Backend;
    USHORT       DeviceId;
    UCHAR        Mac[6];
    ULONG        Filter;              /* NDIS packet-filter classes        */
    ULONG        LinkSpeed;           /* in 100 bps units                  */
    BOOLEAN      Running;
    PVOID        UsbContext;
    BOOLEAN      UsbLinkUp;
    ULONG        PollLock;
    ULONG        TxLock;            /* serialize PCI send reservation/copy */
    ULONG        PollActive;
    ULONG        PollDrained[8];     /* aligned opaque NT 3.1 KEVENT */
    PVOID        RecoveryThread;
    ULONG        RecoveryWake[8];
    BOOLEAN      RecoveryStop,Resetting,ResetFailed;
    BOOLEAN      PciHardwareInitialized,PciManagementOwned,PciStopping;
    struct _N31_BINDING *ResetBinding;
    struct _N31_BINDING *Bindings;

    /* 8139 RX linear ring + TX buffers (contiguous DMA) */
    PUCHAR       RxRing;  ULONG RxRingPhys; ULONG RxOffset;
    PUCHAR       TxBuf[N31_TXN]; ULONG TxPhys[N31_TXN]; ULONG TxCur;

    /* C+ descriptor rings + buffers */
    PRCP_DESC    TxDesc; ULONG TxDescPhys;
    PRCP_DESC    RxDesc; ULONG RxDescPhys;
    PVOID        TxDescAllocation, RxDescAllocation; /* original aligned-ring allocations */
    PUCHAR       CpTx[RCP_NUM_TX_DESC]; ULONG CpTxP[RCP_NUM_TX_DESC];
    PUCHAR       CpRx[RCP_NUM_RX_DESC]; ULONG CpRxP[RCP_NUM_RX_DESC];
    UINT         CpTxHead, CpRxCur;
    UCHAR        NeCurr;              /* NE2000 next-packet read page */

    /* staged RX frame for MacTransferData */
    UCHAR        LastFrame[MAXFRAME];
    UINT         LastLen;
} N31_ADAPTER, *PN31_ADAPTER;

/* Each protocol open owns its filter handle. TCP/IP and NetBIOS can bind
   simultaneously; an adapter-wide handle would make one close remove the
   other protocol's filter. The filter package owns indication references. */
typedef struct _N31_BINDING {
    struct _N31_BINDING *Next;
    PN31_ADAPTER Adapter;
    NDIS_HANDLE NdisBinding, FilterHandle;
    ULONG PacketFilter, Lookahead;
    UINT MulticastBytes;
    UCHAR Multicast[16*6];
    UINT ResetReferences;
    BOOLEAN Closing;
} N31_BINDING, *PN31_BINDING;
static NDIS_STATUS n31_queue_reset(PN31_BINDING binding);

#include "nt31poll.h"

/* The adapter, shared NDIS_TIMER/KDPC and all data the DPC touches must
   live in NON-PAGED memory: a static in .bss can be paged, and the kernel's
   DPC/timer dispatch touches it at DISPATCH_LEVEL (fault => 0xA).  Allocated
   contiguous (always non-paged) in DriverEntry. */
/* Multiple Realtek cards: g_ap[] and g_apN live in non-paged pool (set in
   DriverEntry *before* the image is copied), so the image-context AddAdapter
   handler and the pool-context poll DPC share the same adapter list.  Each
   card is a separate N31_ADAPTER with its own DMA rings, filter and MAC
   binding; a single poll timer services them all. */
#define N31_MAX 8
typedef struct _N31_POLL_SET {
    ULONG Lock;
    NDIS_TIMER Timer;             /* never embedded in a removable adapter */
    BOOLEAN Armed, Stopping;
    ULONG Drained[8];
} N31_POLL_SET;
/* One reference owns the driver lifetime; each armed timer owns another.
   Every callback retains the owner lock until its kernel tail unlock, so
   queued reclamation can join all remaining resident callback epilogues. */
typedef struct _N31_IMAGE_OWNER {
    PVOID WorkFlink, WorkBlink;
    VOID (STDC *Worker)(PVOID);
    PVOID Context;
    ULONG Lock;
    ULONG References;
    PUCHAR Image;
    VOID (STDC *Unload)(PVOID);
} N31_IMAGE_OWNER;
typedef char N31OwnerLockOffsetChecked[__builtin_offsetof(N31_IMAGE_OWNER,Lock)==16?1:-1];
static N31_IMAGE_OWNER *g_owner;
static N31_POLL_SET *g_poll;
static VOID n31_image_get(VOID)
{
    KIRQL old;KeAcquireSpinLock(&g_owner->Lock,&old);g_owner->References++;
    KeReleaseSpinLock(&g_owner->Lock,old);
}
/* Only a cancelled timer (which has no executing epilogue) may drop its
   reference with an ordinary unlock; the driver still owns its reference. */
static VOID n31_image_cancel(VOID)
{
    KIRQL old;KeAcquireSpinLock(&g_owner->Lock,&old);g_owner->References--;
    KeReleaseSpinLock(&g_owner->Lock,old);
}
/* Returns with the owner lock HELD. The outer assembly must tail-call the
   kernel unlock; otherwise a concurrent final owner could free this code
   while an earlier callback was still returning from its reference drop. */
static ULONG STDC n31_release_prepare(VOID) __attribute__((used,noinline));
static ULONG STDC n31_release_prepare(VOID)
{
    KIRQL old;KeAcquireSpinLock(&g_owner->Lock,&old);
    if(--g_owner->References==0)ExQueueWorkItem(g_owner,1);
    return old;
}
static N31_ADAPTER **g_ap;    /* pool array[N31_MAX] of adapters, shared */
static UINT         *g_apN;   /* pool-resident adapter count             */
static NDIS_HANDLE g_wrapper, g_mac;

/* NT 3.1 maps this driver image only in the address-space context of the
   process that loaded it; code called later from an arbitrary DPC/thread
   context faults (0xA at DISPATCH, 0x7A page-in at PASSIVE).  Non-paged pool is
   globally mapped and resident in every context, so DriverEntry copies the
   whole image into pool, applies its base relocations, and routes every entry
   point NDIS may call after load (MAC handlers, Eth filter callbacks, the RX
   timer DPC) through the pool copy via cp().  (A pool machine-code stub was
   verified to run as an NDIS timer DPC where image code faults.) */
static PUCHAR g_img;         /* pool copy of the image (globally mapped) */
static PUCHAR g_drvStart;    /* original image base (DriverObject->DriverStart) */
static PVOID dma_alloc(ULONG len, PULONG phys);   /* forward decl */

/* Copy the loaded image into non-paged pool and fix up HIGHLOW base relocations
   (delta = pool base - original base).  dma_alloc gives contiguous non-paged
   (hence globally mapped, resident, executable) memory. */
static PUCHAR img_to_pool(PVOID drvStart, ULONG drvSize){
    PUCHAR src=(PUCHAR)drvStart, dst; ULONG ph, i, pe, opt, relRva, relSize, delta, off;
    ULONG need=(drvSize+0xFFFUL)&~0xFFFUL;
    dst=(PUCHAR)dma_alloc(need,&ph);
    if(!dst) return NULL;
    for(i=0;i<drvSize;i++) dst[i]=src[i];
    for(;i<need;i++) dst[i]=0;
    delta=(ULONG)dst-(ULONG)src;
    pe=*(ULONG*)(dst+0x3C); opt=pe+24;
    relRva =*(ULONG*)(dst+opt+96+5*8);
    relSize=*(ULONG*)(dst+opt+96+5*8+4);
    for(off=0; relRva && off<relSize; ){
        ULONG pageRva=*(ULONG*)(dst+relRva+off);
        ULONG blk    =*(ULONG*)(dst+relRva+off+4);
        ULONG n, cnt;
        if(blk<8) break;
        cnt=(blk-8)/2;
        for(n=0;n<cnt;n++){
            USHORT e=*(USHORT*)(dst+relRva+off+8+n*2);
            if((e>>12)==3) *(ULONG*)(dst+pageRva+(e&0xFFF))+=delta;  /* HIGHLOW */
        }
        off+=blk;
    }
    return dst;
}
/* Translate an original-image function address to its pool-copy address. */
static void* cp(void* f){ return (void*)(g_img + ((PUCHAR)f - g_drvStart)); }

/* Experimental PASSIVE-level RX poll thread (off: blocked by NT 3.1 driver
   image demand-paging - bugcheck 0x7A; kept for reference, see README). */
#define RTL_NT31_RX_THREAD 0

/* ================= QEMU 0xE9 debug console ========================= */
#define RTL_DBG 1
#if RTL_DBG
static VOID dbg(const char *s){ while(*s) WRITE_PORT_UCHAR((PUCHAR)0xE9,(UCHAR)*s++); }
static VOID dbgh(ULONG v){
    char b[11]; int i; const char *h="0123456789ABCDEF";
    b[0]='0';b[1]='x';for(i=0;i<8;i++) b[2+i]=h[(v>>((7-i)*4))&0xF];b[10]=0;
    dbg(b);
}
#else
#define dbg(s)
#define dbgh(v)
#endif

/* ================= port I/O via HAL ================================ */
/* Valid x86 ports fit in 16 bits; MmMapIoSpace returns a kernel VA. Never
 * truncate a mapped register address into an IN/OUT port operand. */
static UCHAR  in8 (ULONG p){ return p>0xffff?*(volatile UCHAR*)p:READ_PORT_UCHAR((PUCHAR)p); }
static USHORT in16(ULONG p){ return p>0xffff?*(volatile USHORT*)p:READ_PORT_USHORT((PUSHORT)p); }
static ULONG  in32(ULONG p){ return p>0xffff?*(volatile ULONG*)p:READ_PORT_ULONG((PULONG)p); }
static VOID   out8 (ULONG p,UCHAR v){ if(p>0xffff)*(volatile UCHAR*)p=v;else WRITE_PORT_UCHAR((PUCHAR)p,v); }
static VOID   out16(ULONG p,USHORT v){ if(p>0xffff)*(volatile USHORT*)p=v;else WRITE_PORT_USHORT((PUSHORT)p,v); }
static VOID   out32(ULONG p,ULONG v){ if(p>0xffff)*(volatile ULONG*)p=v;else WRITE_PORT_ULONG((PULONG)p,v); }
static VOID   stall(ULONG n){ volatile ULONG i; for(i=0;i<n*200;i++){} }

static VOID n31_unmap(PN31_ADAPTER a)
{
    if(a->MmioLength){MmUnmapIoSpace(a->MappedBase,a->MmioLength);a->MmioLength=0;a->MappedBase=NULL;a->IoBase=0;}
}

/* The unpublished AddAdapter path or the drained passive recovery thread
 * exclusively owns this adapter during PHY/MAC preparation. */
static RTL_FW_U32 n31_phy_read(void*p,RTL_FW_U16 reg){return in32(((PN31_ADAPTER)p)->IoBase+reg);}
static void n31_phy_write(void*p,RTL_FW_U16 reg,RTL_FW_U32 value){out32(((PN31_ADAPTER)p)->IoBase+reg,value);}
static int n31_phy_cancelled(void*p){PN31_ADAPTER a=p;return a->RecoveryStop&&!a->PciStopping;}
static int n31_phy_us(void*p,RTL_FW_U32 us){
    PN31_ADAPTER a=p;LARGE_INT delay;
    if(n31_phy_cancelled(a)||us>2000000U)return 0;
    if(us<=50){KeStallExecutionProcessor(us);return !n31_phy_cancelled(a);}
    delay.LowPart=(ULONG)(-(long)(us*10));delay.HighPart=-1;
    return KeDelayExecutionThread(0,FALSE,&delay)>=0&&!n31_phy_cancelled(a);
}
static int n31_phy_ms(void*p,RTL_FW_U32 ms)
{
    return ms<=2000&&n31_phy_us(p,ms*1000);
}
static int cp_is25(PN31_ADAPTER a);
static BOOLEAN n31_initialize_mac(PN31_ADAPTER,RTL_FW_BUS*,const RTL_FW_CHIP*);
static BOOLEAN n31_initialize_phy(PN31_ADAPTER a)
{
    RTL_FW_BUS bus;RTL_PHY_REPORT report;ULONG txc;UINT i;BOOLEAN gmii=TRUE;
    if(a->Backend!=BK_CPLUS)return TRUE;
    /* The passive owner has acknowledged reset; no later reset may erase
       this preparation before the complete DMA rings are published. */
    if(a->RecoveryStop||in8(a->IoBase+RCP_CR)&(RCP_CR_RST|RCP_CR_TE|RCP_CR_RE))return FALSE;
    txc=in32(a->IoBase+RCP_TCR);if(txc==0xffffffffUL)return FALSE;
    for(i=0;i<RTL_PCI_ID_COUNT;i++)if(RtlPciIds[i].DeviceId==a->DeviceId){gmii=RtlPciIds[i].MaxSpeed>=1000;break;}
    {UCHAR*p=(UCHAR*)&bus;for(i=0;i<sizeof(bus);i++)p[i]=0;}
    bus.Context=a;bus.Read32=n31_phy_read;bus.Write32=n31_phy_write;
    bus.DelayUs=n31_phy_us;bus.DelayMs=n31_phy_ms;bus.Cancelled=n31_phy_cancelled;
    RtlPciPhyConfigureByXid(&bus,(RTL_FW_U16)((txc>>20)&0xfff),gmii,&report);
    dbg("PCI PHY result=");dbgh(report.Result);dbg(" commands=");dbgh(report.Firmware.Executed);dbg("\n");
    return report.Result==RtlFwOk&&n31_initialize_mac(a,&bus,RtlFwChip((RTL_FW_U16)((txc>>20)&0xfff),gmii));
}

/* ================= registry: publish MAC for DHCPNT31 /auto ========= */
/* Publish to the actual adapter instance, not the shared driver service.
   DHCPNT31 enumerates Services/<instance>/Parameters/Tcpip and reads the MAC
   from that instance's Parameters key. NDIS passes its device name to Add. */
typedef struct _OBJ_ATTR {
    ULONG Length; PVOID RootDirectory; PNDIS_STRING ObjectName;
    ULONG Attributes; PVOID SecurityDescriptor; PVOID SecurityQoS;
} OBJ_ATTR;
NDIS_STATUS STDC ZwOpenKey(PVOID*, ULONG, OBJ_ATTR*);
NDIS_STATUS STDC ZwSetValueKey(PVOID, PNDIS_STRING, ULONG, ULONG, PVOID, ULONG);
NDIS_STATUS STDC ZwClose(PVOID);

static void publish_mac(PN31_ADAPTER a,PNDIS_STRING adapterName)
{
    static const WCHAR prefix[]=L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\";
    static const WCHAR suffix[]=L"\\Parameters";
    static WCHAR key[]=L"DhcpHwAddress";
    static const char hx[]="0123456789abcdef";
    WCHAR keyPath[256],data[18];NDIS_STRING path,val;OBJ_ATTR oa;
    PVOID h=NULL;UINT i,n,first=0,p=0;NDIS_STATUS status;
    if(!adapterName||!adapterName->Buffer||(adapterName->Length&1))return;
    n=adapterName->Length/2;
    for(i=0;i<n;i++)if(adapterName->Buffer[i]=='\\')first=i+1;
    if(first==n||n-first>80)return;
    for(i=0;prefix[i];i++)keyPath[p++]=prefix[i];
    for(i=first;i<n;i++){
        WCHAR ch=adapterName->Buffer[i];
        if(!((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||
             (ch>='0'&&ch<='9')||ch=='_'))return;
        keyPath[p++]=ch;
    }
    for(i=0;suffix[i];i++)keyPath[p++]=suffix[i];
    keyPath[p]=0;
    path.Buffer=keyPath;path.Length=(USHORT)(p*2);path.MaximumLength=(USHORT)((p+1)*2);
    for(i=0,p=0;i<6;i++){
        data[p++]=(WCHAR)hx[a->Mac[i]>>4];data[p++]=(WCHAR)hx[a->Mac[i]&15];
        if(i<5)data[p++]=':';
    }
    data[p]=0;
    val.Buffer=key;val.Length=sizeof(key)-2;val.MaximumLength=sizeof(key);
    oa.Length=sizeof(oa);oa.RootDirectory=NULL;oa.ObjectName=&path;
    oa.Attributes=0x40;oa.SecurityDescriptor=NULL;oa.SecurityQoS=NULL;
    if(ZwOpenKey(&h,0x0002 /* KEY_SET_VALUE */,&oa)==0){
        status=ZwSetValueKey(h,&val,0,1 /* REG_SZ */,data,sizeof(data));ZwClose(h);
        if(status==0){dbg("MAC published for adapter instance\n");return;}
    }
    dbg("MAC publication failed for adapter instance\n");
}

/* ================= direct PCI config (type 1) ====================== */
#define PCI_ADDR 0xCF8
#define PCI_DATA 0xCFC
static ULONG pci_cfg(UINT bus,UINT dev,UINT fn,UINT off){
    out32(PCI_ADDR, 0x80000000UL | (bus<<16) | (dev<<11) | (fn<<8) | (off&0xFC));
    return in32(PCI_DATA);
}
static VOID pci_cfg_w(UINT bus,UINT dev,UINT fn,UINT off,ULONG v){
    out32(PCI_ADDR, 0x80000000UL | (bus<<16) | (dev<<11) | (fn<<8) | (off&0xFC));
    out32(PCI_DATA, v);
}
#include "nt31pcimac.h"

/* Use the same explicit PCI ID database as the miniport builds. Unknown
   network devices must not be programmed as an assumed C+ controller. */
static UINT rtl_backend_for(USHORT did){
    UINT i;
    for(i=0;i<RTL_PCI_ID_COUNT;i++)
        if(RtlPciIds[i].DeviceId==did) return RtlPciIds[i].Backend;
    return (UINT)-1;
}

/* Per-device marketing name, so each chip reports its own identity via
   OID_GEN_VENDOR_DESCRIPTION (as the 2000/XP INF entries do by PnP id).  NT 3.1
   has no PnP, so the installed Control Panel name stays generic, but protocols
   and tools that query the description see the actual controller. */
static const char *rtl_name_for(USHORT did){
    switch(did){
    case 0x8029: return "Realtek RTL8029(AS) PCI Ethernet";
    case 0x8129: return "Realtek RTL8129 PCI Fast Ethernet";
    case 0x8139: case 0x8138: case 0x8130: case 0x8100:
                 return "Realtek RTL8139 Family PCI Fast Ethernet";
    case 0x8169: return "Realtek RTL8169 PCI Gigabit Ethernet";
    case 0x8167: return "Realtek RTL8169SC Gigabit Ethernet";
    case 0x8168: case 0x8161: case 0x8162:
                 return "Realtek RTL8168/8111 PCIe Gigabit Ethernet";
    case 0x8136: return "Realtek RTL810xE PCIe Fast Ethernet";
    case 0x8101: return "Realtek RTL8101E/8102E PCIe Fast Ethernet";
    case 0x8125: return "Realtek RTL8125 2.5GbE Ethernet";
    case 0x8126: return "Realtek RTL8126 5GbE Ethernet";
    case 0x8127: return "Realtek RTL8127 10GbE Ethernet";
    default:     return "Realtek LAN Family Adapter";
    }
}
static UINT rtl_strlen(const char *s){ UINT n=0; while(s[n]) n++; return n; }

/* Scan all segment-0 buses. Function 0's header controls function scanning,
   even when function 0 belongs to a different vendor or device class. */
/* TRUE if this bus/dev/fn is already driven by another adapter instance. */
static BOOLEAN bdf_claimed(ULONG bdf){
    UINT i;KIRQL old;BOOLEAN claimed=FALSE;
    KeAcquireSpinLock(&g_poll->Lock,&old);
    for(i=0;i<N31_MAX;i++)if(g_ap[i]&&g_ap[i]->Backend!=BK_USB&&g_ap[i]->Bdf==bdf){claimed=TRUE;break;}
    KeReleaseSpinLock(&g_poll->Lock,old);
    return claimed;
}
/* Size an unclaimed memory BAR with decode and bus mastering disabled.
   Restore both halves and only the command word before accepting the mask;
   PCI status contains W1C bits and must never be echoed. NT3.1 has no PnP
   resource list here, so mapping more than the measured aperture is unsafe. */
static ULONG pci_memory_length(UINT bus,UINT dev,UINT fn,UINT bar){
    UINT offset=0x10+4*bar;ULONG low,high=0,mask,upper=0,command,length;
    BOOLEAN wide;
    if(bar>=6)return 0;
    low=pci_cfg(bus,dev,fn,offset);
    if(low==0xffffffffUL||(low&1)||((low&6)!=0&&(low&6)!=4))return 0;
    wide=(low&6)==4;
    if(wide){if(bar==5)return 0;high=pci_cfg(bus,dev,fn,offset+4);if(high)return 0;}
    command=pci_cfg(bus,dev,fn,4);if(command==0xffffffffUL)return 0;
    command&=0xffff;
    pci_cfg_w(bus,dev,fn,4,command&~7UL);
    if(pci_cfg(bus,dev,fn,4)&7){pci_cfg_w(bus,dev,fn,4,command);return 0;}
    pci_cfg_w(bus,dev,fn,offset,0xffffffffUL);
    if(wide)pci_cfg_w(bus,dev,fn,offset+4,0xffffffffUL);
    mask=pci_cfg(bus,dev,fn,offset);
    if(wide)upper=pci_cfg(bus,dev,fn,offset+4);
    pci_cfg_w(bus,dev,fn,offset,low);
    if(wide)pci_cfg_w(bus,dev,fn,offset+4,high);
    pci_cfg_w(bus,dev,fn,4,command);
    if((mask&15)!=(low&15)||(wide&&upper!=0xffffffffUL))return 0;
    mask&=~15UL;length=~mask+1;
    if(length<0x100||(length&(length-1))||!(low&~15UL)||
       ((low&~15UL)&(length-1))||(low&~15UL)>0xffffffffUL-(length-1))return 0;
    /* Only the first 64 KiB can be addressed by this driver's register API. */
    return length>0x10000?0x10000:length;
}
static BOOLEAN pci_find(PN31_ADAPTER a){
    UINT bus,dev,fn;
    for(bus=0;bus<256;bus++){
        for(dev=0;dev<32;dev++){
            ULONG first=pci_cfg(bus,dev,0,0);
            UINT count;
            if((USHORT)first==0xFFFF || !(USHORT)first) continue;
            count=(pci_cfg(bus,dev,0,0x0C)&0x00800000UL)?8:1;
            for(fn=0;fn<count;fn++){
                ULONG id=pci_cfg(bus,dev,fn,0),bdf,base=0,memory=0,cmd,header;
                USHORT did=(USHORT)(id>>16);
                UINT backend,bar,memoryBar=0;
                if((USHORT)id!=RTL_VENDOR_ID) continue;
                backend=rtl_backend_for(did);
                if(backend==(UINT)-1) continue;
                if((pci_cfg(bus,dev,fn,8)>>16)!=0x0200) continue;
                header=pci_cfg(bus,dev,fn,0x0C);
                if(((header>>16)&0x7F)!=0) continue;
                bdf=(bus<<16)|(dev<<8)|fn;
                if(bdf_claimed(bdf)) continue;
                /* Collect I/O and memory BARs separately. NT3.1 cannot map
                   above-4GB PCI memory, and a 64-bit upper word is never a
                   second BAR. Prefer noncached MMIO for the C+ family. */
                for(bar=0;bar<6;bar++){
                    ULONG value=pci_cfg(bus,dev,fn,0x10+4*bar);
                    if(value==0xFFFFFFFFUL) continue;
                    if(value&1){
                        ULONG candidate=value&~3UL;
                        if(!base&&candidate&&candidate<=0xFF00UL)base=candidate;
                    }else{
                        ULONG candidate=value&~15UL;UINT lowerBar=bar;BOOLEAN usable=(value&6)==0;
                        if((value&6)==4){
                            usable=bar<5&&pci_cfg(bus,dev,fn,0x14+4*bar)==0;bar++;
                        }
                        if(!memory&&usable&&candidate>=0x10000UL&&candidate<=0xffffff00UL){memory=candidate;memoryBar=lowerBar;}
                    }
                }
                if(!base&&!(backend==BK_CPLUS&&memory))continue;
                cmd=pci_cfg(bus,dev,fn,4);
                if(cmd==0xFFFFFFFFUL) continue;
                if(backend==BK_CPLUS&&memory){
                    NDIS_PHYSICAL_ADDRESS physical;PVOID mapped=NULL;
                    ULONG length=pci_memory_length(bus,dev,fn,memoryBar);
                    physical.LowPart=memory;physical.HighPart=0;
                    if(length)mapped=MmMapIoSpace(physical,length,0);
                    if(mapped){a->MappedBase=mapped;a->MmioLength=length;base=(ULONG)(ULONG_PTR)mapped;}
                    else if(!base)continue;
                }
                /* Do not echo write-one-to-clear PCI status bits. */
                pci_cfg_w(bus,dev,fn,4,(cmd&0xFFFFUL)|(a->MmioLength?6:5));
                a->IoBase=base;
                a->Irq=pci_cfg(bus,dev,fn,0x3C)&0xFF;
                a->Bdf=bdf;
                a->DeviceId=did;
                a->Backend=backend;
                dbg("PCI dev=");dbgh(did);dbg(" io=");dbgh(base);
                dbg(" irq=");dbgh(a->Irq);dbg(" bk=");dbgh(backend);dbg("\n");
                return TRUE;
            }
        }
    }
    dbg("PCI: no supported Realtek LAN with usable resources found\n");
    return FALSE;
}

/* ================= contiguous DMA allocation ======================= */
static PVOID dma_alloc(ULONG len, PULONG phys){
    NDIS_PHYSICAL_ADDRESS hi; PVOID va; NDIS_PHYSICAL_ADDRESS pa;
    hi.LowPart = 0xFFFFFFFFUL; hi.HighPart = 0;
    va = MmAllocateContiguousMemory(len, hi);
    if (!va){ *phys = 0; return NULL; }
    pa = MmGetPhysicalAddress(va);
    *phys = pa.LowPart;
    return va;
}

/* For unpublished failed starts only: no timer/filter user can hold this
   adapter. Descriptor pointers may be interior aligned addresses, so free
   the original allocation addresses instead. */
static VOID n31_free_dma(PN31_ADAPTER a){
    UINT i;
    for(i=0;i<N31_TXN;i++) if(a->TxBuf[i]){
        MmFreeContiguousMemory(a->TxBuf[i]); a->TxBuf[i]=NULL;
    }
    for(i=0;i<RCP_NUM_TX_DESC;i++) if(a->CpTx[i]){
        MmFreeContiguousMemory(a->CpTx[i]); a->CpTx[i]=NULL;
    }
    for(i=0;i<RCP_NUM_RX_DESC;i++) if(a->CpRx[i]){
        MmFreeContiguousMemory(a->CpRx[i]); a->CpRx[i]=NULL;
    }
    if(a->RxRing){MmFreeContiguousMemory(a->RxRing);a->RxRing=NULL;}
    if(a->TxDescAllocation){MmFreeContiguousMemory(a->TxDescAllocation);a->TxDescAllocation=NULL;}
    if(a->RxDescAllocation){MmFreeContiguousMemory(a->RxDescAllocation);a->RxDescAllocation=NULL;}
    a->TxDesc=a->RxDesc=NULL;
}

/* ================= RTL8139 backend ================================= */
static BOOLEAN r8139_reset(PN31_ADAPTER a){
    ULONG b=a->IoBase; UINT i;
    out8(b+R8139_CR, R8139_CR_RST);
    for(i=0;i<1000;i++){
        UCHAR cr=in8(b+R8139_CR);
        if(cr==0xFF) return FALSE;
        if(!(cr&(R8139_CR_RST|R8139_CR_TE|R8139_CR_RE))) return TRUE;
        stall(5);
    }
    return FALSE;
}
static VOID r8139_setfilter(PN31_ADAPTER a){
    ULONG b=a->IoBase, rcr = R8139_RCR_RBLEN_32K|R8139_RCR_MXDMA_UNL|R8139_RCR_RXFTH_NO|R8139_RCR_WRAP;
    if (a->Filter & NDIS_PACKET_TYPE_PROMISCUOUS) rcr |= R8139_RCR_AAP|R8139_RCR_APM|R8139_RCR_AM|R8139_RCR_AB;
    else {
        rcr |= R8139_RCR_APM;
        if (a->Filter & NDIS_PACKET_TYPE_BROADCAST) rcr |= R8139_RCR_AB;
        if (a->Filter & (NDIS_PACKET_TYPE_MULTICAST|NDIS_PACKET_TYPE_ALL_MULTICAST)) rcr |= R8139_RCR_AM;
    }
    out32(b+R8139_RCR, rcr);
}
static BOOLEAN r8139_init(PN31_ADAPTER a){
    ULONG b=a->IoBase; UINT i;
    if(!r8139_reset(a)) return FALSE;
    for(i=0;i<6;i++) a->Mac[i]=in8(b+R8139_IDR0+i);
    dbg("8139 MAC="); for(i=0;i<6;i++){dbgh(a->Mac[i]);} dbg("\n");
    out32(b+R8139_RBSTART, a->RxRingPhys);
    out32(b+R8139_TCR, R8139_TCR_MXDMA_2048|R8139_TCR_IFG_STD);
    r8139_setfilter(a);
    for(i=0;i<N31_TXN;i++) out32(b+R8139_TSAD0+i*4, a->TxPhys[i]);
    out16(b+R8139_CAPR, (USHORT)(0-16));
    RtlDmaBarrier();
    if(in32(b+R8139_RBSTART)==0xFFFFFFFFUL)return FALSE;
    out8(b+R8139_CR, R8139_CR_TE|R8139_CR_RE);
    out16(b+R8139_IMR, 0);             /* polled: no interrupts */
    out16(b+R8139_ISR, 0xFFFF);
    a->RxOffset=0; a->TxCur=0;
    a->LinkSpeed = (in8(b+R8139_MSR)&R8139_MSR_SPEED_10) ? 100000 : 1000000;
    return TRUE;
}
static VOID r8139_tx(PN31_ADAPTER a, UINT len){
    ULONG b=a->IoBase, slot=a->TxCur&(N31_TXN-1);
    if (len<60) len=60;
    out32(b+R8139_TSD0+slot*4, (R8139_TX_FIFO_THRESH<<11)|(len&R8139_TSD_SIZE_MASK));
    a->TxCur++;
}
static VOID r8139_poll_rx(PN31_ADAPTER a){
    ULONG b=a->IoBase;
    while(!(in8(b+R8139_CR)&R8139_CR_BUFE)){
        PUCHAR p=a->RxRing+a->RxOffset;
        USHORT status=(USHORT)(p[0]|(p[1]<<8));
        USHORT rxlen =(USHORT)(p[2]|(p[3]<<8));
        if(rxlen==0xFFF0) break;
        if((status&R8139_RX_ROK)&&rxlen>=ETHLEN+4&&rxlen<=MAXFRAME+4){
            UINT dlen=rxlen-4, i;
            if(dlen>MAXFRAME) dlen=MAXFRAME;
            for(i=0;i<dlen;i++) a->LastFrame[i]=p[4+i];
            a->LastLen=dlen;
            if(a->FilterDb){
                UINT look=dlen-ETHLEN;
                EthFilterIndicateReceive(a->FilterDb,(NDIS_HANDLE)a,
                    (PCHAR)a->LastFrame, a->LastFrame, ETHLEN,
                    a->LastFrame+ETHLEN, look, look);
            }
        }
        a->RxOffset=(a->RxOffset+rxlen+4+3)&~3UL;
        if(a->RxOffset>=R8139_RXBUF_LEN) a->RxOffset-=R8139_RXBUF_LEN;
        out16(b+R8139_CAPR,(USHORT)(a->RxOffset-16));
    }
    out16(b+R8139_ISR, in16(b+R8139_ISR));
}

/* ================= C+ backend (RTL8169 .. RTL8127) ================= */
/* RTL8125/8126/8127 relocate IMR/ISR/TPPOLL and keep the C+ datapath always
   on (no CPCR); treat that as the "8125+" family. */
static int cp_is25(PN31_ADAPTER a){
    USHORT d=a->DeviceId;
    return (d==0x8125||d==0x8126||d==0x8127||d==0x5000||d==0x3000||d==0x0e10);
}
static BOOLEAN cplus_reset(PN31_ADAPTER a){
    ULONG b=a->IoBase; UINT i;
    out8(b+RCP_CR, RCP_CR_RST);
    for(i=0;i<1000;i++){
        UCHAR command=in8(b+RCP_CR);
        if(command==0xFF)return FALSE;
        if(!(command&(RCP_CR_RST|RCP_CR_TE|RCP_CR_RE)))return TRUE;
        KeStallExecutionProcessor(10);
    }
    return FALSE;
}
static VOID cplus_setfilter(PN31_ADAPTER a){
    ULONG b=a->IoBase, rcr=RCP_RCR_MXDMA_UNL|RCP_RCR_RXFTH_UNL;
    if(a->Filter & NDIS_PACKET_TYPE_PROMISCUOUS)
        rcr|=RCP_RCR_AAP|RCP_RCR_APM|RCP_RCR_AM|RCP_RCR_AB|RCP_RCR_AR;
    else{
        rcr|=RCP_RCR_APM|RCP_RCR_AR;
        if(a->Filter & NDIS_PACKET_TYPE_BROADCAST) rcr|=RCP_RCR_AB;
        if(a->Filter & (NDIS_PACKET_TYPE_MULTICAST|NDIS_PACKET_TYPE_ALL_MULTICAST)) rcr|=RCP_RCR_AM;
    }
    out32(b+RCP_RCR, rcr);
    out32(b+RCP_MAR0, 0xFFFFFFFFUL); out32(b+RCP_MAR0+4, 0xFFFFFFFFUL);
}
static BOOLEAN cplus_init(PN31_ADAPTER a){
    ULONG b=a->IoBase; UINT i; ULONG ps;
    if(!a->PciHardwareInitialized&&!n31_mac_hardware(a,RtlMacHardwareInitialize))return FALSE;
    if(!cplus_reset(a)) return FALSE;
    if(!n31_mac_hardware(a,RtlMacHardwareRelease))return FALSE;
    if(!n31_mac_hardware(a,RtlMacHardwareAcquire))return FALSE;
    if(!n31_initialize_phy(a))return FALSE;
    for(i=0;i<6;i++) a->Mac[i]=in8(b+RCP_IDR0+i);
    dbg("CP id="); dbgh(a->DeviceId); dbg(" MAC=");
    for(i=0;i<6;i++) dbgh(a->Mac[i]);
    dbg("\n");
    out8(b+RCP_9346CR, 0xC0);                       /* unlock config/regs */
    for(i=0;i<RCP_NUM_TX_DESC;i++){
        a->TxDesc[i].opts1=(i==RCP_NUM_TX_DESC-1)?RCP_DESC_EOR:0;
        a->TxDesc[i].opts2=0; a->TxDesc[i].addr_lo=a->CpTxP[i]; a->TxDesc[i].addr_hi=0;
    }
    for(i=0;i<RCP_NUM_RX_DESC;i++){
        a->RxDesc[i].opts1=RCP_DESC_OWN|((i==RCP_NUM_RX_DESC-1)?RCP_DESC_EOR:0)|RCP_RX_BUF_SIZE;
        a->RxDesc[i].opts2=0; a->RxDesc[i].addr_lo=a->CpRxP[i]; a->RxDesc[i].addr_hi=0;
    }
    a->CpTxHead=0; a->CpRxCur=0;
    RtlDmaBarrier();
    /* Publish complete descriptor rings before allowing hardware DMA. */
    /* Preserve revision-specific CPlusCmd from the MAC source sequence. */
    out32(b+RCP_TNPDS, a->TxDescPhys); out32(b+RCP_TNPDS+4, 0);
    out32(b+RCP_RDSAR, a->RxDescPhys); out32(b+RCP_RDSAR+4, 0);
    out16(b+RCP_RMS, RCP_RX_BUF_SIZE);
    if(in32(b+RCP_RDSAR)==0xFFFFFFFFUL)return FALSE;
    out8(b+RCP_CR, RCP_CR_TE|RCP_CR_RE);
    out32(b+RCP_TCR, 0x03000700UL);                 /* IFG std + MXDMA unlimited */
    cplus_setfilter(a);
    out8(b+RCP_9346CR, 0x00);                       /* lock */
    if(cp_is25(a)){ out32(b+RCP_IMR0_8125,0); out32(b+RCP_ISR0_8125,0xFFFFFFFFUL); }
    else { out16(b+RCP_IMR,0); out16(b+RCP_ISR,0xFFFF); }
    /* PHYSTATUS low byte: 10/100/1000M; the 2.5/5/10G bits live in the high
       byte (read it separately - it is universal and also sidesteps the model's
       byte-only 0x6C read).  LinkSpeed is in 100 bps units. */
    ps=in8(b+RCP_PHYSTATUS);
    if(ps&RCP_PHYS_1000M) a->LinkSpeed=10000000UL;           /* 1 Gbps  */
    else if(ps&RCP_PHYS_100M) a->LinkSpeed=1000000UL;        /* 100M    */
    else a->LinkSpeed=100000UL;                              /* 10M     */
    if(cp_is25(a)){
        UCHAR hi=in8(b+RCP_PHYSTATUS+1);
        if(hi & (RCP_PHYS_10000M>>8)) a->LinkSpeed=100000000UL; /* 10 Gbps */
        else if(hi & (RCP_PHYS_5000M>>8)) a->LinkSpeed=50000000UL;  /* 5 Gbps */
        else if(hi & (RCP_PHYS_2500M>>8)) a->LinkSpeed=25000000UL;  /* 2.5Gbps */
    }
    dbg("CP speed="); dbgh(a->LinkSpeed); dbg("\n");
    return TRUE;
}
static VOID cplus_tx(PN31_ADAPTER a, UINT len){
    ULONG b=a->IoBase; UINT h=a->CpTxHead&(RCP_NUM_TX_DESC-1); ULONG o1;
    if(len<60) len=60;
    o1=RCP_DESC_OWN|RCP_DESC_FS|RCP_DESC_LS|((ULONG)len&RCP_DESC_FRAGLEN);
    if(h==RCP_NUM_TX_DESC-1) o1|=RCP_DESC_EOR;
    a->TxDesc[h].addr_lo=a->CpTxP[h]; a->TxDesc[h].addr_hi=0; a->TxDesc[h].opts2=0;
    RtlDmaBarrier();
    a->TxDesc[h].opts1=o1;
    a->CpTxHead++;
    RtlDmaBarrier();
    if(cp_is25(a)) out32(b+RCP_TPPOLL_8125, 1);
    else           out8(b+RCP_TPPOLL, RCP_TPPOLL_NPQ);
}
static VOID cplus_poll_rx(PN31_ADAPTER a){
    UINT guard;
    for(guard=0; guard<RCP_NUM_RX_DESC; guard++){
        UINT i=a->CpRxCur&(RCP_NUM_RX_DESC-1);
        ULONG o1=a->RxDesc[i].opts1;
        if(o1 & RCP_DESC_OWN) break;                /* still owned by NIC */
        RtlDmaBarrier();
        if(!(o1 & RCP_RDESC_RES) && (o1 & (RCP_DESC_FS|RCP_DESC_LS)) == (RCP_DESC_FS|RCP_DESC_LS)){
            UINT dlen=(UINT)(o1 & RCP_DESC_FRAGLEN), j;
            if(dlen>=ETHLEN+4 && dlen<=MAXFRAME+4){
                PUCHAR src=a->CpRx[i];
                dlen-=4;                            /* strip CRC */
                if(dlen>MAXFRAME) dlen=MAXFRAME;
                for(j=0;j<dlen;j++) a->LastFrame[j]=src[j];
                a->LastLen=dlen;
                if(a->FilterDb){
                    UINT look=dlen-ETHLEN;
                    EthFilterIndicateReceive(a->FilterDb,(NDIS_HANDLE)a,
                        (PCHAR)a->LastFrame, a->LastFrame, ETHLEN,
                        a->LastFrame+ETHLEN, look, look);
                }
            }
        }
        RtlDmaBarrier();
        a->RxDesc[i].opts1=RCP_DESC_OWN|((i==RCP_NUM_RX_DESC-1)?RCP_DESC_EOR:0)|RCP_RX_BUF_SIZE;
        a->CpRxCur++;
    }
}

/* ================= NE2000 backend (RTL8029) ======================== */
/* DP8390 core with 16 KB on-chip buffer reached by remote DMA (I/O data
   port).  Programmed I/O only - no host descriptor ring. */
static VOID ne_dma_read(PN31_ADAPTER a, UINT addr, PUCHAR dst, UINT len){
    ULONG b=a->IoBase; UINT i;
    out8(b+NE_CR, NE_CR_PAGE0|NE_CR_RD2|NE_CR_STA);
    out8(b+NE_P0_ISR, NE_ISR_RDC);
    out8(b+NE_P0_RBCR0, len&0xFF); out8(b+NE_P0_RBCR1, (len>>8)&0xFF);
    out8(b+NE_P0_RSAR0, addr&0xFF); out8(b+NE_P0_RSAR1, (addr>>8)&0xFF);
    out8(b+NE_CR, NE_CR_PAGE0|NE_CR_RD0|NE_CR_STA); /* remote read */
    for(i=0;i+1<len;i+=2){ USHORT w=in16(b+NE_DATA); dst[i]=(UCHAR)w; dst[i+1]=(UCHAR)(w>>8); }
    if(i<len){ USHORT w=in16(b+NE_DATA); dst[i]=(UCHAR)w; }
}
static VOID ne_dma_write(PN31_ADAPTER a, UINT addr, PUCHAR src, UINT len){
    ULONG b=a->IoBase; UINT i;
    out8(b+NE_CR, NE_CR_PAGE0|NE_CR_RD2|NE_CR_STA);
    out8(b+NE_P0_ISR, NE_ISR_RDC);
    out8(b+NE_P0_RBCR0, len&0xFF); out8(b+NE_P0_RBCR1, (len>>8)&0xFF);
    out8(b+NE_P0_RSAR0, addr&0xFF); out8(b+NE_P0_RSAR1, (addr>>8)&0xFF);
    out8(b+NE_CR, NE_CR_PAGE0|NE_CR_RD1|NE_CR_STA); /* remote write */
    for(i=0;i+1<len;i+=2){ out16(b+NE_DATA,(USHORT)(src[i]|(src[i+1]<<8))); }
    if(i<len){ out16(b+NE_DATA,(USHORT)src[i]); }
    for(i=0;i<1000;i++){ if(in8(b+NE_P0_ISR)&NE_ISR_RDC) break; stall(2); }
}
static VOID ne_setfilter(PN31_ADAPTER a){
    ULONG b=a->IoBase; UCHAR rcr=0;
    if(a->Filter & NDIS_PACKET_TYPE_PROMISCUOUS) rcr|=NE_RCR_PRO|NE_RCR_AB|NE_RCR_AM;
    else{
        if(a->Filter & NDIS_PACKET_TYPE_BROADCAST) rcr|=NE_RCR_AB;
        if(a->Filter & (NDIS_PACKET_TYPE_MULTICAST|NDIS_PACKET_TYPE_ALL_MULTICAST)) rcr|=NE_RCR_AM;
    }
    out8(b+NE_CR, NE_CR_PAGE0|NE_CR_RD2|NE_CR_STA);
    out8(b+NE_P0_RCR, rcr);
}
static BOOLEAN ne_init(PN31_ADAPTER a){
    ULONG b=a->IoBase; UINT i; UCHAR prom[32], v;
    v=in8(b+NE_RESET); out8(b+NE_RESET, v);          /* read-then-write reset */
    for(i=0;i<1000;i++){
        v=in8(b+NE_P0_ISR);
        if(v==0xFF) return FALSE;
        if(v&NE_ISR_RST) break;
        stall(5);
    }
    if(i==1000) return FALSE;
    out8(b+NE_CR, NE_CR_PAGE0|NE_CR_RD2|NE_CR_STP);
    out8(b+NE_P0_DCR, NE_DCR_DEFAULT);
    out8(b+NE_P0_RBCR0,0); out8(b+NE_P0_RBCR1,0);
    out8(b+NE_P0_RCR, 0x20);                          /* monitor during init */
    out8(b+NE_P0_TCR, NE_TCR_LOOPBACK);
    out8(b+NE_P0_TPSR, NE_TX_START);
    out8(b+NE_P0_PSTART, NE_RX_START); out8(b+NE_P0_BNRY, NE_RX_START);
    out8(b+NE_P0_PSTOP, NE_RX_STOP);
    out8(b+NE_P0_ISR, 0xFF); out8(b+NE_P0_IMR, 0);
    ne_dma_read(a, 0, prom, 32);                      /* MAC in PROM (word-doubled) */
    for(i=0;i<6;i++) a->Mac[i]=prom[i*2];
    dbg("NE MAC="); for(i=0;i<6;i++) dbgh(a->Mac[i]);
    dbg("\n");
    out8(b+NE_CR, NE_CR_PAGE1|NE_CR_RD2|NE_CR_STP);
    for(i=0;i<6;i++) out8(b+NE_P1_PAR0+i, a->Mac[i]);
    for(i=0;i<8;i++) out8(b+NE_P1_MAR0+i, 0xFF);
    out8(b+NE_P1_CURR, NE_RX_START+1);
    a->NeCurr=NE_RX_START+1;
    out8(b+NE_CR, NE_CR_PAGE0|NE_CR_RD2|NE_CR_STA);
    out8(b+NE_P0_TCR, 0);
    ne_setfilter(a);
    a->LinkSpeed=100000UL;                            /* NE2000 = 10 Mbps */
    return TRUE;
}
static VOID ne_tx(PN31_ADAPTER a, UINT len){
    ULONG b=a->IoBase;
    if(len<60) len=60;
    ne_dma_write(a, NE_TX_START*NE_PAGE_SIZE, a->TxBuf[0], len);
    out8(b+NE_CR, NE_CR_PAGE0|NE_CR_RD2|NE_CR_STA);
    out8(b+NE_P0_TPSR, NE_TX_START);
    out8(b+NE_P0_TBCR0, len&0xFF); out8(b+NE_P0_TBCR1,(len>>8)&0xFF);
    out8(b+NE_CR, NE_CR_PAGE0|NE_CR_RD2|NE_CR_TXP|NE_CR_STA);
}
static VOID ne_poll_rx(PN31_ADAPTER a){
    ULONG b=a->IoBase; UCHAR curr; UINT guard;
    for(guard=0; guard<64; guard++){
        UCHAR hdr[4], bnry; UINT next, cnt;
        out8(b+NE_CR, NE_CR_PAGE1|NE_CR_RD2|NE_CR_STA);
        curr=in8(b+NE_P1_CURR);
        out8(b+NE_CR, NE_CR_PAGE0|NE_CR_RD2|NE_CR_STA);
        if(a->NeCurr==curr) break;                    /* ring empty */
        ne_dma_read(a, a->NeCurr*NE_PAGE_SIZE, hdr, 4);
        next=hdr[1]; cnt=hdr[2]|(hdr[3]<<8);           /* length incl 4-byte hdr */
        if((hdr[0]&0x01) && cnt>=ETHLEN+4 && cnt<=MAXFRAME+8){
            UINT dlen=cnt-4;
            if(dlen>MAXFRAME) dlen=MAXFRAME;
            ne_dma_read(a, a->NeCurr*NE_PAGE_SIZE+4, a->LastFrame, dlen);
            a->LastLen=dlen;
            if(a->FilterDb){
                UINT look=dlen-ETHLEN;
                EthFilterIndicateReceive(a->FilterDb,(NDIS_HANDLE)a,
                    (PCHAR)a->LastFrame, a->LastFrame, ETHLEN,
                    a->LastFrame+ETHLEN, look, look);
            }
        }
        if(next<NE_RX_START || next>=NE_RX_STOP) next=NE_RX_START;  /* sanity */
        a->NeCurr=(UCHAR)next;
        bnry=(UCHAR)((next==NE_RX_START)?(NE_RX_STOP-1):(next-1));
        out8(b+NE_P0_BNRY, bnry);
    }
    out8(b+NE_P0_ISR, in8(b+NE_P0_ISR));
}

/* ================= backend dispatch ================================ */
static BOOLEAN hw_init(PN31_ADAPTER a){
    switch(a->Backend){
    case BK_USB: return TRUE;
    case BK_8139:  return r8139_init(a);
    case BK_CPLUS: return cplus_init(a);
    default:       return ne_init(a);
    }
}
static BOOLEAN n31_stop_hw(PN31_ADAPTER a){
    if(a->UsbContext){N31UsbReset(a->UsbContext);return TRUE;}
    if(a->Backend==BK_8139||a->Backend==BK_CPLUS){
        if(a->Backend==BK_CPLUS && cp_is25(a)) out32(a->IoBase+RCP_IMR0_8125,0);
        else out16(a->IoBase+R8139_IMR,0);
        if(a->Backend==BK_CPLUS){
            if(!cplus_reset(a))return FALSE;
            /* Poll/send/recovery owners are drained by the passive caller.
               Management failure cannot invalidate acknowledged DMA stop. */
            (void)n31_mac_hardware(a,RtlMacHardwareRelease);return TRUE;
        }
        return r8139_reset(a);
    }else{
        out8(a->IoBase+NE_CR,NE_CR_PAGE0|NE_CR_RD2|NE_CR_STP);
        out8(a->IoBase+NE_P0_IMR,0);
    }
    return TRUE;
}
static VOID hw_setfilter(PN31_ADAPTER a){
    switch(a->Backend){
    case BK_USB: N31UsbFilter(a->UsbContext);break;
    case BK_8139:  r8139_setfilter(a); break;
    case BK_CPLUS: cplus_setfilter(a); break;
    default:       ne_setfilter(a); break;
    }
}
static VOID hw_tx(PN31_ADAPTER a, UINT len){
    switch(a->Backend){
    case BK_8139:  r8139_tx(a,len); break;
    case BK_CPLUS: cplus_tx(a,len); break;
    default:       ne_tx(a,len); break;
    }
}
/* current transmit buffer for this backend (n31_send copies the frame here) */
static PUCHAR hw_txbuf(PN31_ADAPTER a){
    switch(a->Backend){
    case BK_CPLUS: {
        UINT slot=a->CpTxHead&(RCP_NUM_TX_DESC-1);
        if(a->TxDesc[slot].opts1&RCP_DESC_OWN)return NULL;
        RtlDmaBarrier();
        return a->CpTx[slot];
    }
    default:       return a->TxBuf[a->TxCur&(N31_TXN-1)];  /* 8139 + NE2000 */
    }
}
static VOID hw_poll(PN31_ADAPTER a){
    switch(a->Backend){
    case BK_USB:
        N31UsbPoll(a->UsbContext);
        a->UsbLinkUp=N31UsbLink(a->UsbContext,&a->LinkSpeed);break;
    case BK_8139:  r8139_poll_rx(a); break;
    case BK_CPLUS: cplus_poll_rx(a); break;
    default:       ne_poll_rx(a); break;
    }
}

VOID N31UsbReceive(PVOID owner,PUCHAR frame,UINT length)
{
    PN31_ADAPTER a=owner;UINT i;
    if(length<ETHLEN||length>MAXFRAME)return;
    for(i=0;i<length;i++)a->LastFrame[i]=frame[i];
    a->LastLen=length;
    if(a->FilterDb)EthFilterIndicateReceive(a->FilterDb,(NDIS_HANDLE)a,
        (PCHAR)a->LastFrame,a->LastFrame,ETHLEN,a->LastFrame+ETHLEN,length-ETHLEN,length-ETHLEN);
}
VOID N31UsbReceiveComplete(PVOID owner){(void)owner;/* shared MAC poll completes the batch */}

/* ================= interrupt (NE2000-style MAC model) ============== */
/* ISR: DIRQL.  Return TRUE if the interrupt is ours; mask the NIC so the
   DPC can process it.  NT 3.1 calls this with the context as the 1st arg. */
static BOOLEAN STDC n31_isr(PVOID ctx){
    PN31_ADAPTER a=(PN31_ADAPTER)ctx;
    USHORT isr;
    if(a->Backend==BK_8139||a->Backend==BK_CPLUS){
        isr=in16(a->IoBase+R8139_ISR);
        if(!isr) return FALSE;                 /* not ours */
        out16(a->IoBase+R8139_IMR, 0);         /* mask; DPC re-enables */
        return TRUE;
    }
    return TRUE;
}
/* RX poll - runs as the NDIS timer function at DISPATCH_LEVEL and re-arms
   itself.  The NDIS timer callback is (SystemSpecific1, FunctionContext,
   SystemSpecific2, SystemSpecific3). The shared pool list supplies every
   adapter independently of callback argument order. The callback itself
   executes from the resident pool image at DISPATCH_LEVEL. */
static VOID STDC n31_dpc(PVOID,PVOID,PVOID,PVOID) __attribute__((used,noinline));
static VOID STDC n31_dpc(PVOID s1, PVOID s2, PVOID s3, PVOID s4){
    UINT i;KIRQL old;
    (void)s1;(void)s2;(void)s3;(void)s4;
    /* One timer services every card: poll each running adapter in turn.
       The adapter is read from the shared g_ap[] list, not the (unreliable
       on NT 3.1) timer argument. */
    for(i=0;i<N31_MAX;i++){
        PN31_ADAPTER a;
        /* Acquire the adapter reference before removal can delist/free it. */
        KeAcquireSpinLock(&g_poll->Lock,&old);
        a=g_ap[i];
        if(a&&!n31_poll_acquire(a))a=NULL;
        KeReleaseSpinLock(&g_poll->Lock,old);
        if(!a)continue;
        hw_poll(a);
        if(a->FilterDb) EthFilterIndicateReceiveComplete(a->FilterDb);
        n31_poll_release(a);
    }
    KeAcquireSpinLock(&g_poll->Lock,&old);
    if(*g_apN&&!g_poll->Stopping){n31_image_get();NdisSetTimer(&g_poll->Timer,3);}
    else{
        g_poll->Armed=FALSE;
        if(g_poll->Stopping)KeSetEvent(g_poll->Drained,0,FALSE);
    }
    KeReleaseSpinLock(&g_poll->Lock,old);
}
#if RTL_NT31_RX_THREAD
/* PASSIVE-level RX poll thread (historical fallback, superseded by the packed
   image + NDIS timer path).  Kept behind RTL_NT31_RX_THREAD for reference. */
static VOID STDC poll_thread(PVOID ctx){
    PN31_ADAPTER a=(PN31_ADAPTER)ctx;
    LARGE_INT delay;
    ULONG tick=0;
    delay.LowPart=(ULONG)(0-50000); delay.HighPart=-1;   /* 5 ms, relative */
    dbg("poll thread up\n");
    while(a->Running){
        KIRQL old;
        KeRaiseIrql(DISPATCH_LEVEL,&old);
        hw_poll(a);
        if(a->FilterDb) EthFilterIndicateReceiveComplete(a->FilterDb);
        KeLowerIrql(old);
        if((++tick % 400)==0) dbg("p");   /* ~0.5 Hz heartbeat */
        KeDelayExecutionThread(0,FALSE,&delay);
    }
    PsTerminateSystemThread(0);
}
#endif

/* ================= Eth filter callbacks ============================ */
static NDIS_STATUS STDC filt_change(UINT oldc, UINT newc, NDIS_HANDLE bind, PNDIS_REQUEST req, BOOLEAN set){
    PN31_ADAPTER a=((PN31_BINDING)bind)->Adapter;
    (void)oldc;(void)req;(void)set;
    a->Filter = newc;
    if(!a->Resetting&&!a->ResetFailed&&!a->RecoveryStop)hw_setfilter(a);
    dbg("filt_change "); dbgh(newc); dbg("\n");
    return NDIS_STATUS_SUCCESS;
}
static NDIS_STATUS STDC addr_change(UINT oc, PVOID oa, UINT nc, PVOID na, NDIS_HANDLE bind, PNDIS_REQUEST req, BOOLEAN set){
    (void)oc;(void)oa;(void)nc;(void)na;(void)bind;(void)req;(void)set;
    return NDIS_STATUS_SUCCESS;      /* multicast list accepted (no HW filter) */
}
static VOID n31_filter_lock(PN31_ADAPTER a){
    KIRQL old;KeAcquireSpinLock(&a->Lock.SpinLock,&old);a->Lock.OldIrql=old;
}
static VOID n31_filter_unlock(PN31_ADAPTER a){
    KeReleaseSpinLock(&a->Lock.SpinLock,(KIRQL)a->Lock.OldIrql);
}
static VOID STDC close_action(NDIS_HANDLE bind){
    PN31_BINDING b=bind;PN31_ADAPTER a=b->Adapter;NDIS_HANDLE ndis=b->NdisBinding;
    /* Eth calls this with its lock held after the final receive reference.
       Drop that lock before calling a protocol completion, then restore it. */
    NdisFreeMemory(b,sizeof(*b),0);
    n31_filter_unlock(a);
    NdisCompleteCloseAdapter(ndis,NDIS_STATUS_SUCCESS);
    n31_filter_lock(a);
}

/* ================= MAC handlers ==================================== */
static NDIS_STATUS STDC
n31_open(PNDIS_STATUS err, NDIS_HANDLE *macBind, PUINT medium, NDIS_MEDIUM *arr,
         UINT arrsz, NDIS_HANDLE ndisBind, NDIS_HANDLE adaptCtx, UINT opts, PVOID addr){
    PN31_ADAPTER a=(PN31_ADAPTER)adaptCtx;   /* MacAdapterContext == the adapter */
    UINT i;PN31_BINDING b;NDIS_PHYSICAL_ADDRESS max={0xffffffffUL,0};
    (void)opts;(void)addr;
    dbg("OPN arrsz="); dbgh(arrsz); dbg("\n");
    for(i=0;i<arrsz;i++) if(arr[i]==NdisMedium802_3){ *medium=i; break; }
    if(i==arrsz){ *err=NDIS_STATUS_FAILURE; return NDIS_STATUS_FAILURE; }
    *macBind=NULL;
    if(NdisAllocateMemory((PVOID*)&b,sizeof(*b),0,max)!=NDIS_STATUS_SUCCESS){
        *err=NDIS_STATUS_RESOURCES;return NDIS_STATUS_RESOURCES;
    }
    b->Adapter=a;b->NdisBinding=ndisBind;b->FilterHandle=NULL;
    b->PacketFilter=0;b->Lookahead=1500;b->MulticastBytes=0;
    b->ResetReferences=0;b->Closing=FALSE;
    n31_filter_lock(a);
    if(a->Resetting||a->RecoveryStop){
        n31_filter_unlock(a);NdisFreeMemory(b,sizeof(*b),0);
        *err=(NDIS_STATUS)0xC001000DUL;return *err;
    }
    if(!EthNoteFilterOpenAdapter(a->FilterDb,b,ndisBind,&b->FilterHandle)){
        n31_filter_unlock(a);NdisFreeMemory(b,sizeof(*b),0);
        dbg("EthNoteFilterOpen FAIL\n"); *err=NDIS_STATUS_RESOURCES;return NDIS_STATUS_RESOURCES;
    }
    b->Next=a->Bindings;a->Bindings=b;
    n31_filter_unlock(a);
    *macBind=b;
    *err=NDIS_STATUS_SUCCESS;
    dbg("open ok\n");
    return NDIS_STATUS_SUCCESS;
}
static NDIS_STATUS STDC n31_close(NDIS_HANDLE macBind){
    PN31_BINDING b=macBind;PN31_ADAPTER a=b->Adapter;NDIS_STATUS status;
    PN31_BINDING *link;
    n31_filter_lock(a);
    if(b->ResetReferences){b->Closing=TRUE;n31_filter_unlock(a);return NDIS_STATUS_PENDING;}
    for(link=&a->Bindings;*link&&*link!=b;link=&(*link)->Next);
    if(*link==b)*link=b->Next;
    status=EthDeleteFilterOpenAdapter(a->FilterDb,b->FilterHandle,NULL);
    if(status!=NDIS_STATUS_SUCCESS&&status!=NDIS_STATUS_CLOSING_INDICATING){b->Next=a->Bindings;a->Bindings=b;}
    n31_filter_unlock(a);
    if(status==NDIS_STATUS_CLOSING_INDICATING)return NDIS_STATUS_PENDING;
    if(status==NDIS_STATUS_SUCCESS)NdisFreeMemory(b,sizeof(*b),0);
    return status;
}
static NDIS_STATUS STDC n31_send(NDIS_HANDLE macBind, PNDIS_PACKET pkt){
    PN31_ADAPTER a=((PN31_BINDING)macBind)->Adapter;
    UINT copied=0;UCHAR frame[MAXFRAME];PUCHAR dst;KIRQL old;
    PNDIS_BUFFER buf=pkt->Private.Head;
    /* Reject malformed/oversized packets instead of sending a truncated frame.
       Copy before touching DMA buffers or the passive USB worker's queue. */
    while(buf){
        PVOID va=NULL;UINT len=0,i;
        NdisQueryBuffer(buf,&va,&len);
        if(len>MAXFRAME-copied||(!va&&len))return NDIS_STATUS_FAILURE;
        for(i=0;i<len;i++)frame[copied+i]=((PUCHAR)va)[i];
        copied+=len;buf=buf->Next;
    }
    if(copied<ETHLEN)return NDIS_STATUS_FAILURE;
    while(copied<60)frame[copied++]=0;
    if(a->UsbContext)return N31UsbSend(a->UsbContext,frame,copied)?NDIS_STATUS_SUCCESS:NDIS_STATUS_RESOURCES;
    KeAcquireSpinLock(&a->TxLock,&old);
    if(a->Resetting||a->ResetFailed||a->RecoveryStop){KeReleaseSpinLock(&a->TxLock,old);return NDIS_STATUS_RESOURCES;}
    dst=hw_txbuf(a);
    if(!dst){KeReleaseSpinLock(&a->TxLock,old);return NDIS_STATUS_RESOURCES;}
    {UINT i;for(i=0;i<copied;i++)dst[i]=frame[i];}
    hw_tx(a,copied);
    KeReleaseSpinLock(&a->TxLock,old);return NDIS_STATUS_SUCCESS;
}
static NDIS_STATUS STDC
n31_transfer(NDIS_HANDLE macBind, NDIS_HANDLE ctx, UINT off, UINT want,
             PNDIS_PACKET pkt, PUINT copied){
    PN31_ADAPTER a=((PN31_BINDING)macBind)->Adapter; UINT done=0; PNDIS_BUFFER buf=pkt->Private.Head;
    PUCHAR src=a->LastFrame+ETHLEN;    /* data (after header) */
    UINT avail=(a->LastLen>ETHLEN)?a->LastLen-ETHLEN:0;
    (void)ctx;
    if(off>avail) off=avail;
    if(want>avail-off) want=avail-off;
    while(buf && done<want){
        PVOID va=NULL; UINT len=0, i;
        NdisQueryBuffer(buf,&va,&len);
        if(done+len>want) len=want-done;
        for(i=0;i<len;i++) ((PUCHAR)va)[i]=src[off+done+i];
        done+=len; buf=buf->Next;
    }
    *copied=done;
    return NDIS_STATUS_SUCCESS;
}
static NDIS_STATUS STDC n31_reset(NDIS_HANDLE macBind){
    PN31_ADAPTER a=((PN31_BINDING)macBind)->Adapter;
    if(a->UsbContext){N31UsbReset(a->UsbContext);return NDIS_STATUS_SUCCESS;}
    return n31_queue_reset(macBind);
}

/* ---- OID request ------------------------------------------------------ */
static NDIS_STATUS put(PNDIS_REQUEST r, const void*src, UINT len){
    UINT avail=r->DATA.QUERY_INFORMATION.InformationBufferLength; UINT i;
    PUCHAR dst=(PUCHAR)r->DATA.QUERY_INFORMATION.InformationBuffer;
    if(avail<len){ r->DATA.QUERY_INFORMATION.BytesNeeded=len; return NDIS_STATUS_BUFFER_TOO_SHORT; }
    for(i=0;i<len;i++) dst[i]=((const UCHAR*)src)[i];
    r->DATA.QUERY_INFORMATION.BytesWritten=len;
    return NDIS_STATUS_SUCCESS;
}
static NDIS_STATUS STDC n31_request(NDIS_HANDLE macBind, PNDIS_REQUEST r){
    PN31_BINDING b=macBind;PN31_ADAPTER a=b->Adapter;
    if(r->RequestType==NdisRequestQueryInformation){
        NDIS_OID oid=r->DATA.QUERY_INFORMATION.Oid; ULONG v;
        switch(oid){
        case OID_802_3_PERMANENT_ADDRESS:
        case OID_802_3_CURRENT_ADDRESS:   return put(r,a->Mac,6);
        case OID_GEN_MEDIA_SUPPORTED:
        case OID_GEN_MEDIA_IN_USE:        v=NdisMedium802_3; return put(r,&v,4);
        case OID_GEN_MAXIMUM_FRAME_SIZE:  v=1500; return put(r,&v,4);
        case OID_GEN_MAXIMUM_TOTAL_SIZE:  v=1514; return put(r,&v,4);
        case OID_GEN_MAXIMUM_LOOKAHEAD:   v=1500; return put(r,&v,4);
        case OID_GEN_CURRENT_LOOKAHEAD:   v=b->Lookahead;return put(r,&v,4);
        case OID_GEN_LINK_SPEED:          v=a->LinkSpeed; return put(r,&v,4);
        case OID_GEN_TRANSMIT_BUFFER_SPACE: v=N31_TXN*2048; return put(r,&v,4);
        case OID_GEN_RECEIVE_BUFFER_SPACE:  v=R8139_RXBUF_LEN; return put(r,&v,4);
        case OID_GEN_TRANSMIT_BLOCK_SIZE:
        case OID_GEN_RECEIVE_BLOCK_SIZE:  v=1514; return put(r,&v,4);
        case OID_GEN_VENDOR_ID:           v=RtlNdisVendorId(a->Mac); return put(r,&v,4);
        case OID_GEN_VENDOR_DESCRIPTION:{
            char usbName[80];const char *nm;
            if(a->UsbContext){N31UsbName(a->UsbContext,usbName,sizeof(usbName));nm=usbName;}
            else nm=rtl_name_for(a->DeviceId);
            return put(r,nm,rtl_strlen(nm)+1); }
        case OID_GEN_DRIVER_VERSION:      v=0x0300; return put(r,&v,2);
        case OID_GEN_HARDWARE_STATUS:     v=0; return put(r,&v,4); /* Ready */
        case OID_GEN_MEDIA_CONNECT_STATUS: v=a->UsbContext&&!a->UsbLinkUp?1:0; return put(r,&v,4);/* Connected */
        case OID_GEN_CURRENT_PACKET_FILTER: v=b->PacketFilter; return put(r,&v,4);
        case OID_802_3_MULTICAST_LIST: return put(r,b->Multicast,b->MulticastBytes);
        case OID_GEN_MAC_OPTIONS:
            v=NDIS_MAC_OPTION_TRANSFERS_NOT_PEND|NDIS_MAC_OPTION_RECEIVE_SERIALIZED|NDIS_MAC_OPTION_NO_LOOPBACK;
            return put(r,&v,4);
        case OID_802_3_MAXIMUM_LIST_SIZE: v=16; return put(r,&v,4);
        case OID_GEN_SUPPORTED_LIST: {
            static const ULONG oids[]={
                OID_GEN_SUPPORTED_LIST,OID_GEN_HARDWARE_STATUS,OID_GEN_MEDIA_SUPPORTED,
                OID_GEN_MEDIA_IN_USE,OID_GEN_MAXIMUM_LOOKAHEAD,OID_GEN_MAXIMUM_FRAME_SIZE,
                OID_GEN_LINK_SPEED,OID_GEN_TRANSMIT_BUFFER_SPACE,OID_GEN_RECEIVE_BUFFER_SPACE,
                OID_GEN_TRANSMIT_BLOCK_SIZE,OID_GEN_RECEIVE_BLOCK_SIZE,OID_GEN_VENDOR_ID,
                OID_GEN_VENDOR_DESCRIPTION,OID_GEN_CURRENT_PACKET_FILTER,OID_GEN_CURRENT_LOOKAHEAD,
                OID_GEN_DRIVER_VERSION,OID_GEN_MAXIMUM_TOTAL_SIZE,OID_GEN_MAC_OPTIONS,
                OID_GEN_MEDIA_CONNECT_STATUS,OID_802_3_PERMANENT_ADDRESS,OID_802_3_CURRENT_ADDRESS,
                OID_802_3_MULTICAST_LIST,OID_802_3_MAXIMUM_LIST_SIZE};
            return put(r,oids,sizeof(oids)); }
        default:
            r->DATA.QUERY_INFORMATION.BytesWritten=0;
            return NDIS_STATUS_INVALID_OID;
        }
    } else if(r->RequestType==NdisRequestSetInformation){
        NDIS_OID oid=r->DATA.SET_INFORMATION.Oid;
        ULONG v=0; UINT i, l=r->DATA.SET_INFORMATION.InformationBufferLength;
        PUCHAR p=(PUCHAR)r->DATA.SET_INFORMATION.InformationBuffer;
        r->DATA.SET_INFORMATION.BytesRead=0;r->DATA.SET_INFORMATION.BytesNeeded=0;
        if(l>=4) for(i=0;i<4;i++) v|=((ULONG)p[i])<<(i*8);
        switch(oid){
        case OID_GEN_CURRENT_PACKET_FILTER:
            /* Route through the Eth filter package so the per-binding filter in
               the filter DB is updated; otherwise EthFilterIndicateReceive drops
               every frame for this binding (it sees an accept-nothing filter).
               EthFilterAdjust invokes filt_change, which sets the HW filter. */
            if(l!=4){r->DATA.SET_INFORMATION.BytesNeeded=4;return NDIS_STATUS_INVALID_LENGTH;}
            if(v&~(NDIS_PACKET_TYPE_DIRECTED|NDIS_PACKET_TYPE_MULTICAST|
                   NDIS_PACKET_TYPE_ALL_MULTICAST|NDIS_PACKET_TYPE_BROADCAST|
                   NDIS_PACKET_TYPE_PROMISCUOUS))return NDIS_STATUS_NOT_SUPPORTED;
            {NDIS_STATUS status;n31_filter_lock(a);
             status=EthFilterAdjust(a->FilterDb,b->FilterHandle,r,v,TRUE);
             n31_filter_unlock(a);
             if(status==NDIS_STATUS_SUCCESS){b->PacketFilter=v;r->DATA.SET_INFORMATION.BytesRead=4;}
             return status;}
        case OID_GEN_CURRENT_LOOKAHEAD:
            if(l!=4){r->DATA.SET_INFORMATION.BytesNeeded=4;return NDIS_STATUS_INVALID_LENGTH;}
            if(v>1500)return NDIS_STATUS_INVALID_LENGTH;
            b->Lookahead=v;r->DATA.SET_INFORMATION.BytesRead=4;return NDIS_STATUS_SUCCESS;
        case OID_802_3_MULTICAST_LIST:
            if(l%6||l>16*6)return NDIS_STATUS_INVALID_LENGTH;
            {NDIS_STATUS status;n31_filter_lock(a);
             status=EthChangeFilterAddresses(a->FilterDb,b->FilterHandle,r,l/6,p,TRUE);
             n31_filter_unlock(a);
             if(status==NDIS_STATUS_SUCCESS){
                 for(i=0;i<l;i++)b->Multicast[i]=p[i];
                 b->MulticastBytes=l;r->DATA.SET_INFORMATION.BytesRead=l;
             }
             return status;}
        default:
            return NDIS_STATUS_INVALID_OID;
        }
    }
    return NDIS_STATUS_NOT_SUPPORTED;
}
static NDIS_STATUS STDC n31_qgs(NDIS_HANDLE adapt, PNDIS_REQUEST r){
    /* global statistics: report zeros */
    PN31_ADAPTER a=(PN31_ADAPTER)adapt; ULONG z=0; (void)a;
    if(r->RequestType==NdisRequestQueryStatistics||r->RequestType==NdisRequestQueryInformation)
        return put(r,&z,4);
    return NDIS_STATUS_SUCCESS;
}
static NDIS_STATUS STDC n31_qps(NDIS_HANDLE adapt, NDIS_HANDLE open, PVOID req){ (void)adapt;(void)open;(void)req; return NDIS_STATUS_SUCCESS; }
static VOID STDC n31_unload(NDIS_HANDLE mac)
{
    KIRQL old;BOOLEAN armed,cancelled;N31_POLL_SET *poll;
    (void)mac;if(!g_poll)return;
    KeAcquireSpinLock(&g_poll->Lock,&old);
    if(*g_apN){KeReleaseSpinLock(&g_poll->Lock,old);return;}
    g_poll->Stopping=TRUE;armed=g_poll->Armed;
    KeReleaseSpinLock(&g_poll->Lock,old);
    if(armed){
        /* NT3.1 NDIS exports no NdisCancelTimer. Its actual initializer puts
           KTIMER at offset zero and KDPC at +0x28; cancel the embedded timer. */
        cancelled=KeCancelTimer(&g_poll->Timer);
        if(cancelled)n31_image_cancel(); /* cancelled queued timer reference */
        else KeWaitForSingleObject(g_poll->Drained,0,0,FALSE,NULL);
    }
    /* The draining DPC signals under this lock. Join its final unlock too,
       before freeing the event and lock storage that it just touched. */
    KeAcquireSpinLock(&g_poll->Lock,&old);g_poll->Armed=FALSE;
    KeReleaseSpinLock(&g_poll->Lock,old);
    poll=g_poll;g_poll=NULL;
    MmFreeContiguousMemory(g_ap);g_ap=NULL;
    MmFreeContiguousMemory(g_apN);g_apN=NULL;
    MmFreeContiguousMemory(poll);
    /* The outer unload wrapper and pending callback epilogues still own
       image references. Their final kernel tail call schedules reclamation. */
}

static BOOLEAN n31_publish(PN31_ADAPTER a)
{
    KIRQL old;UINT i;
    KeAcquireSpinLock(&g_poll->Lock,&old);
    for(i=0;i<N31_MAX&&g_ap[i];i++);
    if(i==N31_MAX||g_poll->Stopping){
        KeReleaseSpinLock(&g_poll->Lock,old);return FALSE;
    }
    a->Running=TRUE;g_ap[i]=a;(*g_apN)++;
    if(!g_poll->Armed){g_poll->Armed=TRUE;n31_image_get();NdisSetTimer(&g_poll->Timer,100);}
    KeReleaseSpinLock(&g_poll->Lock,old);
    return TRUE;
}

#include "nt31recovery.h"

/* ================= Add / Remove adapter ============================ */
static NDIS_STATUS STDC n31_add(NDIS_HANDLE mac, NDIS_HANDLE cfg, PNDIS_STRING name){
    PN31_ADAPTER a; NDIS_STATUS s; UINT i; ULONG dummy; PUCHAR p;KIRQL old;
    BOOLEAN registered=FALSE, hardwareAttempted=FALSE, dmaStopped=TRUE;int usb;
    ULONG info[10];                 /* NDIS_ADAPTER_INFORMATION (40 bytes) */
    dbg("ADD begin\n");
    KeAcquireSpinLock(&g_poll->Lock,&old);
    i=*g_apN;KeReleaseSpinLock(&g_poll->Lock,old);
    if(i>=N31_MAX){ dbg("ADD: too many adapters\n"); return NDIS_STATUS_RESOURCES; }
    /* one non-paged adapter per card (see g_ap comment) */
    a=(N31_ADAPTER*)dma_alloc(sizeof(N31_ADAPTER),&dummy);
    if(!a){ dbg("adapter alloc fail\n"); return NDIS_STATUS_RESOURCES; }
    p=(PUCHAR)a; for(i=0;i<sizeof(N31_ADAPTER);i++) p[i]=0;
    KeInitializeEvent(a->PollDrained,0,FALSE);
    a->Lock.SpinLock=0; a->Lock.OldIrql=0;
    a->MacHandle=mac;

    s=NDIS_STATUS_FAILURE;
    /* The bridge lives in the resident image, like its worker callbacks. */
    usb=((int(*)(PVOID,PVOID,PVOID*,PUCHAR))cp(N31UsbCreate))(cfg,a,&a->UsbContext,a->Mac);
    if(usb<0)goto fail;
    if(usb){a->Backend=BK_USB;goto register_adapter;}
    if(!pci_find(a)) goto fail;
    s=NDIS_STATUS_RESOURCES;

    /* DMA buffers (contiguous, phys==bus on x86), per backend. */
    if(a->Backend==BK_CPLUS){
        /* The RTL8169 latches only the top bits of the descriptor-ring base
           (TNPDS/RDSAR are 256-byte aligned - the low 8 bits are ignored), but
           NT 3.1 MmAllocateContiguousMemory only page-aligns large blocks, so a
           1 KB ring can land on a non-256 boundary.  Over-allocate and align the
           physical (and hence the contiguous virtual) address up to 256. */
        PUCHAR raw; ULONG rawp, off;
        raw=(PUCHAR)dma_alloc(RCP_NUM_TX_DESC*sizeof(RCP_DESC)+256,&rawp);
        if(!raw){ dbg("cp txdesc fail\n"); goto fail; }
        a->TxDescAllocation=raw;
        off=(256-(rawp&0xFF))&0xFF; a->TxDesc=(PRCP_DESC)(raw+off); a->TxDescPhys=rawp+off;
        raw=(PUCHAR)dma_alloc(RCP_NUM_RX_DESC*sizeof(RCP_DESC)+256,&rawp);
        if(!raw){ dbg("cp rxdesc fail\n"); goto fail; }
        a->RxDescAllocation=raw;
        off=(256-(rawp&0xFF))&0xFF; a->RxDesc=(PRCP_DESC)(raw+off); a->RxDescPhys=rawp+off;
        for(i=0;i<RCP_NUM_TX_DESC;i++){
            a->CpTx[i]=dma_alloc(RCP_RX_BUF_SIZE,&a->CpTxP[i]);
            if(!a->CpTx[i]){ dbg("cp tx buf fail\n"); goto fail; }
        }
        for(i=0;i<RCP_NUM_RX_DESC;i++){
            a->CpRx[i]=dma_alloc(RCP_RX_BUF_SIZE,&a->CpRxP[i]);
            if(!a->CpRx[i]){ dbg("cp rx buf fail\n"); goto fail; }
        }
        dbg("cp dma ok txdesc="); dbgh(a->TxDescPhys); dbg(" rxdesc="); dbgh(a->RxDescPhys); dbg("\n");
    } else {
        /* 8139 linear RX ring + TX slots; NE2000 only needs one staging TX buf
           (its RX/TX use the chip's internal buffer RAM via remote DMA). */
        a->RxRing = dma_alloc(R8139_RXBUF_LEN+R8139_RXBUF_PAD+R8139_RXBUF_WRAPPAD, &a->RxRingPhys);
        if(!a->RxRing){ dbg("rxring alloc fail\n"); goto fail; }
        for(i=0;i<N31_TXN;i++){
            a->TxBuf[i]=dma_alloc(2048,&a->TxPhys[i]);
            if(!a->TxBuf[i]){ dbg("txbuf alloc fail\n"); goto fail; }
        }
        dbg("dma ok rxphys="); dbgh(a->RxRingPhys); dbg("\n");
    }

    /* NDIS_ADAPTER_INFORMATION built byte-for-byte like the inbox NE2000
       MAC driver (disassembled): a 40-byte block with one inline port
       descriptor.  Field offsets: [0x0C]=1 (PhysicalMapRegistersNeeded),
       [0x18]=NumberOfPortDescriptors=1, [0x1C]=InitialPort (I/O base),
       [0x20]=NumberOfPorts, [0x24]=PortOffset target.  AdapterType (0x08)
       stays 0 = NdisInterfaceInternal, which avoids NT 3.1's broken PCI
       bus-address translation path. */
register_adapter:
    for(i=0;i<10;i++) info[i]=0;
    info[3]=1;                        /* 0x0C PhysicalMapRegistersNeeded    */
    info[6]=0;                        /* 0x18 NumberOfPortDescriptors=0     */
    /* direct port I/O (PCI I/O base already found) - do not have NDIS claim
       the high PCI I/O range as an ISA/Internal resource (that fails). */
    dbg("pre-reg\n");
    /* arg2 must be the NdisMacHandle from NdisRegisterMac (g_mac), NOT the
       MacMacContext handed to MacAddAdapter (that is DriverObject). */
    s=NdisRegisterAdapter(&a->NdisAdapterHandle, g_mac, (NDIS_HANDLE)a, cfg, name, info);
    dbg("RegisterAdapter="); dbgh((ULONG)s); dbg("\n");
    if(s!=NDIS_STATUS_SUCCESS) goto fail;
    registered=TRUE;
    if(a->MappedBase) a->IoBase=(ULONG)a->MappedBase;   /* use mapped base  */
    dbg("io now="); dbgh(a->IoBase); dbg("\n");

    hardwareAttempted=TRUE;
    s=NDIS_STATUS_FAILURE;
    if(!hw_init(a)) goto fail;   /* reads MAC, programs HW */
    dbg("hw ok, pre-filter\n");
    publish_mac(a,name);    /* expose MAC for the optional DHCPNT31 /auto client */

    /* Eth filter callbacks are called by NDIS at DISPATCH (arbitrary context),
       so they must run from the pool copy, not the image. */
    if(!EthCreateFilter(16, (ETH_ADDRESS_CHANGE)cp(addr_change),
                        (ETH_FILTER_CHANGE)cp(filt_change),
                        (ETH_DEFERRED_CLOSE)cp(close_action),
                        a->Mac, &a->Lock, &a->FilterDb)){
        dbg("EthCreateFilter FAIL\n");
        s=NDIS_STATUS_RESOURCES;
        goto fail;
    }
    dbg("filter created\n");

    if(!n31_start_recovery(a)){s=NDIS_STATUS_RESOURCES;goto fail;}
    if(!n31_publish(a)){s=NDIS_STATUS_RESOURCES;goto fail;}
    dbg("timer armed (pool dpc)\n");
    dbg("ADD ok\n");
    return NDIS_STATUS_SUCCESS;
fail:
    n31_stop_recovery(a);
    if(hardwareAttempted) dmaStopped=n31_stop_hw(a);
    if(a->FilterDb)EthDeleteFilter(a->FilterDb);
    if(registered) NdisDeregisterAdapter(a->NdisAdapterHandle);
    if(a->UsbContext)((VOID(*)(PVOID))cp(N31UsbDestroy))(a->UsbContext);
    if(dmaStopped)n31_free_dma(a);
    else dbg("ADD: unacknowledged DMA retained; reboot required to reclaim\n");
    n31_unmap(a);
    MmFreeContiguousMemory(a);
    return s;
}
static VOID STDC n31_remove(NDIS_HANDLE adapt){
    PN31_ADAPTER a=(PN31_ADAPTER)adapt;KIRQL old;UINT i;BOOLEAN stopped;
    if(!a) return;
    KeAcquireSpinLock(&g_poll->Lock,&old);
    for(i=0;i<N31_MAX&&g_ap[i]!=a;i++);
    if(i<N31_MAX){g_ap[i]=NULL;(*g_apN)--;}
    KeReleaseSpinLock(&g_poll->Lock,old);
    if(i==N31_MAX)return;
    n31_stop_recovery(a);
    n31_poll_stop(a);              /* drain actual users before filter teardown */
    stopped=n31_stop_hw(a);
    if(a->UsbContext){N31UsbDestroy(a->UsbContext);a->UsbContext=NULL;}
    if(a->FilterDb) EthDeleteFilter(a->FilterDb);
    if(a->NdisAdapterHandle) NdisDeregisterAdapter(a->NdisAdapterHandle);
    if(stopped)n31_free_dma(a);
    else dbg("REMOVE: unacknowledged DMA retained; reboot required to reclaim\n");
    n31_unmap(a);
    MmFreeContiguousMemory(a);
}

/* x86 NT3.1 has no affinity/DPC-flush APIs. Do not guess that an event means
 * the callback has returned. These tiny stdcall boundaries transfer directly
 * into kernel unlock code after dropping their reference while holding the
 * owner lock. Queued reclamation must acquire that lock before freeing code.
 * Every prior callback has therefore left the image before reclamation.
 * The passive worker likewise tail-calls MmFreeContiguousMemory.
 */
static PVOID STDC n31_reclaim_prepare(PVOID context) __attribute__((used,noinline));
static PVOID STDC n31_reclaim_prepare(PVOID context)
{
    N31_IMAGE_OWNER *owner=(N31_IMAGE_OWNER*)context;PVOID image;KIRQL old;
    /* Join the final kernel unlock. All former callback epilogues have
       already left the image before they can release this lock. */
    KeAcquireSpinLock(&owner->Lock,&old);image=owner->Image;
    KeReleaseSpinLock(&owner->Lock,old);
    dbg("RECLAIM resident image\n");
    MmFreeContiguousMemory(owner);
    return image;
}
static VOID STDC n31_reclaim_entry(PVOID context) __attribute__((naked,used));
static VOID STDC n31_reclaim_entry(PVOID context)
{
    __asm__("pushl 4(%esp)\n\tcall _n31_reclaim_prepare@4\n\t"
            "movl %eax,4(%esp)\n\tjmp _MmFreeContiguousMemory@4\n\t");
}
static VOID STDC n31_dpc_entry(PVOID a,PVOID b,PVOID c,PVOID d) __attribute__((naked,used));
static VOID STDC n31_dpc_entry(PVOID a,PVOID b,PVOID c,PVOID d)
{
    __asm__("pushl 16(%esp)\n\tpushl 16(%esp)\n\tpushl 16(%esp)\n\tpushl 16(%esp)\n\t"
            "call _n31_dpc@16\n\tcall _n31_release_prepare@0\n\t"
            /* Collapse four callback arguments to Lock and OldIrql. */
            "movl (%esp),%edx\n\tmovl %edx,8(%esp)\n\t"
            "movl _g_owner,%edx\n\taddl $16,%edx\n\tmovl %edx,12(%esp)\n\t"
            "movl %eax,16(%esp)\n\taddl $8,%esp\n\tjmp _KeReleaseSpinLock@8\n\t");
}
static ULONG STDC n31_finish_driver_unload(PVOID driver) __attribute__((used,noinline));
static ULONG STDC n31_finish_driver_unload(PVOID driver)
{
    if(g_owner->Unload)g_owner->Unload(driver);
    if(g_poll){dbg("UNLOAD: MAC still registered; resident image retained\n");return (ULONG)-1;}
    return n31_release_prepare();
}
static VOID STDC n31_driver_unload(PVOID driver) __attribute__((naked,used));
static VOID STDC n31_driver_unload(PVOID driver)
{
    __asm__("pushl 4(%esp)\n\tcall _n31_finish_driver_unload@4\n\tcmpl $-1,%eax\n\tje 1f\n\t"
            /* Expand the single unload argument to two kernel arguments. */
            "movl (%esp),%edx\n\tsubl $4,%esp\n\tmovl %edx,(%esp)\n\t"
            "movl _g_owner,%edx\n\taddl $16,%edx\n\tmovl %edx,4(%esp)\n\t"
            "movl %eax,8(%esp)\n\tjmp _KeReleaseSpinLock@8\n\t"
            "1: ret $4\n\t");
}
static VOID STDC n31_install_unload(PVOID driver)
{
    /* Public x86 DRIVER_OBJECT.DriverUnload offset, also used by USBNT. */
    VOID (STDC **slot)(PVOID)=(VOID(STDC**)(PVOID))((PUCHAR)driver+0x34);
    g_owner->Unload=*slot;*slot=n31_driver_unload;
}
static VOID STDC n31_finish_failed_load(VOID)
{
    n31_unload(NULL);
}

/* ================= DriverEntry ===================================== */
NDIS_STATUS STDC DriverEntry(PVOID DriverObject, PVOID RegistryPath){
    NDIS_MAC_CHARACTERISTICS mc; NDIS_STATUS status, err; UINT i;
    static WCHAR nm[]={'R','L','T','K','L','A','N','T',0};
    PUCHAR z=(PUCHAR)&mc; for(i=0;i<sizeof(mc);i++) z[i]=0;
    dbg("\nRLTKLANT NT3.1 DriverEntry\n");

    /* adapter list + count in non-paged pool, allocated BEFORE the image is
       copied so the pool copy's g_ap/g_apN hold the same pointers - the
       image-context AddAdapter handler and the pool-context poll DPC then
       share one adapter list (see g_ap comment).  Each card's N31_ADAPTER is
       allocated later in n31_add. */
    { ULONG dummy;
      g_ap =(N31_ADAPTER**)dma_alloc(sizeof(N31_ADAPTER*)*N31_MAX,&dummy);
      g_apN=(UINT*)dma_alloc(sizeof(UINT),&dummy);
      g_poll=(N31_POLL_SET*)dma_alloc(sizeof(*g_poll),&dummy);
      g_owner=(N31_IMAGE_OWNER*)dma_alloc(sizeof(*g_owner),&dummy);
      if(!g_ap||!g_apN||!g_poll||!g_owner){
          if(g_ap)MmFreeContiguousMemory(g_ap);
          if(g_apN)MmFreeContiguousMemory(g_apN);
          if(g_poll)MmFreeContiguousMemory(g_poll);
          if(g_owner)MmFreeContiguousMemory(g_owner);
          dbg("adapter list alloc fail\n");return NDIS_STATUS_RESOURCES;
      }
      for(i=0;i<sizeof(*g_owner);i++)((PUCHAR)g_owner)[i]=0;
      g_owner->References=1;g_owner->Context=g_owner;
      for(i=0;i<sizeof(*g_poll);i++)((PUCHAR)g_poll)[i]=0;
      KeInitializeEvent(g_poll->Drained,0,FALSE);
      for(i=0;i<N31_MAX;i++) g_ap[i]=(PN31_ADAPTER)0;
      *g_apN=0;
    }

    /* Copy the image into globally-mapped non-paged pool (after g_ap is set, so
       the copy's g_ap slot already holds the pool adapter pointer) and relocate
       it.  Every handler NDIS may call later runs from the copy via cp(). */
    { PDRV_OBJ drv=(PDRV_OBJ)DriverObject;
      g_drvStart=(PUCHAR)drv->DriverStart;
      dbg("img start="); dbgh((ULONG)g_drvStart); dbg(" size="); dbgh(drv->DriverSize); dbg("\n");
      g_img=img_to_pool(drv->DriverStart, drv->DriverSize);
      if(!g_img){
          MmFreeContiguousMemory(g_ap);MmFreeContiguousMemory(g_apN);MmFreeContiguousMemory(g_poll);MmFreeContiguousMemory(g_owner);
          dbg("img copy fail\n");return NDIS_STATUS_RESOURCES;
      }
      dbg("img pool="); dbgh((ULONG)g_img); dbg("\n");
    }

    g_owner->Image=g_img;g_owner->Worker=(VOID(STDC*)(PVOID))cp(n31_reclaim_entry);
    NdisInitializeTimer(&g_poll->Timer,(NDIS_TIMER_FUNCTION)cp(n31_dpc_entry),g_poll);

    ((VOID(*)(VOID))cp(N31UsbInitialize))();
    NdisInitializeWrapper(&g_wrapper, DriverObject, RegistryPath, NULL);
    mc.MajorNdisVersion=3; mc.MinorNdisVersion=0;
    /* Handlers called post-load (arbitrary context) run from the pool copy.
       AddAdapterHandler runs synchronously inside NdisRegisterMac (loader
       context, image mapped) and reads g_mac, so it stays the image version. */
    mc.OpenAdapterHandler=(OPEN_ADAPTER)cp(n31_open);
    mc.CloseAdapterHandler=(CLOSE_ADAPTER)cp(n31_close);
    mc.SendHandler=(MAC_SEND)cp(n31_send);
    mc.TransferDataHandler=(TRANSFER_DATA)cp(n31_transfer);
    mc.ResetHandler=(MAC_RESET)cp(n31_reset);
    mc.RequestHandler=(MAC_REQUEST)cp(n31_request);
    mc.QueryGlobalStatisticsHandler=(QUERY_GLOBAL_STATS)cp(n31_qgs);
    mc.UnloadMacHandler=(UNLOAD_MAC)cp(n31_unload);
    mc.AddAdapterHandler=n31_add;
    mc.RemoveAdapterHandler=(REMOVE_ADAPTER)cp(n31_remove);
    mc.Name.Buffer=nm; mc.Name.Length=16; mc.Name.MaximumLength=18;

    g_mac=NULL;
    status=NdisRegisterMac(&err,&g_mac,g_wrapper,DriverObject,&mc,sizeof(mc));
    dbg("RegisterMac st="); dbgh((ULONG)status);
    dbg(" err="); dbgh((ULONG)err); dbg(" mac="); dbgh((ULONG)g_mac); dbg("\n");
    /* NT 3.1 registration succeeded iff a MacHandle was produced; the return
       value is not a plain NDIS_STATUS here.  Do not tear the wrapper down on a
       non-zero return, or the MAC is removed and no protocol can bind. */
    if(!g_mac){
        KIRQL old;
        dbg("RegisterMac FAILED\n");NdisTerminateWrapper(g_wrapper,NULL);
        ((VOID(STDC*)(VOID))cp(n31_finish_failed_load))();
        /* This caller is the original loader image, outside the pool copy. */
        old=(KIRQL)n31_release_prepare();KeReleaseSpinLock(&g_owner->Lock,old);
        return NDIS_STATUS_FAILURE;
    }
    ((VOID(*)(PVOID))cp(RtlUsbNtClientStart))(DriverObject);
    ((VOID(STDC*)(PVOID))cp(n31_install_unload))(DriverObject);
    dbg("DriverEntry ok\n");
    return NDIS_STATUS_SUCCESS;
}
