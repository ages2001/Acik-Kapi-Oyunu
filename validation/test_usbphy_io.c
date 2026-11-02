/* Share the byte-enable/paged/TGPHY transport fixture, not implementation.
   Status handshakes and firmware version changes remain synthetic. */
#define main firmware_io_test_main
#include "test_usbfw_io.c"
#undef main
static void phy_setup(RTL_USB_NIC*nic,unsigned v,unsigned package)
{
    setup(nic,v,NULL,0);nic->PackageId=(UCHAR)package;
    phy[0xa420/2]=3;registers[1][0xe001]=1;
    speed_version=v==17?0x22:v==16?0x14:0x21;
}
int main(void)
{
    unsigned v,package,total=0;
    RTL_USB_NIC nic;RTL_UPHY_REPORT report;
    for(v=1;v<=17;v++)for(package=v==17?0x18:0;package<=(v==17?0x1e:0);package+=6){
        unsigned count,i;enum RTL_UFW_EXEC_RESULT result;
        phy_setup(&nic,v,package);
        result=RtlUsbPhyApply(&nic,0,wait_ms,cancelled,NULL,&report);
        if(result){fprintf(stderr,"FAIL control PHY v%u pkg%02x result%u calls%u reg%x\n",v,package,result,calls,report.LastRegister);return 1;}
        count=calls;assert(!nic.Running);
        if(v==16)assert((sram[0x8183]&0xff00)==0x5900);
        if(v==17)assert((sram[0x8415]&0xff00)==0x9300);
        /* All logical operations are faulted by test_usbphyseq. Here cover
           inner control framing with first/last128 plus every97th transfer. */
        for(i=1;i<=count;i++)if(i<=128||count-i<128||!(i%97)){
            phy_setup(&nic,v,package);fail_at=i;
            assert(RtlUsbPhyApply(&nic,0,wait_ms,cancelled,NULL,&report)==RtlUsbFwExecIo&&calls==i);total++;
            phy_setup(&nic,v,package);cancel_at=i;
            assert(RtlUsbPhyApply(&nic,0,wait_ms,cancelled,NULL,&report)==RtlUsbFwExecCancelled&&calls==i-1);total++;
        }
        printf("PASS PHY USB control v%u package%02x: %u low-level transfers\n",v,package,count);fflush(stdout);
    }
    phy_setup(&nic,17,0x1e);nic.Running=TRUE;
    assert(RtlUsbPhyApply(&nic,0,wait_ms,cancelled,NULL,&report)==RtlUsbFwExecArgument&&!calls);
    printf("PASS: %u sampled inner-control failure/cancel schedules; all17 revisions and both RTL8159 packages\n",total);
    return 0;
}
