/* SPDX-License-Identifier: GPL-2.0-only
 * Bounded USB firmware-container validation based on Linux r8152.c at
 * 65ba8f34ec886ec72b2c07ddc8cf3d815c32869d (see source-manifest.json).
 * Validate complete blocks before accessing their fields. Unlike the upstream
 * permissive default, unknown block types/revisions are rejected explicitly.
 * No MCU execution, firmware upload or physical-device support is implied.
 */
#include "usbfw.h"
#define UFW_MAX_BYTES (256U*1024U)
#define UFW_MAX_BLOCKS 128U
static unsigned u16(const RTL_UFW_U8*p){return (unsigned)p[0]|(unsigned)p[1]<<8;}
static RTL_UFW_U32 u32(const RTL_UFW_U8*p){return (RTL_UFW_U32)p[0]|(RTL_UFW_U32)p[1]<<8|(RTL_UFW_U32)p[2]<<16|(RTL_UFW_U32)p[3]<<24;}
static int union_version(unsigned v){return v==10||v==11||v==12||v==13||v==15;}
static enum RTL_UFW_RESULT payload(const RTL_UFW_U8*b,RTL_UFW_U32 n,
                                 unsigned header,unsigned align,RTL_UFW_U32*bytes)
{
    unsigned off;
    if(n<header)return RtlUsbFwBounds;
    off=u16(b+8);
    if(off<header||off>=n||(n-off)%align)return RtlUsbFwBounds;
    *bytes=n-off;return RtlUsbFwOk;
}
static enum RTL_UFW_RESULT mac(const RTL_UFW_U8*b,RTL_UFW_U32 n,
                             unsigned v,unsigned type,RTL_UFW_U32*bytes)
{
    unsigned reg,en,max,i;enum RTL_UFW_RESULT r;
    if(type==1){
        if(v==1||v==2||v==7){reg=0xf800;en=0;max=8;}
        else if(v==3||v==4||v==5||v==6||v==8||v==9||v==11||v==12||v==13||v==15){reg=0xf800;en=0xfc38;max=8;}
        else if(v==14){reg=0xf800;en=0xfc48;max=16;}
        else return RtlUsbFwUnsupported;
    }else{
        if(v==3||v==4||v==5||v==6){reg=0xf800;en=0xfc38;max=8;}
        else if(v==8||v==9||v==11||v==12||v==13||v==14||v==15){reg=0xe600;en=0xfc48;max=16;}
        else return RtlUsbFwUnsupported;
    }
    r=payload(b,n,63,4,bytes);if(r)return r;
    if(u16(b+10)!=reg||u16(b+12)!=0xfc26||u16(b+16)!=en||u16(b+20)!=0xfc28)return RtlUsbFwRegister;
    if(u16(b+22)>max||*bytes>0x10000U-reg)return RtlUsbFwBounds;
    for(i=u16(b+22);i<max;i++)if(u16(b+24+2*i))return RtlUsbFwRegister;
    return RtlUsbFwOk;
}
enum RTL_UFW_RESULT RtlUsbFwValidate(const RTL_UFW_U8*data,RTL_UFW_U32 size,
    unsigned version,RTL_UFW_DIGEST digest,void*context,RTL_UFW_REPORT*report)
{
    RTL_UFW_U32 at=64,flags=0,n,type,bytes,next;
    enum RTL_UFW_RESULT r=RtlUsbFwOk;int rank=-1,started=0,stopped=0;
    if(!report)return RtlUsbFwArgument;
    report->Blocks=report->PayloadBytes=report->EndMarker=0;report->ErrorOffset=0;
    report->Result=RtlUsbFwArgument;
    if(!data||!digest||size<64||size>UFW_MAX_BYTES||version<1||version>17)return report->Result;
    if(!digest(context,data+32,size-32,data)){report->Result=RtlUsbFwDigest;return report->Result;}
    while(at<size){
        const RTL_UFW_U8*b=data+at;int part;
        if(size-at<8||report->Blocks>=UFW_MAX_BLOCKS){r=RtlUsbFwBounds;break;}
        type=u32(b);n=u32(b+4);bytes=0;
        if(n<8||n>size-at){r=RtlUsbFwBounds;break;}
        next=(n+7U)&~7U;
        if(next>size-at){r=RtlUsbFwBounds;break;}
        if(type>14){r=RtlUsbFwUnsupported;break;}
        if(type==0){
            if(n!=8||at+n!=size){r=RtlUsbFwBounds;break;}
            report->EndMarker=1;at+=n;break;
        }
        if(type==1||type==2){
            if(flags&(1U<<type)){r=RtlUsbFwSequence;break;}
            r=mac(b,n,version,type,&bytes);
        }else if(type==3){
            if(started||stopped||rank!=-1){r=RtlUsbFwSequence;break;}
            if(n!=16){r=RtlUsbFwBounds;break;}
            started=1;
        }else if(type==4){
            if(!started||stopped){r=RtlUsbFwSequence;break;}
            if(n!=8){r=RtlUsbFwBounds;break;}
            stopped=1;
        }else if(type==5){
            if(!started||stopped||rank>=0){r=RtlUsbFwSequence;break;}
            if(version<4||version>6){r=RtlUsbFwUnsupported;break;}
            r=payload(b,n,40,2,&bytes);if(r)break;
            if(u16(b+10)!=0xa014||u16(b+12)!=0xa012||u16(b+16)!=0xa01a||u16(b+20)!=0xb820||u16(b+28)!=0xa000)r=RtlUsbFwRegister;
            if(u16(b+30)>4)r=RtlUsbFwBounds;
            rank=0;
        }else if(type==6||type==14){
            if(!union_version(version)){r=RtlUsbFwUnsupported;break;}
            if(n!=16){r=RtlUsbFwBounds;break;}
            if(type==6){
                if(u16(b+8)!=0xb820||u16(b+10)!=0x80||u16(b+12)>3)r=RtlUsbFwRegister;
            }else{
                if(started||stopped||rank!=-1||(flags&(1U<<14))){r=RtlUsbFwSequence;break;}
                if(u16(b+8)!=0x801e)r=RtlUsbFwRegister;
            }
        }else if(type>=7&&type<=12){
            if(!union_version(version)){r=RtlUsbFwUnsupported;break;}
            if(type!=12){
                part=(int)type-7;
                if(!started||stopped||rank>=part){r=RtlUsbFwSequence;break;}
                rank=part;
            }
            r=payload(b,n,58,2,&bytes);if(r)break;
            if(b[56]>2||b[57]>8)r=RtlUsbFwBounds;
        }else if(type==13){
            if(version!=13&&version!=15&&version!=16&&version!=17){r=RtlUsbFwUnsupported;break;}
            if(flags&(1U<<13)){r=RtlUsbFwSequence;break;}
            r=payload(b,n,16,4,&bytes);if(r)break;
            if(u16(b+12)!=0x9a00)r=RtlUsbFwRegister;
        }
        if(r)break;
        flags|=1U<<type;report->Blocks++;report->PayloadBytes+=bytes;at+=next;
    }
    if(!r&&!report->EndMarker)r=RtlUsbFwBounds;
    if(!r&&started&&!stopped)r=RtlUsbFwSequence;
    if(!r&&!report->Blocks)r=RtlUsbFwSequence;
    report->Result=r;report->ErrorOffset=r?at:0;return r;
}
