/* Device names follow detected silicon, never a possibly reused USB PID.
 * Revision families: pinned vendor r8152 v2.22.1 rtl_ops_init/get_hw_ver;
 * RTL8153D/0x7420 corroborated by OpenBSD if_ure.c revision 1.38.
 * A hardware signature identifies the revision without guessing a package
 * suffix (for example BG) that the signature does not encode. */
#ifndef RTL_USB_NAMES_H
#define RTL_USB_NAMES_H
#include "rtl815x.h"
static const char *RtlUsbDetectedName(PRTL_USB_NIC n)
{
    if(n->Chip && n->Chip->DeviceId==0x8150)
        return "Realtek RTL8150 USB 10/100 Fast Ethernet";
    switch(n->HwVersion){
    case 0x4c00:return "Realtek RTL8152 USB 10/100 Fast Ethernet (rev 4C00)";
    case 0x4c10:return "Realtek RTL8152 USB 10/100 Fast Ethernet (rev 4C10)";
    case 0x4800:return "Realtek RTL8152 USB 10/100 Fast Ethernet (rev 4800)";
    case 0x5c00:return "Realtek RTL8153 USB Gigabit Ethernet (rev 5C00)";
    case 0x5c10:return "Realtek RTL8153 USB Gigabit Ethernet (rev 5C10)";
    case 0x5c20:return "Realtek RTL8153 USB Gigabit Ethernet (rev 5C20)";
    case 0x5c30:return "Realtek RTL8153 USB Gigabit Ethernet (rev 5C30)";
    case 0x6000:return "Realtek RTL8153B USB Gigabit Ethernet (rev 6000)";
    case 0x6010:return "Realtek RTL8153B USB Gigabit Ethernet (rev 6010)";
    case 0x6400:return "Realtek RTL8153C USB Gigabit Ethernet (rev 6400)";
    case 0x7420:return "Realtek RTL8153D USB Gigabit Ethernet (rev 7420)";
    case 0x7020:return "Realtek RTL8156 USB 2.5G Ethernet (rev 7020)";
    case 0x7030:return "Realtek RTL8156 USB 2.5G Ethernet (rev 7030)";
    case 0x7400:return "Realtek RTL8156B USB 2.5G Ethernet (rev 7400)";
    case 0x7410:return "Realtek RTL8156B USB 2.5G Ethernet (rev 7410)";
    case 0x1030:return "Realtek RTL8157 USB 5G Ethernet (rev 1030)";
    case 0x2020:return "Realtek RTL8159 USB 10G Ethernet (rev 2020)";
    default:return "Realtek USB Ethernet (unknown silicon)";
    }
}
#endif
