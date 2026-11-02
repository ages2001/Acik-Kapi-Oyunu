/* Passive NT3.1 MAC transport. Type-1 PCI cycles share the resident list
 * lock. NT3.1 has no exported HalSetBusDataByOffset; this does not claim
 * cross-driver SMP serialization for its existing direct-PCI transport. */
static int n31_mac_valid(PN31_ADAPTER a,RTL_FW_U16 offset,unsigned width)
{
    ULONG extent=a->MmioLength?a->MmioLength:0x100;
    return !n31_phy_cancelled(a)&&(width==1||width==2||width==4)&&
        !(offset%width)&&extent>=width&&offset<=extent-width&&in8(a->IoBase+RCP_CR)!=0xff;
}
static int n31_mac_read(void *p,RTL_FW_U16 offset,unsigned width,RTL_FW_U32 *value)
{
    PN31_ADAPTER a=p;
    if(!value||!n31_mac_valid(a,offset,width))return 0;
    *value=width==1?in8(a->IoBase+offset):width==2?in16(a->IoBase+offset):in32(a->IoBase+offset);
    return in8(a->IoBase+RCP_CR)!=0xff;
}
static int n31_mac_write(void *p,RTL_FW_U16 offset,unsigned width,RTL_FW_U32 value)
{
    PN31_ADAPTER a=p;
    if(!n31_mac_valid(a,offset,width))return 0;
    if(width==1)out8(a->IoBase+offset,(UCHAR)value);
    else if(width==2)out16(a->IoBase+offset,(USHORT)value);
    else out32(a->IoBase+offset,value);
    return in8(a->IoBase+RCP_CR)!=0xff;
}
static int n31_mac_config(PN31_ADAPTER a,int write,RTL_FW_U16 offset,unsigned width,RTL_FW_U32 *value)
{
    KIRQL old;ULONG address,port;
    if(!value||n31_phy_cancelled(a)||(width!=1&&width!=2&&width!=4)||offset%width||offset>256-width)return 0;
    address=0x80000000UL|(a->Bdf&0xff0000)|(((a->Bdf>>8)&31)<<11)|((a->Bdf&7)<<8)|(offset&0xfc);
    port=PCI_DATA+(offset&3);
    KeAcquireSpinLock(&g_poll->Lock,&old);out32(PCI_ADDR,address);
    if(write){
        if(width==1)out8(port,(UCHAR)*value);
        else if(width==2)out16(port,(USHORT)*value);
        else out32(port,*value);
    }else *value=width==1?in8(port):width==2?in16(port):in32(port);
    KeReleaseSpinLock(&g_poll->Lock,old);return 1;
}
static int n31_mac_config_read(void *p,RTL_FW_U16 off,unsigned width,RTL_FW_U32 *value)
{return n31_mac_config(p,0,off,width,value);}
static int n31_mac_config_write(void *p,RTL_FW_U16 off,unsigned width,RTL_FW_U32 value)
{return n31_mac_config(p,1,off,width,&value);}
static BOOLEAN n31_mac_io(PN31_ADAPTER a,RTL_MAC_IO *result,enum RTL_FW_PHY_ACCESS access,const RTL_FW_CHIP *chip)
{
    RTL_MAC_IO io;RTL_FW_U32 status,next,header;UCHAR seen[64];UINT i;
    if(!chip||chip->MacVersion==52)return FALSE;
    {UCHAR*p=(UCHAR*)&io;for(i=0;i<sizeof(io);i++)p[i]=0;}
    for(i=0;i<64;i++)seen[i]=0;
    io.Context=a;io.Read=n31_mac_read;io.Write=n31_mac_write;
    io.PciRead=n31_mac_config_read;io.PciWrite=n31_mac_config_write;
    io.DelayUs=n31_phy_us;io.Cancelled=n31_phy_cancelled;io.PhyAccess=access;
    io.RegisterBytes=a->MmioLength?a->MmioLength:0x100;
    if(io.RegisterBytes>65536)io.RegisterBytes=65536;
    io.MaxOperations=1000000;io.MaxDelayUs=120000000;io.PciFunction=a->Bdf&7;
    if(!n31_mac_config_read(a,6,2,&status)||status==0xffff)return FALSE;
    if(status&0x10){
        if(!n31_mac_config_read(a,0x34,1,&next))return FALSE;
        for(i=0;next&&i<48;i++){
            if(next<0x40||next>0xfc||(next&3)||seen[next>>2])return FALSE;
            seen[next>>2]=1;
            if(!n31_mac_config_read(a,(RTL_FW_U16)next,2,&header)||header==0xffff)return FALSE;
            if((header&255)==0x10){if(next>0xec)return FALSE;io.PcieCapability=(RTL_FW_U16)next;next=0;break;}
            next=(header>>8)&255;
        }
        if(next)return FALSE;
    }
    *result=io;return TRUE;
}

static BOOLEAN n31_mac_hardware(PN31_ADAPTER a,enum RTL_MAC_HARDWARE_STAGE stage)
{
    RTL_MAC_IO io;RTL_MAC_REPORT report;const RTL_FW_CHIP *chip;
    ULONG txc;UINT i;BOOLEAN gmii=TRUE,ok=FALSE;UCHAR command;
    enum RTL_FW_PHY_ACCESS access;
    if(a->PciStopping)return FALSE;
    if(stage==RtlMacHardwareRelease&&!a->PciManagementOwned)return TRUE;
    if(stage!=RtlMacHardwareRelease&&a->RecoveryStop)return FALSE;
    if(stage==RtlMacHardwareAcquire&&(!a->PciHardwareInitialized||a->PciManagementOwned))return FALSE;
    command=in8(a->IoBase+RCP_CR);
    if(command==0xff||(stage!=RtlMacHardwareInitialize&&(command&(RCP_CR_RST|RCP_CR_TE|RCP_CR_RE))))return FALSE;
    txc=in32(a->IoBase+RCP_TCR);if(txc==0xffffffffUL)return FALSE;
    for(i=0;i<RTL_PCI_ID_COUNT;i++)if(RtlPciIds[i].DeviceId==a->DeviceId){gmii=RtlPciIds[i].MaxSpeed>=1000;break;}
    chip=RtlFwChip((RTL_FW_U16)((txc>>20)&0xfff),gmii);if(!chip)return FALSE;
    access=chip->MacVersion>=40?RtlFwPhyOcp:
        (chip->MacVersion==28||chip->MacVersion==31)?RtlFwPhyMdioDp:RtlFwPhyMdio;
    /* Only the joined passive stop owner bypasses its own stop flag. */
    a->PciStopping=stage==RtlMacHardwareRelease;
    if(n31_mac_io(a,&io,access,chip)){
        if(stage==RtlMacHardwareAcquire)a->PciManagementOwned=TRUE;
        if(RtlPciMacHardware(&io,chip,stage,&report)==RtlFwOk){
            if(stage==RtlMacHardwareInitialize)a->PciHardwareInitialized=TRUE;
            if(stage==RtlMacHardwareRelease)a->PciManagementOwned=FALSE;
            ok=TRUE;
        }
    }
    a->PciStopping=FALSE;return ok;
}

static BOOLEAN n31_initialize_mac(PN31_ADAPTER a,RTL_FW_BUS *bus,const RTL_FW_CHIP *chip)
{
    RTL_MAC_IO io;RTL_MAC_REPORT report;
    if(!a->PciHardwareInitialized||!a->PciManagementOwned||!n31_mac_io(a,&io,bus->Access,chip))return FALSE;
    if(RtlPciMacConfigure(&io,chip,&report)!=RtlFwOk)return FALSE;
    if(a->RecoveryStop||in8(a->IoBase+RCP_CR)&(RCP_CR_RST|RCP_CR_TE|RCP_CR_RE))return FALSE;
    dbg("NT31 PCI PHY/MAC prepared\n");return TRUE;
}
