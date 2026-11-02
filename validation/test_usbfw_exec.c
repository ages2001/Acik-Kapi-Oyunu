/* Independent upstream write trace plus production failure/cancel boundaries.
   Register storage and handshakes are synthetic, not a PHY MCU simulator. */
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "../RLTKLANT/USBNT/src/usbfwexec.h"
typedef uint8_t u8;typedef uint16_t u16;typedef uint32_t u32;
typedef uint16_t __le16;typedef uint32_t __le32;
#define __packed __attribute__((packed))
#define __le16_to_cpu(x) (x)
#define __le32_to_cpu(x) (x)
#define BIT(n) (1U<<(n))
#define ALIGN(v,n) (((v)+(n)-1)&~((n)-1))
static u8 regs[4][65536],trace[1048576],expected[1048576];
static unsigned trace_bytes,calls,fail_at,cancel_at,speed_version,speed_bytes;
static int stalled,all_ones,wrong_version;
static unsigned word(const u8*p){return p[0]|(unsigned)p[1]<<8;}
static unsigned dword(const u8*p){return word(p)|(u32)word(p+2)<<16;}
static void setword(unsigned space,unsigned addr,unsigned value){regs[space][addr]=value;regs[space][addr+1]=value>>8;}
static int event(void){calls++;assert(!fail_at||calls<=fail_at);assert(!cancel_at||calls<cancel_at);return calls!=fail_at;}
static int io_read(void*ctx,enum RTL_UFW_SPACE space,unsigned addr,u8*p,unsigned n)
{
    (void)ctx;assert(space<=3&&addr<=65536&&n<=65536-addr);
    if(!event())return 0;
    if(all_ones)memset(p,255,n);else memcpy(p,regs[space]+addr,n);
    return 1;
}
static int io_write(void*ctx,enum RTL_UFW_SPACE space,unsigned addr,const u8*p,unsigned n)
{
    unsigned header[3]={space,addr,n};(void)ctx;
    assert(space<=3&&addr<=65536&&n<=65536-addr);
    if(!event())return 0;
    assert(trace_bytes+sizeof(header)+n<sizeof(trace));
    memcpy(trace+trace_bytes,header,sizeof(header));trace_bytes+=sizeof(header);
    memcpy(trace+trace_bytes,p,n);trace_bytes+=n;memcpy(regs[space]+addr,p,n);
    if(space==2&&addr==0xb820){
        if(!stalled)setword(2,0xb800,(word(p)&0x10)?0x40:0);
        if(!(word(p)&0x10)&&speed_bytes&&!wrong_version)setword(3,0x801e,speed_version);
    }
    if(space==0&&addr==0x9a00)speed_bytes+=n;
    if(space==1&&addr==0xdc6a&&!stalled)setword(1,addr,word(p)&~0x10U);
    return 1;
}
static int io_delay(void*ctx,unsigned ms){(void)ctx;assert(ms<=3);return event();}
static int cancelled(void*ctx){(void)ctx;return cancel_at&&calls+1>=cancel_at;}
#include "oracle.h"
#include "vendor-oracle.h"
static void reset(const u8*data,unsigned size,int variant)
{
    unsigned at;memset(regs,0,sizeof(regs));trace_bytes=calls=fail_at=cancel_at=speed_bytes=0;
    stalled=all_ones=wrong_version=0;speed_version=0;
    /* Exercise optional SUPER-only / BND writes and existing firmware skip. */
    if(variant==1){regs[0][0xcfef]=1;regs[0][0xd81f]=4;}
    if(variant==2)regs[1][0xd38e]=1;
    for(at=64;at<size;){
        const u8*b=data+at;unsigned type=dword(b),n=dword(b+4);
        if(type==13)speed_version=word(b+10);
        if(variant==2){
            if((type==1||type==2)&&word(b+60))regs[0][word(b+60)]=b[62];
            if(type==14)setword(3,word(b+8),word(b+10));
            if(type==13)setword(3,0x801e,word(b+10));
        }
        at+=ALIGN(n,8);
    }
}
static void upstream(const u8*data,unsigned size,unsigned v)
{
    struct r8152 tp={0};struct firmware fw={data,size};
    tp.version=v;tp.rtl_fw.fw=&fw;
    switch(v){
    case 4:tp.rtl_fw.pre_fw=r8153_pre_firmware_1;tp.rtl_fw.post_fw=r8153_post_firmware_1;break;
    case 5:tp.rtl_fw.pre_fw=r8153_pre_firmware_2;tp.rtl_fw.post_fw=r8153_post_firmware_2;break;
    case 6:tp.rtl_fw.post_fw=r8153_post_firmware_3;break;
    case 9:tp.rtl_fw.pre_fw=r8153b_pre_firmware_1;tp.rtl_fw.post_fw=r8153b_post_firmware_1;break;
    case 11:tp.rtl_fw.post_fw=r8156a_post_firmware_1;break;
    case 14:tp.rtl_fw.pre_fw=r8153b_pre_firmware_1;tp.rtl_fw.post_fw=r8153c_post_firmware_1;break;
    }
    rtl8152_apply_firmware(&tp,false);
}
int main(int argc,char**argv)
{
    RTL_UFW_BUS bus={0,io_read,io_write,io_delay,cancelled,200000,20000,0};
    RTL_UFW_EXEC_REPORT r;unsigned arg,total=0;
    assert(argc>1&&argc%2==1);
    for(arg=1;arg<(unsigned)argc;arg+=2){
        FILE*f=fopen(argv[arg],"rb");long length;u8*data;unsigned size,v=atoi(argv[arg+1]),i,count,variant;
        assert(f);assert(!fseek(f,0,SEEK_END));length=ftell(f);assert(length>64&&length<262144);rewind(f);
        size=(unsigned)length;data=malloc(size);assert(data&&fread(data,1,size,f)==size);fclose(f);
        for(variant=0;variant<3;variant++){
            unsigned bytes;reset(data,size,variant);upstream(data,size,v);bytes=trace_bytes;memcpy(expected,trace,bytes);
            reset(data,size,variant);assert(!RtlUsbFwExecute(data,size,v,&bus,&r));assert(!r.PatchRequested);
            if(bytes!=trace_bytes||memcmp(expected,trace,bytes)){
                fprintf(stderr,"upstream trace mismatch: %s v%u variant%u expected%u actual%u\n",argv[arg],v,variant,bytes,trace_bytes);abort();
            }
        }
        if(v>=16){
            unsigned flash,powercut,bytes;
            for(flash=0;flash<4;flash++)for(powercut=0;powercut<2;powercut++){
                struct r8152 tp={0};tp.version=v;
                reset(data,size,flash==3?2:0);
                if(flash==1||flash==2)regs[1][0xd3ae]=2;
                if(flash==2)regs[0][0xd284]=0x20;
                assert(!vendor_ram_code_speed_up(&tp,!powercut));
                bytes=trace_bytes;memcpy(expected,trace,bytes);
                reset(data,size,flash==3?2:0);
                if(flash==1||flash==2)regs[1][0xd3ae]=2;
                if(flash==2)regs[0][0xd284]=0x20;
                bus.Flags=RtlUsbFwVendorFlash|(powercut?RtlUsbFwPowerCut:0);
                assert(!RtlUsbFwExecute(data,size,v,&bus,&r)&&!r.PatchRequested);
                assert(trace_bytes==bytes&&!memcmp(expected,trace,bytes));
                count=calls;
                for(i=1;i<=count;i++){
                    reset(data,size,flash==3?2:0);
                    if(flash==1||flash==2)regs[1][0xd3ae]=2;
                    if(flash==2)regs[0][0xd284]=0x20;
                    fail_at=i;
                    assert(RtlUsbFwExecute(data,size,v,&bus,&r)==RtlUsbFwExecIo&&calls==i);total++;
                    reset(data,size,flash==3?2:0);
                    if(flash==1||flash==2)regs[1][0xd3ae]=2;
                    if(flash==2)regs[0][0xd284]=0x20;
                    cancel_at=i;
                    assert(RtlUsbFwExecute(data,size,v,&bus,&r)==RtlUsbFwExecCancelled&&calls==i-1);total++;
                }
            }
            printf("PASS vendor ram%u: exact unchanged-vendor write traces for flash/no-flash/already-bypassed/newest and both power-cut modes\n",v);
        }
        bus.Flags=v>=16?RtlUsbFwVendorFlash:0;
        reset(data,size,0);assert(!RtlUsbFwExecute(data,size,v,&bus,&r));count=calls;
        for(i=1;i<=count;i++){
            reset(data,size,0);fail_at=i;
            assert(RtlUsbFwExecute(data,size,v,&bus,&r)==RtlUsbFwExecIo&&calls==i);total++;
            reset(data,size,0);cancel_at=i;
            assert(RtlUsbFwExecute(data,size,v,&bus,&r)==RtlUsbFwExecCancelled&&calls==i-1);total++;
        }
        reset(data,size,0);bus.MaxOperations=1;
        assert(RtlUsbFwExecute(data,size,v,&bus,&r)==RtlUsbFwExecBudget&&calls==1);bus.MaxOperations=200000;
        reset(data,size,0);bus.MaxDelayMs=1;
        assert(RtlUsbFwExecute(data,size,v,&bus,&r)==RtlUsbFwExecBudget&&r.DelayMs<=1);bus.MaxDelayMs=20000;
        reset(data,size,0);data[0]^=1;
        assert(RtlUsbFwExecute(data,size,v,&bus,&r)==RtlUsbFwExecInvalid&&!calls);data[0]^=1;
        reset(data,size,0);all_ones=1;
        assert(RtlUsbFwExecute(data,size,v,&bus,&r)!=RtlUsbFwExecOk);
        if(v==4||v==5||v==6||v==11||v==13||v==15||v==16||v==17){
            reset(data,size,0);stalled=1;
            assert(RtlUsbFwExecute(data,size,v,&bus,&r)==RtlUsbFwExecTimeout&&r.DelayMs<20000);
        }
        if(v==13||v==15||v==16||v==17){
            reset(data,size,0);wrong_version=1;
            assert(RtlUsbFwExecute(data,size,v,&bus,&r)==RtlUsbFwExecVersion);
        }
        printf("PASS USB replay %s v%u: three exact upstream write traces; %u transfer/wait boundaries\n",argv[arg],v,count);
        reset(data,size,0);bus.Flags=0x80;
        assert(RtlUsbFwExecute(data,size,v,&bus,&r)==RtlUsbFwExecArgument&&!calls);
        free(data);bus.Flags=0;
    }
    printf("PASS: %u I/O-failure and cancellation schedules; SHA rejection before I/O, bounded timeouts/budgets and version readback\n",total);
    return 0;
}
