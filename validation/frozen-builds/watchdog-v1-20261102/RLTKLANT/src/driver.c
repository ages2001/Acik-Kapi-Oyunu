/*
 * driver.c - DriverEntry and NDIS miniport registration for RLTKLANT.
 * Copyright (C) 2026 ages2001.
 */
#include "rltklant.h"
#include "backend_usb.h"

#ifdef RTL_DBG
static VOID dbg(const char *s){ while(*s) RtlOutByte(0xE9,(UCHAR)*s++); }
static VOID dbgh(ULONG v){ char b[11]; int i; b[0]='0'; b[1]='x';
    for(i=0;i<8;i++){ UCHAR n=(UCHAR)((v>>((7-i)*4))&0xF); b[2+i]=(char)(n<10?'0'+n:'A'+n-10);} b[10]=0; dbg(b); }
#else
#define dbg(s)
#define dbgh(v)
#endif

NDIS_HANDLE RtlWrapperHandle;

#if defined(RTL_NDIS5)
/* NDIS 5.1 requires a shutdown handler. Shutdown can run at high IRQL:
 * stop hardware without waiting, releasing memory, or entering NDIS. */
static VOID STDC RtlShutdown(PVOID context)
{
    PRLTK_ADAPTER a = (PRLTK_ADAPTER)context;
    a->Halting = TRUE;
    if (a->Backend == RTL_BACKEND_USBNT) return; /* worker owns USB I/O */
    if (a->Backend == RTL_BACKEND_NE2000) {
        RtlWrite8(a, NE_CR, NE_CR_PAGE0 | NE_CR_RD2 | NE_CR_STP);
        RtlWrite8(a, NE_P0_IMR, 0);
    } else {
        a->Ops->EnableInts(a, FALSE);
        RtlWrite8(a, R8139_CR, 0); /* common 8139/C+ command register */
        (void)RtlRead8(a, R8139_CR); /* flush a posted MMIO write */
    }
}
#endif

VOID RtlUsbNtClientStart(PVOID DriverObject);   /* usbntclient.c: claim a Realtek USB NIC via USBNT */

NDIS_STATUS STDC DriverEntry(PVOID DriverObject, PVOID RegistryPath)
{
    NDIS_MINIPORT_CHARACTERISTICS c;
    NDIS_STATUS status;

    dbg("\nRLTK DriverEntry\n");
    RtlInitSlots();     /* init the multi-adapter slot lock before any Initialize */
    RtlUsbAdaptersInit();
    NdisInitializeWrapper(&RtlWrapperHandle, DriverObject, RegistryPath, NULL);
    if (!RtlWrapperHandle)
        return NDIS_STATUS_FAILURE;

    NdisZeroMemory(&c, sizeof(c));
#if defined(RTL_NDIS5)
    c.MajorNdisVersion = 5;
    c.MinorNdisVersion = 1;   /* default: NDIS 5.1 (XP, 2003, XP x64) */
    c.AdapterShutdownHandler = RtlShutdown;
#else
    c.MajorNdisVersion = 3;
    c.MinorNdisVersion = 0;
#endif
    c.CheckForHangHandler     = RtlCheckForHang;
    /* NT4 synchronizes these callbacks through the registered interrupt
       object. USB instances have none. PCI backends mask in their ISR and
       re-enable at the end of HandleInterrupt, so these optional callbacks
       must be absent for this mixed hardware/software miniport driver. */
    c.DisableInterruptHandler = NULL;
    c.EnableInterruptHandler  = NULL;
    c.HaltHandler             = RtlHalt;
    c.HandleInterruptHandler  = RtlHandleInterrupt;
    c.InitializeHandler       = RtlInitialize;
    c.ISRHandler              = RtlIsr;
    c.QueryInformationHandler = RtlQueryInformation;
    c.ReconfigureHandler      = NULL;
    c.ResetHandler            = RtlReset;
    c.SendHandler             = RtlSend;
    c.SetInformationHandler   = RtlSetInformation;
    c.TransferDataHandler     = RtlTransferData;

    status = NdisMRegisterMiniport(RtlWrapperHandle, &c, sizeof(c));
#if defined(RTL_NDIS5)
    /* Windows 2000 implements NDIS 5.0. Pass the corresponding prefix
       length when retrying registration with that version. */
    if (status == NDIS_STATUS_BAD_VERSION) {
        c.MinorNdisVersion = 0;
        status = NdisMRegisterMiniport(RtlWrapperHandle, &c,
                                       RTL_NDIS50_CHARACTERISTICS_SIZE);
    }
#endif
    dbg("RegisterMiniport st="); dbgh(status); dbg("\n");

    /* A failed DriverEntry allows the loader to discard this image. Never
       publish callbacks into it unless NDIS registration has succeeded. */
    if (status != NDIS_STATUS_SUCCESS) {
        NdisTerminateWrapper(RtlWrapperHandle, NULL);
        return status;
    }

#if !defined(RTL_NDIS5)
    RtlUsbNtClientStart(DriverObject);
#endif
    return NDIS_STATUS_SUCCESS;
}
