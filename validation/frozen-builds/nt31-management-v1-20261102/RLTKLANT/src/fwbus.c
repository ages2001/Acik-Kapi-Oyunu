/* Independent implementation of the r8169 register protocol. Register
 * reference: pinned Linux r8169_main.c (see validation/source-manifest.json).
 * Firmware selection and revision-specific pre/post PHY steps are separate.
 */
#include "fwbus.h"
#define PHYAR 0x60
#define MACOCP 0xb0
#define PHYOCP 0xb8
#define DPGATE 0xd0
#define FLAG 0x80000000U
#define ABSENT 0xffffffffU
static int Live(RTL_FW_BUS *b){return !b->Removed&&!b->Cancelled(b->Context);}
static int Read(RTL_FW_BUS*b,RTL_FW_U16 port,RTL_FW_U32 *value)
{
    if(!Live(b))return 0;
    *value=b->Read32(b->Context,port);
    /* The highest OCP address with data 0xffff can legitimately produce an
       all-ones transaction word. Confirm absence using the MAC TxConfig. */
    if(!Live(b))return 0;
    if(*value==ABSENT&&(port==0x40||b->Read32(b->Context,0x40)==ABSENT)){b->Removed=1;return 0;}
    return Live(b);
}
static int Valid(RTL_FW_BUS *b)
{
    return b&&b->Read32&&b->Write32&&b->DelayUs&&b->DelayMs&&b->Cancelled&&
           b->PollLimit&&b->PollLimit<=2000&&b->Access<=RtlFwPhyOcp&&
           b->Access>=RtlFwPhyMdio;
}
static int Phy(RTL_FW_BUS *b,int write,RTL_FW_U16 reg,RTL_FW_U16 *data)
{
    RTL_FW_U32 address,command,v=0,gate=0,i;RTL_FW_U16 port;int ok=0;
    if(reg>31||!Live(b))return 0;
    if(b->Access==RtlFwPhyOcp){
        if(reg==31){
            if(write){if(*data>0xfff)return 0;b->Base=*data?(RTL_FW_U32)*data<<4:0xa400;}
            else *data=(RTL_FW_U16)(b->Base==0xa400?0:b->Base>>4);
            return 1;
        }
        address=b->Base+reg*2;
        if(b->Base!=0xa400){if(address<32)return 0;address-=32;}
        if(address>0xfffe||(address&1))return 0;
        port=PHYOCP;command=address<<15;
    }else{
        port=PHYAR;command=(RTL_FW_U32)reg<<16;
        if(b->Access==RtlFwPhyMdioDp){
            if(!write&&reg==3){*data=0xc912;return 1;}
            if(!Read(b,DPGATE,&gate))return 0;
            b->Write32(b->Context,DPGATE,gate&~0x20000U);
        }
    }
    b->Write32(b->Context,port,command|(write?FLAG|*data:0));
    for(i=0;i<b->PollLimit;i++){
        if(!Live(b))break;
        if(!Read(b,port,&v))break;
        if(write?!(v&FLAG):!!(v&FLAG)){if(!write)*data=(RTL_FW_U16)v;ok=1;break;}
        if(!b->DelayUs(b->Context,25))break;
    }
    if(b->Access!=RtlFwPhyOcp&&Live(b)){
        if(!b->DelayUs(b->Context,20))ok=0;
        if(b->Access==RtlFwPhyMdioDp&&Live(b))b->Write32(b->Context,DPGATE,gate);
    }
    return ok&&Live(b);
}
static int Mac(RTL_FW_BUS*b,int write,RTL_FW_U16 reg,RTL_FW_U16*data)
{
    RTL_FW_U32 address,v;
    if(!Live(b))return 0;
    if(write&&reg==31){if(*data>0xfff)return 0;b->Base=(RTL_FW_U32)*data<<4;return 1;}
    address=b->Base+reg;
    if(address>0xfffe||(address&1))return 0;
    b->Write32(b->Context,MACOCP,(address<<15)|(write?FLAG|*data:0));
    /* Reading also flushes a posted write and detects a missing function. */
    if(!Read(b,write?0x40:MACOCP,&v))return 0;
    if(!write)*data=(RTL_FW_U16)v;
    return 1;
}
int RtlFwBusRead(void *context,int mac,RTL_FW_U16 reg,RTL_FW_U16 *data)
{
    RTL_FW_BUS*b=context;if(!Valid(b)||!data||mac<0||mac>1)return 0;
    return mac?Mac(b,0,reg,data):Phy(b,0,reg,data);
}
int RtlFwBusWrite(void *context,int mac,RTL_FW_U16 reg,RTL_FW_U16 data)
{
    RTL_FW_BUS*b=context;if(!Valid(b)||mac<0||mac>1)return 0;
    return mac?Mac(b,1,reg,&data):Phy(b,1,reg,&data);
}
int RtlFwBusOcp(RTL_FW_BUS*b,int mac,int write,RTL_FW_U16 address,RTL_FW_U16*data)
{
    RTL_FW_U32 v,i;RTL_FW_U16 port=mac?MACOCP:PHYOCP;
    if(!Valid(b)||!data||(address&1)||mac<0||mac>1||write<0||write>1||!Live(b))return 0;
    b->Write32(b->Context,port,((RTL_FW_U32)address<<15)|(write?FLAG|*data:0));
    if(mac){
        if(!Read(b,write?0x40:port,&v))return 0;
        if(!write)*data=(RTL_FW_U16)v;
        return 1;
    }
    for(i=0;i<b->PollLimit;i++){
        if(!Read(b,port,&v))return 0;
        if(write?!(v&FLAG):!!(v&FLAG)){if(!write)*data=(RTL_FW_U16)v;return 1;}
        if(!Live(b)||!b->DelayUs(b->Context,25))return 0;
    }
    return 0;
}
static int WaitMs(void*context,RTL_FW_U16 ms)
{
    RTL_FW_BUS*b=context;return Live(b)&&b->DelayMs(b->Context,ms)&&Live(b);
}
static int Cancelled(void*context){return !Live((RTL_FW_BUS*)context);}
enum RTL_FW_RESULT RtlFwBusUpload(RTL_FW_BUS*b,const RTL_FW_U8*image,
                                  RTL_FW_U32 size,RTL_FW_REPORT*report)
{
    RTL_FW_IO io;enum RTL_FW_RESULT result;RTL_FW_U16 bmcr;unsigned i;
    result=RtlFwInspect(image,size,report);if(result!=RtlFwOk)return result;
    if(!Valid(b)||!b->MaxCommands)return RtlFwInvalidArgument;
    b->Base=0xa400;
    if(!Live(b))return RtlFwCancelled;
    if(!RtlFwBusWrite(b,0,31,0))return Live(b)?RtlFwIoError:RtlFwCancelled;
    io.Context=b;io.Read=RtlFwBusRead;io.Write=RtlFwBusWrite;
    io.DelayMs=WaitMs;io.Cancelled=Cancelled;
    io.MaxCommands=b->MaxCommands;io.MaxDelayMs=b->MaxDelayMs;
    result=RtlFwExecute(image,size,&io,report);
    b->Base=0xa400;
    if(!Live(b))return RtlFwCancelled;
    if(!RtlFwBusWrite(b,0,31,0))return !Live(b)?RtlFwCancelled:(result==RtlFwOk?RtlFwIoError:result);
    if(result!=RtlFwOk)return result;
    /* A payload can leave a PHY reset in progress. Bound it to 600 ms,
       independently of the firmware program's own instruction budget. */
    for(i=0;i<=12;i++){
        if(!Live(b))return RtlFwCancelled;
        if(!RtlFwBusRead(b,0,0,&bmcr))return Live(b)?RtlFwIoError:RtlFwCancelled;
        if(!(bmcr&0x8000))return RtlFwOk;
        if(i==12)return RtlFwBudgetExceeded;
        if(!WaitMs(b,50))return Live(b)?RtlFwIoError:RtlFwCancelled;
    }
    return RtlFwBudgetExceeded;
}
