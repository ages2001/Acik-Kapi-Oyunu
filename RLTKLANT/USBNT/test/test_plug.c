/* Hot-plug scenarios for the RLTKLANT USB-NIC port manager (real sources). */
#include <stdio.h>
#include <string.h>
#include "usbplug.h"

static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); fails++; } \
                           else printf("ok  : %s\n", msg); } while (0)

static UCHAR regReturn[64];
static USHORT phyStatus=0x0013;
static UCHAR legacyStatus=4;
static int failLink;
static NDIS_STATUS mockSubmit(PVOID dev, UCHAR kind, UCHAR ep, PUCHAR setup,
                              PVOID buffer, ULONG length, PULONG done)
{
    (void)dev;(void)ep;
    if (kind == 0 && setup && (setup[0] & 0x80) && buffer && length <= sizeof(regReturn))
    {
        USHORT off = (USHORT)(setup[2] | (setup[3] << 8));
        if ((off==0xe908 || off==0x0137) && failLink) return NDIS_STATUS_FAILURE;
        memcpy(buffer, regReturn, length);
        if (off==0xe908) {
            memset(buffer,0,length);((PUCHAR)buffer)[0]=(UCHAR)phyStatus;
            ((PUCHAR)buffer)[1]=(UCHAR)(phyStatus>>8);
        }
        if (off==0x0137) ((PUCHAR)buffer)[0]=legacyStatus;
        if (off == 0xE610 && length == 4) {
            ((PUCHAR)buffer)[2] = 0x30;
            ((PUCHAR)buffer)[3] = 0x70;
        }
    }
    if (done) *done = length;
    return NDIS_STATUS_SUCCESS;
}

static int mediaOn = -1, mediaEvents = 0;
static void onMedia(PVOID ctx, BOOLEAN connected) { (void)ctx; mediaOn = connected; mediaEvents++; }

static USBNT_OSVER OS(unsigned maj, unsigned min)
{ USBNT_OSVER o; o.Major=(USHORT)maj; o.Minor=(USHORT)min; return o; }

int main(void)
{
    UCHAR mac[6] = {0x00,0xE0,0x4C,0xAA,0xBB,0xCC};
    memcpy(regReturn, mac, 6);

    /* Scenario A: NT 4.0, device plugged in AFTER the driver loaded. */
    {
        USBNIC_PORT p;
        USBNIC_STATE s;
        mediaOn = -1; mediaEvents = 0;
        UsbNicPortInit(&p, OS(4,0), onMedia, (PVOID)0);
        CHECK(p.State == UsbStateIdle, "A: port idle at load (no device yet)");
        CHECK(!UsbNicPortLinkUp(&p), "A: link down while idle");

        s = UsbNicPortArrival(&p, UsbHcUnknown, 0x0BDA, 0x8156, (PVOID)0x10, mockSubmit);
        CHECK(s == UsbStateAttached, "A: later arrival attaches on NT4");
        CHECK(mediaOn == 1, "A: media connect reported");
        CHECK(UsbNicPortLinkUp(&p), "A: link up after arrival");
    }

    /* Scenario B: NT 3.51, present at boot (arrival fires immediately). */
    {
        USBNIC_PORT p; USBNIC_STATE s;
        mediaEvents = 0;
        UsbNicPortInit(&p, OS(3,51), onMedia, (PVOID)0);
        s = UsbNicPortArrival(&p, UsbHcUnknown, 0x0BDA, 0x8153, (PVOID)0x20, mockSubmit);
        CHECK(s == UsbStateAttached, "B: present-at-boot arrival attaches on NT 3.51");
    }

    /* Scenario C: surprise removal then re-insert, link toggles both ways. */
    {
        USBNIC_PORT p;
        mediaOn = -1; mediaEvents = 0;
        UsbNicPortInit(&p, OS(4,0), onMedia, (PVOID)0);
        UsbNicPortArrival(&p, UsbHcUnknown, 0x0BDA, 0x8153, (PVOID)0x30, mockSubmit);
        CHECK(mediaOn == 1, "C: connect on first insert");
        UsbNicPortRemoval(&p);
        CHECK(mediaOn == 0 && p.State == UsbStateIdle, "C: disconnect + re-armed on removal");
        CHECK(!UsbNicPortLinkUp(&p), "C: link down after removal");
        UsbNicPortArrival(&p, UsbHcUnknown, 0x0BDA, 0x8153, (PVOID)0x31, mockSubmit);
        CHECK(p.State == UsbStateAttached && mediaOn == 1, "C: re-insert attaches again");
        CHECK(p.Arrivals == 2 && p.Removals == 1, "C: arrival/removal counters correct");
    }

    /* Scenario D: data path refused while unplugged, works while attached. */
    {
        USBNIC_PORT p; UCHAR f[60]; int k;
        for (k=0;k<60;k++) f[k]=(UCHAR)k;
        UsbNicPortInit(&p, OS(4,0), onMedia, (PVOID)0);
        CHECK(UsbNicPortSend(&p, f, 60) == NDIS_STATUS_FAILURE, "D: send refused while idle");
        UsbNicPortArrival(&p, UsbHcUnknown, 0x0BDA, 0x8153, (PVOID)0x40, mockSubmit);
        CHECK(UsbNicPortSend(&p, f, 60) == NDIS_STATUS_SUCCESS, "D: send works once attached");
        UsbNicPortRemoval(&p);
        CHECK(UsbNicPortSend(&p, f, 60) == NDIS_STATUS_FAILURE, "D: send refused after unplug");
    }

    /* Scenario E: Windows 2000, NIC on EHCI -> port stays idle (in-box USB),
       but on an xHCI (USB3) port it attaches. */
    {
        USBNIC_PORT p;
        UsbNicPortInit(&p, OS(5,0), onMedia, (PVOID)0);
        UsbNicPortArrival(&p, UsbHcEhci, 0x0BDA, 0x8156, (PVOID)0x50, mockSubmit);
        CHECK(p.State == UsbStateIdle, "E: 2000 EHCI NIC left to in-box stack (idle)");
        UsbNicPortArrival(&p, UsbHcXhci, 0x0BDA, 0x8156, (PVOID)0x51, mockSubmit);
        CHECK(p.State == UsbStateAttached, "E: 2000 xHCI (USB3) NIC attaches on USBNT");
    }

    /* Scenario F: unsupported device arrival is ignored (stays idle). */
    {
        USBNIC_PORT p;
        UsbNicPortInit(&p, OS(4,0), onMedia, (PVOID)0);
        UsbNicPortArrival(&p, UsbHcUnknown, 0x1234, 0x5678, (PVOID)0x60, mockSubmit);
        CHECK(p.State == UsbStateIdle && p.Arrivals == 0, "F: non-Realtek arrival ignored");
    }

    /* USB attachment does not imply that an Ethernet cable has carrier. */
    {
        USBNIC_PORT p;
        phyStatus=0;mediaEvents=0;mediaOn=-1;
        UsbNicPortInit(&p,OS(4,0),onMedia,0);
        UsbNicPortArrival(&p,UsbHcEhci,0x0bda,0x8153,(PVOID)1,mockSubmit);
        CHECK(p.State==UsbStateAttached && !UsbNicPortLinkUp(&p) && !mediaEvents,
              "attached without carrier does not fabricate media connect");
        phyStatus=0x13;
        CHECK(UsbNicPortRefreshLink(&p)==NDIS_STATUS_SUCCESS && mediaOn==1 && mediaEvents==1,
              "cable connect is detected independently of USB arrival");
        UsbNicPortRefreshLink(&p);
        CHECK(mediaEvents==1,"stable carrier does not emit duplicate notifications");
        failLink=1;
        CHECK(UsbNicPortRefreshLink(&p)!=NDIS_STATUS_SUCCESS && !UsbNicPortLinkUp(&p) && mediaEvents==2,
              "failed PHY status read drops cached carrier");
        failLink=0;phyStatus=0xffff;
        CHECK(UsbNicPortRefreshLink(&p)!=NDIS_STATUS_SUCCESS && mediaEvents==2,
              "all-ones PHY status is not link up");
        phyStatus=0x13;UsbNicPortRefreshLink(&p);UsbNicPortRemoval(&p);
        CHECK(mediaEvents==4 && mediaOn==0,"physical removal disconnects established carrier once");
        legacyStatus=2;mediaEvents=0;
        UsbNicPortInit(&p,OS(4,0),onMedia,0);
        UsbNicPortArrival(&p,UsbHcUhci,0x0bda,0x8150,(PVOID)1,mockSubmit);
        CHECK(!UsbNicPortLinkUp(&p),"RTL8150 MSR bit1 is not carrier");
        legacyStatus=4;UsbNicPortRefreshLink(&p);
        CHECK(UsbNicPortLinkUp(&p)&&mediaEvents==1,"RTL8150 carrier is MSR bit2");
        UsbNicPortRemoval(&p);
    }

    printf("\n%s (%d failures)\n", fails ? "TEST FAILED" : "ALL HOT-PLUG TESTS PASSED", fails);
    return fails ? 1 : 0;
}
