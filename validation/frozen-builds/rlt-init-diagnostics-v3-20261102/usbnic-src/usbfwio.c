/* SPDX-License-Identifier: GPL-2.0-only
 * USB control/PHY binding for the bounded firmware transaction. Reuse the
 * production byte-enable and paged/TGPHY protocols; check cancellation at
 * every control transfer, including the middle of a multi-DWORD write.
 */
#include "usbfwio.h"
#include "usbphy.h"
typedef struct {
    RTL_USB_NIC Nic;
    PRTL_USB_NIC Original;
    int (*Wait)(void*,unsigned),(*Cancel)(void*);
    void*Context;
    int Failed;
} IO;
static int cancelled(void*p){IO*x=p;return x->Cancel(x->Context);}
static NDIS_STATUS control(PUSBNT_PIPE pipe,UCHAR rt,UCHAR request,USHORT value,
                           USHORT index,PVOID buffer,USHORT length)
{
    IO*x=pipe->HcContext;NDIS_STATUS status;
    if(x->Failed||cancelled(x))return NDIS_STATUS_FAILURE;
    status=x->Original->Pipe.Control(&x->Original->Pipe,rt,request,value,index,buffer,length);
    if(!USB_OK(status))x->Failed=1;
    return status;
}
static int wait_ms(void*p,unsigned ms)
{
    IO*x=p;
    if(x->Failed||cancelled(x))return 0;
    if(!x->Wait(x->Context,ms)){x->Failed=1;return 0;}
    return !cancelled(x);
}
static VOID phy_wait(PVOID p,ULONG ms){(void)wait_ms(p,(unsigned)ms);}
static int phy_read(IO*x,unsigned reg,USHORT*v)
{
    return USB_OK(RtlUsbPhyRead(&x->Nic,(USHORT)reg,v,phy_wait,x))&&!x->Failed;
}
static int phy_write(IO*x,unsigned reg,unsigned v)
{
    return USB_OK(RtlUsbPhyWrite(&x->Nic,(USHORT)reg,(USHORT)v,phy_wait,x))&&!x->Failed;
}
static int read_io(void*p,enum RTL_UFW_SPACE space,unsigned reg,RTL_UFW_U8*out,unsigned n)
{
    IO*x=p;USHORT v;
    if(reg>0xffff||!n||n>0xffff)return 0;
    if(space==RtlUsbFwUsb||space==RtlUsbFwPla)
        return USB_OK(RtlUsbReadRegs(&x->Nic,space==RtlUsbFwPla?RTLU_MCU_TYPE_PLA:RTLU_MCU_TYPE_USB,
                                     (USHORT)reg,out,(USHORT)n));
    if(n!=2)return 0;
    if(space==RtlUsbFwSram){if(!phy_write(x,0xa436,reg)||!phy_read(x,0xa438,&v))return 0;}
    else if(space==RtlUsbFwPhy){if(!phy_read(x,reg,&v))return 0;}
    else return 0;
    out[0]=(RTL_UFW_U8)v;out[1]=(RTL_UFW_U8)(v>>8);return 1;
}
static int write_io(void*p,enum RTL_UFW_SPACE space,unsigned reg,const RTL_UFW_U8*in,unsigned n)
{
    IO*x=p;unsigned v;
    if(reg>0xffff||!n||n>0xffff)return 0;
    if(space==RtlUsbFwUsb||space==RtlUsbFwPla)
        return USB_OK(RtlUsbWriteRegs(&x->Nic,space==RtlUsbFwPla?RTLU_MCU_TYPE_PLA:RTLU_MCU_TYPE_USB,
                                      (USHORT)reg,RTLU_BYTE_EN_DWORD,(PVOID)in,(USHORT)n));
    if(n!=2)return 0;
    v=in[0]|(unsigned)in[1]<<8;
    if(space==RtlUsbFwSram)return phy_write(x,0xa436,reg)&&phy_write(x,0xa438,v);
    if(space==RtlUsbFwPhy)return phy_write(x,reg,v);
    return 0;
}
enum RTL_UFW_EXEC_RESULT RtlUsbFirmwareApply(PRTL_USB_NIC nic,const RTL_UFW_U8*data,
    RTL_UFW_U32 size,unsigned flags,int(*wait)(void*,unsigned),int(*cancel)(void*),void*context,RTL_UFW_EXEC_REPORT*report)
{
    IO io;RTL_UFW_BUS bus;unsigned i;
    if(!report)return RtlUsbFwExecArgument;
    for(i=0;i<sizeof(*report);i++)((RTL_UFW_U8*)report)[i]=0;
    if(!nic||!nic->Pipe.Control||nic->Running||!wait||!cancel){
        report->Result=RtlUsbFwExecArgument;return report->Result;
    }
    io.Nic=*nic;io.Original=nic;io.Wait=wait;io.Cancel=cancel;io.Context=context;io.Failed=0;
    io.Nic.Pipe.Control=control;io.Nic.Pipe.HcContext=&io;
    bus.Context=&io;bus.Read=read_io;bus.Write=write_io;bus.DelayMs=wait_ms;
    bus.Cancelled=cancelled;bus.MaxOperations=200000;bus.MaxDelayMs=20000;
    bus.Flags=flags;
    return RtlUsbFwExecute(data,size,nic->Version,&bus,report);
}
enum RTL_UFW_EXEC_RESULT RtlUsbPhyApply(PRTL_USB_NIC nic,unsigned options,
    int(*wait)(void*,unsigned),int(*cancel)(void*),void*context,RTL_UPHY_REPORT*report)
{
    IO io;RTL_UFW_BUS bus;unsigned i;
    if(!report)return RtlUsbFwExecArgument;
    for(i=0;i<sizeof(*report);i++)((RTL_UFW_U8*)report)[i]=0;
    if(!nic||!nic->Pipe.Control||nic->Running||!wait||!cancel){
        report->Result=RtlUsbFwExecArgument;return report->Result;
    }
    io.Nic=*nic;io.Original=nic;io.Wait=wait;io.Cancel=cancel;io.Context=context;io.Failed=0;
    io.Nic.Pipe.Control=control;io.Nic.Pipe.HcContext=&io;
    bus.Context=&io;bus.Read=read_io;bus.Write=write_io;bus.DelayMs=wait_ms;
    bus.Cancelled=cancelled;bus.MaxOperations=200000;bus.MaxDelayMs=30000;bus.Flags=0;
    return RtlUsbPhyConfigure(nic->Version,nic->PackageId,options,&bus,report);
}
static int hardware_io(IO*io,RTL_UFW_BUS*bus,PRTL_USB_NIC nic,
    int(*wait)(void*,unsigned),int(*cancel)(void*),void*context,RTL_UPHY_REPORT*report)
{
    unsigned i;
    if(!report)return 0;
    for(i=0;i<sizeof(*report);i++)((RTL_UFW_U8*)report)[i]=0;
    if(!nic||!nic->Pipe.Control||nic->Running||!wait||!cancel){
        report->Result=RtlUsbFwExecArgument;return 0;
    }
    io->Nic=*nic;io->Original=nic;io->Wait=wait;io->Cancel=cancel;io->Context=context;io->Failed=0;
    io->Nic.Pipe.Control=control;io->Nic.Pipe.HcContext=io;
    bus->Context=io;bus->Read=read_io;bus->Write=write_io;bus->DelayMs=wait_ms;
    bus->Cancelled=cancelled;bus->MaxOperations=200000;bus->MaxDelayMs=30000;bus->Flags=0;
    return 1;
}
enum RTL_UFW_EXEC_RESULT RtlUsbHardwareApply(PRTL_USB_NIC nic,RTL_USB_HW_STATE*state,
    unsigned usb_speed,unsigned vid,unsigned pid,unsigned rx_buffer_size,
    int(*wait)(void*,unsigned),int(*cancel)(void*),void*context,RTL_UPHY_REPORT*report)
{
    IO io;RTL_UFW_BUS bus;
    if(!hardware_io(&io,&bus,nic,wait,cancel,context,report))return RtlUsbFwExecArgument;
    /* EEE/green default off until the driver exposes a supported policy. */
    return RtlUsbHardwarePrepare(state,nic->Version,nic->PackageId,0,
        usb_speed,vid,pid,rx_buffer_size,&bus,report);
}
enum RTL_UFW_EXEC_RESULT RtlUsbHardwareEnableApply(PRTL_USB_NIC nic,RTL_USB_HW_STATE*state,
    int(*wait)(void*,unsigned),int(*cancel)(void*),void*context,RTL_UPHY_REPORT*report)
{
    IO io;RTL_UFW_BUS bus;enum RTL_UFW_EXEC_RESULT result;
    if(!hardware_io(&io,&bus,nic,wait,cancel,context,report))return RtlUsbFwExecArgument;
    result=RtlUsbHardwareEnable(state,&bus,report);
    if(result==RtlUsbFwExecOk)nic->Running=TRUE;
    return result;
}
