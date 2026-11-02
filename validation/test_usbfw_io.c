/* Firmware binding through production DWORD byte enables and PHY paging.
 * The fixture supplies deterministic patch acknowledgements/version updates.
 * It does not execute payload instructions or validate physical calibration.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../RLTKLANT/USBNT/src/usbfwio.h"
static unsigned char registers[2][65536];
static unsigned short phy[32768],sram[65536],sram2[65536],selector,selector2;
static unsigned calls,fail_at,cancel_at,page_writes,speed_version,speed_bytes;
static unsigned word(const unsigned char*p){return p[0]|(unsigned)p[1]<<8;}
static unsigned dword(const unsigned char*p){return word(p)|(unsigned)word(p+2)<<16;}
static unsigned phy_get(unsigned reg){return reg==0xa438?sram[selector]:reg==0xb87e?sram2[selector2]:phy[reg/2];}
static void phy_put(unsigned reg,unsigned value)
{
    phy[reg/2]=(unsigned short)value;
    if(reg==0xa436)selector=(unsigned short)value;
    if(reg==0xa438)sram[selector]=(unsigned short)value;
    if(reg==0xb87c)selector2=(unsigned short)value;
    if(reg==0xb87e)sram2[selector2]=(unsigned short)value;
    if(reg==0xb820){
        phy[0xb800/2]=(value&0x10)?0x40:0;
        if(!(value&0x10)&&speed_bytes)sram[0x801e]=(unsigned short)speed_version;
    }
}
static unsigned mapped(unsigned off){return (word(registers[1]+0xe86c)&0xf000)|(off&0xfff);}
static unsigned byte_get(unsigned bank,unsigned off)
{
    if(bank==1&&off>=0xb000&&off<0xc000)return (phy_get(mapped(off)&~1U)>>((off&1)*8))&255;
    return registers[bank][off];
}
static void byte_put(unsigned bank,unsigned off,unsigned value)
{
    if(bank==1&&off>=0xb000&&off<0xc000){
        unsigned addr=mapped(off)&~1U,shift=(off&1)*8,old=phy_get(addr);
        phy_put(addr,(old&~(255U<<shift))|(value<<shift));
    }else{
        registers[bank][off]=(unsigned char)value;
        if(bank==1&&off==0xe86c)page_writes++;
        if(bank==0&&off>=0x9a00&&off<0xa200)speed_bytes++;
        if(bank==1&&off==0xdc6a)registers[bank][off]&=~0x10U;
        /* Newer host-side PHY accesses use the TGPHY command mailbox. */
        if(bank==0&&off==0xd634&&(value&1)){
            unsigned address=word(registers[0]+0xd630);
            assert(!(address&1));
            if(value&2)phy_put(address,word(registers[0]+0xd632));
            else{
                unsigned result=phy_get(address);
                registers[0][0xd632]=(unsigned char)result;
                registers[0][0xd633]=(unsigned char)(result>>8);
            }
            registers[0][off]&=~1U;
        }
    }
}
static NDIS_STATUS control(PUSBNT_PIPE pipe,UCHAR rt,UCHAR request,USHORT value,
                           USHORT index,PVOID data,USHORT length)
{
    unsigned i,bank=!!(index&0x100);unsigned char*p=data;(void)pipe;
    calls++;assert(!fail_at||calls<=fail_at);assert(!cancel_at||calls<cancel_at);
    if(calls==fail_at)return NDIS_STATUS_FAILURE;
    assert(request==5&&length==4&&!(value&3)&&!(index&~0x1ff));
    if(rt==0xc0){assert(!(index&255));for(i=0;i<4;i++)p[i]=byte_get(bank,value+i);}
    else{
        assert(rt==0x40&&((index&15)==((index>>4)&15)));
        for(i=0;i<4;i++)if(index&(1U<<i))byte_put(bank,value+i,p[i]);
    }
    return NDIS_STATUS_SUCCESS;
}
static int wait_ms(void*p,unsigned ms){(void)p;assert(ms<=20);return 1;}
static int cancelled(void*p){(void)p;return cancel_at&&calls+1>=cancel_at;}
static void setup(RTL_USB_NIC*nic,unsigned version,const unsigned char*data,unsigned size)
{
    unsigned at;
    memset(nic,0,sizeof(*nic));nic->Version=(UCHAR)version;nic->Pipe.Control=control;
    memset(registers,0,sizeof(registers));memset(phy,0,sizeof(phy));memset(sram,0,sizeof(sram));
    memset(sram2,0,sizeof(sram2));selector2=0;
    calls=fail_at=cancel_at=page_writes=speed_version=speed_bytes=selector=0;
    for(at=64;at<size;at+=(dword(data+at+4)+7)&~7U)
        if(dword(data+at)==13)speed_version=word(data+at+10);
}
int main(int argc,char**argv)
{
    unsigned arg,total=0;assert(argc>1&&argc%2==1);
    for(arg=1;arg<(unsigned)argc;arg+=2){
        RTL_USB_NIC nic;RTL_UFW_EXEC_REPORT report;FILE*f=fopen(argv[arg],"rb");
        long length;unsigned char*data;unsigned size,version=atoi(argv[arg+1]),count,i,at;
        assert(f);assert(!fseek(f,0,SEEK_END));length=ftell(f);assert(length>64&&length<262144);rewind(f);
        size=(unsigned)length;data=malloc(size);assert(data&&fread(data,1,size,f)==size);fclose(f);
        setup(&nic,version,data,size);
        assert(RtlUsbFirmwareApply(&nic,data,size,version>=16?RtlUsbFwVendorFlash:0,wait_ms,cancelled,NULL,&report)==RtlUsbFwExecOk);
        count=calls;assert(!nic.Running&&!report.PatchRequested);
        for(at=64;at<size;at+=(dword(data+at+4)+7)&~7U){
            unsigned type=dword(data+at),n=dword(data+at+4);
            if(type==1||type==2){
                unsigned bank=type==1,addr=word(data+at+10),off=word(data+at+8);
                assert(!memcmp(registers[bank]+addr,data+at+off,n-off));
            }
            if(type==3)assert(page_writes&&!(phy[0xb820/2]&0x10));
        }
        for(i=1;i<=count;i++){
            setup(&nic,version,data,size);fail_at=i;
            assert(RtlUsbFirmwareApply(&nic,data,size,version>=16?RtlUsbFwVendorFlash:0,wait_ms,cancelled,NULL,&report)==RtlUsbFwExecIo&&calls==i);total++;
            setup(&nic,version,data,size);cancel_at=i;
            assert(RtlUsbFirmwareApply(&nic,data,size,version>=16?RtlUsbFwVendorFlash:0,wait_ms,cancelled,NULL,&report)==RtlUsbFwExecCancelled&&calls==i-1);total++;
        }
        setup(&nic,version,data,size);nic.Running=TRUE;
        assert(RtlUsbFirmwareApply(&nic,data,size,version>=16?RtlUsbFwVendorFlash:0,wait_ms,cancelled,NULL,&report)==RtlUsbFwExecArgument&&!calls);
        nic.Running=FALSE;
        assert(RtlUsbFirmwareApply(&nic,data,size,version>=16?RtlUsbFwVendorFlash:0,NULL,cancelled,NULL,&report)==RtlUsbFwExecArgument&&!calls);
        printf("PASS USB control binding %s v%u: %u actual control boundaries\n",argv[arg],version,count);free(data);
    }
    printf("PASS: %u per-control failure/cancel schedules; exact MAC payload bytes, PHY paging and stopped-device guards\n",total);
    return 0;
}
