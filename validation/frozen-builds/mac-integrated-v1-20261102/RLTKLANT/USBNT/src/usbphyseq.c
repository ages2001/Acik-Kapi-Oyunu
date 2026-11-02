/* SPDX-License-Identifier: GPL-2.0-only
 * Serialized, fallible boundary for the pinned vendor PHY configuration.
 * Generated original sequences retain their Realtek notices and GPL license.
 */
#include "usbphyseq.h"
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef int bool;
#define true 1
#define false 0
#define NULL ((void*)0)
#define BIT(n) (1U<<(n))
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define min(a,b) ((a)<(b)?(a):(b))
#define fallthrough __attribute__((fallthrough))
#define ALIGN(x,a) (((x)+(a)-1)&~((a)-1))
#define min_t(t,a,b) ((t)(a)<(t)(b)?(t)(a):(t)(b))
#define max_t(t,a,b) ((t)(a)>(t)(b)?(t)(a):(t)(b))
#define VLAN_ETH_HLEN 18
#define VLAN_ETH_FRAME_LEN 1518
#define ETH_FCS_LEN 4
#define ETH_ZLEN 60
#define MAX_SKB_FRAGS 17
/* Private feature word: the NT path always supplies zero, retaining VLAN
   tags in frame data. This is not a Linux netdev_features_t ABI. */
#define NETIF_F_HW_VLAN_CTAG_RX 1
#define ENODEV 19
#define EINVAL 22
#define EPERM 1
#define ETIME 62
#define ETIMEDOUT 110
#define KERNEL_VERSION(a,b,c) (((a)<<16)|((b)<<8)|(c))
#define LINUX_VERSION_CODE KERNEL_VERSION(6,4,10)
#define USB_SPEED_FULL 2
#define USB_SPEED_LOW 1
#define USB_SPEED_HIGH 3
#define USB_SPEED_SUPER 5
#define USB_SPEED_SUPER_PLUS 6
#define AUTONEG_DISABLE 0
#define AUTONEG_ENABLE 1
#define DUPLEX_HALF 0
#define DUPLEX_FULL 1
#define SPEED_10 10
#define SPEED_100 100
#define SPEED_1000 1000
#define SPEED_2500 2500
#define SPEED_5000 5000
#define SPEED_10000 10000
/* IEEE 802.3 clause22/45 register and advertisement constants. */
#define MII_BMCR 0
#define MII_ADVERTISE 4
#define MII_CTRL1000 9
#define BMCR_PDOWN 0x0800
#define BMCR_SPEED10 0
#define BMCR_SPEED1000 0x0040
#define BMCR_FULLDPLX 0x0100
#define BMCR_ANRESTART 0x0200
#define BMCR_ANENABLE 0x1000
#define BMCR_SPEED100 0x2000
#define BMCR_RESET 0x8000
#define ADVERTISE_10HALF 0x0020
#define ADVERTISE_10FULL 0x0040
#define ADVERTISE_100HALF 0x0080
#define ADVERTISE_100FULL 0x0100
#define ADVERTISE_1000HALF 0x0100
#define ADVERTISE_1000FULL 0x0200
#define ADVERTISE_PAUSE_CAP 0x0400
#define ADVERTISE_PAUSE_ASYM 0x0800
#define MDIO_MMD_AN 7
#define MDIO_AN_EEE_ADV 60
#define MDIO_AN_10GBT_CTRL_ADV2_5G 0x0080
#define MDIO_AN_10GBT_CTRL_ADV5G 0x0100
#define MDIO_AN_10GBT_CTRL_ADV10G 0x1000
/* Linux ethtool wake-option ABI, not register masks. */
#define WAKE_PHY BIT(0)
#define WAKE_UCAST BIT(1)
#define WAKE_MCAST BIT(2)
#define WAKE_BCAST BIT(3)
#define WAKE_MAGIC BIT(5)
#include "../../third_party/r8152/phy-constants.h"
struct usb_device {
    unsigned dev,speed;
    struct {u16 idVendor,idProduct;} descriptor;
};
struct net_device {unsigned mtu,features;};
static const unsigned agg_buf_sz=16384;
struct r8152 {
    const RTL_UFW_BUS *bus;
    RTL_UPHY_REPORT *report;
    unsigned version, flags, ocp_base, eee_en, eee_adv, eee_adv2;
    unsigned advertising,autoneg,coalesce,duplex,saved_wolopts,speed;
    unsigned support_10gfull,support_2500full,support_5000full;
    unsigned AutoloadPolls,ResetPolls;
    struct {unsigned advertising,force_media,full_duplex,supports_gmii;} mii;
    struct {void (*hw_phy_cfg)(struct r8152*);} rtl_ops;
    struct usb_device *udev;
    struct net_device *netdev;
    unsigned rx_buf_sz,fc_pause_off,fc_pause_on,sg_use,tx_qlen;
    struct {unsigned size,align;} rx_desc,tx_desc;
    struct {
        unsigned _10m_ckdiv,_250m_ckdiv,aldps,ctap_short_off,eee,eee_ckdiv;
        unsigned eee_cmod_lv,eee_plloff_100,eee_plloff_giga,flow_control;
        unsigned green,lite_mode,r_tune,speed_duplex;
    } ups_info;
};
static int fail(struct r8152 *tp, enum RTL_UFW_EXEC_RESULT result)
{
    if(!tp->report->Result) tp->report->Result=result;
    tp->flags|=BIT(RTL8152_UNPLUG);
    return -ENODEV;
}
static int gate(struct r8152 *tp)
{
    if(tp->report->Result) return 0;
    if(tp->bus->Cancelled(tp->bus->Context)) {fail(tp,RtlUsbFwExecCancelled);return 0;}
    if(tp->report->Operations>=tp->bus->MaxOperations) {fail(tp,RtlUsbFwExecBudget);return 0;}
    tp->report->Operations++;
    return 1;
}
static int transfer(struct r8152 *tp, unsigned space,unsigned reg,u8 *data,unsigned size,int write)
{
    RTL_UPHY_REPORT *r=tp->report;int ok;
    if(!data||!size||reg>0xffff||size>0x10000-reg)return fail(tp,RtlUsbFwExecArgument);
    if(!gate(tp))return -ENODEV;
    r->LastSpace=space;r->LastRegister=reg;r->LastSize=size;
    if(write)ok=tp->bus->Write(tp->bus->Context,(enum RTL_UFW_SPACE)space,reg,data,size);
    else ok=tp->bus->Read(tp->bus->Context,(enum RTL_UFW_SPACE)space,reg,data,size);
    if(!ok)return fail(tp,tp->bus->Cancelled(tp->bus->Context)?RtlUsbFwExecCancelled:RtlUsbFwExecIo);
    return 0;
}
static int read_reg(struct r8152 *tp,unsigned space,unsigned reg,u32 *value,unsigned size)
{
    u8 data[4]={0,0,0,0};int ret;unsigned i;
    *value=0;ret=transfer(tp,space,reg,data,size,0);
    if(ret<0)return ret;
    for(i=0;i<size;i++)*value|=(u32)data[i]<<(8*i);
    if(space==RtlUsbFwPla&&reg==PLA_BOOT_CTRL&&size==2){
        if(*value==0xffff)return fail(tp,RtlUsbFwExecIo);
        if(*value&AUTOLOAD_DONE)tp->AutoloadPolls=0;
        else if(++tp->AutoloadPolls>=500)return fail(tp,RtlUsbFwExecTimeout);
    }
    if(space==RtlUsbFwPhy&&reg==OCP_BASE_MII&&tp->ResetPolls){
        if(*value==0xffff)return fail(tp,RtlUsbFwExecIo);
        if(!(*value&BMCR_RESET))tp->ResetPolls=0;
        else if(++tp->ResetPolls>50)return fail(tp,RtlUsbFwExecTimeout);
    }
    return 0;
}
static int write_reg(struct r8152 *tp,unsigned space,unsigned reg,u32 value,unsigned size)
{
    u8 data[4];unsigned i;
    for(i=0;i<size;i++)data[i]=(u8)(value>>(8*i));
    if(space==RtlUsbFwPhy&&reg==OCP_BASE_MII&&(value&BMCR_RESET))tp->ResetPolls=1;
    return transfer(tp,space,reg,data,size,1);
}
#define REG_ACCESS(suffix,size) \
static int ocp_read_##suffix(struct r8152*t,u16 type,u16 reg,u32*v) \
{return read_reg(t,type==MCU_TYPE_PLA?RtlUsbFwPla:RtlUsbFwUsb,reg,v,size);} \
static int ocp_write_##suffix(struct r8152*t,u16 type,u16 reg,u32 v) \
{return write_reg(t,type==MCU_TYPE_PLA?RtlUsbFwPla:RtlUsbFwUsb,reg,v,size);}
REG_ACCESS(byte,1)
REG_ACCESS(word,2)
REG_ACCESS(dword,4)
static int ocp_reg_read(struct r8152*t,u16 reg,u16*v)
{
    u32 value=0;int ret=read_reg(t,RtlUsbFwPhy,reg,&value,2);
    *v=(u16)value;return ret;
}
static int ocp_reg_write(struct r8152*t,u16 reg,u16 value)
{return write_reg(t,RtlUsbFwPhy,reg,value,2);}
static int generic_ocp_write(struct r8152*t,u16 reg,u16 byteen,u16 size,void*data,u16 type)
{
    unsigned at,space=type==MCU_TYPE_PLA?RtlUsbFwPla:RtlUsbFwUsb;
    u8 *bytes=data;
    if((reg&3)||(size&3)||!size||!data||(unsigned)reg+size>0xffff)
        return fail(t,RtlUsbFwExecArgument);
    /* Vendor first/last-DWORD nibble semantics. Inner DWORDs enable all bytes.
       A four-byte transfer consumes only the starting nibble, as upstream. */
    for(at=0;at<size;at+=4){
        unsigned mask=at==0?(byteen&15):at+4==size?((byteen>>4)&15):15;
        if(mask==15){if(transfer(t,space,reg+at,bytes+at,4,1)<0)return -ENODEV;}
        else {unsigned i;for(i=0;i<4;i++)if(mask&BIT(i))
            if(transfer(t,space,reg+at+i,bytes+at+i,1,1)<0)return -ENODEV;}
    }
    return 0;
}
static void delay(struct r8152 *tp,unsigned ms)
{
    if(!gate(tp))return;
    if(ms>tp->bus->MaxDelayMs-tp->report->DelayMs){fail(tp,RtlUsbFwExecBudget);return;}
    tp->report->DelayMs+=ms;
    if(!tp->bus->DelayMs(tp->bus->Context,ms))
        fail(tp,tp->bus->Cancelled(tp->bus->Context)?RtlUsbFwExecCancelled:RtlUsbFwExecIo);
}
static void warning(struct r8152 *tp,int condition)
{if(condition){tp->report->Warnings++;fail(tp,RtlUsbFwExecTimeout);}}
#define msleep(ms) delay(tp,(ms))
#define usleep_range(lo,hi) delay(tp,((lo)+999)/1000)
#define WARN_ON_ONCE(condition) warning(tp,!!(condition))
#define WARN_ON(condition) warning(tp,!!(condition))
#define dev_dbg(...) ((void)0)
#define dev_err(...) ((void)fail(tp,RtlUsbFwExecVersion))
#define set_bit(n,p) (*(p)|=BIT(n))
#define clear_bit(n,p) (*(p)&=~BIT(n))
#define test_bit(n,p) (!!(*(p)&BIT(n)))
static int take_bit(unsigned n,unsigned*p){int result=!!(*p&BIT(n));*p&=~BIT(n);return result;}
#define test_and_clear_bit(n,p) take_bit(n,p)
static void *zero_mem(void*p,int value,unsigned n){u8*b=p;while(n--)*b++=(u8)value;return p;}
#define memset(p,v,n) zero_mem(p,v,n)
#define netif_dbg(...) ((void)0)
#define netif_warn(...) ((void)0)
/* MAC enable runs before the NT owner publishes/starts its packet queues. */
#define skb_queue_len_lockless(queue) 0
#define netdev_reset_queue(dev) ((void)0)
/* NT owns host wake/LPM. Preserve requests; Linux USB-core calls do not
   execute register operations and are not substituted with host side effects. */
#define device_set_wakeup_enable(dev,on) ((void)(tp->report->HostWakeRequested=!!(on)))
#define usb_enable_lpm(dev) ((void)(tp->report->HostLpmRequested=1))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#include "../../third_party/r8152/phy-sequences.inc"
#pragma GCC diagnostic pop

static int rtl8152_nic_reset(struct r8152*tp)
{
    int result=vendor_rtl8152_nic_reset(tp);u32 status;
    if(result<0)return result;
    if(ocp_read_byte(tp,MCU_TYPE_PLA,PLA_CR,&status)<0)return -ENODEV;
    if(status==0xff||(status&(CR_RST|CR_RE|CR_TE)))return fail(tp,RtlUsbFwExecTimeout);
    return tp->report->Result?-ENODEV:0;
}
static void wait_oob_link_list_ready(struct r8152*tp)
{
    u32 status;vendor_wait_oob_link_list_ready(tp);
    if(ocp_read_byte(tp,MCU_TYPE_PLA,PLA_OOB_CTRL,&status)<0)return;
    if(status==0xff||!(status&LINK_LIST_READY))fail(tp,RtlUsbFwExecTimeout);
}

static int wait_adv_cmd_ready(struct r8152*tp)
{
    int result=vendor_wait_adv_cmd_ready(tp);
    if(result<0)return fail(tp,RtlUsbFwExecTimeout);
    return result;
}
static int r8159_wait_backup_restore(struct r8152*tp)
{
    int result=vendor_r8159_wait_backup_restore(tp);
    if(result<0)return fail(tp,RtlUsbFwExecTimeout);
    return result;
}

/* Upstream has several best-effort waits. A driver initialization transaction
   must not report success after an unmet status or handshake. */
static int r8153_phy_status(struct r8152 *tp,u16 desired)
{
    int result=vendor_r8153_phy_status(tp,desired);
    if(result<0)return result;
    if(desired ? result!=desired : result!=PHY_STAT_LAN_ON&&result!=PHY_STAT_PWRDN&&result!=PHY_STAT_EXT_INIT)
        return fail(tp,RtlUsbFwExecTimeout);
    return result;
}
static int rtl_phy_patch_request(struct r8152*tp,bool request,bool wait)
{
    int result=vendor_rtl_phy_patch_request(tp,request,wait);u16 status;
    if(result<0)return result;
    if(wait){
        if(ocp_reg_read(tp,OCP_PHY_PATCH_STAT,&status)<0)return -ENODEV;
        if(status==0xffff||!!(status&PATCH_READY)!=!!request)return fail(tp,RtlUsbFwExecTimeout);
    }
    return tp->report->Result?-ENODEV:0;
}
static int r8156b_wait_loading_flash(struct r8152*tp)
{
    int result=vendor_r8156b_wait_loading_flash(tp);u32 status;
    if(result<0)return result;
    if(r8156b_flash_used(tp)){
        if(ocp_read_word(tp,MCU_TYPE_USB,USB_GPHY_CTRL,&status)<0)return -ENODEV;
        if(status==0xffff||!(status&GPHY_PATCH_DONE))return fail(tp,RtlUsbFwExecTimeout);
    }
    return tp->report->Result?-ENODEV:0;
}
static void r8153_wdt1_end(struct r8152*tp)
{
    u32 status;vendor_r8153_wdt1_end(tp);
    if(ocp_read_byte(tp,MCU_TYPE_USB,USB_WDT1_CTRL,&status)<0)return;
    if(status&WTD1_EN)fail(tp,RtlUsbFwExecTimeout);
}
static int r8153_aldps_en(struct r8152*tp,bool enable)
{
    int result=vendor_r8153_aldps_en(tp,enable);u32 status;
    if(result<0)return result;
    if(!enable){
        if(ocp_read_word(tp,MCU_TYPE_PLA,0xe000,&status)<0)return -ENODEV;
        if(status==0xffff||!(status&0x100))return fail(tp,RtlUsbFwExecTimeout);
    }
    return tp->report->Result?-ENODEV:0;
}
static int initialize(struct r8152*tp,unsigned version,unsigned package,
    unsigned options,const RTL_UFW_BUS*bus,RTL_UPHY_REPORT*report)
{
    unsigned i;
    if(!report)return 0;
    for(i=0;i<sizeof(*report);i++)((u8*)report)[i]=0;
    if(!bus||!bus->Read||!bus->Write||!bus->DelayMs||!bus->Cancelled||
       !bus->MaxOperations||!bus->MaxDelayMs||bus->Flags||(options&~3U)||
       version<1||version>17||(version!=17&&package)||
       (version==17&&package!=0x1e&&package!=0x1c&&package!=0x18&&package!=0x1a)){
        report->Result=RtlUsbFwExecArgument;return 0;
    }
    for(i=0;i<sizeof(*tp);i++)((u8*)tp)[i]=0;
    tp->bus=bus;tp->report=report;
    tp->version=version<10?version:version+1;
    if(version==17)tp->version=package==0x1e||package==0x1c?RTL_VER_17_QFN68:RTL_VER_17_QFN100;
    tp->eee_en=!!(options&RtlUsbPhyEee);
    if(options&RtlUsbPhyGreen)tp->flags|=BIT(GREEN_ETHERNET);
    tp->eee_adv=version==1||version==2||version==7?2:6;
    tp->eee_adv2=version>=16?3:version==11||version==12||version==13||version==15?1:0;
    switch(version){
    case 1:case 2:case 7:tp->rtl_ops.hw_phy_cfg=r8152b_hw_phy_cfg;break;
    case 3:case 4:case 5:case 6:tp->rtl_ops.hw_phy_cfg=r8153_hw_phy_cfg;break;
    case 8:case 9:tp->rtl_ops.hw_phy_cfg=r8153b_hw_phy_cfg;break;
    case 10:case 11:tp->rtl_ops.hw_phy_cfg=r8156_hw_phy_cfg;break;
    case 12:case 13:case 15:tp->rtl_ops.hw_phy_cfg=r8156b_hw_phy_cfg;break;
    case 14:tp->rtl_ops.hw_phy_cfg=r8153c_hw_phy_cfg;break;
    case 16:tp->rtl_ops.hw_phy_cfg=r8157_hw_phy_cfg;break;
    case 17:tp->rtl_ops.hw_phy_cfg=r8159_hw_phy_cfg;break;
    }
    return 1;
}
enum RTL_UFW_EXEC_RESULT RtlUsbPhyConfigure(unsigned version,unsigned package,
    unsigned options,const RTL_UFW_BUS*bus,RTL_UPHY_REPORT*report)
{
    struct r8152 state;struct r8152*tp=&state;
    if(!initialize(tp,version,package,options,bus,report))return RtlUsbFwExecArgument;
    tp->rtl_ops.hw_phy_cfg(tp);
    if(!report->Result&&bus->Cancelled(bus->Context))fail(tp,RtlUsbFwExecCancelled);
    return report->Result;
}
static int setup_mac(struct r8152*tp,struct usb_device*device,unsigned version,
                     unsigned usb_speed,unsigned vid,unsigned pid)
{
    if((usb_speed!=2&&usb_speed!=3&&usb_speed!=5&&usb_speed!=6)||vid>0xffff||pid>0xffff){
        fail(tp,RtlUsbFwExecArgument);return 0;
    }
    device->dev=0;device->speed=usb_speed;
    device->descriptor.idVendor=(u16)vid;device->descriptor.idProduct=(u16)pid;tp->udev=device;
    tp->mii.supports_gmii=version!=1&&version!=2&&version!=7;
    tp->support_2500full=(version>=10&&version<=13)||version>=16;
    tp->support_5000full=version>=16;tp->support_10gfull=version==17;
    tp->speed=tp->support_10gfull?10000:tp->support_5000full?5000:
        tp->support_2500full?2500:tp->mii.supports_gmii?1000:100;
    tp->autoneg=AUTONEG_ENABLE;tp->duplex=DUPLEX_FULL;tp->advertising=15;
    if(tp->mii.supports_gmii)tp->advertising|=RTL_ADVERTISED_1000_FULL;
    if(tp->support_2500full)tp->advertising|=RTL_ADVERTISED_2500_FULL;
    if(tp->support_5000full)tp->advertising|=RTL_ADVERTISED_5000_FULL;
    if(tp->support_10gfull)tp->advertising|=RTL_ADVERTISED_10GT_FULL;
    return 1;
}
static int cold_start(struct r8152*tp,unsigned version)
{
    int result=0;
    switch(version){
    case 1:case 2:case 7:result=r8152b_init(tp);break;
    case 3:case 4:case 5:case 6:result=r8153_init(tp);break;
    case 8:case 9:result=r8153b_init(tp);break;
    case 10:case 11:result=r8156_init(tp);break;
    case 12:case 13:case 15:result=r8156b_init(tp);break;
    case 14:result=r8153c_init(tp);break;
    case 16:result=r8157_init(tp);break;
    case 17:result=r8159_init(tp);break;
    }
    return result;
}
enum RTL_UFW_EXEC_RESULT RtlUsbMacPrepare(unsigned version,unsigned package,
    unsigned options,unsigned usb_speed,unsigned vid,unsigned pid,
    const RTL_UFW_BUS*bus,RTL_UPHY_REPORT*report)
{
    struct r8152 state;struct r8152*tp=&state;struct usb_device device;
    if(!initialize(tp,version,package,options,bus,report))return RtlUsbFwExecArgument;
    if(!setup_mac(tp,&device,version,usb_speed,vid,pid))return report->Result;
    if(cold_start(tp,version)<0)fail(tp,RtlUsbFwExecTimeout);
    if(!report->Result&&bus->Cancelled(bus->Context))fail(tp,RtlUsbFwExecCancelled);
    return report->Result;
}

/* may_alias permits access through the public opaque caller-owned storage. */
typedef struct __attribute__((__may_alias__)) {
    unsigned Magic,Version;
    struct r8152 Nic;
    struct usb_device Device;
    struct net_device Net;
} HARDWARE;
typedef char hardware_fits[(sizeof(HARDWARE)<=sizeof(RTL_USB_HW_STATE))?1:-1];
#define HARDWARE_READY 0x52485542U
enum RTL_UFW_EXEC_RESULT RtlUsbHardwarePrepare(RTL_USB_HW_STATE*state,unsigned version,
    unsigned package,unsigned options,unsigned usb_speed,unsigned vid,unsigned pid,
    unsigned rx_buffer_size,const RTL_UFW_BUS*bus,RTL_UPHY_REPORT*report)
{
    HARDWARE*h=(HARDWARE*)state;struct r8152*tp;int result=0;
    if(!state||!report){if(report){zero_mem(report,0,sizeof(*report));report->Result=RtlUsbFwExecArgument;}return RtlUsbFwExecArgument;}
    h->Magic=0;h->Version=version;tp=&h->Nic;
    if(!initialize(tp,version,package,options,bus,report))return RtlUsbFwExecArgument;
    if(rx_buffer_size<2048||rx_buffer_size>65536||(rx_buffer_size&15)){
        fail(tp,RtlUsbFwExecArgument);return report->Result;
    }
    if(!setup_mac(tp,&h->Device,version,usb_speed,vid,pid))return report->Result;
    h->Net.mtu=1500;h->Net.features=0;tp->netdev=&h->Net;
    tp->rx_buf_sz=rx_buffer_size;tp->rx_desc.size=version>=16?16:24;
    tp->rx_desc.align=version>=16?16:8;tp->tx_desc.size=version>=16?16:8;
    if(cold_start(tp,version)<0)fail(tp,RtlUsbFwExecTimeout);
    if(report->Result)return report->Result;
    tp->rtl_ops.hw_phy_cfg(tp);
    if(report->Result)return report->Result;
    if(rtl8152_set_speed(tp,tp->autoneg,tp->speed,tp->duplex,tp->advertising)<0)
        fail(tp,RtlUsbFwExecTimeout);
    if(report->Result)return report->Result;
    switch(version){
    case 1:case 2:case 7:result=rtl8152_up(tp);break;
    case 3:case 4:case 5:case 6:result=rtl8153_up(tp);break;
    case 8:case 9:result=rtl8153b_up(tp);break;
    case 14:result=rtl8153c_up(tp);break;
    case 16:case 17:result=rtl8157_up(tp);break;
    default:result=rtl8156_up(tp);break;
    }
    if(result<0)fail(tp,RtlUsbFwExecTimeout);
    if(!report->Result&&bus->Cancelled(bus->Context))fail(tp,RtlUsbFwExecCancelled);
    if(!report->Result)h->Magic=HARDWARE_READY;
    return report->Result;
}
enum RTL_UFW_EXEC_RESULT RtlUsbHardwareEnable(RTL_USB_HW_STATE*state,
    const RTL_UFW_BUS*bus,RTL_UPHY_REPORT*report)
{
    HARDWARE*h=(HARDWARE*)state;struct r8152*tp;int result=0;
    if(!report)return RtlUsbFwExecArgument;
    zero_mem(report,0,sizeof(*report));
    if(!state||h->Magic!=HARDWARE_READY||h->Version<1||h->Version>17||
       !bus||!bus->Read||!bus->Write||!bus->DelayMs||!bus->Cancelled||
       !bus->MaxOperations||!bus->MaxDelayMs||bus->Flags){
        report->Result=RtlUsbFwExecArgument;return report->Result;
    }
    tp=&h->Nic;tp->bus=bus;tp->report=report;tp->udev=&h->Device;tp->netdev=&h->Net;
    switch(h->Version){
    case 1:case 2:case 7:result=rtl8152_enable(tp);break;
    case 3:case 4:case 5:case 6:case 8:case 9:case 14:result=rtl8153_enable(tp);break;
    case 10:case 11:result=rtl8156_enable(tp);break;
    case 12:case 13:case 15:result=rtl8156b_enable(tp);break;
    case 16:case 17:result=rtl8157_enable(tp);break;
    }
    if(result<0)fail(tp,RtlUsbFwExecTimeout);
    if(!report->Result&&bus->Cancelled(bus->Context))fail(tp,RtlUsbFwExecCancelled);
    if(report->Result)h->Magic=0;
    return report->Result;
}
