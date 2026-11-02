#define main phy_sequence_test_main
#include "test_usbphyseq.c"
#undef main
static RTL_UFW_BUS bus={NULL,read_io,write_io,wait_ms,cancel,200000,30000,0};
static void prepare(RTL_USB_HW_STATE*state,unsigned v,unsigned package,RTL_UPHY_REPORT*report)
{
    enum RTL_UFW_EXEC_RESULT result;
    reset(v);result=RtlUsbHardwarePrepare(state,v,package,0,3,0x0bda,0x8153,2048,&bus,report);
    if(result){fprintf(stderr,"FAIL prepare v%u pkg%02x result%u calls%u space%u reg%x\n",v,package,result,calls,report->LastSpace,report->LastRegister);exit(1);}
    assert(!(regs[1][0xe813]&0x0c));
}
int main(void)
{
    RTL_USB_HW_STATE state;RTL_UPHY_REPORT report;unsigned v,package,total=0;
    for(v=1;v<=17;v++)for(package=v==17?0x18:0;package<=(v==17?0x1e:0);package+=6){
        unsigned prepare_count,enable_count,i;enum RTL_UFW_EXEC_RESULT result;
        prepare(&state,v,package,&report);prepare_count=calls;
        calls=0;put(regs[1]+0xe908,v==17?0x4003:v==16?0x1003:v>=10&&v<=13?0x403:0x13);
        result=RtlUsbHardwareEnable(&state,&bus,&report);
        if(result){fprintf(stderr,"FAIL enable v%u result%u calls%u reg%x\n",v,result,calls,report.LastRegister);return 1;}
        enable_count=calls;assert((regs[1][0xe813]&0x0c)==0x0c);
        for(i=1;i<=prepare_count;i++)if(i<=64||prepare_count-i<64||!(i%97)){
            reset(v);fail_at=i;
            assert(RtlUsbHardwarePrepare(&state,v,package,0,3,0x0bda,0x8153,2048,&bus,&report)==RtlUsbFwExecIo&&calls==i);total++;
            fail_at=0;assert(RtlUsbHardwareEnable(&state,&bus,&report)==RtlUsbFwExecArgument&&calls==i);
            reset(v);cancel_at=i;
            assert(RtlUsbHardwarePrepare(&state,v,package,0,3,0x0bda,0x8153,2048,&bus,&report)==RtlUsbFwExecCancelled&&calls==i-1);total++;
        }
        for(i=1;i<=enable_count;i++){
            prepare(&state,v,package,&report);calls=0;fail_at=i;
            put(regs[1]+0xe908,v==17?0x4003:v==16?0x1003:v>=10&&v<=13?0x403:0x13);
            assert(RtlUsbHardwareEnable(&state,&bus,&report)==RtlUsbFwExecIo&&calls==i);total++;
            fail_at=0;assert(RtlUsbHardwareEnable(&state,&bus,&report)==RtlUsbFwExecArgument&&calls==i);
            prepare(&state,v,package,&report);calls=0;cancel_at=i;
            put(regs[1]+0xe908,v==17?0x4003:v==16?0x1003:v>=10&&v<=13?0x403:0x13);
            assert(RtlUsbHardwareEnable(&state,&bus,&report)==RtlUsbFwExecCancelled&&calls==i-1);total++;
        }
        printf("PASS complete hardware sequence v%u pkg%02x: prepare%u enable%u boundaries\n",v,package,prepare_count,enable_count);fflush(stdout);
    }
    reset(17);assert(RtlUsbHardwarePrepare(&state,17,0x1e,0,3,0x0bda,0x8159,1024,&bus,&report)==RtlUsbFwExecArgument&&!calls);
    printf("PASS: %u combined preparation/enable failure-cancel schedules; invalidation after failure and MAC stopped-before-enable\n",total);
    return 0;
}
