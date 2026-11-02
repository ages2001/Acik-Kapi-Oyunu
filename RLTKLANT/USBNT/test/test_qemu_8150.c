/* Real legacy driver against the QEMU RTL8150 register/framing core. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "usbnic.h"
#include "rtl8150-core.h"
static R8150Core core;
static UCHAR rx[1600],tx[1600],sinkbuf[1600];
static ULONG rxlen,txlen,sinklen;
static NDIS_STATUS submit(PVOID target,UCHAR kind,UCHAR ep,PUCHAR setup,
                           PVOID data,ULONG len,PULONG done){
 (void)target;*done=0;
 if(!kind){
  USHORT off=(USHORT)(setup[2]|((USHORT)setup[3]<<8));
  USHORT idx=(USHORT)(setup[4]|((USHORT)setup[5]<<8));
  if(setup[1]!=5||(setup[0]!=0xc0&&setup[0]!=0x40))return NDIS_STATUS_FAILURE;
  if(r8150c_control(&core,setup[0]==0xc0,off,idx,data,(USHORT)len))return NDIS_STATUS_FAILURE;
 }else if(ep==2){
  const uint8_t *frame;
  if(!core.link_up||!core.tx_enabled)return NDIS_STATUS_FAILURE;
  txlen=r8150c_tx_decode(data,len,&frame);assert(txlen&&txlen<=sizeof(tx));
  memcpy(tx,frame,txlen);
 }else if(ep==0x81){
  if(!core.link_up||!core.rx_enabled)return NDIS_STATUS_FAILURE;
  assert(rxlen<=len);memcpy(data,rx,rxlen);len=rxlen;rxlen=0;
 }else return NDIS_STATUS_FAILURE;
 *done=len;return NDIS_STATUS_SUCCESS;
}
static VOID receive(PVOID ctx,PUCHAR frame,ULONG len){(void)ctx;assert(len<=sizeof(sinkbuf));memcpy(sinkbuf,frame,len);sinklen=len;}
int main(void){
 USBNIC nic;USBNT_OSVER os={4,0};UCHAR frame[128],tmp[8];BOOLEAN link;unsigned i;
 memset(&core,0,sizeof(core));memcpy(core.mac,"\x52\x54\x00\x12\x34\x50",6);r8150c_reset(&core);
 assert(USB_OK(UsbNicAttach(&nic,os,UsbHcUhci,0x0bda,0x8150,&core,submit)));
 assert(nic.Nic.Running&&!memcmp(nic.Nic.Mac,core.mac,6));
 assert(core.reg[0x130]==0x9e&&core.reg[0x12f]==0xd8&&core.reg[0x12e]==0xc);
 assert(USB_OK(UsbNicReadLink(&nic,&link))&&!link);
 r8150c_set_link(&core,1);assert(USB_OK(UsbNicReadLink(&nic,&link))&&link);
 for(i=0;i<sizeof(frame);i++)frame[i]=(UCHAR)i;
 assert(USB_OK(UsbNicSend(&nic,frame,42))&&txlen==60&&!memcmp(tx,frame,42));
 for(i=42;i<60;i++)assert(!tx[i]);
 assert(USB_OK(UsbNicSend(&nic,frame,64))&&txlen==65&&!memcmp(tx,frame,64)&&!tx[64]);
 rxlen=r8150c_rx_encode(frame,100,rx);
 assert(USB_OK(UsbNicPoll(&nic,receive,0))&&sinklen==100&&!memcmp(sinkbuf,frame,100));
 r8150c_set_link(&core,0);assert(USB_OK(UsbNicReadLink(&nic,&link))&&!link);
 assert(!USB_OK(UsbNicSend(&nic,frame,64)));
 UsbNicDetach(&nic);assert(!nic.Nic.Running&&!core.rx_enabled&&!core.tx_enabled);
 for(i=0;i<20;i++){
  r8150c_reset(&core);r8150c_set_link(&core,1);
  assert(USB_OK(UsbNicAttach(&nic,os,UsbHcUhci,0x0bda,0x8150,&core,submit)));
  assert(USB_OK(UsbNicReadLink(&nic,&link))&&link);
  {BOOLEAN reset=FALSE;
   assert(USB_OK(UsbNicCheckReset(&nic,&reset))&&!reset);
   r8150c_reset(&core);r8150c_set_link(&core,1);
   assert(USB_OK(UsbNicCheckReset(&nic,&reset))&&reset);
   assert(USB_OK(UsbNicRecoverReset(&nic))&&nic.Nic.Running&&core.rx_enabled&&core.tx_enabled);
   assert(USB_OK(UsbNicSend(&nic,frame,64))&&txlen==65&&!memcmp(tx,frame,64));
  }
  UsbNicDetach(&nic);assert(!core.rx_enabled&&!core.tx_enabled);
 }
 assert(r8150c_control(&core,1,0x120,0x100,tmp,4));
 assert(r8150c_control(&core,1,0xffff,0,tmp,4));
 assert(r8150c_control(&core,1,0x1ff,0,tmp,2));
 memset(tmp,0xff,sizeof(tmp));assert(!r8150c_control(&core,0,0x137,0,tmp,1)&&core.reg[0x137]==0x1c);
 puts("PASS: RTL8150 attach, MAC, padded TX, RX trailer, cable, 20 reattaches, 20 bus-reset recoveries and invalid requests (host interoperability only)");
 return 0;
}
