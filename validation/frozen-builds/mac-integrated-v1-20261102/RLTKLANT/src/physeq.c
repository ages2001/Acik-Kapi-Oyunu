/* SPDX-License-Identifier: GPL-2.0-only
 * Error-tracking transport for third_party/r8169/phy_config.c.
 * No allocation, kernel ABI or global mutable state. Errors latch so the
 * remaining register sequence cannot continue programming a failed device.
 */
#include "../third_party/r8169/phycompat.h"
static int Ready(struct phy_device*p)
{
    if(p->Report->Result!=RtlFwOk)return 0;
    if(p->Bus->Removed||p->Bus->Cancelled(p->Bus->Context)){
        p->Report->Result=RtlFwCancelled;
        p->Report->FailedOperation=p->Report->Operations;return 0;
    }
    return 1;
}
static int Complete(struct phy_device*p,int ok)
{
    p->Report->Operations++;
    if(!ok&&p->Report->Result==RtlFwOk){
        p->Report->Result=(p->Bus->Removed||p->Bus->Cancelled(p->Bus->Context))?RtlFwCancelled:RtlFwIoError;
        p->Report->FailedOperation=p->Report->Operations-1;
    }
    return ok?0:-1;
}
int phy_read(struct phy_device*p,int reg)
{
    u16 value;if(!Ready(p))return -1;
    if(reg<0||reg>31)return Complete(p,0);
    if(Complete(p,RtlFwBusRead(p->Bus,0,(u16)reg,&value)))return -1;
    return value;
}
int phy_write(struct phy_device*p,int reg,u16 value)
{
    if(!Ready(p))return -1;
    if(reg<0||reg>31)return Complete(p,0);
    return Complete(p,RtlFwBusWrite(p->Bus,0,(u16)reg,value));
}
int phy_modify(struct phy_device*p,int reg,u16 clear,u16 set)
{
    int old=phy_read(p,reg);u16 value;if(old<0)return -1;
    value=((u16)old&~clear)|set;
    return value==(u16)old?0:phy_write(p,reg,value);
}
int phy_select_page(struct phy_device*p,int page)
{
    if(!Ready(p))return -1;
    if(page<0||page>0xffff)return Complete(p,0);
    int old=phy_read(p,31);if(old<0)return -1;
    if(old!=page&&phy_write(p,31,(u16)page))return -1;
    return old;
}
int phy_restore_page(struct phy_device*p,int old,int result)
{
    if(old<0||!Ready(p))return -1;
    return phy_write(p,31,(u16)old)?-1:result;
}
int phy_read_paged(struct phy_device*p,int page,int reg)
{
    int old=phy_select_page(p,page),value=phy_read(p,reg);
    return phy_restore_page(p,old,value);
}
int phy_write_paged(struct phy_device*p,int page,int reg,u16 value)
{
    int old=phy_select_page(p,page),result=phy_write(p,reg,value);
    return phy_restore_page(p,old,result);
}
int phy_modify_paged(struct phy_device*p,int page,int reg,u16 clear,u16 set)
{
    int old=phy_select_page(p,page),result=phy_modify(p,reg,clear,set);
    return phy_restore_page(p,old,result);
}
static int Mmd(struct phy_device*p,int dev,u16 reg,int write,u16*value)
{
    int old,result;
    if(!Ready(p))return -1;
    if(p->Bus->Access==RtlFwPhyOcp){
        if(dev==MDIO_MMD_AN&&reg==MDIO_AN_EEE_ADV)reg=0xa5d0;
        else if(dev!=MDIO_MMD_VEND2)return Complete(p,0);
        return Complete(p,RtlFwBusOcp(p->Bus,0,write,reg,value));
    }
    /* Clause 22 MMD indirection for the older PHY implementation. */
    if(dev<0||dev>31)return Complete(p,0);
    old=phy_select_page(p,0);
    phy_write(p,13,(u16)dev);phy_write(p,14,reg);phy_write(p,13,(u16)(dev|0x4000));
    if(write)result=phy_write(p,14,*value);
    else {result=phy_read(p,14);if(result>=0){*value=(u16)result;result=0;}}
    return phy_restore_page(p,old,result);
}
int phy_write_mmd(struct phy_device*p,int dev,u16 reg,u16 value)
{
    return Mmd(p,dev,reg,1,&value);
}
int __phy_modify_mmd(struct phy_device*p,int dev,u16 reg,u16 clear,u16 set)
{
    u16 old,value;if(Mmd(p,dev,reg,0,&old))return -1;
    value=(old&~clear)|set;return old==value?0:Mmd(p,dev,reg,1,&value);
}
void RtlPhyDelay(struct phy_device*p,unsigned ms)
{
    if(Ready(p))Complete(p,p->Bus->DelayMs(p->Bus->Context,ms));
}
void RtlPhyNotReady(struct phy_device*p)
{
    if(Ready(p))Complete(p,0); /* never report success if a required patch was skipped */
}
void r8169_apply_firmware(struct rtl8169_private*tp)
{
    struct phy_device*p=tp->Phy;enum RTL_FW_RESULT result;
    if(!Ready(p)||!p->Chip->Image)return;
    result=RtlFwBusUpload(p->Bus,p->Chip->Image->Bytes,p->Chip->Image->Size,&p->Report->Firmware);
    if(result!=RtlFwOk){p->Report->Result=result;p->Report->FailedOperation=p->Report->Operations;}
    else p->Report->FirmwareApplied=1;
}
u8 rtl8168d_efuse_read(struct rtl8169_private*tp,int reg)
{
    struct phy_device*p=tp->Phy;RTL_FW_U32 value;unsigned i;
    if(!Ready(p))return 0xff;
    if(reg<0||reg>0x3ff){Complete(p,0);return 0xff;}
    p->Bus->Write32(p->Bus->Context,0xdc,(RTL_FW_U32)reg<<8);
    for(i=0;i<300;i++){
        if(!Ready(p))return 0xff;
        value=p->Bus->Read32(p->Bus->Context,0xdc);
        if(value==0xffffffffU){p->Bus->Removed=1;Complete(p,0);return 0xff;}
        if(value&0x80000000U){Complete(p,1);return (u8)value;}
        if(Complete(p,p->Bus->DelayUs(p->Bus->Context,100)))return 0xff;
    }
    Complete(p,0);return 0xff;
}
u16 rtl8168h_2_get_adc_bias_ioffset(struct rtl8169_private*tp)
{
    struct phy_device*p=tp->Phy;u16 selector=0x807d,high,low,value;
    if(!Ready(p))return 0;
    if(Complete(p,RtlFwBusOcp(p->Bus,1,1,0xdd02,&selector))||
       Complete(p,RtlFwBusOcp(p->Bus,1,0,0xdd02,&high))||
       Complete(p,RtlFwBusOcp(p->Bus,1,0,0xdd00,&low)))return 0;
    value=((low>>1)&0x7ff8)|(low&7);if(high&0x80)value|=0x8000;return value;
}
enum RTL_FW_RESULT RtlPciPhyConfigure(RTL_FW_BUS*bus,const RTL_FW_CHIP*chip,RTL_PHY_REPORT*report)
{
    struct phy_device phy;struct rtl8169_private tp;unsigned i;
    if(!report)return RtlFwInvalidArgument;
    {unsigned char*p=(unsigned char*)report;for(i=0;i<sizeof(*report);i++)p[i]=0;}
    report->FailedOperation=~0U;
    if(!bus||!chip||!bus->Read32||!bus->Write32||!bus->DelayUs||!bus->DelayMs||!bus->Cancelled||
       !bus->PollLimit||bus->PollLimit>2000||bus->Access<RtlFwPhyMdio||bus->Access>RtlFwPhyOcp){
        report->Result=RtlFwInvalidArgument;return report->Result;
    }
    /* Suffixes are sparse. Reject unknown entries before touching the bus;
       a NULL dispatch slot is valid only for the documented 10/31 cases. */
    switch(chip->MacVersion){
    case 2:case 3:case 4:case 5:case 6:case 7:case 8:case 9:case 10:
    case 14:case 17:case 18:case 19:case 20:case 21:case 22:case 23:case 24:
    case 25:case 26:case 28:case 29:case 30:case 31:case 32:case 33:case 34:
    case 35:case 36:case 37:case 38:case 39:case 40:case 42:case 43:case 44:
    case 46:case 48:case 51:case 52:case 61:case 63:case 64:case 65:case 66:
    case 70:case 80:break;
    default:report->Result=RtlFwInvalidArgument;return report->Result;
    }
    phy.Bus=bus;phy.Chip=chip;phy.Report=report;tp.Phy=&phy;
    /* Match rtl_get_sfp_mode / rtl_is_8116af in the pinned r8169 source.
       The package identifier is authoritative across the complete 8125+
       family, not just the RTL8127 XID. SerDes has a separate reset/link
       sequence; do not program a copper PHY on one of these packages. */
    if(chip->MacVersion>=61){
        u16 hardware;
        if(!Ready(&phy)||Complete(&phy,RtlFwBusOcp(bus,1,0,0xd006,&hardware)))return report->Result;
        if((hardware&0xff)==7){report->Result=RtlFwInvalidArgument;return report->Result;}
    }else if(chip->MacVersion==52){
        u16 package,hardware;
        if(!Ready(&phy)||Complete(&phy,RtlFwBusOcp(bus,1,0,0xdc00,&package)))return report->Result;
        if((package&0x78)==0x30){
            if(!Ready(&phy)||Complete(&phy,RtlFwBusOcp(bus,1,0,0xd006,&hardware)))return report->Result;
            if(!(hardware&0xff)){report->Result=RtlFwInvalidArgument;return report->Result;}
        }
    }
    phy_write(&phy,31,0);
    if(Ready(&phy))r8169_hw_phy_config(&tp,&phy,(enum mac_version)chip->MacVersion);
    /* RTL8117 applies its payload later in rtl_hw_start_8117 (MAC startup),
       not in this PHY sequence. FirmwareApplied deliberately stays false. */
    if(Ready(&phy)&&chip->Image&&chip->MacVersion!=52&&!report->FirmwareApplied)Complete(&phy,0);
    if(Ready(&phy))phy_write(&phy,31,0);
    return report->Result;
}

enum RTL_FW_RESULT RtlPciPhyConfigureByXid(RTL_FW_BUS*bus,RTL_FW_U16 xid,int gmii,RTL_PHY_REPORT*report)
{
    const RTL_FW_CHIP*chip=RtlFwChip(xid,gmii);
    if(bus&&chip){
        bus->Access=chip->MacVersion>=40?RtlFwPhyOcp:
                    (chip->MacVersion==28||chip->MacVersion==31)?RtlFwPhyMdioDp:RtlFwPhyMdio;
        bus->Base=0xa400;bus->PollLimit=2000;bus->MaxCommands=200000;bus->MaxDelayMs=2000;
    }
    /* The RTL8117 MAC-only payload needs rtl_hw_start_8117, which is not
       provided by this PHY component. Refuse to start DMA on that revision
       until the separate MAC integration exists. */
    if(chip&&chip->MacVersion==52&&chip->Image)chip=0;
    return RtlPciPhyConfigure(bus,chip,report);
}
