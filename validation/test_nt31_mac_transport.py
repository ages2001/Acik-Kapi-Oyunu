"""Production NT3.1 bounded MAC register and serialized PCI callbacks."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "macseq.h"
typedef uint32_t ULONG;typedef uint16_t USHORT;typedef uint8_t UCHAR,KIRQL;typedef unsigned UINT;typedef int BOOLEAN;
#define FALSE 0
#define TRUE 1
#define RCP_TCR 0x40
#define RCP_CR 0x37
#define RCP_CR_RST 0x10
#define RCP_CR_TE 4
#define RCP_CR_RE 8
#define PCI_ADDR 0xcf8
#define PCI_DATA 0xcfc
typedef struct {ULONG IoBase,MmioLength,Bdf;int RecoveryStop,PciHardwareInitialized,PciManagementOwned,PciStopping;ULONG DeviceId;} Adapter,*PN31_ADAPTER;
static struct {unsigned Lock;} poll,*g_poll=&poll;
static UCHAR regs[65536],config[256];static ULONG latch;static unsigned calls,macs;static int fail_mac,firmware_missing;
static void KeAcquireSpinLock(unsigned*p,KIRQL*old){assert(!*p);*p=1;*old=0;}
static void KeReleaseSpinLock(unsigned*p,KIRQL old){assert(*p&&!old);*p=0;}
static unsigned readio(ULONG p,unsigned width){unsigned v=0;calls++;
 if(p>=0x100000){assert(p-0x100000+width<=65536);memcpy(&v,regs+p-0x100000,width);}
 else {assert(poll.Lock&&p>=0xcfc&&p+width<=0xd00);memcpy(&v,config+(latch&0xfc)+p-0xcfc,width);}return v;}
static void writeio(ULONG p,unsigned width,unsigned v){calls++;
 if(p>=0x100000){assert(p-0x100000+width<=65536);memcpy(regs+p-0x100000,&v,width);}
 else if(p==0xcf8){assert(width==4&&poll.Lock);latch=v;}
 else {assert(poll.Lock&&p>=0xcfc&&p+width<=0xd00);memcpy(config+(latch&0xfc)+p-0xcfc,&v,width);}}
#define in8(p) readio(p,1)
#define in16(p) readio(p,2)
#define in32(p) readio(p,4)
#define out8(p,v) writeio(p,1,v)
#define out16(p,v) writeio(p,2,v)
#define out32(p,v) writeio(p,4,v)
static int n31_phy_us(void*p,unsigned us){(void)p;(void)us;assert(!poll.Lock);return 1;}
static int n31_phy_cancelled(void*p){Adapter*a=p;return a->RecoveryStop&&!a->PciStopping;}
static void dbg(const char*s){(void)s;}
static const struct {ULONG DeviceId,MaxSpeed;} RtlPciIds[]={{0x8127,10000}};
#define RTL_PCI_ID_COUNT 1
static RTL_FW_CHIP hardware_chip={0,0,80,0};static int hardware_fail=-1;static unsigned phases[3];
const RTL_FW_CHIP*RtlFwChip(RTL_FW_U16 xid,int gmii){assert(xid==0x6c9&&gmii);return &hardware_chip;}
enum RTL_FW_RESULT RtlPciMacHardware(const RTL_MAC_IO*io,const RTL_FW_CHIP*c,enum RTL_MAC_HARDWARE_STAGE stage,RTL_MAC_REPORT*r){
 Adapter*a=io->Context;(void)r;assert(!poll.Lock&&c==&hardware_chip&&!io->Cancelled(a));
 assert(io->PhyAccess==RtlFwPhyOcp&&io->RegisterBytes==0x8000);phases[stage]++;
 if(stage==RtlMacHardwareAcquire)assert(a->PciHardwareInitialized&&a->PciManagementOwned);
 if(stage==RtlMacHardwareRelease)assert(a->PciStopping&&a->PciManagementOwned&&io->DelayUs(a,100));
 return hardware_fail==(int)stage?RtlFwIoError:RtlFwOk;
}
enum RTL_FW_RESULT RtlPciMacConfigure(const RTL_MAC_IO*io,const RTL_FW_CHIP*chip,RTL_MAC_REPORT*r){
 r->FirmwareApplied=!firmware_missing;assert(!poll.Lock&&(chip->MacVersion==80||chip->MacVersion==52)&&io->RegisterBytes==0x8000&&io->PcieCapability==0x80&&io->PciFunction==3&&io->PhyAccess==RtlFwPhyOcp);macs++;
 return fail_mac?RtlFwIoError:RtlFwOk;
}
'''
test=r'''
int main(void){Adapter a={.IoBase=0x100000,.MmioLength=0x8000,.Bdf=0x020503,.DeviceId=0x8127,.PciHardwareInitialized=1,.PciManagementOwned=1};unsigned width,off,i,before;RTL_FW_U32 v;RTL_FW_BUS bus={0};RTL_FW_CHIP chip={0,0,80,0};bus.Access=RtlFwPhyOcp;
 for(width=1;width<=4;width*=2)for(off=0;off<65536;off++){
  int valid=!(off%width)&&off+width<=0x8000;before=calls;assert(n31_mac_valid(&a,off,width)==valid);if(!valid)assert(calls==before);
 }
 memset(regs+0x40,0xff,4);assert(n31_mac_read(&a,0x40,4,&v)&&v==0xffffffffU);
 regs[RCP_CR]=0xff;before=calls;assert(!n31_mac_write(&a,0x40,4,0)&&calls==before+1);regs[RCP_CR]=0;
 assert(n31_mac_write(&a,0x6048,2,1546)&&n31_mac_read(&a,0x6048,2,&v)&&v==1546);
 for(width=1;width<=4;width*=2)for(off=0;off+width<=256;off+=width){
  memset(config,0x5a,sizeof(config));assert(n31_mac_config_write(&a,off,width,0xaabbccdd));
  assert(latch==(0x80022b00U|(off&0xfc))&&!poll.Lock);
  for(i=0;i<256;i++)if(i<off||i>=off+width)assert(config[i]==0x5a);
 }
 for(width=0;width<8;width++)for(off=0;off<260;off++)if((width!=1&&width!=2&&width!=4)||off%width||off>256-width){before=calls;assert(!n31_mac_config_read(&a,off,width,&v)&&calls==before);}
 memset(config,0,sizeof(config));config[6]=0x10;config[0x34]=0x40;config[0x40]=1;config[0x41]=0x44;config[0x44]=2;config[0x45]=0x40;
 assert(!n31_initialize_mac(&a,&bus,&chip)&&!macs);config[0x45]=0x80;config[0x80]=0x10;
 assert(n31_initialize_mac(&a,&bus,&chip)&&macs==1);fail_mac=1;assert(!n31_initialize_mac(&a,&bus,&chip)&&macs==2);fail_mac=0;
 chip.MacVersion=52;assert(n31_initialize_mac(&a,&bus,&chip)&&macs==3);
 firmware_missing=1;assert(!n31_initialize_mac(&a,&bus,&chip)&&macs==4);firmware_missing=0;chip.MacVersion=80;
 a.RecoveryStop=1;before=calls;assert(!n31_initialize_mac(&a,&bus,&chip)&&calls==before);a.RecoveryStop=0;
 for(off=1;off<256;off++)if(off<0x40||(off&3)){config[0x34]=off;assert(!n31_initialize_mac(&a,&bus,&chip)&&macs==4);}
 config[0x34]=0x80;writeio(a.IoBase+RCP_TCR,4,0x6c900000);
 a.PciHardwareInitialized=0;a.PciManagementOwned=0;
 assert(!n31_mac_hardware(&a,RtlMacHardwareAcquire));
 hardware_fail=0;assert(!n31_mac_hardware(&a,RtlMacHardwareInitialize)&&!a.PciHardwareInitialized);hardware_fail=-1;
 assert(n31_mac_hardware(&a,RtlMacHardwareInitialize)&&a.PciHardwareInitialized);
 hardware_fail=1;assert(!n31_mac_hardware(&a,RtlMacHardwareAcquire)&&a.PciManagementOwned);hardware_fail=-1;
 before=phases[1];assert(!n31_mac_hardware(&a,RtlMacHardwareAcquire)&&phases[1]==before);
 a.RecoveryStop=1;hardware_fail=2;
 assert(!n31_mac_hardware(&a,RtlMacHardwareRelease)&&a.PciManagementOwned&&!a.PciStopping);
 hardware_fail=-1;assert(n31_mac_hardware(&a,RtlMacHardwareRelease)&&!a.PciManagementOwned&&!a.PciStopping);
 assert(!n31_mac_hardware(&a,RtlMacHardwareAcquire));a.RecoveryStop=0;
 assert(n31_mac_hardware(&a,RtlMacHardwareAcquire));
 regs[RCP_CR]=4;assert(!n31_mac_hardware(&a,RtlMacHardwareRelease)&&a.PciManagementOwned);
 regs[RCP_CR]=0xff;assert(!n31_mac_hardware(&a,RtlMacHardwareRelease)&&a.PciManagementOwned);
 regs[RCP_CR]=0;assert(n31_mac_hardware(&a,RtlMacHardwareRelease));
 puts("PASS: NT3.1 196608 register bounds; exact PCI BDF/widths under lock; valid all-ones data versus removal; cyclic/misaligned capabilities; cancellation and MAC failure; hardware/acquire/release ownership including failed acquire, stopped/absent DMA, failed release and joined-stop cancellation bypass");
}
'''
with tempfile.TemporaryDirectory(prefix='rltk-nt31-mac-') as td:
    p=Path(td)/'test.c';p.write_text(prefix+(root/'RLTKLANT/src/nt31pcimac.h').read_text()+test);exe=Path(td)/'test'
    subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(root/'RLTKLANT/src'),str(p),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
