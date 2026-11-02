#define main phy_sequence_test_main
#include "test_usbphyseq.c"
#undef main
int main(void)
{
    RTL_UFW_BUS bus={NULL,read_io,write_io,wait_ms,cancel,200000,30000,0};
    RTL_UPHY_REPORT report;unsigned v,package,speed,i,total=0;
    unsigned speeds[]={2,3,5,6};
    for(v=1;v<=17;v++)for(package=v==17?0x18:0;package<=(v==17?0x1e:0);package+=2)
    for(speed=0;speed<4;speed++){
        unsigned count;enum RTL_UFW_EXEC_RESULT result;
        reset(v);result=RtlUsbMacPrepare(v,package,0,speeds[speed],0x0bda,0x8153,&bus,&report);
        if(result){fprintf(stderr,"FAIL MAC prepare v%u pkg%02x speed%u result%u calls%u space%u reg%x delay%u\n",v,package,speeds[speed],result,calls,report.LastSpace,report.LastRegister,report.DelayMs);return 1;}
        count=calls;assert(!(regs[1][0xe813]&0x0c)); /* no CR.RE/TE */
        if(speed==1)for(i=1;i<=count;i++){
            reset(v);fail_at=i;
            assert(RtlUsbMacPrepare(v,package,0,3,0x0bda,0x8153,&bus,&report)==RtlUsbFwExecIo&&calls==i);total++;
            reset(v);cancel_at=i;
            assert(RtlUsbMacPrepare(v,package,0,3,0x0bda,0x8153,&bus,&report)==RtlUsbFwExecCancelled&&calls==i-1);total++;
        }
        printf("PASS MAC prepare v%u package%02x USBspeed%u: %u boundaries\n",v,package,speeds[speed],count);fflush(stdout);
    }
    reset(17);regs[1][0xe004]=0;
    assert(RtlUsbMacPrepare(17,0x1e,0,3,0x0bda,0x8159,&bus,&report)==RtlUsbFwExecTimeout);
    assert(report.LastRegister==0xe004);
    reset(17);put(regs[1]+0xe004,0xffff);
    assert(RtlUsbMacPrepare(17,0x1e,0,3,0x0bda,0x8159,&bus,&report)==RtlUsbFwExecIo);
    reset(17);assert(RtlUsbMacPrepare(17,0x1e,0,0,0x0bda,0x8159,&bus,&report)==RtlUsbFwExecArgument&&!calls);
    printf("PASS: %u MAC preparation failure/cancel schedules; autoload timeout/all-ones and USB speed guards\n",total);
    return 0;
}
