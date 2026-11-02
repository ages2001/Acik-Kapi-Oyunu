/* Passive, exclusive PCI PHY/MAC preparation after reset and before DMA.
 * Initialization owns an unpublished adapter; recovery drains the link timer
 * and registered interrupt before entering the same transaction.
 */
#include "rltklant.h"
#include "physeq.h"
#include "macseq.h"
#if defined(RTL_NDIS5)
#include "native_pci.h"
#endif

UCHAR STDC KeGetCurrentIrql(VOID);
NTSTATUS STDC KeDelayExecutionThread(int, BOOLEAN, PLARGE_INTEGER);
#if !defined(RTL_NDIS5)
ULONG STDC HalGetBusDataByOffset(ULONG, ULONG, ULONG, PVOID, ULONG, ULONG);
ULONG STDC HalSetBusDataByOffset(ULONG, ULONG, ULONG, PVOID, ULONG, ULONG);
#endif

static int PciCancelled(void *p){return ((PRLTK_ADAPTER)p)->Halting;}
static int PciDelayUs(void *p,RTL_FW_U32 us)
{
    LARGE_INTEGER interval;
    if(PciCancelled(p)||us>2000000U)return 0;
    if(us<=50){NdisStallExecution(us);return !PciCancelled(p);}
    interval.QuadPart=(ULONGLONG)(-(long long)us*10);
    return KeDelayExecutionThread(0,FALSE,&interval)>=0&&!PciCancelled(p);
}
static int PciDelayMs(void *p,RTL_FW_U32 ms)
{
    return ms<=2000&&PciDelayUs(p,ms*1000);
}
static RTL_FW_U32 PciPhyRead(void *p,RTL_FW_U16 port){return RtlRead32(p,port);}
static void PciPhyWrite(void *p,RTL_FW_U16 port,RTL_FW_U32 value){RtlWrite32(p,port,value);}
static ULONG PciRegisterBytes(PRLTK_ADAPTER a)
{
    ULONG bytes=a->IoBase?a->MemLen:a->IoPortLen;
    return bytes>65536?65536:bytes;
}
static int PciRegisterValid(PRLTK_ADAPTER a,RTL_FW_U16 offset,unsigned width)
{
    ULONG bytes=PciRegisterBytes(a);
    return !a->Halting&&(a->IoBase||a->IoPortVa)&&
        (width==1||width==2||width==4)&&!(offset%width)&&
        bytes>=width&&offset<=bytes-width&&RtlRead8(a,RCP_CR)!=0xff;
}
static int PciMacRead(void *p,RTL_FW_U16 offset,unsigned width,RTL_FW_U32 *value)
{
    PRLTK_ADAPTER a=p;
    if(!value||!PciRegisterValid(a,offset,width))return 0;
    *value=width==1?RtlRead8(a,offset):width==2?RtlRead16(a,offset):RtlRead32(a,offset);
    /* All-ones data can be valid; a separate command read detects removal. */
    return RtlRead8(a,RCP_CR)!=0xff;
}
static int PciMacWrite(void *p,RTL_FW_U16 offset,unsigned width,RTL_FW_U32 value)
{
    PRLTK_ADAPTER a=p;
    if(!PciRegisterValid(a,offset,width))return 0;
    if(width==1)RtlWrite8(a,offset,(UCHAR)value);
    else if(width==2)RtlWrite16(a,offset,(USHORT)value);
    else RtlWrite32(a,offset,value);
    return RtlRead8(a,RCP_CR)!=0xff;
}
static int PciConfig(PRLTK_ADAPTER a,int write,RTL_FW_U16 offset,unsigned width,RTL_FW_U32 *value)
{
    if(!value||a->Halting||(width!=1&&width!=2&&width!=4)||
       offset%width||offset>256-width||KeGetCurrentIrql()!=0)return 0;
#if defined(RTL_NDIS5)
    return RtlNativePciConfig(a->PciPdo,write,offset,width,value);
#else
    if(!a->PciSlot)return 0;
    if(!write)*value=0;
    /* Exact widths preserve neighboring W1C status bits. The HAL owns BDF
       serialization; never use a private CF8/CFC lock on a running system. */
    return (write?HalSetBusDataByOffset:HalGetBusDataByOffset)
        (4,a->PciBus,(a->PciSlot-1)&255,value,offset,width)==width;
#endif
}
static int PciConfigRead(void *p,RTL_FW_U16 offset,unsigned width,RTL_FW_U32 *value)
{return PciConfig(p,0,offset,width,value);}
static int PciConfigWrite(void *p,RTL_FW_U16 offset,unsigned width,RTL_FW_U32 value)
{return PciConfig(p,1,offset,width,&value);}

static int PciExpressCapability(PRLTK_ADAPTER a,RTL_FW_U16 *cap)
{
    RTL_FW_U32 status,next,header;UCHAR seen[64];unsigned count;
    *cap=0;NdisZeroMemory(seen,sizeof(seen));
    if(!PciConfigRead(a,6,2,&status)||status==0xffff)return 0;
    if(!(status&0x10))return 1;
    if(!PciConfigRead(a,0x34,1,&next))return 0;
    for(count=0;next&&count<48;count++){
        if(next<0x40||next>0xfc||(next&3)||seen[next>>2])return 0;
        seen[next>>2]=1;
        if(!PciConfigRead(a,(RTL_FW_U16)next,2,&header)||header==0xffff)return 0;
        if((header&255)==0x10){if(next>0xec)return 0;*cap=(RTL_FW_U16)next;return 1;}
        next=(header>>8)&255;
    }
    return next==0;
}

BOOLEAN RtlPreparePci(PRLTK_ADAPTER a)
{
    RTL_FW_BUS bus;RTL_PHY_REPORT phy;RTL_MAC_IO io;RTL_MAC_REPORT mac;
    const RTL_FW_CHIP *chip;ULONG txc;UCHAR command;
    if(KeGetCurrentIrql()!=0||a->Halting||a->Backend!=RTL_BACKEND_CPLUS)return FALSE;
    if(PciRegisterBytes(a)<0x100)return FALSE;
    command=RtlRead8(a,RCP_CR);
    if(command==0xff||(command&(RCP_CR_RST|RCP_CR_TE|RCP_CR_RE)))return FALSE;
    txc=RtlRead32(a,RCP_TCR);if(txc==0xffffffffUL)return FALSE;
    chip=RtlFwChip((RTL_FW_U16)((txc>>20)&0xfff),!(a->Flags&RTL_F_FASTET));
    /* RTL8117 and SFP/power prerequisites remain separate work. Keeping the
       PHY dispatch gate prevents an incomplete firmware path enabling DMA. */
    if(!chip||chip->MacVersion==52)return FALSE;
    NdisZeroMemory(&io,sizeof(io));io.Context=a;
    io.Read=PciMacRead;io.Write=PciMacWrite;io.PciRead=PciConfigRead;io.PciWrite=PciConfigWrite;
    io.DelayUs=PciDelayUs;io.Cancelled=PciCancelled;io.RegisterBytes=PciRegisterBytes(a);
    io.MaxOperations=1000000;io.MaxDelayUs=120000000;
    if(io.RegisterBytes<(chip->MacVersion>=61?0x604aU:0x100U)||!PciExpressCapability(a,&io.PcieCapability))return FALSE;
#if defined(RTL_NDIS5)
    if(!RtlNativePciFunction(a->PciPdo,&io.PciFunction))return FALSE;
#else
    if(!a->PciSlot)return FALSE;
    io.PciFunction=(UCHAR)(((a->PciSlot-1)>>5)&7);
#endif
    NdisZeroMemory(&bus,sizeof(bus));bus.Context=a;
    bus.Read32=PciPhyRead;bus.Write32=PciPhyWrite;bus.DelayUs=PciDelayUs;
    bus.DelayMs=PciDelayMs;bus.Cancelled=PciCancelled;
    if(RtlPciPhyConfigureByXid(&bus,(RTL_FW_U16)((txc>>20)&0xfff),!(a->Flags&RTL_F_FASTET),&phy)!=RtlFwOk)return FALSE;
    a->PhyPage=0;io.PhyAccess=bus.Access;
    if(RtlPciMacConfigure(&io,chip,&mac)!=RtlFwOk)return FALSE;
    command=RtlRead8(a,RCP_CR);
    if(a->Halting||command==0xff||(command&(RCP_CR_RST|RCP_CR_TE|RCP_CR_RE)))return FALSE;
#ifdef RTL_DBG
    {const char*s="PCI PHY/MAC prepared\n";while(*s)RtlOutByte(0xe9,(UCHAR)*s++);}
#endif
    return TRUE;
}
