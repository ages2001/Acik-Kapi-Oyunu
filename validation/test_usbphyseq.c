/* Behavioral transport fixture; no MCU/analog emulation or hardware claim. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../RLTKLANT/USBNT/src/usbphyseq.h"
static unsigned char regs[2][65536];
static unsigned short phy[32768],sram[2][65536],selector[2];
static unsigned advanced[2][65536];
static unsigned calls,fail_at,cancel_at,version,uploaded,status_fault,wait_fault;
static unsigned word(const unsigned char*p){return p[0]|(unsigned)p[1]<<8;}
static void put(unsigned char*p,unsigned v){p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);}
static int before(void){calls++;assert(!fail_at||calls<=fail_at);assert(!cancel_at||calls<cancel_at);return calls!=fail_at;}
static int read_io(void*p,enum RTL_UFW_SPACE space,unsigned reg,unsigned char*out,unsigned n)
{
    unsigned value=0;(void)p;if(!before())return 0;
    assert(reg+n<=65536&&space<=RtlUsbFwPhy);
    if(space!=RtlUsbFwPhy){memcpy(out,regs[space]+reg,n);return 1;}
    assert(n==2&&!(reg&1));value=phy[reg/2];
    if(reg==0xa438)value=sram[0][selector[0]];
    if(reg==0xb87e)value=sram[1][selector[1]];
    if(reg==0xa420&&status_fault)value=0;
    put(out,value);return 1;
}
static int write_io(void*p,enum RTL_UFW_SPACE space,unsigned reg,const unsigned char*in,unsigned n)
{
    unsigned v;(void)p;if(!before())return 0;
    assert(reg+n<=65536&&space<=RtlUsbFwPhy);
    if(space!=RtlUsbFwPhy){
        memcpy(regs[space]+reg,in,n);
        if(space==RtlUsbFwUsb&&reg>=0x9a00&&reg<0xa200)uploaded+=n;
        if(space==RtlUsbFwPla&&reg==0xdc6a)regs[space][reg]&=~0x10U;
        if(space==RtlUsbFwPla&&reg==0xe813&&(in[0]&0x10))regs[space][reg]=0;
        if(space==RtlUsbFwUsb&&reg==0xd5dc&&(in[0]&1)){
            unsigned bank=!!(in[0]&4),addr=word(regs[0]+0xd5d6);
            if(in[0]&2)advanced[bank][addr]=word(regs[0]+0xd5d8)|(unsigned)word(regs[0]+0xd5da)<<16;
            else {put(regs[0]+0xd5d8,advanced[bank][addr]);put(regs[0]+0xd5da,advanced[bank][addr]>>16);}
            regs[0][0xd5dc]&=~1U;
        }
        return 1;
    }
    assert(n==2&&!(reg&1));v=word(in);phy[reg/2]=(unsigned short)v;
    if(reg==0xa436)selector[0]=(unsigned short)v;
    if(reg==0xb87c)selector[1]=(unsigned short)v;
    if(reg==0xa438)sram[0][selector[0]]=(unsigned short)v;
    if(reg==0xb87e)sram[1][selector[1]]=(unsigned short)v;
    if(reg==0xa400)phy[reg/2]&=~0x8000U;
    if(reg==0xb820){
        phy[0xb800/2]=v&0x10?0x40:0;
        if(!(v&0x10)&&uploaded)sram[0][0x801e]=(unsigned short)(version==17?0x22:version==16?0x14:0x21);
    }
    return 1;
}
static int wait_ms(void*p,unsigned ms){(void)p;assert(ms<=20);return before()&&!wait_fault;}
static int cancel(void*p){(void)p;return cancel_at&&calls+1>=cancel_at;}
static void reset(unsigned v)
{
    memset(regs,0,sizeof(regs));memset(phy,0,sizeof(phy));memset(sram,0,sizeof(sram));
    memset(advanced,0,sizeof(advanced));
    selector[0]=selector[1]=0;calls=fail_at=cancel_at=uploaded=status_fault=wait_fault=0;version=v;
    phy[0xa420/2]=3;put(regs[1]+0xe000,0x100);
    put(regs[1]+0xe004,2);
    regs[1][0xe84f]=2;
}
int main(void)
{
    RTL_UFW_BUS bus={NULL,read_io,write_io,wait_ms,cancel,200000,30000,0};
    RTL_UPHY_REPORT report;unsigned v,package,opt,total=0;
    for(v=1;v<=17;v++)for(package=v==17?0x18:0;package<=(v==17?0x1e:0);package+=2)
    for(opt=0;opt<4;opt++){
        unsigned count,i;enum RTL_UFW_EXEC_RESULT result;
        reset(v);result=RtlUsbPhyConfigure(v,package,opt,&bus,&report);
        if(result){fprintf(stderr,"FAIL v%u pkg%u opt%u result%u calls%u space%u reg%x\n",v,package,opt,result,calls,report.LastSpace,report.LastRegister);return 1;}
        count=calls;
        /* Every external I/O and wait boundary in each normal revision path.
           All option/package combinations additionally run without injection. */
        if(!opt&&(package==0||package==0x18||package==0x1e))for(i=1;i<=count;i++){
            reset(v);fail_at=i;
            assert(RtlUsbPhyConfigure(v,package,opt,&bus,&report)==RtlUsbFwExecIo&&calls==i);total++;
            reset(v);cancel_at=i;
            assert(RtlUsbPhyConfigure(v,package,opt,&bus,&report)==RtlUsbFwExecCancelled&&calls==i-1);total++;
        }
        printf("PASS vendor PHY v%u package%02x options%u: %u callback boundaries\n",v,package,opt,count);fflush(stdout);
    }
    reset(17);assert(RtlUsbPhyConfigure(17,0,0,&bus,&report)==RtlUsbFwExecArgument&&!calls);
    reset(17);assert(RtlUsbPhyConfigure(17,0xff,0,&bus,&report)==RtlUsbFwExecArgument&&!calls);
    reset(16);status_fault=1;
    assert(RtlUsbPhyConfigure(16,0,0,&bus,&report)==RtlUsbFwExecTimeout);
    reset(16);bus.MaxOperations=4;
    assert(RtlUsbPhyConfigure(16,0,0,&bus,&report)==RtlUsbFwExecBudget&&calls==4);
    printf("PASS: %u failure/cancel schedules; revision/package dispatch, EEE/green variants, invalid/status/budget checks\n",total);
    return 0;
}
