/* Independently implemented bounded interpreter for Realtek's PCI PHY
 * firmware word format. Format/opcode reference: Linux r8169_firmware.c,
 * commit 65ba8f34ec886ec72b2c07ddc8cf3d815c32869d. This module does not
 * select a silicon revision or replace its required initialization sequence.
 */
#include "fwvm.h"
#define FW_MAX_BYTES (1024U*1024U)
typedef struct {const RTL_FW_U8 *Code;RTL_FW_U32 Count;} FW_VIEW;
static RTL_FW_U32 Word(const RTL_FW_U8 *p)
{
    return (RTL_FW_U32)p[0]|((RTL_FW_U32)p[1]<<8)|
           ((RTL_FW_U32)p[2]<<16)|((RTL_FW_U32)p[3]<<24);
}
static enum RTL_FW_RESULT Inspect(const RTL_FW_U8 *image,RTL_FW_U32 size,
                                  FW_VIEW *view,RTL_FW_REPORT *report)
{
    RTL_FW_U32 i,offset=0,count,word,op,reg;RTL_FW_U8 checksum=0;
    if(!report)return RtlFwInvalidArgument;
    report->CommandCount=report->Executed=report->DelayMs=0;
    report->FailedIndex=~0U;
    for(i=0;i<sizeof(report->Version);i++)report->Version[i]=0;
    if(!image||size<4||size>FW_MAX_BYTES)return RtlFwInvalidImage;
    if(Word(image)==0){
        if(size<45)return RtlFwInvalidImage;
        for(i=0;i<size;i++)checksum=(RTL_FW_U8)(checksum+image[i]);
        if(checksum)return RtlFwInvalidImage;
        offset=Word(image+36);count=Word(image+40);
        if(offset<45||offset>size||count>(size-offset)/4)return RtlFwInvalidImage;
        for(i=0;i<32;i++)report->Version[i]=(char)image[i+4];
    }else{
        if(size%4)return RtlFwInvalidImage;
        count=size/4;
    }
    if(!count)return RtlFwInvalidImage;
    view->Code=image+offset;view->Count=count;report->CommandCount=count;
    /* Validate the whole program before any register operation. Branches
       must land inside the program, even when a condition is not taken. */
    for(i=0;i<count;i++){
        word=Word(view->Code+4*i);op=word>>28;reg=(word>>16)&4095;
        switch(op){
        case 0:case 1:case 2:case 7:case 8:case 12:case 14:break;
        case 3:if(reg>i)goto invalid;break;
        case 4:if((word&65535)>1)goto invalid;break;
        case 9:if(count-i<=2)goto invalid;break;
        case 10:case 11:case 13:if(reg>=count-i-1)goto invalid;break;
        default:goto invalid;
        }
    }
    return RtlFwOk;
invalid:
    report->FailedIndex=i;return RtlFwInvalidCommand;
}
enum RTL_FW_RESULT RtlFwInspect(const RTL_FW_U8 *image,RTL_FW_U32 size,RTL_FW_REPORT *report)
{
    FW_VIEW view;return Inspect(image,size,&view,report);
}
enum RTL_FW_RESULT RtlFwExecute(const RTL_FW_U8 *image,RTL_FW_U32 size,
                               const RTL_FW_IO *io,RTL_FW_REPORT *report)
{
    FW_VIEW view;enum RTL_FW_RESULT result;RTL_FW_U32 pc=0,reads=0,word,next,reg,op;
    RTL_FW_U16 value=0,data;int mac=0;
    result=Inspect(image,size,&view,report);
    if(result!=RtlFwOk)return result;
    if(!io||!io->Read||!io->Write||!io->DelayMs||!io->Cancelled||!io->MaxCommands)
        return RtlFwInvalidArgument;
    while(pc<view.Count){
        report->FailedIndex=pc;
        if(io->Cancelled(io->Context))return RtlFwCancelled;
        if(report->Executed==io->MaxCommands)return RtlFwBudgetExceeded;
        report->Executed++;
        word=Word(view.Code+4*pc);op=word>>28;reg=(word>>16)&4095;data=(RTL_FW_U16)word;
        next=pc+1;
        if(op==0){
            if(!io->Read(io->Context,mac,(RTL_FW_U16)reg,&value))return RtlFwIoError;
            reads++;
        }else if(op==8||op==12){
            if(!io->Write(io->Context,mac,(RTL_FW_U16)reg,op==8?data:value))return RtlFwIoError;
        }else if(op==14){
            if(data>io->MaxDelayMs-report->DelayMs)return RtlFwBudgetExceeded;
            if(!io->DelayMs(io->Context,data))return RtlFwIoError;
            report->DelayMs+=data;
        }else if(op==1)value=(RTL_FW_U16)(value|data);
        else if(op==2)value=(RTL_FW_U16)(value&data);
        else if(op==3)next=pc-reg;
        else if(op==4)mac=data;
        else if(op==7)reads=0;
        else if(op==9&&reads==data)next++;
        else if(op==13||(op==10&&value==data)||(op==11&&value!=data))next+=reg;
        pc=next;
    }
    report->FailedIndex=~0U;return RtlFwOk;
}
