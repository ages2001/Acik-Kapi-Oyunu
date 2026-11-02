/* Source-defined RTL8150 enable/disable sequence and partial-I/O failures. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "rtl8150.h"
static UCHAR regs[0x200];
static unsigned calls,fail_at;
static USHORT offsets[16],lengths[16];
static NDIS_STATUS control(PUSBNT_PIPE p,UCHAR rt,UCHAR req,USHORT off,
                           USHORT index,PVOID data,USHORT len){
 (void)p;assert(req==5&&!index&&off+len<=sizeof(regs));
 offsets[calls]=off;lengths[calls]=len;calls++;
 if(calls==fail_at)return NDIS_STATUS_FAILURE;
 if(rt==0xc0)memcpy(data,regs+off,len);
 else{assert(rt==0x40);memcpy(regs+off,data,len);}
 return NDIS_STATUS_SUCCESS;
}
static void reset(void){memset(regs,0,sizeof(regs));calls=fail_at=0;}
int main(void){
 RTL_USB_NIC n;unsigned k;
 memset(&n,0,sizeof(n));n.Pipe.Control=control;
 reset();regs[R8150_RCR+1]=0xa5;
 assert(USB_OK(Rtl8150Start(&n))&&n.Running&&calls==3);
 assert(offsets[0]==0x130&&offsets[1]==0x12f&&offsets[2]==0x12e);
 assert(lengths[0]==1&&lengths[1]==1&&lengths[2]==1);
 assert(regs[0x130]==0x9e&&regs[0x131]==0xa5&&regs[0x12f]==0xd8&&regs[0x12e]==0xc);
 for(k=1;k<=3;k++){
  reset();n.Running=TRUE;fail_at=k;
  assert(!USB_OK(Rtl8150Start(&n))&&!n.Running&&calls==k&&!regs[0x12e]);
 }
 reset();n.Running=TRUE;regs[0x12e]=0x2f;
 assert(USB_OK(Rtl8150Stop(&n))&&!n.Running&&calls==2&&regs[0x12e]==0x23);
 for(k=1;k<=2;k++){
  reset();n.Running=TRUE;regs[0x12e]=0x2f;fail_at=k;
  assert(!USB_OK(Rtl8150Stop(&n))&&!n.Running&&calls==k&&regs[0x12e]==0x2f);
 }
 reset();regs[0x12e]=0xff;n.Running=TRUE;
 assert(!USB_OK(Rtl8150Stop(&n))&&!n.Running&&calls==1);
 reset();fail_at=1;n.Running=TRUE;
 assert(!USB_OK(Rtl8150Reset(&n))&&!n.Running&&calls==1);
 puts("PASS: RTL8150 RCR/TCR/CR sequence, preserved control bits, each start/stop failure and reset state");
 return 0;
}
