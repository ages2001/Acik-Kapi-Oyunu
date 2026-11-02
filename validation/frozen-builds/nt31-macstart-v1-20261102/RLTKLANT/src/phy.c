/*
 * phy.c - PHY / media configuration with ceiling + SpeedDuplex enforcement.
 * Copyright (C) 2026 ages2001.
 */
#include "rltklant.h"

/* standard MII registers */
#define MII_BMCR        0x00
#define MII_ANAR        0x04
#define MII_GBCR        0x09
#define BMCR_RESET      0x8000
#define BMCR_SPEED100   0x2000
#define BMCR_ANE        0x1000
#define BMCR_RAN        0x0200
#define BMCR_FULLDUP    0x0100
#define ANAR_10H        0x0020
#define ANAR_10F        0x0040
#define ANAR_100H       0x0080
#define ANAR_100F       0x0100
#define ANAR_PAUSE      0x0400
#define ANAR_SELECT     0x0001
#define GBCR_1000F      0x0200

/* Translate the user's SpeedDuplex choice into an advertisement mask,
   clamped to the chip's media ceiling. 0 (auto) advertises everything up
   to the ceiling. Returns the 1000BASE-T control bits via *gb. */
static USHORT RtlAdvert(PRLTK_ADAPTER a, USHORT *gb)
{
    USHORT anar = ANAR_SELECT | ANAR_PAUSE;
    *gb = 0;
    switch (a->SpeedDuplex) {
    case RLTK_SD_10H:   anar |= ANAR_10H; break;
    case RLTK_SD_10F:   anar |= ANAR_10F; break;
    case RLTK_SD_100H:  anar |= ANAR_100H; break;
    case RLTK_SD_100F:  anar |= ANAR_100F; break;
    case RLTK_SD_1000F: *gb = GBCR_1000F; break;
    case RLTK_SD_2500F:
    case RLTK_SD_5000F:
    case RLTK_SD_10000F: break; /* selected in vendor advertisement below */
    default: /* auto, up to ceiling */
        anar |= ANAR_10H | ANAR_10F;
        if (a->MaxSpeedMbit >= RTL_SPEED_100)  anar |= ANAR_100H | ANAR_100F;
        if (a->MaxSpeedMbit >= RTL_SPEED_1000) *gb = GBCR_1000F;
        break;
    }
    return anar;
}

BOOLEAN RtlConfigurePhy(PRLTK_ADAPTER a)
{
    USHORT anar, gb, current;
    if (a->Backend != RTL_BACKEND_CPLUS) return TRUE;

    anar = RtlAdvert(a, &gb);
    if (!RtlCpMdioWrite(a, MII_ANAR, anar)) return FALSE;
    if (!RtlCpMdioRead(a, MII_GBCR, &current) || current == 0xFFFF) return FALSE;
    if (!RtlCpMdioWrite(a, MII_GBCR, (USHORT)((current & ~0x0300) | gb))) return FALSE;
    if (a->Is8125) {
        USHORT adv, wanted = 0;
        if (!RtlCpPhyOcpRead(a, 0xA5D4, &adv) || adv == 0xFFFF) return FALSE;
        if (a->SpeedDuplex == RLTK_SD_2500F ||
            (!a->SpeedDuplex && a->MaxSpeedMbit >= 2500)) wanted |= 0x0080;
        if (a->SpeedDuplex == RLTK_SD_5000F ||
            (!a->SpeedDuplex && a->MaxSpeedMbit >= 5000)) wanted |= 0x0100;
        if (a->SpeedDuplex == RLTK_SD_10000F ||
            (!a->SpeedDuplex && a->MaxSpeedMbit >= 10000)) wanted |= 0x1000;
        if (!RtlCpPhyOcpWrite(a, 0xA5D4, (adv & ~0x1180) | wanted)) return FALSE;
    }
    return RtlCpMdioWrite(a, MII_BMCR, BMCR_ANE | BMCR_RAN);
}

/* RTL8139 integrated PHY: force or auto-negotiate per SpeedDuplex, clamped
   to Fast Ethernet. */
VOID Rtl8139ConfigureMedia(PRLTK_ADAPTER a)
{
    USHORT bmcr;
    switch (a->SpeedDuplex) {
    case RLTK_SD_10H:  bmcr = 0; break;
    case RLTK_SD_10F:  bmcr = BMCR_FULLDUP; break;
    case RLTK_SD_100H: bmcr = BMCR_SPEED100; break;
    case RLTK_SD_100F: bmcr = BMCR_SPEED100 | BMCR_FULLDUP; break;
    default:
        RtlWrite16(a, R8139_BMCR, BMCR_ANE | BMCR_RAN);
        return;
    }
    RtlWrite16(a, R8139_BMCR, bmcr);
}
