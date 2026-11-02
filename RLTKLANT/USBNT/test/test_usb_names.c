#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "usbnames.h"
int main(void){
 RTL_USB_NIC n;unsigned i;const char*name;
 const unsigned short signatures[]={0x4c00,0x4c10,0x4800,0x5c00,0x5c10,0x5c20,0x5c30,0x6000,0x6010,0x6400,0x7420,0x7020,0x7030,0x7400,0x7410,0x1030,0x2020};
 const char*models[]={"RTL8152 ","RTL8152 ","RTL8152 ","RTL8153 ","RTL8153 ","RTL8153 ","RTL8153 ","RTL8153B ","RTL8153B ","RTL8153C ","RTL8153D ","RTL8156 ","RTL8156 ","RTL8156B ","RTL8156B ","RTL8157 ","RTL8159 "};
 memset(&n,0,sizeof(n));n.Chip=RtlUsbMatch(0x0bda,0x8153);
 for(i=0;i<sizeof(signatures)/sizeof(signatures[0]);i++){
  char revision[16];n.HwVersion=signatures[i];name=RtlUsbDetectedName(&n);
  sprintf(revision,"rev %04X",signatures[i]);assert(strstr(name,models[i]));assert(strstr(name,revision));assert(strlen(name)<80);assert(!strstr(name,"/815")&&!strstr(name,"/B"));
 }
 n.HwVersion=0xffff;assert(strstr(RtlUsbDetectedName(&n),"unknown silicon"));
 n.Chip=RtlUsbMatch(0x0bda,0x8150);assert(!strcmp(RtlUsbDetectedName(&n),"Realtek RTL8150 USB 10/100 Fast Ethernet"));
 puts("PASS: 17 silicon identities, reused PID, unknown revision and legacy name");return 0;
}
