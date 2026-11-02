/* Production MAC sequences against a bounded register model. Ready bits are
 * synthetic; this is not PHY MCU, silicon or Windows lifecycle validation. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../RLTKLANT/src/macseq.h"
typedef RTL_FW_U32 U32;typedef RTL_FW_U16 U16;
typedef struct {
 unsigned char regs[32768],pci[256];U16 mac[32768],phy[32768],mdio[4096][32],page,ephy[32];U32 eri[1024],csi[1024];
 U32 oob[1024],dp[1024];
 unsigned events,fail_at,cancel_at,failed,timeout_port,delay_us,version,oob_adjusted;
} DEVICE;
static DEVICE d;
static int event(DEVICE *s){assert(!s->failed);assert(!s->cancel_at||s->events<s->cancel_at);s->events++;if(s->fail_at==s->events){s->failed=1;return 0;}return 1;}
static U32 get(unsigned offset,unsigned width){U32 v=0;unsigned i;for(i=0;i<width;i++)v|=(U32)d.regs[offset+i]<<(i*8);return v;}
static void put(unsigned offset,unsigned width,U32 v){unsigned i;for(i=0;i<width;i++)d.regs[offset+i]=(unsigned char)(v>>(i*8));}
static int rd(void *ctx,U16 offset,unsigned width,U32 *value){DEVICE*s=ctx;if(!event(s))return 0;assert(offset+width<=sizeof(s->regs));*value=get(offset,width);return 1;}
static int wr(void *ctx,U16 offset,unsigned width,U32 word)
{
 DEVICE*s=ctx;unsigned addr=(word>>15)&0xfffe,reg=(word>>16)&31;U16 value=(U16)word;
 if(!event(s))return 0;
 assert(offset+width<=sizeof(s->regs));put(offset,width,word);
 if(offset==0xb0&&width==4&&s->version!=28&&s->version!=31){if(word>>31)s->mac[addr/2]=value;put(offset,4,s->mac[addr/2]);}
 if(offset==0xb4&&width==4&&(s->version==28||s->version==31)){
  unsigned a=(word&0xfff)/4,i,status=(s->version==31?0xb8:0x10)/4;
  if(word>>31){for(i=0;i<4;i++)if(word&(1U<<(12+i))){s->dp[a]&=~(255U<<(8*i));s->dp[a]|=get(0xb0,4)&(255U<<(8*i));}
   if(a==0x30/4&&(s->eri[0xe8/4]&255)==5)s->dp[status]|=0x800;
   if(a==0x30/4&&(s->eri[0xe8/4]&255)==6)s->dp[status]&=~0x800U;
   put(offset,4,word&~0x80000000U);
  }else {put(0xb0,4,s->dp[a]);put(offset,4,word|0x80000000U);}
 }
 if(offset==0xb8&&width==4){if(word>>31){s->phy[addr/2]=addr==0xa400?value&~0x8000U:value;put(offset,4,word&~0x80000000U);}else put(offset,4,word|0x80000000U|s->phy[addr/2]);}
 if(offset==0x60&&width==4){if(word>>31){if(reg==31){assert(value<4096);s->page=value;}else s->mdio[s->page][reg]=(s->page==0&&reg==0)?value&~0x8000U:value;put(offset,4,word&~0x80000000U);}else put(offset,4,word|0x80000000U|(reg==31?s->page:s->mdio[s->page][reg]));}
 if(offset==0x80&&width==4){if(word>>31){s->ephy[reg]=value;put(offset,4,word&~0x80000000U);}else put(offset,4,word|0x80000000U|s->ephy[reg]);}
 if(offset==0x74&&width==4){unsigned a=(word&0xfff)/4,i,type=(word>>16)&3;U32*bank=type==2?s->oob:s->eri;assert(!(word&3));assert(type==0||type==2);
  if(type==2&&s->version==52){assert((word&0x7ffc0000U)==(0xf70U<<18));s->oob_adjusted++;}
  if(word>>31){for(i=0;i<4;i++)if(word&(1U<<(12+i))){bank[a]&=~(255U<<(8*i));bank[a]|=get(0x70,4)&(255U<<(8*i));}
   if(type==2&&a==0x180/4&&(bank[a]&255)==5)s->oob[0x124/4]|=1;
   if(type==2&&a==0x180/4&&(bank[a]&255)==6)s->oob[0x124/4]&=~1U;
   put(offset,4,word&~0x80000000U);
  }else{put(0x70,4,bank[a]);put(offset,4,word|0x80000000U);}}
 if(offset==0x68&&width==4){unsigned a=(word&0xfff)/4;if(word>>31){s->csi[a]=get(0x64,4);put(offset,4,word&~0x80000000U);}else{put(0x64,4,s->csi[a]);put(offset,4,word|0x80000000U);}}
 if(offset==s->timeout_port&&width==4)put(offset,4,word); /* never acknowledge */
 return 1;
}
static int pci_rd(void *ctx,U16 off,unsigned width,U32 *v){DEVICE*s=ctx;unsigned i;if(!event(s))return 0;assert(off+width<=256);*v=0;for(i=0;i<width;i++)*v|=(U32)s->pci[off+i]<<(8*i);return 1;}
static int pci_wr(void *ctx,U16 off,unsigned width,U32 v){DEVICE*s=ctx;unsigned i;if(!event(s))return 0;assert(off+width<=256);for(i=0;i<width;i++)s->pci[off+i]=(unsigned char)(v>>(8*i));return 1;}
static int delay(void *ctx,U32 us){DEVICE*s=ctx;if(!event(s))return 0;s->delay_us+=us;return 1;}
static int cancelled(void *ctx){DEVICE*s=ctx;return s->cancel_at&&s->events>=s->cancel_at;}
static RTL_MAC_IO setup(unsigned version)
{
 RTL_MAC_IO io;memset(&d,0,sizeof(d));d.version=version;memset(&io,0,sizeof(io));put(0x40,4,0x6c900700);put(0xfb,1,0x20);d.phy[0xc426/2]=256;d.mdio[0xc42][0x13]=256;
 io.Context=&d;io.Read=rd;io.Write=wr;io.PciRead=pci_rd;io.PciWrite=pci_wr;io.DelayUs=delay;io.Cancelled=cancelled;
 io.RegisterBytes=sizeof(d.regs);io.MaxOperations=1000000;io.MaxDelayUs=120000000;io.PcieCapability=0x70;
 io.PhyAccess=version>=40?RtlFwPhyOcp:(version==28||version==31)?RtlFwPhyMdioDp:RtlFwPhyMdio;return io;
}
int main(void)
{
 RTL_MAC_IO io;RTL_MAC_REPORT r;RTL_FW_CHIP chip;unsigned xid,gmii,version,i,mode,count,total=0,versions=0;unsigned char seen[81]={0};
 for(gmii=0;gmii<2;gmii++)for(xid=0;xid<4096;xid++){
  const RTL_FW_CHIP*c=RtlFwChip((U16)xid,gmii);enum RTL_FW_RESULT result;if(!c||seen[c->MacVersion])continue;
  version=c->MacVersion;seen[version]=1;chip=*c;io=setup(version);
  result=RtlPciMacConfigure(&io,&chip,&r);if(result!=RtlFwOk)fprintf(stderr,"baseline version=%u result=%u op=%u events=%u\n",version,result,r.FailedOperation,d.events);assert(result==RtlFwOk);
  assert(r.FirmwareApplied==(version==52));assert(!(get(0x37,1)&12)); /* subtree must not start DMA */
  count=d.events;versions++;
  if(version>=61){assert(get(0x6048,2)==1546);assert(get(0x382,2)==0x221b);assert(get(0x1880,2)==0);assert(d.mac[0xe098/2]==0xc302);}
  for(mode=0;mode<2;mode++)for(i=1;i<=count;i++){
   io=setup(version);if(mode)d.cancel_at=i;else d.fail_at=i;
   result=RtlPciMacConfigure(&io,&chip,&r);
   if(result!=(mode?RtlFwCancelled:RtlFwIoError))fprintf(stderr,"fault version=%u mode=%u boundary=%u/%u result=%u events=%u\n",version,mode,i,count,result,d.events);
   assert(result==(mode?RtlFwCancelled:RtlFwIoError));assert(d.events==i&&r.FailedOperation!=~0U);total++;
  }
 }
 for(i=0;i<=81;i++)if(i>80||!seen[i]){io=setup(80);chip.MacVersion=i;chip.Image=0;assert(RtlPciMacConfigure(&io,&chip,&r)==RtlFwInvalidArgument&&!d.events);}
 chip=*RtlFwChip(0x6c9,1);
 for(i=0x100;i<0x604a;i+=0x100){io=setup(80);io.RegisterBytes=i;assert(RtlPciMacConfigure(&io,&chip,&r)==RtlFwInvalidArgument&&!d.events);}
 io=setup(80);io.RegisterBytes=0x604a;assert(RtlPciMacConfigure(&io,&chip,&r)==RtlFwOk);
 for(i=1;i<=256;i++)if(i<0x40||i>0xec||(i&3)){io=setup(80);io.PcieCapability=i;assert(RtlPciMacConfigure(&io,&chip,&r)==RtlFwInvalidArgument&&!d.events);}
 io=setup(80);io.MaxOperations=5;assert(RtlPciMacConfigure(&io,&chip,&r)==RtlFwBudgetExceeded&&d.events==5);
 io=setup(80);io.MaxDelayUs=1;d.mac[0xe00e/2]=0x2000;assert(RtlPciMacConfigure(&io,&chip,&r)==RtlFwBudgetExceeded&&d.delay_us<=1);
 io=setup(80);d.timeout_port=0x68;assert(RtlPciMacConfigure(&io,&chip,&r)==RtlFwIoError&&d.events<1000);
 chip=*RtlFwChip(0x609,1);io=setup(61);d.timeout_port=0x80;assert(RtlPciMacConfigure(&io,&chip,&r)==RtlFwIoError&&d.events<1000);
 chip=*RtlFwChip(0x541,1);io=setup(46);d.timeout_port=0x74;assert(RtlPciMacConfigure(&io,&chip,&r)==RtlFwIoError&&d.events<1000);
 chip=*RtlFwChip(0x54a,1);io=setup(52);put(0xfb,1,0);assert(RtlPciMacConfigure(&io,&chip,&r)==RtlFwIoError&&d.delay_us==100000000);
 /* Exact immutable RTL8117 payload now follows its MAC prerequisite subtree. */
 io=setup(52);assert(RtlPciMacConfigure(&io,&chip,&r)==RtlFwOk&&r.FirmwareApplied);printf("RTL8117 real payload: %u commands (transport only)\n",r.Firmware.Executed);
 chip.Image=0;io=setup(52);assert(RtlPciMacConfigure(&io,&chip,&r)==RtlFwInvalidArgument&&!d.events);
 printf("PASS: %u MAC dispatch versions, %u failed/cancelled boundary schedules, bounded CSI/ERI/EPHY/CMAC waits, mapping and operation/delay budgets; no DMA enable\n",versions,total);
 return 0;
}
