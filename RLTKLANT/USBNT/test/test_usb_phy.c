/* Independent vendor-protocol fixture: page window versus TGPHY command.
 * Tests production USB PHY accesses, including every transfer failure point.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "usbphy.h"
static UCHAR pla[65536],usb[65536];
static USHORT phy[32768];
static unsigned calls,fail_at,waits,busy_left,completion_delay;
static int stuck,absent,pending;
static USHORT offsets[64],indices[64];
static UCHAR directions[64];
static USHORT word(UCHAR *p){return (USHORT)(p[0]|((USHORT)p[1]<<8));}
static void setword(UCHAR *p,USHORT v){p[0]=(UCHAR)v;p[1]=(UCHAR)(v>>8);}
static void complete(void){
 if(pending){
  unsigned reg=word(usb+0xd630)/2;
  if(usb[0xd634]&2)phy[reg]=word(usb+0xd632);
  else setword(usb+0xd632,phy[reg]);
 }
 usb[0xd634]&=(UCHAR)~1;pending=0;
}
static void delay(PVOID ctx,ULONG ms){
 assert(ctx==&waits && ms==1);waits++;
 if(!stuck && busy_left && !--busy_left)complete();
}
static NDIS_STATUS control(PUSBNT_PIPE p,UCHAR rt,UCHAR req,USHORT off,
                           USHORT index,PVOID buffer,USHORT len){
 UCHAR *bank,*data=(UCHAR*)buffer;unsigned i;int is_read=!!(rt&0x80);
 (void)p;assert(req==5 && !(off&3) && len==4);
 assert(calls<64);offsets[calls]=off;indices[calls]=index;directions[calls]=is_read;calls++;
 if(calls==fail_at)return NDIS_STATUS_FAILURE;
 assert((index&0xff00)==0 || (index&0xff00)==0x100);
 bank=(index&0x100)?pla:usb;
 if(is_read){
  if(absent){memset(data,0xff,len);return NDIS_STATUS_SUCCESS;}
  if(bank==pla && off>=0xb000 && off<0xc000){
   for(i=0;i<len;i+=2)setword(data+i,phy[((word(pla+0xe86c)&0xf000)|(off+i-0xb000))/2]);
  }else memcpy(data,bank+off,len);
 }else{
  for(i=0;i<len;i++)if(index&(1<<i)){
   if(bank==pla && off+i>=0xb000 && off+i<0xc000){
    unsigned a=(word(pla+0xe86c)&0xf000)|(off+i-0xb000);
    phy[a/2]=(USHORT)((phy[a/2]&((a&1)?0xff:0xff00))|((USHORT)data[i]<<((a&1)*8)));
   }else bank[off+i]=data[i];
  }
  if(bank==usb && off==0xd634 && (index&1) && (usb[0xd634]&1)){
   pending=1;busy_left=completion_delay;
   if(!stuck && !busy_left)complete();
  }
 }
 return NDIS_STATUS_SUCCESS;
}
static void reset(void){
 memset(pla,0,sizeof(pla));memset(usb,0,sizeof(usb));memset(phy,0,sizeof(phy));
 calls=fail_at=waits=busy_left=completion_delay=0;stuck=absent=pending=0;
}
int main(void){
 RTL_USB_NIC n;USHORT value;unsigned v,k,count;
 memset(&n,0,sizeof(n));n.Pipe.Control=control;
 for(v=1;v<=17;v++){
  n.Version=(UCHAR)v;reset();phy[0xa5d4/2]=0x5a97;
  assert(USB_OK(RtlUsbPhyRead(&n,0xa5d4,&value,delay,&waits))&&value==0x5a97);
  count=calls;
  if(v<16){
   assert(count==2&&offsets[0]==0xe86c&&indices[0]==0x133&&!directions[0]);
   assert(word(pla+0xe86c)==0xa000&&offsets[1]==0xb5d4&&indices[1]==0x100&&directions[1]);
  }else{
   assert(count==5);
   assert(offsets[0]==0xd634&&indices[0]==0&&directions[0]);
   assert(offsets[1]==0xd630&&indices[1]==0x33&&!directions[1]);
   assert(offsets[2]==0xd634&&indices[2]==0x33&&!directions[2]);
   assert(offsets[3]==0xd634&&directions[3]);
   assert(offsets[4]==0xd630&&directions[4]);
  }
  for(k=1;k<=count;k++){
   reset();fail_at=k;value=0x7777;
   assert(!USB_OK(RtlUsbPhyRead(&n,0xa5d4,&value,delay,&waits))&&value==0xffff&&calls==k);
  }
  reset();assert(USB_OK(RtlUsbPhyWrite(&n,0xc402,0x9876,delay,&waits))&&phy[0xc402/2]==0x9876);
  count=calls;
  if(v>=16){assert(count==5&&offsets[1]==0xd630&&indices[1]==0xcc);assert(word(usb+0xd634)==2);}
  for(k=1;k<=count;k++){
   reset();fail_at=k;assert(!USB_OK(RtlUsbPhyWrite(&n,0xc402,0x9876,delay,&waits))&&calls==k);
  }
  reset();phy[0xa5d4/2]=0xffff;
  assert(USB_OK(RtlUsbPhyRead(&n,0xa5d4,&value,delay,&waits))&&value==0xffff);
 }
 n.Version=4;reset();phy[0xa5d4/2]=1;phy[0xc5d4/2]=2;
 assert(USB_OK(RtlUsbPhyRead(&n,0xa5d4,&value,0,0))&&value==1);
 assert(USB_OK(RtlUsbPhyRead(&n,0xc5d4,&value,0,0))&&value==2);
 setword(pla+0xe86c,0x3000); /* another user changed the page */
 assert(USB_OK(RtlUsbPhyRead(&n,0xa5d4,&value,0,0))&&value==1);
 for(v=16;v<=17;v++){
  n.Version=(UCHAR)v;reset();usb[0xd634]=1;busy_left=2;completion_delay=3;phy[0xa400/2]=0x1234;
  assert(USB_OK(RtlUsbPhyRead(&n,0xa400,&value,delay,&waits))&&value==0x1234&&waits==5);
  reset();completion_delay=3;
  assert(USB_OK(RtlUsbPhyWrite(&n,0xa400,0x33,delay,&waits))&&waits==3&&phy[0xa400/2]==0x33);
  reset();usb[0xd634]=1;stuck=1;
  assert(!USB_OK(RtlUsbPhyRead(&n,0xa400,&value,delay,&waits))&&waits==10&&calls==10&&value==0xffff);
  reset();stuck=1;
  assert(!USB_OK(RtlUsbPhyWrite(&n,0xa400,3,delay,&waits))&&waits==10&&calls==14);
  reset();usb[0xd634]=1;
  assert(!USB_OK(RtlUsbPhyRead(&n,0xa400,&value,0,0))&&calls==1);
  reset();absent=1;
  assert(!USB_OK(RtlUsbPhyRead(&n,0xa400,&value,delay,&waits))&&calls==1&&!waits);
 }
 reset();n.Version=0;assert(!USB_OK(RtlUsbPhyRead(&n,0xa400,&value,0,0))&&!calls);
 n.Version=18;assert(!USB_OK(RtlUsbPhyWrite(&n,0xa400,1,0,0))&&!calls);
 n.Version=4;assert(!USB_OK(RtlUsbPhyRead(&n,0xa401,&value,0,0))&&!calls);
 assert(!USB_OK(RtlUsbPhyRead(&n,0xa400,0,0,0))&&!calls);
 puts("PASS: 17-revision PHY transport, vendor traces, page isolation, all I/O failure boundaries, timed busy completion, timeout and absent-device rejection");
 return 0;
}
