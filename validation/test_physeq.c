/* The real imported PHY sequences and firmware VM against a deterministic
 * register transport. Synthetic firmware isolates pre/post sequencing; this
 * test does not execute a real PHY MCU or establish physical calibration. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../RLTKLANT/third_party/r8169/phycompat.h"
typedef struct {
    u16 phy[32768],mac[32768],mdio[4096][32],page;
    RTL_FW_U32 response[256],events,reads,writes,delays,cancel_at,ms;
    int version,efuse_timeout,delay_fail,not_ready;
} DEVICE;
static DEVICE d;
static void event(DEVICE*s){assert(!s->cancel_at||s->events<s->cancel_at);s->events++;}
static RTL_FW_U32 rd(void*ctx,u16 port)
{
    DEVICE*s=ctx;event(s);s->reads++;
    if(port==0x40)return 0x6c900700;
    return s->response[port];
}
static void wr(void*ctx,u16 port,RTL_FW_U32 word)
{
    DEVICE*s=ctx;unsigned addr=(word>>15)&0xfffe,reg=(word>>16)&31;u16 value=(u16)word;
    event(s);s->writes++;
    if(port==0xd0){s->response[port]=word;return;}
    if(port==0xdc){s->response[port]=s->efuse_timeout?0:0x800000b1U;return;}
    if(port==0xb0){
        if(word>>31)s->mac[addr/2]=value;
        s->response[port]=s->mac[addr/2];return;
    }
    assert(port==0xb8||port==0x60);
    if(port==0x60){
        if(word>>31){
            if(reg==31){assert(value<4096);s->page=value;}
            else s->mdio[s->page][reg]=(s->page==0&&reg==0)?value&~0x8000U:value;
            s->response[port]=word&~0x80000000U;
        }else{
            value=reg==31?s->page:s->mdio[s->page][reg];
            if(s->page==5&&reg==6&&s->mdio[5][5]==0x1b&&!s->not_ready)
                value=s->version==25?0xbf00:0xb300;
            s->response[port]=word|0x80000000U|value;
        }
    }else{
        if(word>>31){s->phy[addr/2]=addr==0xa400?value&~0x8000U:value;s->response[port]=word&~0x80000000U;}
        else s->response[port]=word|0x80000000U|s->phy[addr/2];
    }
}
static int us(void*ctx,RTL_FW_U32 value){DEVICE*s=ctx;(void)value;event(s);s->delays++;return !s->delay_fail;}
static int ms(void*ctx,RTL_FW_U32 value){DEVICE*s=ctx;event(s);s->delays++;s->ms+=value;return !s->delay_fail;}
static int cancel(void*ctx){DEVICE*s=ctx;return s->cancel_at&&s->events>=s->cancel_at;}
static RTL_FW_BUS setup(unsigned version)
{
    RTL_FW_BUS b;memset(&d,0,sizeof(d));memset(&b,0,sizeof(b));d.version=version;
    b.Context=&d;b.Read32=rd;b.Write32=wr;b.DelayUs=us;b.DelayMs=ms;b.Cancelled=cancel;
    b.PollLimit=2;b.MaxCommands=100;b.MaxDelayMs=1000;b.Base=0xa400;
    b.Access=version>=40?RtlFwPhyOcp:RtlFwPhyMdio;return b;
}
static const RTL_FW_U8 program[]={0,0,0x1f,0x80,0,0,0,0x80};
static const RTL_FW_IMAGE patch={"synthetic-sequence-test",program,sizeof(program)};
static void no_more_io(RTL_FW_BUS*b,const RTL_FW_CHIP*chip,RTL_PHY_REPORT*r)
{
    struct phy_device p={b,chip,r};unsigned count=d.events;
    assert(phy_write(&p,1,0x1234)<0);RtlPhyDelay(&p,10);assert(d.events==count);
}
int main(void)
{
    RTL_FW_BUS b;RTL_PHY_REPORT r;RTL_FW_CHIP chip;unsigned xid,gmii,i,count,n=0,faults=0;
    unsigned char seen[81]={0};
    for(gmii=0;gmii<2;gmii++)for(xid=0;xid<4096;xid++){
        const RTL_FW_CHIP*c=RtlFwChip((u16)xid,gmii);if(!c||seen[c->MacVersion])continue;
        seen[c->MacVersion]=1;chip=*c;if(chip.Image)chip.Image=&patch;
        b=setup(chip.MacVersion);
        {enum RTL_FW_RESULT result=RtlPciPhyConfigure(&b,&chip,&r);
         if(result!=RtlFwOk)fprintf(stderr,"baseline version=%u result=%u operation=%u events=%u\n",chip.MacVersion,result,r.FailedOperation,d.events);
         assert(result==RtlFwOk);}
        assert(r.FirmwareApplied==(!!chip.Image&&chip.MacVersion!=52)&&b.Base==0xa400&&d.page==0);
        count=d.events;n++;
        /* Every MMIO/wait boundary: cancellation must stop all later bus I/O. */
        for(i=1;i<=count;i++){
            enum RTL_FW_RESULT result;
            b=setup(chip.MacVersion);d.cancel_at=i;
            result=RtlPciPhyConfigure(&b,&chip,&r);
            if(result!=RtlFwCancelled)fprintf(stderr,"version=%u boundary=%u/%u result=%u events=%u\n",chip.MacVersion,i,count,result,d.events);
            assert(result==RtlFwCancelled);
            assert(d.events==i&&r.FailedOperation!=~0U);no_more_io(&b,&chip,&r);faults++;
        }
    }
    /* Reject sparse-dispatch holes and bad bus arguments before any I/O. */
    for(i=0;i<=81;i++)if(i>80||!seen[i]){
        b=setup(80);chip.MacVersion=i;chip.Image=0;
        assert(RtlPciPhyConfigure(&b,&chip,&r)==RtlFwInvalidArgument&&!d.events);
    }
    b=setup(80);chip.MacVersion=80;b.PollLimit=0;
    assert(RtlPciPhyConfigure(&b,&chip,&r)==RtlFwInvalidArgument&&!d.events);
    /* Firmware readiness is required, and efuse cannot poll forever. */
    chip.MacVersion=25;chip.Image=&patch;b=setup(25);d.not_ready=1;
    assert(RtlPciPhyConfigure(&b,&chip,&r)==RtlFwIoError&&!r.FirmwareApplied);no_more_io(&b,&chip,&r);
    b=setup(25);d.efuse_timeout=1;
    assert(RtlPciPhyConfigure(&b,&chip,&r)==RtlFwIoError&&!r.FirmwareApplied);no_more_io(&b,&chip,&r);
    b=setup(29);chip.MacVersion=29;d.delay_fail=1;
    assert(RtlPciPhyConfigure(&b,&chip,&r)==RtlFwIoError);no_more_io(&b,&chip,&r);
    /* Raw MMD access must preserve the paged access base. */
    b=setup(80);memset(&r,0,sizeof(r));chip.MacVersion=80;
    {struct phy_device p={&b,&chip,&r};
     assert(!phy_write(&p,31,0xa43));
     assert(!phy_write_mmd(&p,MDIO_MMD_VEND2,0xb87c,0x88d7));
     assert(!phy_write_mmd(&p,MDIO_MMD_VEND2,0xb87e,0x1234));
     assert(!__phy_modify_mmd(&p,MDIO_MMD_VEND2,0xb87e,0xff,0xab));
     assert(d.phy[0xb87e/2]==0x12ab&&d.phy[0xb87c/2]==0x88d7&&b.Base==0xa430);
     assert(!phy_write_mmd(&p,MDIO_MMD_AN,MDIO_AN_EEE_ADV,0x55aa)&&d.phy[0xa5d0/2]==0x55aa);
     count=d.events;assert(phy_read(&p,32)<0&&r.Result==RtlFwIoError&&d.events==count);
     no_more_io(&b,&chip,&r);}
    /* Package detection precedes ALL copper programming and firmware I/O.
       Check every supported modern dispatch version, including high ID bits. */
    for(i=61;i<=80;i++)if(seen[i]){
        b=setup(i);chip.MacVersion=i;chip.Image=&patch;d.mac[0xd006/2]=0xab07;
        assert(RtlPciPhyConfigure(&b,&chip,&r)==RtlFwInvalidArgument&&d.writes==1);
        assert(!d.phy[0xa400/2]&&!r.FirmwareApplied);
        no_more_io(&b,&chip,&r);
    }
    b=setup(52);chip.MacVersion=52;chip.Image=0;
    d.mac[0xdc00/2]=0xffb7;d.mac[0xd006/2]=0xab00;
    assert(RtlPciPhyConfigure(&b,&chip,&r)==RtlFwInvalidArgument&&d.writes==2);
    assert(!d.phy[0xa400/2]&&!r.FirmwareApplied);no_more_io(&b,&chip,&r);
    /* Similar IDs outside the masks are not SFP packages. */
    b=setup(52);d.mac[0xdc00/2]=0x30;d.mac[0xd006/2]=1;
    assert(RtlPciPhyConfigure(&b,&chip,&r)==RtlFwOk);
    b=setup(52);d.mac[0xdc00/2]=0x38;
    assert(RtlPciPhyConfigure(&b,&chip,&r)==RtlFwOk);
    b=setup(80);
    assert(RtlPciPhyConfigureByXid(&b,0xfff,1,&r)==RtlFwInvalidArgument&&!d.events);
    b=setup(52);
    assert(RtlPciPhyConfigureByXid(&b,0x54a,1,&r)==RtlFwInvalidArgument&&!d.events);
    assert(RtlPciPhyPrepareForMac(&b,0x54a,1,&r)==RtlFwOk&&d.events&&!r.FirmwareApplied);
    assert(b.Access==RtlFwPhyOcp&&b.Base==0xa400&&b.PollLimit==2000);
    printf("PASS: %u PHY dispatch versions, %u cancellation boundaries, bounded efuse/readiness errors and raw MMD/page isolation\n",n,faults);
    /* Exercise each selected immutable payload with the same transport.
       Polling is bounded but the backing register array does not interpret
       uploaded MCU instructions or synthesize analog/patch readiness. */
    {const RTL_FW_IMAGE*images[33]={0};unsigned used=0,j;
     for(gmii=0;gmii<2;gmii++)for(xid=0;xid<4096;xid++){
        const RTL_FW_CHIP*c=RtlFwChip((u16)xid,gmii);enum RTL_FW_RESULT result;
        if(!c||!c->Image)continue;
        for(j=0;j<used;j++)if(images[j]==c->Image)break;
        if(j<used)continue;
        assert(used<33);images[used++]=c->Image;
        b=setup(c->MacVersion);b.MaxCommands=200000;b.MaxDelayMs=2000;
        if(c->MacVersion==52){
            /* RTL8117 payload transport only; its MAC preconditions are
               outside RtlPciPhyConfigure and this deterministic fixture. */
            result=RtlFwBusUpload(&b,c->Image->Bytes,c->Image->Size,&r.Firmware);
        }else result=RtlPciPhyConfigureByXid(&b,(u16)xid,gmii,&r);
        printf("PAYLOAD %s result=%u commands=%u delay=%u\n",c->Image->Name,result,r.Firmware.Executed,r.Firmware.DelayMs);
        fflush(stdout);assert(result==RtlFwOk);
     }
     printf("PASS: %u selected real payloads traversed the bounded register transport (MCU execution not modeled)\n",used);}
    return 0;
}
