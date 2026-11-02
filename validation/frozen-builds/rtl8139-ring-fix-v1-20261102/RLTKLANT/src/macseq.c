/* SPDX-License-Identifier: GPL-2.0-only
 * Fallible transport for verbatim pinned r8169 MAC configuration sequences.
 * First error latches; the remaining bounded sequence cannot touch the bus.
 * pci_start.c supplies the miniport's bounded register windows and passive
 * initialization/reset ownership. NT3.1 has a separate full-MAC lifecycle.
 */
#include "macseq.h"
#include "../third_party/r8169/versions.h"
#define RTL_GIGA_MAC_VER_LAST RTL_GIGA_MAC_VER_80
#define fallthrough __attribute__((fallthrough))
typedef RTL_FW_U8 u8;
typedef RTL_FW_U16 u16;
typedef RTL_FW_U32 u32;
typedef _Bool bool;
#define true 1
#define false 0
#define NULL ((void*)0)
#define BIT(n) (1U<<(n))
#define GENMASK(h,l) ((~0U<<(l))&(~0U>>(31-(h))))
#define ARRAY_SIZE(a) ((int)(sizeof(a)/sizeof((a)[0])))
#define READ_ONCE(value) (value)
#define ETH_HLEN 14
static unsigned field_prep(unsigned mask,unsigned value){unsigned shift=0;while(mask&&!(mask&1)){mask>>=1;shift++;}return (value&mask)<<shift;}
#define FIELD_PREP(m,v) field_prep(m,v)
static unsigned ilog2(unsigned v){unsigned n=0;while(v>1){v>>=1;n++;}return n;}
#include "../third_party/r8169/mac-constants.h"
struct rtl8169_private;
typedef void (*rtl_generic_fct)(struct rtl8169_private *);
struct pci_dev {struct rtl8169_private *owner;unsigned cfg_size,devfn,revision;};
struct phy_device {struct rtl8169_private *owner;};
struct net_device {unsigned mtu;};
struct rtl8169_private {
    const RTL_MAC_IO *Io;const RTL_FW_CHIP *Chip;RTL_MAC_REPORT *Report;
    RTL_FW_BUS FirmwareBus;
    struct pci_dev pci,*pci_dev;struct phy_device phy,*phydev;
    unsigned mac_version,cp_cmd,num_rx_rings,irq_nvecs,mac_ocp_lock;
    unsigned tx_lpi_timer,aspm_manageable;
    enum rtl_dash_type dash_type;
    unsigned dash_enabled;
    struct net_device net,*dev;
};
static void MacFail(struct rtl8169_private *tp,enum RTL_FW_RESULT error)
{
    if(tp->Report->Result==RtlFwOk){tp->Report->Result=error;tp->Report->FailedOperation=tp->Report->Operations;}
}
static int MacReady(struct rtl8169_private *tp)
{
    if(tp->Report->Result!=RtlFwOk)return 0;
    if(tp->Io->Cancelled(tp->Io->Context)){MacFail(tp,RtlFwCancelled);return 0;}
    if(tp->Report->Operations>=tp->Io->MaxOperations){MacFail(tp,RtlFwBudgetExceeded);return 0;}
    return 1;
}
static int MacComplete(struct rtl8169_private *tp,int ok)
{
    if(!ok)MacFail(tp,tp->Io->Cancelled(tp->Io->Context)?RtlFwCancelled:RtlFwIoError);
    tp->Report->Operations++;return ok;
}
static u32 MacRead(struct rtl8169_private *tp,unsigned offset,unsigned width)
{
    u32 value=~0U;if(!MacReady(tp))return value;
    if((width!=1&&width!=2&&width!=4)||offset%width||offset>tp->Io->RegisterBytes-width){MacFail(tp,RtlFwInvalidArgument);return value;}
    MacComplete(tp,tp->Io->Read(tp->Io->Context,(u16)offset,width,&value));return value;
}
static void MacWrite(struct rtl8169_private *tp,unsigned offset,unsigned width,u32 value)
{
    if(!MacReady(tp))return;
    if((width!=1&&width!=2&&width!=4)||offset%width||offset>tp->Io->RegisterBytes-width){MacFail(tp,RtlFwInvalidArgument);return;}
    MacComplete(tp,tp->Io->Write(tp->Io->Context,(u16)offset,width,value));
}
static int MacDelay(struct rtl8169_private *tp,u32 us)
{
    if(!MacReady(tp))return 0;
    if(us>tp->Io->MaxDelayUs-tp->Report->DelayUs){MacFail(tp,RtlFwBudgetExceeded);return 0;}
    tp->Report->DelayUs+=us;return MacComplete(tp,tp->Io->DelayUs(tp->Io->Context,us));
}
#define RTL_R8(t,r) ((u8)MacRead(t,r,1))
#define RTL_R16(t,r) ((u16)MacRead(t,r,2))
#define RTL_R32(t,r) MacRead(t,r,4)
#define RTL_W8(t,r,v) MacWrite(t,r,1,(u8)(v))
#define RTL_W16(t,r,v) MacWrite(t,r,2,(u16)(v))
#define RTL_W32(t,r,v) MacWrite(t,r,4,(u32)(v))
#define udelay(us) ((void)MacDelay(tp,(us)))
#define mdelay(ms) ((void)MacDelay(tp,(ms)*1000U))
#define msleep(ms) ((void)MacDelay(tp,(ms)*1000U))
#define fsleep(us) ((void)MacDelay(tp,(us)))
#define WARN(test,...) ((test)?(MacFail(tp,RtlFwInvalidArgument),1):0)
#define netdev_notice_once(...) ((void)0)
/* Entire transaction is already exclusively owned, including MCU windows. */
#define raw_spin_lock_irqsave(lock,flags) ((void)(lock),(flags)=0)
#define raw_spin_unlock_irqrestore(lock,flags) ((void)(lock),(void)(flags))
#define PCI_FUNC(devfn) ((devfn)&7)
#define PCIBIOS_SUCCESSFUL 0
#define PCI_EXP_LNKCTL 0x10
#define PCI_EXP_LNKCTL_CLKREQ_EN 0x100
static int MacPci(struct pci_dev *p,unsigned offset,unsigned width,int write,u32 *value)
{
    struct rtl8169_private *tp=p->owner;
    if(!MacReady(tp))return -1;
    if(offset>256-width||offset%width){MacFail(tp,RtlFwInvalidArgument);return -1;}
    return MacComplete(tp,write?tp->Io->PciWrite(tp->Io->Context,(u16)offset,width,*value):tp->Io->PciRead(tp->Io->Context,(u16)offset,width,value))?0:-1;
}
static int pci_read_config_byte(struct pci_dev *p,unsigned offset,u8 *value){u32 v=0;int result=MacPci(p,offset,1,0,&v);*value=(u8)v;return result;}
static int pci_write_config_byte(struct pci_dev *p,unsigned offset,u8 value){u32 v=value;return MacPci(p,offset,1,1,&v);}
static void MacPcieModify(struct pci_dev *p,unsigned offset,u16 clear,u16 set)
{
    u32 v;struct rtl8169_private *tp=p->owner;unsigned cap=tp->Io->PcieCapability;
    if(!cap){MacFail(tp,RtlFwInvalidArgument);return;}
    if(MacPci(p,cap+offset,2,0,&v))return;
    v=(v&~clear)|set;MacPci(p,cap+offset,2,1,&v);
}
#define pcie_capability_clear_word(p,o,v) MacPcieModify(p,o,v,0)
#define pcie_capability_set_word(p,o,v) MacPcieModify(p,o,0,v)
struct rtl_cond {bool (*check)(struct rtl8169_private *);};
#define DECLARE_RTL_COND(name) static bool name##_check(struct rtl8169_private *); static const struct rtl_cond name={name##_check}; static bool name##_check(struct rtl8169_private *tp)
static bool MacWait(struct rtl8169_private *tp,const struct rtl_cond *cond,unsigned long delay,int count,int high)
{
    int i;if(!MacReady(tp))return false;
    if(count<1||count>2000||delay>50000){MacFail(tp,RtlFwInvalidArgument);return false;}
    for(i=0;i<count&&MacReady(tp);i++){
        int state=!!cond->check(tp);
        if(!MacReady(tp))return false;
        if(state==high)return true;
        if(!MacDelay(tp,(u32)delay))return false;
    }
    MacFail(tp,RtlFwIoError);return false;
}
static bool rtl_loop_wait_low(struct rtl8169_private *tp,const struct rtl_cond *cond,unsigned long delay,int count){return MacWait(tp,cond,delay,count,0);}
static bool rtl_loop_wait_high(struct rtl8169_private *tp,const struct rtl_cond *cond,unsigned long delay,int count){return MacWait(tp,cond,delay,count,1);}
static int MacWaitFlag(struct rtl8169_private *tp,unsigned offset,u32 mask,int high,unsigned delay,unsigned count)
{
    unsigned i;
    for(i=0;i<count&&MacReady(tp);i++){
        u32 value=RTL_R32(tp,offset);
        if(!MacReady(tp))return 0;
        if(!!(value&mask)==high)return 1;
        if(!MacDelay(tp,delay))return 0;
    }
    MacFail(tp,RtlFwIoError);return 0;
}
static u32 rtl_eri_read(struct rtl8169_private *tp,int addr)
{
    if(addr<0||addr>0xfff||(addr&3)){MacFail(tp,RtlFwInvalidArgument);return ~0U;}
    RTL_W32(tp,ERIAR,ERIAR_MASK_1111|(u32)addr);
    return MacWaitFlag(tp,ERIAR,ERIAR_FLAG,1,100,100)?RTL_R32(tp,ERIDR):~0U;
}
static void rtl_eri_write(struct rtl8169_private *tp,int addr,u32 mask,u32 value)
{
    if(addr<0||addr>0xfff||(addr&3)||!mask||(mask&~ERIAR_MASK_1111)){MacFail(tp,RtlFwInvalidArgument);return;}
    RTL_W32(tp,ERIDR,value);RTL_W32(tp,ERIAR,ERIAR_WRITE_CMD|mask|(u32)addr);
    MacWaitFlag(tp,ERIAR,ERIAR_FLAG,0,100,100);
}
static u16 rtl_ephy_read(struct rtl8169_private *tp,int address)
{
    if(address<0||address>0x7f){MacFail(tp,RtlFwInvalidArgument);return 0xffff;}
    RTL_W32(tp,EPHYAR,((u32)address&EPHYAR_REG_MASK)<<EPHYAR_REG_SHIFT);
    return MacWaitFlag(tp,EPHYAR,EPHYAR_FLAG,1,10,100)?(u16)RTL_R32(tp,EPHYAR):0xffff;
}
static void rtl_ephy_write(struct rtl8169_private *tp,int address,int value)
{
    if(address<0||address>0x7f){MacFail(tp,RtlFwInvalidArgument);return;}
    RTL_W32(tp,EPHYAR,EPHYAR_WRITE_CMD|(((u32)address&EPHYAR_REG_MASK)<<EPHYAR_REG_SHIFT)|((u32)value&EPHYAR_DATA_MASK));
    MacWaitFlag(tp,EPHYAR,EPHYAR_FLAG,0,10,100);MacDelay(tp,10);
}
static u32 MacFwRead(void *ctx,u16 offset){return MacRead(ctx,offset,4);}
static void MacFwWrite(void *ctx,u16 offset,u32 value){MacWrite(ctx,offset,4,value);}
static int MacFwUs(void *ctx,u32 us){return MacDelay(ctx,us);}
static int MacFwMs(void *ctx,u32 ms){if(ms>2000){MacFail(ctx,RtlFwInvalidArgument);return 0;}return MacDelay(ctx,ms*1000);}
static int MacFwCancelled(void *ctx){return !MacReady(ctx);}
static u16 r8168_mac_ocp_read(struct rtl8169_private *tp,u32 address)
{
    u16 v=0xffff;if(!MacReady(tp))return v;
    if(address>0xffff||(address&1)){MacFail(tp,RtlFwInvalidArgument);return v;}
    if(!RtlFwBusOcp(&tp->FirmwareBus,1,0,(u16)address,&v))MacFail(tp,RtlFwIoError);
    return v;
}
static void r8168_mac_ocp_write(struct rtl8169_private *tp,u32 address,u32 value)
{
    u16 v=(u16)value;if(!MacReady(tp))return;
    if(address>0xffff||(address&1)){MacFail(tp,RtlFwInvalidArgument);return;}
    if(!RtlFwBusOcp(&tp->FirmwareBus,1,1,(u16)address,&v))MacFail(tp,RtlFwIoError);
}
#define __r8168_mac_ocp_write r8168_mac_ocp_write
#define __r8168_mac_ocp_read r8168_mac_ocp_read
static int phy_read_paged(struct phy_device *phy,int page,int reg)
{
    struct rtl8169_private *tp=phy->owner;u16 old=0,value=0xffff;
    if(!MacReady(tp))return -1;
    if(page<0||page>0xfff||reg<0||reg>31){MacFail(tp,RtlFwInvalidArgument);return -1;}
    if(!RtlFwBusRead(&tp->FirmwareBus,0,31,&old)||!RtlFwBusWrite(&tp->FirmwareBus,0,31,(u16)page)||!RtlFwBusRead(&tp->FirmwareBus,0,(u16)reg,&value)||!RtlFwBusWrite(&tp->FirmwareBus,0,31,old))MacFail(tp,RtlFwIoError);
    return MacReady(tp)?value:-1;
}
static void r8169_apply_firmware(struct rtl8169_private *tp)
{
    enum RTL_FW_RESULT result;if(!MacReady(tp)||!tp->Chip->Image)return;
    result=RtlFwBusUpload(&tp->FirmwareBus,tp->Chip->Image->Bytes,tp->Chip->Image->Size,&tp->Report->Firmware);
    if(result!=RtlFwOk)MacFail(tp,result);else tp->Report->FirmwareApplied=1;
}
struct ephy_info {unsigned offset;u16 mask,bits;};
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#include "../third_party/r8169/mac-sequences.inc"
#pragma GCC diagnostic pop
static enum RTL_FW_RESULT MacInitialize(const RTL_MAC_IO *io,const RTL_FW_CHIP *chip,RTL_MAC_REPORT *report,struct rtl8169_private *tp)
{
    unsigned i;unsigned char *p;
    if(!report)return RtlFwInvalidArgument;
    p=(unsigned char*)report;for(i=0;i<sizeof(*report);i++)p[i]=0;report->FailedOperation=~0U;
    if(!io||!chip||!io->Read||!io->Write||!io->PciRead||!io->PciWrite||!io->DelayUs||!io->Cancelled||io->PciFunction>7||io->RegisterBytes>65536||io->RegisterBytes<(chip->MacVersion>=61?0x604aU:0x100U)||!io->MaxOperations||!io->MaxDelayUs||io->PhyAccess>RtlFwPhyOcp||io->PhyAccess<RtlFwPhyMdio){report->Result=RtlFwInvalidArgument;return report->Result;}
    switch(chip->MacVersion){
    case 2:case 3:case 4:case 5:case 6:case 7:case 8:case 9:case 10:case 14:
    case 17:case 18:case 19:case 20:case 21:case 22:case 23:case 24:case 25:case 26:
    case 28:case 29:case 30:case 31:case 32:case 33:case 34:case 35:case 36:case 37:
    case 38:case 39:case 40:case 42:case 43:case 44:case 46:case 48:case 51:case 52:
    case 61:case 63:case 64:case 65:case 66:case 70:case 80:break;
    default:report->Result=RtlFwInvalidArgument;return report->Result;
    }
    /* Reject an impossible capability location or unusable MAC payload
       before changing any register. PCIe capabilities live in the first
       256 bytes; Link Control is the highest field accessed here. */
    if(io->PcieCapability&&(io->PcieCapability<0x40||io->PcieCapability>0xec||(io->PcieCapability&3))){report->Result=RtlFwInvalidArgument;return report->Result;}
    if(chip->MacVersion==52){
        if(!chip->Image){report->Result=RtlFwInvalidArgument;return report->Result;}
        report->Result=RtlFwInspect(chip->Image->Bytes,chip->Image->Size,&report->Firmware);
        if(report->Result!=RtlFwOk)return report->Result;
    }
    p=(unsigned char*)tp;for(i=0;i<sizeof(*tp);i++)p[i]=0;
    tp->Io=io;tp->Chip=chip;tp->Report=report;tp->mac_version=chip->MacVersion;
    tp->num_rx_rings=tp->irq_nvecs=1;tp->pci.owner=tp;tp->pci.cfg_size=256;tp->pci.devfn=io->PciFunction;tp->pci_dev=&tp->pci;tp->phy.owner=tp;tp->phydev=&tp->phy;
    tp->net.mtu=1500;tp->dev=&tp->net;
    tp->FirmwareBus.Context=tp;tp->FirmwareBus.Read32=MacFwRead;tp->FirmwareBus.Write32=MacFwWrite;tp->FirmwareBus.DelayUs=MacFwUs;tp->FirmwareBus.DelayMs=MacFwMs;tp->FirmwareBus.Cancelled=MacFwCancelled;tp->FirmwareBus.Access=io->PhyAccess;tp->FirmwareBus.Base=0xa400;tp->FirmwareBus.PollLimit=2000;tp->FirmwareBus.MaxCommands=200000;tp->FirmwareBus.MaxDelayMs=2000;
    return RtlFwOk;
}
enum RTL_FW_RESULT RtlPciMacConfigure(const RTL_MAC_IO *io,const RTL_FW_CHIP *chip,RTL_MAC_REPORT *report)
{
    struct rtl8169_private tp;
    enum RTL_FW_RESULT result=MacInitialize(io,chip,report,&tp);
    if(result!=RtlFwOk)return result;
    /* Match rtl_hw_start's configuration ordering. This driver supports
       one queue, MTU 1500 and software checksums/VLAN handling. It has no
       OS ASPM ownership, so the source's aspm_manageable=false path stays
       disabled. Descriptor publication and DMA enable remain the caller's
       next phase; no reset may intervene after this transaction. */
    rtl_unlock_config_regs(&tp);
    rtl_hw_aspm_clkreq_enable(&tp,false);
    RTL_W16(&tp,CPlusCmd,0);
    rtl_set_eee_txidle_timer(&tp);
    if(chip->MacVersion<=6)rtl_hw_start_8169(&tp);
    else if(chip->MacVersion>=61)rtl_hw_start_8125(&tp);
    else rtl_hw_start_8168(&tp);
    rtl_enable_exit_l1(&tp);
    rtl_hw_aspm_clkreq_enable(&tp,true);
    rtl_lock_config_regs(&tp);
    if(MacReady(&tp)&&chip->MacVersion==52&&(!chip->Image||!report->FirmwareApplied))MacFail(&tp,RtlFwInvalidArgument);
    return report->Result;
}

/* Lifecycle callers own DMA and OS power state. Keep these phases separate:
 * rtl_hw_initialize precedes the initial reset in the pinned source, while
 * the DASH acquire notification precedes PHY/MAC startup. */
enum RTL_FW_RESULT RtlPciMacHardware(const RTL_MAC_IO *io,const RTL_FW_CHIP *chip,
                                    enum RTL_MAC_HARDWARE_STAGE stage,RTL_MAC_REPORT *report)
{
    struct rtl8169_private tp;u32 revision=0;enum RTL_FW_RESULT result;
    if(stage<RtlMacHardwareInitialize||stage>RtlMacHardwareRelease){
        if(report){unsigned i;unsigned char*p=(unsigned char*)report;
            for(i=0;i<sizeof(*report);i++)p[i]=0;
            report->Result=RtlFwInvalidArgument;report->FailedOperation=~0U;}
        return RtlFwInvalidArgument;
    }
    result=MacInitialize(io,chip,report,&tp);if(result!=RtlFwOk)return result;
    if(MacPci(&tp.pci,8,1,0,&revision))return report->Result;
    tp.pci.revision=revision;tp.dash_type=rtl_get_dash_type(&tp);
    tp.dash_enabled=rtl_dash_is_enabled(&tp);
    report->DashType=(unsigned)tp.dash_type;report->DashEnabled=tp.dash_enabled;
    if(!MacReady(&tp))return report->Result;
    if(stage==RtlMacHardwareInitialize)rtl_hw_initialize(&tp);
    else if(tp.dash_type!=RTL_DASH_NONE){
        if(stage==RtlMacHardwareAcquire)rtl8168_driver_start(&tp);
        else rtl8168_driver_stop(&tp);
    }
    return report->Result;
}
