/* Real firmware bus callbacks under register latency, removal and faults. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "fwbus.h"
typedef struct {
    RTL_FW_U16 phy[32768],mac[32768],mdio[32];
    RTL_FW_U32 response[256],writes,reads,us,ms,gate;
    int absent,cancel,timeout,wait_fail,reset_release;
} DEVICE;
static RTL_FW_U32 read32(void *ctx,RTL_FW_U16 port)
{
    DEVICE*d=ctx;d->reads++;
    if(d->absent)return ~0U;
    if(port==0x40)return 0x6c900700;
    if(port==0xd0)return d->gate;
    return d->response[port];
}
static void write32(void*ctx,RTL_FW_U16 port,RTL_FW_U32 v)
{
    DEVICE*d=ctx;unsigned addr=(v>>15)&0xfffe,reg=(v>>16)&31;
    d->writes++;
    if(port==0xd0){d->gate=v;return;}
    if(port==0xb0){
        if(v>>31)d->mac[addr/2]=(RTL_FW_U16)v;
        d->response[port]=d->mac[addr/2];return;
    }
    assert(port==0xb8||port==0x60);
    if(d->timeout){d->response[port]=v;return;}
    if(v>>31){
        if(port==0xb8)d->phy[addr/2]=(RTL_FW_U16)v;else d->mdio[reg]=(RTL_FW_U16)v;
        d->response[port]=v&~0x80000000U;
    }else d->response[port]=v|0x80000000U|(port==0xb8?d->phy[addr/2]:d->mdio[reg]);
}
static int delayus(void*ctx,RTL_FW_U32 n){DEVICE*d=ctx;d->us+=n;return !d->wait_fail;}
static int delayms(void*ctx,RTL_FW_U32 n){DEVICE*d=ctx;d->ms+=n;if(d->reset_release&&d->ms>=100)d->phy[0xa400/2]&=~0x8000;return !d->wait_fail;}
static int cancelled(void*ctx){return ((DEVICE*)ctx)->cancel;}
static RTL_FW_BUS setup(DEVICE*d,enum RTL_FW_PHY_ACCESS access)
{
    RTL_FW_BUS b;memset(d,0,sizeof(*d));memset(&b,0,sizeof(b));
    b.Context=d;b.Read32=read32;b.Write32=write32;b.DelayUs=delayus;b.DelayMs=delayms;b.Cancelled=cancelled;
    b.Access=access;b.Base=0xa400;b.PollLimit=10;b.MaxCommands=100;b.MaxDelayMs=1000;return b;
}
static void word(unsigned char*p,unsigned n){p[0]=n;p[1]=n>>8;p[2]=n>>16;p[3]=n>>24;}
int main(void)
{
    static DEVICE d;RTL_FW_BUS b;RTL_FW_U16 v;RTL_FW_REPORT report;unsigned before,i;
    unsigned char program[24];
    b=setup(&d,RtlFwPhyOcp);
    assert(RtlFwBusWrite(&b,0,31,0xa43));assert(RtlFwBusWrite(&b,0,0x13,0x1234));
    assert(d.phy[0xa436/2]==0x1234);assert(RtlFwBusRead(&b,0,0x13,&v)&&v==0x1234);
    assert(RtlFwBusWrite(&b,0,31,0xa40));assert(RtlFwBusRead(&b,0,31,&v)&&v==0);
    before=d.writes;assert(!RtlFwBusWrite(&b,0,32,1)&&!RtlFwBusWrite(&b,0,31,0x1000)&&d.writes==before);
    assert(RtlFwBusWrite(&b,0,31,0xfff));d.phy[0xfffe/2]=0xffff;
    assert(RtlFwBusRead(&b,0,23,&v)&&v==0xffff&&!b.Removed);
    assert(!RtlFwBusRead(&b,0,24,&v));
    assert(RtlFwBusWrite(&b,1,31,0xf80));assert(RtlFwBusWrite(&b,1,0,0xabcd));
    assert(d.mac[0xf800/2]==0xabcd&&RtlFwBusRead(&b,1,0,&v)&&v==0xabcd);
    assert(!RtlFwBusWrite(&b,1,1,0)&&!RtlFwBusWrite(&b,1,31,0xffff));
    /* Raw MMD registers do not change the current paged-MDIO base. */
    {unsigned base=b.Base;
     v=0xbeef;assert(RtlFwBusOcp(&b,0,1,0xb87e,&v));
     v=0;assert(RtlFwBusOcp(&b,0,0,0xb87e,&v)&&v==0xbeef&&b.Base==base);
     v=0xffff;assert(RtlFwBusOcp(&b,1,1,0xfffe,&v));
     assert(RtlFwBusOcp(&b,1,0,0xfffe,&v)&&v==0xffff&&!b.Removed);
     before=d.writes;
     assert(!RtlFwBusOcp(&b,0,1,0xb87d,&v)&&!RtlFwBusOcp(&b,2,1,0xb87e,&v));
     assert(!RtlFwBusOcp(&b,0,2,0xb87e,&v)&&d.writes==before);
     d.timeout=1;assert(!RtlFwBusOcp(&b,0,0,0xb87e,&v));
     d.cancel=1;before=d.writes;assert(!RtlFwBusOcp(&b,1,1,0xdd00,&v)&&d.writes==before);}
    b=setup(&d,RtlFwPhyMdioDp);d.gate=0x345678;
    assert(RtlFwBusWrite(&b,0,7,0xffff)&&d.gate==0x345678&&d.us==20);
    assert(RtlFwBusRead(&b,0,7,&v)&&v==0xffff&&d.gate==0x345678);
    before=d.writes;assert(RtlFwBusRead(&b,0,3,&v)&&v==0xc912&&d.writes==before);
    d.timeout=1;assert(!RtlFwBusWrite(&b,0,7,1)&&d.gate==0x345678);
    b=setup(&d,RtlFwPhyOcp);d.timeout=1;
    assert(!RtlFwBusRead(&b,0,0,&v)&&d.us==250);
    b=setup(&d,RtlFwPhyOcp);d.absent=1;
    assert(!RtlFwBusRead(&b,0,0,&v)&&b.Removed);before=d.writes;
    assert(!RtlFwBusWrite(&b,0,1,1)&&!RtlFwBusRead(&b,1,0,&v)&&d.writes==before);
    /* Switch MAC/PHY access through the actual command interpreter. */
    b=setup(&d,RtlFwPhyOcp);
    {unsigned words[]={0x40000001,0x801f0f80,0x80001234,0x40000000,0x801f0000,0x80000000};
     for(i=0;i<6;i++)word(program+4*i,words[i]);}
    assert(RtlFwBusUpload(&b,program,sizeof(program),&report)==RtlFwOk);
    assert(d.mac[0xf800/2]==0x1234&&b.Base==0xa400&&report.Executed==6);
    before=d.writes;word(program,0xf0000000);
    assert(RtlFwBusUpload(&b,program,4,&report)==RtlFwInvalidCommand&&d.writes==before);
    word(program,0x80008000);b=setup(&d,RtlFwPhyOcp);
    assert(RtlFwBusUpload(&b,program,4,&report)==RtlFwBudgetExceeded&&d.ms==600);
    b=setup(&d,RtlFwPhyOcp);d.reset_release=1;
    assert(RtlFwBusUpload(&b,program,4,&report)==RtlFwOk&&d.ms==100);
    b=setup(&d,RtlFwPhyOcp);d.cancel=1;
    assert(RtlFwBusUpload(&b,program,4,&report)==RtlFwCancelled&&!d.writes&&!d.reads);
    b=setup(&d,RtlFwPhyOcp);d.wait_fail=1;
    assert(RtlFwBusUpload(&b,program,4,&report)==RtlFwIoError);
    puts("PASS: firmware bus addressing, mode switches, DP restore, valid all-ones data, timeout/removal/cancellation and bounded reset wait");
    return 0;
}
