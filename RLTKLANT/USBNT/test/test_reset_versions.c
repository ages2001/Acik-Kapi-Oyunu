/* Revision reset traces independently transcribed from vendor r8152 v2.22.1
 * rtl8152_nic_reset. The register model checks preserved neighboring bytes
 * and every control-transfer failure boundary; it is not PHY validation. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "rtl815x.h"
static UCHAR regs[2][65536];
static struct { USHORT bank,off; UCHAR value; } writes[32];
static unsigned nw,calls,fail_at;
static int stuck;
static NDIS_STATUS control(PUSBNT_PIPE pipe,UCHAR rt,UCHAR req,USHORT off,
                           USHORT index,PVOID data,USHORT length)
{
 unsigned bank=(index&0x100)?1:0,i;UCHAR *bytes=data;
 (void)pipe;assert(req==5&&length==4&&!(off&3));
 calls++;if(calls==fail_at)return NDIS_STATUS_FAILURE;
 if(rt&0x80){memcpy(data,regs[bank]+off,length);return NDIS_STATUS_SUCCESS;}
 for(i=0;i<4;i++)if(index&(1U<<i)){
  assert(nw<32);writes[nw].bank=(USHORT)bank;writes[nw].off=(USHORT)(off+i);writes[nw++].value=bytes[i];
  regs[bank][off+i]=bytes[i];
  if(bank==1&&off+i==0xe813&&bytes[i]==0x10&&!stuck)regs[bank][off+i]=0;
 }
 return NDIS_STATUS_SUCCESS;
}
static void init(RTL_USB_NIC *n,unsigned version){
 memset(n,0,sizeof(*n));memset(regs,0x5a,sizeof(regs));memset(writes,0,sizeof(writes));
 n->Version=(UCHAR)version;n->Running=TRUE;n->Pipe.Control=control;
 regs[1][0xe813]=0x6c;regs[0][0xd4b0]=0xa5;regs[0][0xd406]=0x20;
 nw=calls=fail_at=0;stuck=0;
}
static void checkwrite(unsigned i,unsigned bank,unsigned off,unsigned value){
 assert(i<nw&&writes[i].bank==bank&&writes[i].off==off&&writes[i].value==value);
}
int main(void){
 RTL_USB_NIC n;unsigned v,k,total;
 for(v=1;v<=17;v++){
  init(&n,v);assert(RtlUsbReset(&n)==NDIS_STATUS_SUCCESS&&!n.Running);total=calls;
  if(v==10||v==11){
   assert(nw==10);
   checkwrite(0,1,0xe813,0x68);
   checkwrite(1,0,0xd4b0,0xa4);checkwrite(2,0,0xd4b1,0x5a);
   checkwrite(3,0,0xd406,0x28);checkwrite(4,0,0xd407,0x5a);
   checkwrite(5,1,0xe813,0x60);
   checkwrite(6,0,0xd4b0,0xa5);checkwrite(7,0,0xd4b1,0x5a);
   checkwrite(8,0,0xd406,0x20);checkwrite(9,0,0xd407,0x5a);
  }else if(v>=16){assert(nw==1);checkwrite(0,1,0xe813,0x60);}
  else {assert(nw==1);checkwrite(0,1,0xe813,0x10);}
  assert(regs[1][0xe812]==0x5a&&regs[0][0xd404]==0x5a&&regs[0][0xd405]==0x5a);
  for(k=1;k<=total;k++){
   init(&n,v);fail_at=k;
   assert(RtlUsbReset(&n)!=NDIS_STATUS_SUCCESS&&!n.Running&&calls==k);
  }
 }
 init(&n,4);stuck=1;assert(RtlUsbReset(&n)!=NDIS_STATUS_SUCCESS&&!n.Running);
 init(&n,0);assert(RtlUsbReset(&n)!=NDIS_STATUS_SUCCESS&&!calls);
 init(&n,18);assert(RtlUsbReset(&n)!=NDIS_STATUS_SUCCESS&&!calls);
 puts("PASS: 17 revision reset traces, preserved bits, all I/O failure boundaries, stuck reset, unknown revisions");
 return 0;
}
