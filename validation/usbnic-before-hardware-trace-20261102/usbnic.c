/*
 * usbnic.c - RLTKLANT USB-NIC front end (see usbnic.h).
 *
 * Copyright (C) 2026 ages2001.
 */
#include "usbnic.h"
#include "rtl8150.h"

#define USBNIC_IS_8150(u)  ((u)->Nic.Chip->DeviceId == RTL8150_DEVICE_ID)

USBNIC_ROUTE UsbNicRoute(USBNT_OSVER os, USBNT_HC_KIND hc)
{
    if (UsbntUseForNic(os, hc))
        return UsbRouteUsbnt;       /* NT<=4 (any), or 2000+ xHCI/USB4     */
    return UsbRouteInbox;           /* 2000+ UHCI/OHCI/EHCI -> in-box USB   */
}

static NDIS_STATUS Attach(PUSBNIC u, USBNT_OSVER os, USBNT_HC_KIND hc,
                         USHORT vendor, USHORT device,
                         PVOID devObject, PFN_USB_SUBMIT submit,const USBNIC_PLATFORM*platform)
{
    const RTL_USB_ID *chip;
    NDIS_STATUS st;
    ULONG i;

    for (i = 0; i < sizeof(USBNIC); i++) ((PUCHAR)u)[i] = 0;
    if(platform){
        if(!platform->WaitMs||!platform->Cancelled)return NDIS_STATUS_FAILURE;
        u->Platform=*platform;
    }

    chip = RtlUsbMatch(vendor, device);
    if (!chip) return NDIS_STATUS_ADAPTER_NOT_FOUND;

    u->Os = os;
    u->Hc = hc;
    u->Route = UsbNicRoute(os, hc);
    u->Nic.Chip = chip;

    /* The caller supplies a synchronous transfer primitive. USBNT uses its
       generic device interface; NT5 uses per-adapter native URB submission. */
    u->Bind.DevObject = devObject;
    u->Bind.Submit    = submit;
    UsbntBindPipe(&u->Nic.Pipe, &u->Bind);

    /* Legacy RTL8150 (USB 1.1) uses its own protocol backend; the whole
       r8152 family (8152..8159) shares the rtl815x.c backend. */
    if (USBNIC_IS_8150(u)) {
        st = Rtl8150Reset(&u->Nic);
        if (!USB_OK(st)) return st;
        st = Rtl8150ReadMac(&u->Nic);
        if (!USB_OK(st)) return st;
        st = Rtl8150Start(&u->Nic);
        return st;
    }

    st = RtlUsbDetectVersion(&u->Nic);
    if (!USB_OK(st)) return st;
    if(platform){
        RTL_UPHY_REPORT report;
        if(RtlUsbHardwareApply(&u->Nic,&u->Hardware,platform->BusSpeed,vendor,device,
           RTLU_RX_BUF_SIZE,platform->WaitMs,platform->Cancelled,platform->Context,&report)!=RtlUsbFwExecOk)
            return NDIS_STATUS_FAILURE;
        st=RtlUsbReadMac(&u->Nic);
        if(!USB_OK(st))return st;
        u->HardwareReady=TRUE;
        return NDIS_STATUS_SUCCESS;
    }
    st = RtlUsbReset(&u->Nic);
    if (!USB_OK(st)) return st;
    st = RtlUsbReadMac(&u->Nic);
    if (!USB_OK(st)) return st;
    st = RtlUsbStart(&u->Nic);
    return st;
}
NDIS_STATUS UsbNicAttach(PUSBNIC u,USBNT_OSVER os,USBNT_HC_KIND hc,USHORT vid,USHORT pid,
                         PVOID object,PFN_USB_SUBMIT submit)
{return Attach(u,os,hc,vid,pid,object,submit,0);}
NDIS_STATUS UsbNicAttachEx(PUSBNIC u,USBNT_OSVER os,USBNT_HC_KIND hc,USHORT vid,USHORT pid,
                           PVOID object,PFN_USB_SUBMIT submit,const USBNIC_PLATFORM*platform)
{
    if(!platform)return NDIS_STATUS_FAILURE;
    return Attach(u,os,hc,vid,pid,object,submit,platform);
}
NDIS_STATUS UsbNicApplyCarrier(PUSBNIC u,BOOLEAN up,ULONG speed,BOOLEAN duplex)
{
    RTL_UPHY_REPORT report;NDIS_STATUS st;
    if(!u->Platform.WaitMs||USBNIC_IS_8150(u))return NDIS_STATUS_SUCCESS;
    if(!u->HardwareReady)return NDIS_STATUS_FAILURE;
    if(u->HardwareActive==up&&(!up||(u->HardwareSpeed==speed&&u->HardwareDuplex==duplex)))
        return NDIS_STATUS_SUCCESS;
    if(u->Nic.Running){
        st=RtlUsbStop(&u->Nic);
        if(!USB_OK(st)){u->HardwareReady=FALSE;return st;}
    }
    u->HardwareActive=FALSE;
    if(up){
        if(RtlUsbHardwareEnableApply(&u->Nic,&u->Hardware,u->Platform.WaitMs,
           u->Platform.Cancelled,u->Platform.Context,&report)!=RtlUsbFwExecOk){
            u->HardwareReady=FALSE;return NDIS_STATUS_FAILURE;
        }
        u->HardwareActive=TRUE;u->HardwareSpeed=speed;u->HardwareDuplex=duplex;
    }
    return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS UsbNicCheckReset(PUSBNIC u,PBOOLEAN reset)
{
    UCHAR command=0;NDIS_STATUS status;
    *reset=FALSE;
    if(!u->Nic.Running)return NDIS_STATUS_SUCCESS;
    if(USBNIC_IS_8150(u))
        status=u->Nic.Pipe.Control(&u->Nic.Pipe,RTLU_RT_READ,RTLU_REQ_GET_REGS,R8150_CR,0,&command,1);
    else status=RtlUsbReadRegs(&u->Nic,RTLU_MCU_TYPE_PLA,RTLU_PLA_CR,&command,1);
    if(!USB_OK(status))return status;
    if(command==0xff)return NDIS_STATUS_FAILURE;
    *reset=(command&(RTLU_CR_TE|RTLU_CR_RE))!=(RTLU_CR_TE|RTLU_CR_RE);
    return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS UsbNicRecoverReset(PUSBNIC u)
{
    UCHAR mac[6],i;USHORT version=u->Nic.HwVersion;UCHAR package=u->Nic.PackageId;
    NDIS_STATUS status;RTL_UPHY_REPORT report;
    for(i=0;i<6;i++)mac[i]=u->Nic.Mac[i];
    u->Nic.Running=FALSE;u->HardwareActive=FALSE;u->HardwareReady=FALSE;
    if(USBNIC_IS_8150(u)){
        status=Rtl8150Reset(&u->Nic);
        if(USB_OK(status))status=Rtl8150ReadMac(&u->Nic);
    }else{
        status=RtlUsbDetectVersion(&u->Nic);
        if(USB_OK(status)&&(version!=u->Nic.HwVersion||package!=u->Nic.PackageId))
            status=NDIS_STATUS_ADAPTER_NOT_FOUND;
        if(USB_OK(status)){
            if(u->Platform.WaitMs){
                if(RtlUsbHardwareApply(&u->Nic,&u->Hardware,u->Platform.BusSpeed,
                   RTL_USB_VENDOR_ID,u->Nic.Chip->DeviceId,RTLU_RX_BUF_SIZE,
                   u->Platform.WaitMs,u->Platform.Cancelled,u->Platform.Context,&report)!=RtlUsbFwExecOk)
                    status=NDIS_STATUS_FAILURE;
            }else status=RtlUsbReset(&u->Nic);
        }
        if(USB_OK(status))status=RtlUsbReadMac(&u->Nic);
    }
    for(i=0;i<6;i++){
        if(u->Nic.Mac[i]!=mac[i])status=NDIS_STATUS_ADAPTER_NOT_FOUND;
        u->Nic.Mac[i]=mac[i]; /* retain binding identity on failed recovery */
    }
    if(!USB_OK(status))return status;
    if(USBNIC_IS_8150(u))return Rtl8150Start(&u->Nic);
    if(!u->Platform.WaitMs)return RtlUsbStart(&u->Nic);
    u->HardwareReady=TRUE; /* carrier enables the MAC after negotiation */
    return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS UsbNicSend(PUSBNIC u, PUCHAR frame, ULONG len)
{
    UCHAR out[16 + RTLU_MAX_FRAME];
    ULONG total, done = 0;
    NDIS_STATUS st;

    if (!u->Nic.Running) return NDIS_STATUS_FAILURE;
    if (len > RTLU_MAX_FRAME) return NDIS_STATUS_FAILURE;

    if (USBNIC_IS_8150(u)) total = Rtl8150BuildTx(frame, len, out);
    else if (u->Nic.DescriptorV2)
        total = Rtl8157BuildTx(frame, len, out);
    else total = RtlUsbBuildTx(frame, len, out);
    st = u->Nic.Pipe.Bulk(&u->Nic.Pipe, RTLU_EP_BULK_OUT, out, total, &done);
    if (USB_OK(st) && done != total) return NDIS_STATUS_FAILURE;
    return st;
}

NDIS_STATUS UsbNicPoll(PUSBNIC u, PFN_USB_RX_SINK sink, PVOID ctx)
{
    UCHAR buf[RTLU_RX_BUF_SIZE];
    ULONG got = 0, at;
    NDIS_STATUS st;

    if (!u->Nic.Running) return NDIS_STATUS_FAILURE;

    st = u->Nic.Pipe.Bulk(&u->Nic.Pipe, RTLU_EP_BULK_IN, buf,
                          RTLU_RX_BUF_SIZE, &got);
    if (!USB_OK(st)) return st;
    if (got > sizeof(buf)) return NDIS_STATUS_FAILURE;

    if (USBNIC_IS_8150(u)) {
        /* one frame per bulk-IN buffer, trailing 4-byte status stripped */
        PUCHAR frame; ULONG flen;
        Rtl8150NextRx(buf, got, &frame, &flen);
        if (frame && flen) sink(ctx, frame, flen);
        return NDIS_STATUS_SUCCESS;
    }

    at = 0;
    for (;;) {
        PUCHAR frame; ULONG flen; ULONG next;
        if (u->Nic.DescriptorV2)
            next = Rtl8157NextRx(buf, got, at, &frame, &flen);
        else next = RtlUsbNextRx(buf, got, at, &frame, &flen);
        if (frame && flen) sink(ctx, frame, flen);
        if (!next) break;
        at = next;
    }
    return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS UsbNicReadLink(PUSBNIC u, PBOOLEAN connected)
{
    UCHAR data[2];
    USHORT status;
    NDIS_STATUS st;
    *connected = FALSE;
    if (!u->Nic.Running&&!u->HardwareReady) return NDIS_STATUS_SUCCESS;
    if (USBNIC_IS_8150(u)) {
        st = u->Nic.Pipe.Control(&u->Nic.Pipe, RTLU_RT_READ,
                 RTLU_REQ_GET_REGS, R8150_MSR, 0, data, 1);
        if (!USB_OK(st)) return st;
        if (data[0] == 0xFF) return NDIS_STATUS_FAILURE;
        *connected = (BOOLEAN)!!(data[0] & R8150_MSR_LINK);
    } else {
        st = RtlUsbReadRegs(&u->Nic, RTLU_MCU_TYPE_PLA, 0xE908, data, 2);
        if (!USB_OK(st)) return st;
        status = (USHORT)(data[0] | ((USHORT)data[1] << 8));
        if (status == 0xFFFF) return NDIS_STATUS_FAILURE;
        *connected = (BOOLEAN)!!(status & 0x0002); /* PLA_PHYSTATUS LINK_STATUS */
    }
    return NDIS_STATUS_SUCCESS;
}

VOID UsbNicDetach(PUSBNIC u)
{
    if (u->Nic.Running) {
        if (USBNIC_IS_8150(u)) Rtl8150Stop(&u->Nic);
        else RtlUsbStop(&u->Nic);
    }
    u->Route = UsbRouteNone;
    u->HardwareReady=u->HardwareActive=FALSE;
}

NDIS_STATUS UsbNicReadMedia(PUSBNIC u,PBOOLEAN connected,PULONG speed,PBOOLEAN duplex)
{
    UCHAR data[2];USHORT status,bits;NDIS_STATUS st;ULONG detected=0;
    *connected=FALSE;*speed=0;*duplex=FALSE;
    if(!u->Nic.Running&&!u->HardwareReady)return NDIS_STATUS_SUCCESS;
    if(USBNIC_IS_8150(u)){
        st=u->Nic.Pipe.Control(&u->Nic.Pipe,RTLU_RT_READ,RTLU_REQ_GET_REGS,R8150_MSR,0,data,1);
        if(!USB_OK(st))return st;
        if(data[0]==0xff)return NDIS_STATUS_FAILURE;
        if(!(data[0]&R8150_MSR_LINK))return NDIS_STATUS_SUCCESS;
        detected=(data[0]&R8150_MSR_SPEED_100)?100:10;*duplex=!!(data[0]&0x10);
    }else{
        st=RtlUsbReadRegs(&u->Nic,RTLU_MCU_TYPE_PLA,0xe908,data,2);
        if(!USB_OK(st))return st;
        status=(USHORT)(data[0]|((USHORT)data[1]<<8));
        if(status==0xffff)return NDIS_STATUS_FAILURE;
        if(!(status&2))return NDIS_STATUS_SUCCESS;
        bits=(USHORT)(status&0x571c); /* 10/100/500/1000/1250/2500/5000/10000 */
        if(!bits||(bits&(bits-1)))return NDIS_STATUS_FAILURE;
        switch(bits){
        case 0x0004:detected=10;break;case 0x0008:detected=100;break;
        case 0x0010:detected=1000;break;case 0x0100:detected=500;break;
        case 0x0200:detected=1250;break;case 0x0400:detected=2500;break;
        case 0x1000:detected=5000;break;case 0x4000:detected=10000;break;
        }
        if(!detected||!u->Nic.MaxSpeedMbit||detected>u->Nic.MaxSpeedMbit)return NDIS_STATUS_FAILURE;
        *duplex=!!(status&1);
    }
    *speed=detected;*connected=TRUE;return NDIS_STATUS_SUCCESS;
}
