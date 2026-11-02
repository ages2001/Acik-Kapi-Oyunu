/* Pinned initialization/DASH transactions; synthetic ownership handshakes.
 * No OS PCI power state or physical management controller is simulated. */
#define main previous_mac_checks
#include "test_macseq.c"
#undef main
static RTL_MAC_IO hardware_setup(unsigned version,unsigned revision,unsigned active,unsigned stage)
{
 RTL_MAC_IO io=setup(version);d.pci[8]=revision;
 put(0x40,4,get(0x40,4)|0x800);put(0xd3,1,0xb2);put(0xe2,2,0x103);
 d.dp[(version==31?0xb8:0x10)/4]=(active?0x8000:0)|(stage==RtlMacHardwareRelease?0x800:0);
 d.oob[0x128/4]=active;d.oob[0x124/4]=stage==RtlMacHardwareRelease;
 return io;
}
int main(void)
{
 RTL_FW_CHIP chips[81];unsigned char seen[81]={0};unsigned xid,g,version,revision,active,stage,mode,i;
 unsigned baselines=0,faults=0;RTL_MAC_IO io;RTL_MAC_REPORT r;enum RTL_FW_RESULT result;
 assert(!previous_mac_checks());
 for(g=0;g<2;g++)for(xid=0;xid<4096;xid++){
  const RTL_FW_CHIP*c=RtlFwChip(xid,g);if(c&&!seen[c->MacVersion]){seen[c->MacVersion]=1;chips[c->MacVersion]=*c;}
 }
 for(version=0;version<81;version++)if(seen[version])
 for(revision=0;revision<=4;revision+=4)for(active=0;active<2;active++)for(stage=0;stage<3;stage++){
  unsigned expected=(version==28||version==31)?1:(version==51||version==52)?2:(version==66||(version==80&&revision==4))?3:0;
  unsigned count;
  io=hardware_setup(version,revision,active,stage);result=RtlPciMacHardware(&io,&chips[version],stage,&r);
  if(result||r.DashType!=expected||r.DashEnabled!=(expected&&active))fprintf(stderr,"hardware version=%u revision=%u active=%u stage=%u result=%u events=%u dash=%u enabled=%u expected=%u\n",version,revision,active,stage,result,d.events,r.DashType,r.DashEnabled,expected);
  assert(result==RtlFwOk&&r.DashType==expected&&r.DashEnabled==(expected&&active));
  assert(!(get(0x37,1)&12)&&!r.FirmwareApplied); /* no DMA or PHY payload here */
  if(version==52)assert(d.oob_adjusted);
  if(stage!=RtlMacHardwareInitialize&&expected){
   unsigned cmd=stage==RtlMacHardwareAcquire?5:6;
   if(expected==1){assert((d.eri[0xe8/4]&255)==cmd);assert(!!(d.dp[(version==31?0xb8:0x10)/4]&0x800)==(cmd==5));}
   else if(expected==2){assert((d.oob[0x180/4]&255)==cmd);assert(!!(d.oob[0x124/4]&1)==(cmd==5));}
   else {assert((d.oob[0x14/4]&255)==cmd&&d.oob[0x18/4]==0&&d.oob[0x10/4]==1);}
  }
  count=d.events;baselines++;
  for(mode=0;mode<2;mode++)for(i=1;i<=count;i++){
   io=hardware_setup(version,revision,active,stage);if(mode)d.cancel_at=i;else d.fail_at=i;
   result=RtlPciMacHardware(&io,&chips[version],stage,&r);
   if(result!=(mode?RtlFwCancelled:RtlFwIoError))fprintf(stderr,"hardware fault version=%u stage=%u mode=%u boundary=%u/%u result=%u events=%u\n",version,stage,mode,i,count,result,d.events);
   assert(result==(mode?RtlFwCancelled:RtlFwIoError)&&d.events==i);faults++;
  }
 }
 io=hardware_setup(80,4,1,0);assert(RtlPciMacHardware(&io,&chips[80],(enum RTL_MAC_HARDWARE_STAGE)3,&r)==RtlFwInvalidArgument&&!d.events);
 io=hardware_setup(52,0,1,0);d.timeout_port=0x74;assert(RtlPciMacHardware(&io,&chips[52],0,&r)==RtlFwIoError&&d.delay_us<=10000);
 io=hardware_setup(28,0,1,0);d.timeout_port=0xb4;assert(RtlPciMacHardware(&io,&chips[28],0,&r)==RtlFwIoError&&d.delay_us<=2000);
 io=hardware_setup(80,0,0,0);put(0xd3,1,0);assert(RtlPciMacHardware(&io,&chips[80],0,&r)==RtlFwIoError&&d.delay_us<=6200);
 io=hardware_setup(52,0,0,0);put(0xfb,1,0);io.MaxDelayUs=50000;assert(RtlPciMacHardware(&io,&chips[52],0,&r)==RtlFwBudgetExceeded&&d.delay_us<=50000);
 printf("PASS: %u hardware initialization/DASH cases, %u failed/cancelled boundaries; DP/EP/2.5G-BP selection, RTL8127 revision gate, RTL8117 OOB address adjustment, bounded waits; no DMA enable\n",baselines,faults);
 return 0;
}
