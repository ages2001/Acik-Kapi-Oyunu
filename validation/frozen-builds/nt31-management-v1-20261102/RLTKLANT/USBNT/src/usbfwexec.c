/* SPDX-License-Identifier: GPL-2.0-only
 * Error-tracking replay of the public Linux r8152 firmware-container format.
 * Reference: 65ba8f34ec886ec72b2c07ddc8cf3d815c32869d, rtl8152_apply_firmware,
 * its block handlers, patch handshake and pre/post hooks. See source audit.
 * Unlike upstream void helpers, failures stop the transaction immediately.
 * This transport does not execute MCU instructions or initialize the NIC.
 */
#include "usbfwexec.h"
typedef struct {const RTL_UFW_BUS*Bus;RTL_UFW_EXEC_REPORT*Report;unsigned Version;} EXEC;
static unsigned le16(const RTL_UFW_U8*p){return p[0]|(unsigned)p[1]<<8;}
static RTL_UFW_U32 le32(const RTL_UFW_U8*p){return le16(p)|(RTL_UFW_U32)le16(p+2)<<16;}
static int fail(EXEC*x,enum RTL_UFW_EXEC_RESULT r)
{
    if(x->Report->Result==RtlUsbFwExecOk)x->Report->Result=r;
    return 0;
}
static int ready(EXEC*x)
{
    if(x->Report->Result)return 0;
    if(x->Bus->Cancelled(x->Bus->Context))return fail(x,RtlUsbFwExecCancelled);
    if(x->Report->Operations>=x->Bus->MaxOperations)return fail(x,RtlUsbFwExecBudget);
    x->Report->Operations++;return 1;
}
static int read_bytes(EXEC*x,enum RTL_UFW_SPACE space,unsigned reg,RTL_UFW_U8*p,unsigned n)
{
    if(!ready(x))return 0;
    if(!x->Bus->Read(x->Bus->Context,space,reg,p,n))
        return fail(x,x->Bus->Cancelled(x->Bus->Context)?RtlUsbFwExecCancelled:RtlUsbFwExecIo);
    return 1;
}
static int write_bytes(EXEC*x,enum RTL_UFW_SPACE space,unsigned reg,const RTL_UFW_U8*p,unsigned n)
{
    if(!ready(x))return 0;
    if(!x->Bus->Write(x->Bus->Context,space,reg,p,n))
        return fail(x,x->Bus->Cancelled(x->Bus->Context)?RtlUsbFwExecCancelled:RtlUsbFwExecIo);
    return 1;
}
static int read_reg(EXEC*x,enum RTL_UFW_SPACE space,unsigned reg,unsigned n,unsigned*value)
{
    RTL_UFW_U8 p[4]={0,0,0,0};
    if(!read_bytes(x,space,reg,p,n))return 0;
    *value=le32(p);return 1;
}
static int write_reg(EXEC*x,enum RTL_UFW_SPACE space,unsigned reg,unsigned n,unsigned value)
{
    RTL_UFW_U8 p[4];unsigned i;
    for(i=0;i<4;i++)p[i]=(RTL_UFW_U8)(value>>(i*8));
    return write_bytes(x,space,reg,p,n);
}
static int modify(EXEC*x,enum RTL_UFW_SPACE space,unsigned reg,unsigned n,unsigned clear,unsigned set)
{
    unsigned value;
    return read_reg(x,space,reg,n,&value)&&write_reg(x,space,reg,n,(value&~clear)|set);
}
static int delay(EXEC*x,unsigned ms)
{
    if(!ready(x))return 0;
    if(ms>x->Bus->MaxDelayMs-x->Report->DelayMs)return fail(x,RtlUsbFwExecBudget);
    x->Report->DelayMs+=ms;
    if(!x->Bus->DelayMs(x->Bus->Context,ms))
        return fail(x,x->Bus->Cancelled(x->Bus->Context)?RtlUsbFwExecCancelled:RtlUsbFwExecIo);
    return 1;
}
static int poll(EXEC*x,enum RTL_UFW_SPACE space,unsigned reg,unsigned width,
                unsigned mask,unsigned expected,unsigned tries,unsigned ms)
{
    unsigned i,v;
    for(i=0;i<tries;i++){
        if(ms&&!delay(x,ms))return 0;
        if(!read_reg(x,space,reg,width,&v))return 0;
        if(v==(width==1?0xffU:0xffffU))return fail(x,RtlUsbFwExecIo);
        if((v&mask)==expected)return 1;
    }
    return fail(x,RtlUsbFwExecTimeout);
}
static int patch(EXEC*x,int request)
{
    if(!modify(x,RtlUsbFwPhy,0xb820,2,0x10,request?0x10:0))return 0;
    x->Report->PatchRequested=request;
    if(x->Bus->Flags&RtlUsbFwPowerCut)return 1;
    return poll(x,RtlUsbFwPhy,0xb800,2,0x40,request?0x40:0,5000,1);
}
static int key(EXEC*x,unsigned reg,unsigned value)
{
    if(!reg)return fail(x,RtlUsbFwExecInvalid);
    if(value)return write_reg(x,RtlUsbFwSram,reg,2,value)&&write_reg(x,RtlUsbFwSram,0xb82e,2,1);
    return write_reg(x,RtlUsbFwSram,0,2,0)&&modify(x,RtlUsbFwPhy,0xb82e,2,1,0)&&
           write_reg(x,RtlUsbFwSram,reg,2,0);
}
static int clear_bp(EXEC*x,enum RTL_UFW_SPACE space)
{
    static const RTL_UFW_U8 zero[32]={0};
    unsigned n=8,v=x->Version;
    if(v>=14&&v!=15){
        n=16;if(!write_reg(x,space,0xfc48,2,0))return 0;
    }else if(v>=8&&space==RtlUsbFwUsb){
        n=16;if(!write_reg(x,space,0xfc48,2,0))return 0;
    }else if(v!=1&&v!=2&&v!=7){
        if(!write_reg(x,space,0xfc38,1,0))return 0;
    }
    return write_bytes(x,space,0xfc28,zero,n*2)&&delay(x,3)&&write_reg(x,space,0xfc26,2,0);
}
static int mac(EXEC*x,const RTL_UFW_U8*b)
{
    enum RTL_UFW_SPACE space=le32(b)==1?RtlUsbFwPla:RtlUsbFwUsb;
    unsigned v,reg=le16(b+60),off=le16(b+8),n;
    if(reg){
        if(!read_reg(x,RtlUsbFwUsb,reg,1,&v))return 0;
        if(v==0xff)return fail(x,RtlUsbFwExecIo);
        if(v>=b[62])return 1;
    }
    if(!clear_bp(x,space))return 0;
    if(x->Version==4&&space==RtlUsbFwPla){
        if(!read_reg(x,space,0xd38e,2,&v))return 0;
        if(!(v&1)&&(!write_reg(x,space,0xd38c,2,0x82)||!write_reg(x,space,0xd38e,2,0x82)))return 0;
    }
    if(!write_bytes(x,space,le16(b+10),b+off,le32(b+4)-off)||
       !write_reg(x,space,le16(b+12),2,le16(b+14)))return 0;
    n=(le16(b+22)*2+3)&~3U;
    if(n&&!write_bytes(x,space,le16(b+20),b+24,n))return 0;
    if(le16(b+16)&&!write_reg(x,space,le16(b+16),2,le16(b+18)))return 0;
    return !reg||write_reg(x,RtlUsbFwUsb,reg,1,b[62]);
}
static int stream(EXEC*x,const RTL_UFW_U8*b)
{
    unsigned at=le16(b+8),n=le32(b+4);
    if(!write_reg(x,RtlUsbFwPhy,0xa436,2,le16(b+10)))return 0;
    for(;at<n;at+=2)if(!write_reg(x,RtlUsbFwPhy,0xa438,2,le16(b+at)))return 0;
    return 1;
}
static int nc(EXEC*x,const RTL_UFW_U8*b)
{
    unsigned i;
    if(!write_reg(x,RtlUsbFwSram,le16(b+20),2,le16(b+22))||
       !write_reg(x,RtlUsbFwSram,le16(b+12),2,le16(b+14))||!stream(x,b)||
       !write_reg(x,RtlUsbFwSram,le16(b+16),2,le16(b+18)))return 0;
    for(i=0;i<le16(b+30);i++)
        if(!write_reg(x,RtlUsbFwSram,le16(b+28)+2*i,2,le16(b+32+2*i)))return 0;
    return write_reg(x,RtlUsbFwSram,le16(b+20),2,le16(b+24));
}
static int phy_union(EXEC*x,const RTL_UFW_U8*b)
{
    unsigned i;
    for(i=0;i<b[56];i++)if(!write_reg(x,RtlUsbFwSram,le16(b+12+4*i),2,le16(b+14+4*i)))return 0;
    if(!stream(x,b))return 0;
    for(i=0;i<b[57];i++)if(!write_reg(x,RtlUsbFwSram,le16(b+20+4*i),2,le16(b+22+4*i)))return 0;
    return !b[57]||!le16(b+52)||write_reg(x,RtlUsbFwSram,le16(b+52),2,le16(b+54));
}
static int speedup(EXEC*x,const RTL_UFW_U8*b)
{
    unsigned v,at=le16(b+8),length=le32(b+4),n,version=le16(b+10);
    if(!read_reg(x,RtlUsbFwSram,0x801e,2,&v))return 0;
    if(v==0xffff)return fail(x,RtlUsbFwExecIo);
    if(v>=version)return 1;
    if(x->Bus->Flags&RtlUsbFwVendorFlash){
        if(!read_reg(x,RtlUsbFwPla,0xd3ae,2,&v))return 0;
        if(v==0xffff)return fail(x,RtlUsbFwExecIo);
        if(v&2){
            if(!read_reg(x,RtlUsbFwUsb,0xd284,2,&v))return 0;
            if(v==0xffff)return fail(x,RtlUsbFwExecIo);
            if(!(v&0x20)&&!modify(x,RtlUsbFwUsb,0xd284,2,0,0x20))return 0;
        }
    }
    if(!patch(x,1))return 0;
    while(at<length){
        n=length-at;if(n>2048)n=2048;
        if(!modify(x,RtlUsbFwUsb,0xd284,2,0,0x44)||
           !write_bytes(x,RtlUsbFwUsb,le16(b+12),b+at,n)||
           !modify(x,RtlUsbFwPla,0xdc6a,2,0,0x10)||
           !poll(x,RtlUsbFwPla,0xdc6a,2,0x10,0,1000,0))return 0;
        at+=n;
    }
    if(!patch(x,0)||!read_reg(x,RtlUsbFwSram,0x801e,2,&v))return 0;
    return v==version?1:fail(x,RtlUsbFwExecVersion);
}
static int pre(EXEC*x)
{
    if(x->Version==4||x->Version==5){
        if(!poll(x,RtlUsbFwUsb,0xe404,1,1,0,104,1))return 0;
        if(x->Version==5&&!modify(x,RtlUsbFwUsb,0xcfca,2,0x4000,0))return 0;
    }
    if(x->Version==9||x->Version==14)return write_reg(x,RtlUsbFwUsb,0xd340,2,0x807d);
    return 1;
}
static int post(EXEC*x)
{
    unsigned v;
    if(x->Version==4||x->Version==5){
        if(!read_reg(x,RtlUsbFwUsb,0xcfef,1,&v))return 0;
        if(v&1){
            if(x->Version==4){if(!write_reg(x,RtlUsbFwUsb,0xfc30,2,0x1578))return 0;}
            else if(!modify(x,RtlUsbFwPla,0xfc38,2,0,1))return 0;
        }
        if(!write_reg(x,RtlUsbFwPla,0xd388,2,2250))return 0;
        if(x->Version==5&&(!write_reg(x,RtlUsbFwPla,0xd398,2,0x84)||
           !modify(x,RtlUsbFwUsb,0xcfca,2,0,0x4000)||!modify(x,RtlUsbFwUsb,0xb41e,1,0,3)))return 0;
    }
    if(x->Version==6&&(!modify(x,RtlUsbFwUsb,0xb41e,1,0,3)||
       !modify(x,RtlUsbFwUsb,0xcfcc,2,0,0x200)))return 0;
    if(x->Version==9){
        if(!read_reg(x,RtlUsbFwUsb,0xd81f,1,&v))return 0;
        if((v&4)&&!modify(x,RtlUsbFwPla,0xfc38,2,0,1))return 0;
        if(!modify(x,RtlUsbFwUsb,0xd334,2,0,2)||!modify(x,RtlUsbFwUsb,0xd4e8,2,0,2)||
           !modify(x,RtlUsbFwUsb,0xcfcc,2,0,0x200))return 0;
    }
    if(x->Version==14)return modify(x,RtlUsbFwUsb,0xd334,2,0,0x100)&&modify(x,RtlUsbFwUsb,0xd4e8,2,0,2);
    if(x->Version==11)return modify(x,RtlUsbFwUsb,0xcfcc,2,0,0x200)&&
        write_reg(x,RtlUsbFwUsb,0xd480,4,0x4026840e)&&write_reg(x,RtlUsbFwUsb,0xd480,4,0x4001acc9);
    return 1;
}
enum RTL_UFW_EXEC_RESULT RtlUsbFwExecute(const RTL_UFW_U8*data,RTL_UFW_U32 size,
    unsigned version,const RTL_UFW_BUS*bus,RTL_UFW_EXEC_REPORT*report)
{
    EXEC x;unsigned at=64,keyreg=0,patch_phy=1,i;
    if(!report)return RtlUsbFwExecArgument;
    for(i=0;i<sizeof(*report);i++)((RTL_UFW_U8*)report)[i]=0;
    if(!bus||!bus->Read||!bus->Write||!bus->DelayMs||!bus->Cancelled||
       !bus->MaxOperations||bus->MaxOperations>200000||!bus->MaxDelayMs||bus->MaxDelayMs>20000||
       (bus->Flags&~(RtlUsbFwPowerCut|RtlUsbFwVendorFlash))){
        report->Result=RtlUsbFwExecArgument;return report->Result;
    }
    if(RtlUsbFwValidate(data,size,version,RtlUsbFwCheckDigest,0,&report->Validation)){
        report->Result=RtlUsbFwExecInvalid;return report->Result;
    }
    x.Bus=bus;x.Report=report;x.Version=version;
    if(!pre(&x))return report->Result;
    while(at<size){
        const RTL_UFW_U8*b=data+at;unsigned type=le32(b),v,n;
        report->Offset=at;
        if(!type)break;
        if(type==1||type==2){if(!mac(&x,b))break;}
        else if(type==3&&patch_phy){keyreg=le16(b+8);if(!patch(&x,1)||!key(&x,keyreg,le16(b+10)))break;}
        else if(type==4&&patch_phy){if(!key(&x,keyreg,0)||!patch(&x,0))break;}
        else if(type==5){if(!nc(&x,b))break;}
        else if(type==14){
            if(!read_reg(&x,RtlUsbFwSram,le16(b+8),2,&v))break;
            if(v==0xffff){fail(&x,RtlUsbFwExecIo);break;}
            patch_phy=v<le16(b+10);
            if(patch_phy&&!write_reg(&x,RtlUsbFwSram,le16(b+8),2,le16(b+10)))break;
        }else if(type==6&&patch_phy){
            if(!read_reg(&x,RtlUsbFwPhy,le16(b+8),2,&v))break;
            n=le16(b+10);
            switch(le16(b+12)){case 0:v&=n;break;case 1:v|=n;break;case 2:v&=~n;break;case 3:v^=n;break;}
            if(!write_reg(&x,RtlUsbFwPhy,le16(b+8),2,v))break;
        }else if(type>=7&&type<=12&&patch_phy){if(!phy_union(&x,b))break;}
        else if(type==13){if(!speedup(&x,b))break;}
        report->Blocks++;at+=(le32(b+4)+7)&~7U;
    }
    if(!report->Result&&post(&x))report->Offset=0;
    return report->Result;
}
