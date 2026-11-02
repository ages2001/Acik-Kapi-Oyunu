/*
 * usbntclient.c - RLTKLANT as a client of USBNT's generic USB device
 * interface (\Device\UsbNtDev, usbntcli.h).  Called from DriverEntry: it
 * opens the interface, registers for device arrivals, and when a Realtek
 * USB NIC appears it brings the chip up through the interface (MAC read,
 * RX/TX enable) using the shared rtl815x/rtl8150 protocol layer.  On the
 * systems with no USB stack of their own (NT 3.1-4.0) this is how a USB
 * Ethernet adapter reaches RLTKLANT, on top of usbnt.sys.
 *
 * Copyright (C) 2026 ages2001.
 */
#include "ndiskit.h"
#include "usbnic.h"
#include "usbchip.h"
#include "usbbind.h"
#include "usbntcli.h"
#include "usbcli.h"

#ifdef RTL_DBG
VOID __stdcall WRITE_PORT_UCHAR(PUCHAR, UCHAR);
static VOID udbg(const char *s){ while (*s) WRITE_PORT_UCHAR((PUCHAR)0xE9, (UCHAR)*s++); }
static VOID udbgh(ULONG v){ int i; char b[11]; b[0]='0';b[1]='x';
    for(i=0;i<8;i++){ int n=(v>>((7-i)*4))&0xF; b[2+i]=(char)(n<10?'0'+n:'A'+n-10);} b[10]=0; udbg(b); }
#else
static VOID udbg(const char *s){ (void)s; }
static VOID udbgh(ULONG v){ (void)v; }
#endif

/* one USB NIC instance for the bring-up proof (the data path is per-NIC;
   full NDIS adapter presentation builds on this). */
static const USBNT_DEV_IF *g_Ifc;
static USBNIC            g_UNic;
static RTL_USBNT_TARGET  g_UTarget;
static BOOLEAN           g_Up;

static USBNT_OSVER OsVer(void){ USBNT_OSVER v; v.Major=4; v.Minor=0; return v; }

#ifdef RTL_DBG
/* -------- on-target data-path proof (debug build only) ----------------
   A debug build drives one real exchange through the chip once it is up:
   a DHCP DISCOVER is transmitted (bulk-OUT) and the bulk-IN path is polled
   for the DHCP OFFER a server returns.  Under QEMU -netdev user the built-in
   server answers, so this proves TX onto the wire, RX off the wire, and a
   full DHCP round trip - the foundation TCP/IP needs.  A release build never
   sends anything on its own; it only brings the adapter up. */

static BOOLEAN g_GotOffer;
static ULONG   g_OfferIp;

/* one's-complement IP header checksum over n bytes */
static USHORT IpCksum(const UCHAR *p, int n)
{
    ULONG sum = 0; int i;
    for (i = 0; i + 1 < n; i += 2) sum += (ULONG)((p[i] << 8) | p[i+1]);
    if (i < n) sum += (ULONG)(p[i] << 8);
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (USHORT)(~sum & 0xFFFF);
}

/* build a DHCP DISCOVER for our MAC into out[], return its length */
static ULONG BuildDhcpDiscover(const UCHAR *mac, UCHAR *out)
{
    UCHAR *ip, *udp, *bp, *o;
    USHORT ck, iplen, udplen;
    int i;

    for (i = 0; i < 320; i++) out[i] = 0;

    /* Ethernet */
    for (i = 0; i < 6; i++) out[i] = 0xFF;          /* dst broadcast */
    for (i = 0; i < 6; i++) out[6 + i] = mac[i];    /* src = our MAC */
    out[12] = 0x08; out[13] = 0x00;                 /* IPv4 */

    ip  = out + 14;
    udp = ip + 20;
    bp  = udp + 8;

    /* BOOTP/DHCP */
    bp[0] = 1; bp[1] = 1; bp[2] = 6; bp[3] = 0;     /* op/htype/hlen/hops */
    bp[4] = 0x12; bp[5] = 0x34; bp[6] = 0x56; bp[7] = 0x78;   /* xid */
    bp[10] = 0x80; bp[11] = 0x00;                   /* flags = broadcast */
    for (i = 0; i < 6; i++) bp[28 + i] = mac[i];    /* chaddr */
    bp[236] = 0x63; bp[237] = 0x82; bp[238] = 0x53; bp[239] = 0x63; /* magic */
    o = bp + 240;
    *o++ = 53; *o++ = 1; *o++ = 1;                  /* DHCP msg type = DISCOVER */
    *o++ = 55; *o++ = 3; *o++ = 1; *o++ = 3; *o++ = 6; /* param request list */
    *o++ = 255;                                     /* end */
    udplen = (USHORT)(8 + (o - bp));

    /* UDP */
    udp[0] = 0x00; udp[1] = 0x44;                   /* src port 68 */
    udp[2] = 0x00; udp[3] = 0x43;                   /* dst port 67 */
    udp[4] = (UCHAR)(udplen >> 8); udp[5] = (UCHAR)udplen;
    udp[6] = 0x00; udp[7] = 0x00;                   /* checksum optional */

    /* IP */
    iplen = (USHORT)(20 + udplen);
    ip[0] = 0x45; ip[1] = 0x00;
    ip[2] = (UCHAR)(iplen >> 8); ip[3] = (UCHAR)iplen;
    ip[8] = 0x40; ip[9] = 0x11;                     /* TTL=64, proto=UDP */
    for (i = 0; i < 4; i++) ip[16 + i] = 0xFF;      /* dst 255.255.255.255 */
    ck = IpCksum(ip, 20);
    ip[10] = (UCHAR)(ck >> 8); ip[11] = (UCHAR)ck;

    return (ULONG)(14 + iplen);
}

/* RX sink: recognise a DHCP server reply (BOOTP op=2, UDP src port 67) and
   capture the offered address (yiaddr). */
static VOID DhcpSink(PVOID ctx, PUCHAR f, ULONG n)
{
    (void)ctx;
    if (n < 62) return;
    if (f[12] != 0x08 || f[13] != 0x00) return;     /* IPv4         */
    if (f[23] != 0x11) return;                      /* UDP          */
    if (f[34] != 0x00 || f[35] != 0x43) return;     /* src port 67  */
    if (f[42] != 0x02) return;                      /* BOOTP reply  */
    g_OfferIp = ((ULONG)f[58] << 24) | ((ULONG)f[59] << 16) |
                ((ULONG)f[60] << 8)  |  (ULONG)f[61];   /* yiaddr */
    g_GotOffer = TRUE;
}

static VOID DhcpProof(void)
{
    static UCHAR disc[320];
    ULONG len, tries;
    NDIS_STATUS st;

    len = BuildDhcpDiscover(g_UNic.Nic.Mac, disc);
    udbg("UNT-DHCP tx len="); udbgh(len); udbg("\n");
    st = UsbNicSend(&g_UNic, disc, len);
    udbg("UNT-DHCP tx st="); udbgh((ULONG)st); udbg("\n");
    if (st != NDIS_STATUS_SUCCESS) return;

    for (tries = 0; tries < 4000 && !g_GotOffer; tries++)
        UsbNicPoll(&g_UNic, DhcpSink, (PVOID)0);

    if (g_GotOffer) { udbg("UNT-DHCP OFFER ip="); udbgh(g_OfferIp); udbg("\n"); }
    else            { udbg("UNT-DHCP no-offer\n"); }
}
#endif /* RTL_DBG */

static void UC_CALL UsbNtArrival(void *ctx, const UC_DEVINFO *info)
{
    const RTL_USB_ID *chip;
    USBNT_BIND bind;
    NDIS_STATUS st;
    int i;

    (void)ctx;
    if (!info) return;
    udbg("UNT-ARR vid="); udbgh(info->Vid); udbg(" pid="); udbgh(info->Pid); udbg("\n");
    if (info->Vid != RTL_USB_VENDOR_ID) return;
    chip = RtlUsbMatch(info->Vid, info->Pid);
    if (!chip) { udbg("  UNT-NOTRTL\n"); return; }
    if (g_Up) { udbg("  UNT-DUP\n"); return; }

    udbg("UNT-RTLNIC "); udbg(chip->Name); udbg("\n");
    RtlUsbNtBind(&bind, &g_UTarget, g_Ifc, info->Id);
    udbg("UNT-ATTACH-CALL\n");
    st = UsbNicAttach(&g_UNic, OsVer(), UsbHcEhci, info->Vid, info->Pid,
                      bind.DevObject, bind.Submit);
    if (st == NDIS_STATUS_SUCCESS) {
        g_Up = TRUE;
        udbg("UNT-UP MAC=");
        for (i = 0; i < 6; i++) udbgh(g_UNic.Nic.Mac[i]);
        udbg("\n");
#ifdef RTL_DBG
        DhcpProof();            /* debug build only: prove the data path */
#endif
    } else {
        udbg("UNT-ATTACH-FAIL st="); udbgh((ULONG)st); udbg("\n");
    }
}

static void UC_CALL UsbNtRemoval(void *ctx, ULONG id)
{
    (void)ctx; (void)id;
    if (g_Up) { UsbNicDetach(&g_UNic); g_Up = FALSE; udbg("USB NIC removed\n"); }
}

/* open \Device\UsbNtDev, read its interface table, register for arrivals.
   USBNT's CliRegister replays the devices already present, so a NIC that
   usbnt.sys enumerated before we loaded is brought up here synchronously. */
VOID RtlUsbNtClientStart(VOID)
{
    static WCHAR nm[] = { '\\','D','e','v','i','c','e','\\',
                          'U','s','b','N','t','D','e','v', 0 };
    UNICODE_STRING us;
    PFILE_OBJECT fo = (PFILE_OBJECT)0;
    PDEVICE_OBJECT devo = (PDEVICE_OBJECT)0;
    USBNT_DEV_EXT *ext;
    UC_CLIENT cli;
    NTSTATUS st;

    us.Buffer = nm;
    us.Length = 16 * 2;             /* "\Device\UsbNtDev" = 16 WCHARs */
    us.MaximumLength = 17 * 2;

    st = IoGetDeviceObjectPointer(&us, FILE_READ_DATA, &fo, &devo);
    if (!NT_SUCCESS(st) || !devo) { udbg("USBNT not present (no USB stack)\n"); return; }

    ext = (USBNT_DEV_EXT *)devo->DeviceExtension;
    if (!ext || ext->Magic != USBNT_DEV_EXT_MAGIC || !ext->If) {
        udbg("\\Device\\UsbNtDev has no interface\n");
        if (fo) ObDereferenceObject(fo);
        return;
    }
    g_Ifc = ext->If;
    udbg("UNT-IF-BOUND\n");

    cli.Context = (void *)0;
    cli.Arrival = UsbNtArrival;
    cli.Removal = UsbNtRemoval;
    udbg("UNT-REG\n");
    g_Ifc->Register(&cli);          /* replays present devices synchronously */
    udbg("UNT-REG-RET\n");

    if (fo) ObDereferenceObject(fo);
}
