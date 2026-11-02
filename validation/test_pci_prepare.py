"""Production startup boundary: passive ownership, exact windows/config widths,
bounded capability discovery, and failed PHY/MAC transactions never succeeding.
"""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
code=(root/'RLTKLANT/src/pci_start.c').read_text().replace('#include "rltklant.h"','').replace('#include "native_pci.h"','')
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "physeq.h"
#include "macseq.h"
typedef unsigned char UCHAR;typedef uint16_t USHORT;typedef uint32_t ULONG;
typedef uint64_t ULONGLONG;typedef int BOOLEAN,NTSTATUS;typedef void VOID,*PVOID;
typedef struct {uint64_t QuadPart;} LARGE_INTEGER,*PLARGE_INTEGER;
#define STDC
#define TRUE 1
#define FALSE 0
#define RCP_CR 0x37
#define RCP_TCR 0x40
#define RCP_CR_RST 0x10
#define RCP_CR_TE 4
#define RCP_CR_RE 8
#define RTL_BACKEND_CPLUS 2
#define RTL_F_FASTET 1
#define NdisZeroMemory(p,n) memset(p,0,n)
typedef struct {int Halting,Backend,PciHardwareInitialized,PciManagementOwned,PciStopping;void*IoBase,*IoPortVa,*PciPdo;ULONG MemLen,IoPortLen,PciSlot,PciBus,Flags;USHORT PhyPage;} ADAPTER,*PRLTK_ADAPTER;
static unsigned char regs[65536],cfg[256];static unsigned irql,writes,reads,config_calls,stalls,sleeps,stage;
static int cfg_fail,phy_fail,mac_fail,fn_fail,delay_fail,change_dma;
static int firmware_missing;static int hw_fail=-1;static unsigned hw_calls[3];
static RTL_FW_CHIP chip={0,0,80,0};
static UCHAR KeGetCurrentIrql(void){return irql;}
static unsigned rd(void*p,unsigned off,unsigned n){(void)p;unsigned v=0;assert(off+n<=sizeof(regs));memcpy(&v,regs+off,n);reads++;return v;}
static void wr(void*p,unsigned off,unsigned n,unsigned v){(void)p;assert(off+n<=sizeof(regs));memcpy(regs+off,&v,n);writes++;}
#define RtlRead8(p,o) rd(p,o,1)
#define RtlRead16(p,o) rd(p,o,2)
#define RtlRead32(p,o) rd(p,o,4)
#define RtlWrite8(p,o,v) wr(p,o,1,v)
#define RtlWrite16(p,o,v) wr(p,o,2,v)
#define RtlWrite32(p,o,v) wr(p,o,4,v)
static void NdisStallExecution(unsigned n){assert(n<=50);stalls++;}
static NTSTATUS KeDelayExecutionThread(int mode,BOOLEAN alert,PLARGE_INTEGER t){assert(!mode&&!alert&&(int64_t)t->QuadPart<0);sleeps++;return delay_fail?-1:0;}
static int cfgio(int write,unsigned off,unsigned n,void*p){assert(off+n<=256&&!irql);config_calls++;if(cfg_fail)return 0;if(write)memcpy(cfg+off,p,n);else memcpy(p,cfg+off,n);return 1;}
#ifdef RTL_NDIS5
static int RtlNativePciConfig(void*p,int w,unsigned short off,unsigned n,unsigned int*v){assert(p);return cfgio(w,off,n,v);}
static int RtlNativePciFunction(void*p,unsigned char*f){assert(p);*f=3;return !fn_fail;}
#else
static ULONG HalGetBusDataByOffset(ULONG type,ULONG bus,ULONG slot,PVOID p,ULONG off,ULONG n){assert(type==4&&bus==2&&slot==0x65);return cfgio(0,off,n,p)?n:0;}
static ULONG HalSetBusDataByOffset(ULONG type,ULONG bus,ULONG slot,PVOID p,ULONG off,ULONG n){assert(type==4&&bus==2&&slot==0x65);return cfgio(1,off,n,p)?n:0;}
#endif
const RTL_FW_CHIP*RtlFwChip(RTL_FW_U16 xid,int gmii){assert(xid==0x688&&gmii);return &chip;}
enum RTL_FW_RESULT RtlPciPhyPrepareForMac(RTL_FW_BUS*b,RTL_FW_U16 xid,int gmii,RTL_PHY_REPORT*r){
 (void)r;assert(!stage&&xid==0x688&&gmii&&!irql&&!(regs[RCP_CR]&0x1c));stage=1;b->Access=RtlFwPhyOcp;
 return phy_fail?RtlFwIoError:RtlFwOk;
}
enum RTL_FW_RESULT RtlPciMacHardware(const RTL_MAC_IO*i,const RTL_FW_CHIP*c,enum RTL_MAC_HARDWARE_STAGE phase,RTL_MAC_REPORT*r){
 ADAPTER*a=i->Context;(void)r;assert(c==&chip&&phase<=2&&!irql);
 assert(!i->Cancelled(a));hw_calls[phase]++;
 if(phase==RtlMacHardwareAcquire)assert(a->PciHardwareInitialized&&a->PciManagementOwned&&!stage);
 if(phase==RtlMacHardwareRelease){assert(a->PciStopping&&a->PciManagementOwned);assert(i->DelayUs(a,100));}
 return (int)phase==hw_fail?RtlFwIoError:RtlFwOk;
}
enum RTL_FW_RESULT RtlPciMacConfigure(const RTL_MAC_IO*i,const RTL_FW_CHIP*c,RTL_MAC_REPORT*r){
 r->FirmwareApplied=!firmware_missing;assert(stage==1&&c==&chip&&i->PciFunction==3&&!irql&&i->PcieCapability==0x80);
 assert(i->RegisterBytes==0x8000&&i->PhyAccess==RtlFwPhyOcp);stage=2;
 if(change_dma)regs[RCP_CR]=4;
 return mac_fail?RtlFwIoError:RtlFwOk;
}
'''
test=r'''
int main(void){ADAPTER a;unsigned off,n,count=0;RTL_FW_U32 v;RTL_FW_U16 cap;
 memset(&a,0,sizeof(a));a.IoBase=&a;a.IoPortVa=&a;a.PciPdo=&a;a.MemLen=0x8000;a.IoPortLen=0x100;a.Backend=2;a.PciBus=2;a.PciSlot=(2<<8|0x65)+1;
 for(n=1;n<=4;n*=2)for(off=0;off<65536;off++){
  int valid=!(off%n)&&off+n<=a.MemLen;
  unsigned before=reads;assert(PciRegisterValid(&a,off,n)==valid);
  if(!valid)assert(reads==before);count++;
 }
 a.IoBase=0;assert(!PciRegisterValid(&a,0x100,1));assert(PciRegisterValid(&a,0xfc,4));a.IoBase=&a;
 a.MemLen=100000;assert(PciRegisterBytes(&a)==65536);a.MemLen=0x8000;
 regs[0x40]=regs[0x41]=regs[0x42]=regs[0x43]=0xff;
 assert(PciMacRead(&a,0x40,4,&v)&&v==0xffffffffU);
 regs[RCP_CR]=0xff;assert(!PciMacRead(&a,0x40,4,&v));assert(!PciMacWrite(&a,0x40,4,0));regs[RCP_CR]=0;
 for(n=1;n<=4;n*=2)for(off=0;off+n<=256;off+=n){
  memset(cfg,0x5a,sizeof(cfg));assert(PciConfigWrite(&a,off,n,0xaabbccdd));
  for(unsigned j=0;j<256;j++)if(j<off||j>=off+n)assert(cfg[j]==0x5a);
 }
 memset(cfg,0,sizeof(cfg));assert(PciExpressCapability(&a,&cap)&&!cap);
 cfg[6]=0x10;
 for(off=1;off<256;off++)if(off<0x40||(off&3)){
  cfg[0x34]=off;assert(!PciExpressCapability(&a,&cap));
 }
 cfg[0x34]=0x40;cfg[0x40]=1;cfg[0x41]=0x44;cfg[0x44]=2;cfg[0x45]=0x40;
 assert(!PciExpressCapability(&a,&cap));
 cfg[0x45]=0x80;cfg[0x80]=0x10;assert(PciExpressCapability(&a,&cap)&&cap==0x80);
 cfg_fail=1;assert(!PciExpressCapability(&a,&cap));cfg_fail=0;
 assert(PciDelayUs(&a,10)&&stalls==1&&!sleeps);assert(PciDelayUs(&a,100)&&sleeps==1);
 assert(!PciDelayUs(&a,2000001)&&!PciDelayMs(&a,2001));delay_fail=1;assert(!PciDelayMs(&a,10));delay_fail=0;
 wr(&a,0x40,4,0x68800000);
 assert(!RtlPreparePci(&a)&&!stage);
 hw_fail=0;assert(!RtlInitializePciHardware(&a)&&!a.PciHardwareInitialized);hw_fail=-1;
 assert(RtlInitializePciHardware(&a)&&a.PciHardwareInitialized&&hw_calls[0]==2);
 assert(RtlInitializePciHardware(&a)&&hw_calls[0]==2);
 hw_fail=1;assert(!RtlPreparePci(&a)&&!stage&&a.PciManagementOwned);hw_fail=-1;
 assert(!RtlInitializePciHardware(&a));
 hw_fail=2;a.Halting=1;
 assert(!RtlReleasePciHardware(&a)&&a.PciManagementOwned&&!a.PciStopping);
 hw_fail=-1;assert(RtlReleasePciHardware(&a)&&!a.PciManagementOwned&&!a.PciStopping);
 assert(!PciDelayUs(&a,1));a.Halting=0;
 assert(RtlPreparePci(&a)&&stage==2);
 assert(!RtlPreparePci(&a));
 assert(RtlReleasePciHardware(&a));
 stage=0;phy_fail=1;assert(!RtlPreparePci(&a)&&stage==1);phy_fail=0;assert(RtlReleasePciHardware(&a));
 stage=0;mac_fail=1;assert(!RtlPreparePci(&a)&&stage==2);mac_fail=0;assert(RtlReleasePciHardware(&a));
 stage=0;change_dma=1;assert(!RtlPreparePci(&a)&&stage==2);change_dma=0;
 assert(!RtlReleasePciHardware(&a)&&a.PciManagementOwned&&!a.PciStopping);
 regs[RCP_CR]=0;assert(RtlReleasePciHardware(&a));
 stage=0;a.MemLen=0x100;assert(!RtlPreparePci(&a)&&!stage);a.MemLen=0x8000;
 chip.MacVersion=52;assert(RtlPreparePci(&a)&&stage==2);assert(RtlReleasePciHardware(&a));stage=0;
 firmware_missing=1;assert(!RtlPreparePci(&a)&&stage==2&&a.PciManagementOwned);assert(RtlReleasePciHardware(&a));
 firmware_missing=0;stage=0;chip.MacVersion=80;
 irql=2;assert(!RtlPreparePci(&a)&&!stage);irql=0;
 a.Halting=1;assert(!RtlPreparePci(&a)&&!stage&&!PciDelayUs(&a,1));a.Halting=0;
 a.PciManagementOwned=1;regs[RCP_CR]=0xff;
 assert(!RtlReleasePciHardware(&a)&&a.PciManagementOwned&&!a.PciStopping);
 regs[RCP_CR]=0;irql=2;assert(!RtlReleasePciHardware(&a));irql=0;
 assert(RtlReleasePciHardware(&a));
 for(n=4;n<=16;n*=2){regs[RCP_CR]=n;assert(!RtlPreparePci(&a)&&!stage);}regs[RCP_CR]=0;
#ifdef RTL_NDIS5
 fn_fail=1;assert(!RtlPreparePci(&a)&&!stage);fn_fail=0;
#else
 a.PciSlot=0;assert(!RtlPreparePci(&a)&&!stage);
#endif
 printf("PASS: %u register-window boundaries; exact PCI write widths; malformed/cyclic capabilities; passive/delay/cancellation guards; initialize once, acquire before PHY/MAC, exclusive passive release during halt; failed/absent/busy release retains ownership; no success after failure or DMA enable\n",count);
}
'''
with tempfile.TemporaryDirectory(prefix='rltk-pci-prepare-') as td:
    p=Path(td)/'test.c';p.write_text(prefix+code+test)
    for mode in ('legacy','native'):
        exe=Path(td)/mode
        cmd=['gcc','-std=c99','-Wall','-Wextra','-Werror','-Wno-unused-variable','-Wno-misleading-indentation','-fsanitize=address,undefined','-I',str(root/'RLTKLANT/src')]
        if mode=='native':cmd+=['-DRTL_NDIS5']
        subprocess.run(cmd+[str(p),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
