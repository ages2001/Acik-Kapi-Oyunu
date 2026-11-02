/*
 * rltklant.h - driver-wide definitions for RLTKLANT.
 *
 * One NDIS miniport driving every Realtek LAN controller from the RTL8029
 * to the RTL8127 on Windows NT 3.5x, NT 4.0, 2000, XP and XP x64.  The
 * exact part and its media ceiling are resolved at run time from the PCI
 * device id and the chip XID (see rtlchip.h).
 *
 * Copyright (C) 2026 ages2001.
 */
#ifndef RLTKLANT_H
#define RLTKLANT_H

#include "ndiskit.h"
#include "rltkhw.h"
#include "rtlchip.h"

#define RLTK_VERSION_MAJOR          1
#define RLTK_VERSION_MINOR          0
#define RLTK_ETH_ADDR_LEN           6
#define RLTK_MAX_MULTICAST          32
#define RLTK_MAX_FRAME              1514
#define RLTK_HEADER_SIZE            14
#define RLTK_LOOKAHEAD              (RLTK_MAX_FRAME - RLTK_HEADER_SIZE)

#define PCI_CFG_VENDOR              0x00
#define PCI_CFG_COMMAND             0x04
#define PCI_CFG_REVISION            0x08
#define PCI_CFG_BAR0                0x10
#define PCI_CFG_BAR1                0x14
#define PCI_CFG_BAR2                0x18
#define PCI_CFG_SUBSYS              0x2C
#define PCI_CFG_INTLINE             0x3C

/* user "SpeedDuplex" setting (registry / INF) */
#define RLTK_SD_AUTO        0
#define RLTK_SD_10H         1
#define RLTK_SD_10F         2
#define RLTK_SD_100H        3
#define RLTK_SD_100F        4
#define RLTK_SD_1000F       5
#define RLTK_SD_2500F       6
#define RLTK_SD_5000F       7
#define RLTK_SD_10000F      8

struct _RLTK_ADAPTER;
typedef struct _RLTK_ADAPTER RLTK_ADAPTER, *PRLTK_ADAPTER;

typedef struct _RLTK_OPS {
    const char *BackendName;
    BOOLEAN (*Init)(PRLTK_ADAPTER);
    VOID    (*Halt)(PRLTK_ADAPTER);
    VOID    (*SetRxMode)(PRLTK_ADAPTER);
    BOOLEAN (*Transmit)(PRLTK_ADAPTER, PUCHAR, UINT);
    BOOLEAN (*Isr)(PRLTK_ADAPTER);                  /* TRUE = ours */
    VOID    (*HandleInterrupt)(PRLTK_ADAPTER);
    VOID    (*EnableInts)(PRLTK_ADAPTER, BOOLEAN);
    BOOLEAN (*LinkState)(PRLTK_ADAPTER, PULONG, PBOOLEAN);
} RLTK_OPS, *PRLTK_OPS;

struct _RLTK_ADAPTER {
    NDIS_HANDLE     MiniportHandle;
    NDIS_HANDLE     WrapperConfig;

    /* identity */
    USHORT          PhyPage;
    ULONG           PciSlot;                /* NDIS 3.0: (bus<<8 | slot) + 1; 0 = none */
    ULONG           PciBus;                 /* segment-0 PCI bus for legacy HAL access */
    PVOID           PciPdo;                 /* referenced NT5 PCI physical device */
    USHORT          PciDeviceId;
    USHORT          PciSubsys;
    UCHAR           Revision;
    UCHAR           Backend;
    ULONG           MaxSpeedMbit;
    ULONG           Flags;
    char            FriendlyName[80];
    ULONG           SpeedDuplex;            /* RLTK_SD_*, clamped */

    /* resources */
    PUCHAR          IoBase;                 /* MMIO va (or NULL)  */
    ULONG           MemLen;
    PVOID           IoPortVa;               /* NdisMRegisterIoPortRange */
    ULONG           IoPortBase;
    ULONG           IoPortPhys;
    ULONG           IoPortLen;
    BOOLEAN         UseMmio;
    BOOLEAN         MapRegs;
    BOOLEAN         IntRegistered;
    NDIS_MINIPORT_INTERRUPT Interrupt;
    ULONG           IrqVector, IrqLevel;
    PVOID           RecoveryThread;
    union { ULONG_PTR Alignment; UCHAR Storage[64]; } RecoveryWake;
    BOOLEAN         RecoveryStop, ResetPending, ResetFailed;
    ULONG           TxProgress;
    UCHAR           TxNoProgressChecks;

    /* addresses / filter */
    UCHAR           PermanentAddr[6];
    UCHAR           CurrentAddr[6];
    ULONG           PacketFilter;
    ULONG           McastCount;
    UCHAR           McastList[RLTK_MAX_MULTICAST][6];

    /* link */
    BOOLEAN         LinkUp;
    BOOLEAN         FullDuplex;
    ULONG           LinkSpeedMbit;
    NDIS_MINIPORT_TIMER LinkTimer;
    BOOLEAN         TimerSet;
    union { ULONG_PTR Alignment; UCHAR Storage[64]; } LinkTimerStopped;

    /* receive indication */
    PVOID           EthFilter;              /* NDIS ETH_FILTER (located lazily) */
    PUCHAR          RxFrame;                /* frame being indicated   */
    UINT            RxFrameLen;
    UCHAR           RxCopy[RLTK_MAX_FRAME + 18];

    /* interrupt status latched by ISR for the DPC */
    ULONG           PendingIsr;

    /* RTL8139 */
    PUCHAR          RxRing;
    NDIS_PHYSICAL_ADDRESS RxRingPhys;
    ULONG           RxOffset;
    PUCHAR          TxBuf[R8139_TX_DESCS];
    NDIS_PHYSICAL_ADDRESS TxBufPhys[R8139_TX_DESCS];
    ULONG           TxCur, TxDirty;

    /* C+ */
    PRCP_DESC       TxDesc, RxDesc;
    NDIS_PHYSICAL_ADDRESS TxDescPhys, RxDescPhys;
    PUCHAR          CpTxBuf[RCP_NUM_TX_DESC];
    NDIS_PHYSICAL_ADDRESS CpTxBufPhys[RCP_NUM_TX_DESC];
    PUCHAR          CpRxBuf[RCP_NUM_RX_DESC];
    NDIS_PHYSICAL_ADDRESS CpRxBufPhys[RCP_NUM_RX_DESC];
    ULONG           CpTxHead, CpTxTail, CpRxCur;
    BOOLEAN         Is8125;

    /* NE2000 */
    UCHAR           NeNext;

    /* statistics */
    ULONG           XmitOk, RcvOk, XmitErr, RcvErr, RcvNoBuf;
    ULONG           AlignErr, OneColl, MoreColl;

    NDIS_SPIN_LOCK  Lock;
    PRLTK_OPS       Ops;
    PVOID           UsbContext;             /* queued USBNT miniport state */
    PVOID           NativeUsb;              /* NT5 URB owner, joined before Halt */
    BOOLEAN         NativeUsbRequested;
    BOOLEAN         Halting;
    BOOLEAN         TxStalled;              /* told NDIS "resources" */
};

VOID RtlReleaseSlot(PRLTK_ADAPTER a);
BOOLEAN RtlStartRecovery(PRLTK_ADAPTER a);
VOID RtlStopRecovery(PRLTK_ADAPTER a);
NDIS_STATUS RtlQueuePciReset(PRLTK_ADAPTER a);
NDIS_STATUS RtlRecoverPci(PRLTK_ADAPTER a);
VOID RtlResumeLinkTimer(PRLTK_ADAPTER a);
VOID STDC RtlLinkTimer(PVOID, PVOID, PVOID, PVOID);

extern RLTK_OPS RtlOps8139;
extern RLTK_OPS RtlOpsCplus;
extern RLTK_OPS RtlOpsNe2000;

/* ---- port I/O (rtlport.c) ------------------------------------------ */
UCHAR  RtlInByte(ULONG_PTR port);
USHORT RtlInWord(ULONG_PTR port);
ULONG  RtlInDword(ULONG_PTR port);
VOID   RtlOutByte(ULONG_PTR port, UCHAR v);
VOID   RtlOutWord(ULONG_PTR port, USHORT v);
VOID   RtlOutDword(ULONG_PTR port, ULONG v);

static __inline UCHAR RtlRead8(PRLTK_ADAPTER a, ULONG r)
{ return a->UseMmio ? *(volatile UCHAR *)(a->IoBase + r) : RtlInByte(a->IoPortBase + r); }
static __inline USHORT RtlRead16(PRLTK_ADAPTER a, ULONG r)
{ return a->UseMmio ? *(volatile USHORT *)(a->IoBase + r) : RtlInWord(a->IoPortBase + r); }
static __inline ULONG RtlRead32(PRLTK_ADAPTER a, ULONG r)
{ return a->UseMmio ? *(volatile ULONG *)(a->IoBase + r) : RtlInDword(a->IoPortBase + r); }
static __inline VOID RtlWrite8(PRLTK_ADAPTER a, ULONG r, UCHAR v)
{ if (a->UseMmio) *(volatile UCHAR *)(a->IoBase + r) = v; else RtlOutByte(a->IoPortBase + r, v); }
static __inline VOID RtlWrite16(PRLTK_ADAPTER a, ULONG r, USHORT v)
{ if (a->UseMmio) *(volatile USHORT *)(a->IoBase + r) = v; else RtlOutWord(a->IoPortBase + r, v); }
static __inline VOID RtlWrite32(PRLTK_ADAPTER a, ULONG r, ULONG v)
{ if (a->UseMmio) *(volatile ULONG *)(a->IoBase + r) = v; else RtlOutDword(a->IoPortBase + r, v); }

/* ---- hwdetect.c ---------------------------------------------------- */
const RTL_PCI_ID *RtlFindPciId(USHORT dev);
VOID  RtlResolveName(PRLTK_ADAPTER a, ULONG txConfig);
ULONG RtlCapSpeed(PRLTK_ADAPTER a, ULONG mbit);
ULONG RtlCrc32Bits(const UCHAR *addr);

/* ---- phy.c --------------------------------------------------------- */
BOOLEAN RtlCpMdioRead(PRLTK_ADAPTER a, UINT reg, USHORT *value);
BOOLEAN RtlCpMdioWrite(PRLTK_ADAPTER a, UINT reg, USHORT val);
BOOLEAN RtlCpPhyOcpRead(PRLTK_ADAPTER a, USHORT address, USHORT *value);
BOOLEAN RtlCpPhyOcpWrite(PRLTK_ADAPTER a, USHORT address, USHORT value);
BOOLEAN RtlConfigurePhy(PRLTK_ADAPTER a);
BOOLEAN RtlStartBackend(PRLTK_ADAPTER a);
BOOLEAN RtlPreparePci(PRLTK_ADAPTER a);
VOID   Rtl8139ConfigureMedia(PRLTK_ADAPTER a);

/* ---- rxbind.c ------------------------------------------------------ */
VOID   RtlIndicateFrame(PRLTK_ADAPTER a, PUCHAR frame, UINT len);
VOID   RtlIndicateComplete(PRLTK_ADAPTER a);

/* ---- regname.c ----------------------------------------------------- */
VOID   RtlPublishName(PRLTK_ADAPTER a);
VOID   RtlReadSettings(PRLTK_ADAPTER a);

/* ---- NDIS handlers ------------------------------------------------- */
VOID             RtlInitSlots(VOID);
NDIS_STATUS STDC RtlInitialize(PNDIS_STATUS, PUINT, PNDIS_MEDIUM, UINT, NDIS_HANDLE, NDIS_HANDLE);
VOID        STDC RtlHalt(NDIS_HANDLE);
VOID        STDC RtlIsr(PBOOLEAN, PBOOLEAN, NDIS_HANDLE);
VOID        STDC RtlHandleInterrupt(NDIS_HANDLE);
NDIS_STATUS STDC RtlReset(PBOOLEAN, NDIS_HANDLE);
BOOLEAN     STDC RtlCheckForHang(NDIS_HANDLE);
NDIS_STATUS STDC RtlQueryInformation(NDIS_HANDLE, NDIS_OID, PVOID, ULONG, PULONG, PULONG);
NDIS_STATUS STDC RtlSetInformation(NDIS_HANDLE, NDIS_OID, PVOID, ULONG, PULONG, PULONG);
NDIS_STATUS STDC RtlSend(NDIS_HANDLE, PNDIS_PACKET, UINT);
NDIS_STATUS STDC RtlTransferData(PNDIS_PACKET, PUINT, NDIS_HANDLE, NDIS_HANDLE, UINT, UINT);
VOID        STDC RtlEnableInterrupt(NDIS_HANDLE);
VOID        STDC RtlDisableInterrupt(NDIS_HANDLE);

#endif /* RLTKLANT_H */
