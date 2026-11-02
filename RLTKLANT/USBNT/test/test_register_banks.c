/* Vendor register bank, address, and byte-enable contract of the model. */
#include <assert.h>
#include <stdio.h>
#include "rtl815x-core.h"
static R815xCore core;
int main(void){
 uint8_t data[12],out[12];unsigned i;
 memset(&core,0,sizeof(core));r815c_reset(&core);
 for(i=0;i<sizeof(data);i++)data[i]=(uint8_t)(i+1);
 assert(!r815c_control_write(&core,0xd400,0x00ff,data,4));
 assert(!r815c_control_read(&core,0xd400,0,out,4));assert(!memcmp(out,data,4));
 assert(!r815c_control_read(&core,0xd400,0x100,out,4));assert(!out[0]&&!out[1]&&!out[2]&&!out[3]);
 data[3]=0x0c;
 assert(!r815c_control_write(&core,0xe810,0x0088,data,4));assert(!core.rx_enabled&&!core.tx_enabled);
 assert(!r815c_control_write(&core,0xe810,0x0188,data,4));assert(core.rx_enabled&&core.tx_enabled);
 data[3]=0;
 assert(!r815c_control_write(&core,0xe810,0x0088,data,4));assert(core.rx_enabled&&core.tx_enabled);
 for(i=0;i<sizeof(data);i++)data[i]=(uint8_t)(i+1);
 assert(!r815c_control_write(&core,0xd400,0x0181,data,12));
 assert(!r815c_control_read(&core,0xd400,0x100,out,12));
 for(i=0;i<12;i++)assert(out[i]==((i==0||(i>=4&&i<8)||i==11)?data[i]:0));
 assert(r815c_control_read(&core,0xfffc,0,out,8));
 assert(r815c_control_write(&core,0xfffc,0xff,data,8));
 assert(r815c_control_read(&core,0xc000,0x200,out,4));
 assert(r815c_control_write(&core,0xc000,0x2ff,data,4));
 assert(r815c_control_read(&core,0xc001,0x100,out,4));
 assert(r815c_control_read(&core,0xc000,0x100,out,3));
 r815c_reset(&core);assert(!core.usb_reg[0xd400]&&!core.rx_enabled&&!core.tx_enabled);
 {
  static const uint16_t versions[]={0x4c00,0x4c10,0x5c00,0x5c10,0x5c20,0x5c30,0x4800,0x6000,0x6010,0x7020,0x7030,0x7400,0x7410,0x6400,0x7420,0x1030,0x2020};
  static const uint16_t statuses[]={0xb,0xb,0x13,0x13,0x13,0x13,0xb,0x13,0x13,0x403,0x403,0x403,0x403,0x13,0x13,0x1003,0x4003};
  for(i=0;i<sizeof(versions)/sizeof(versions[0]);i++){
   core.version=versions[i];r815c_set_link(&core,1);
   assert(!r815c_control_read(&core,0xe908,0x100,out,4));
   assert((out[0]|((uint16_t)out[1]<<8))==statuses[i]);
   r815c_reset(&core);assert(core.link_up);
   assert((core.reg[0xe908]|((uint16_t)core.reg[0xe909]<<8))==statuses[i]);
   r815c_set_link(&core,0);assert(!core.reg[0xe908]&&!core.reg[0xe909]);
   r815c_reset(&core);assert(!core.link_up&&!core.reg[0xe908]&&!core.reg[0xe909]);
  }
 }
 puts("PASS: independent USB/PLA banks, command side effects, first/middle/last byte enables and malformed transfers");
 puts("PASS: revision-specific PHY status, cable changes and reset preservation");
 return 0;
}
