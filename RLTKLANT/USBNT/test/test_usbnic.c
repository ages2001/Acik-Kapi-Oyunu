/* Host unit test for the RLTKLANT USB-NIC module (real module sources). */
#include <stdio.h>
#include <string.h>
#include "usbnic.h"
#include "usbchip.h"
#include "rtl8150.h"

static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); fails++; } \
                           else printf("ok  : %s\n", msg); } while (0)

/* ---- capture the last URB the module submitted ------------------- */
static UCHAR  capKind, capEp, capSetup[8];
static UCHAR  capData[2048]; static ULONG capLen;
/* canned register values the mock returns on control-IN */
static UCHAR  regReturn[64];
static USHORT hwVersion = 0x5c10;
static ULONG packageId = 0x1e;
static int packageFailure;
static int stuckReset;
static int shortTransfer;

static NDIS_STATUS mockSubmit(PVOID dev, UCHAR kind, UCHAR ep, PUCHAR setup,
                              PVOID buffer, ULONG length, PULONG done)
{
    (void)dev;
    capKind = kind; capEp = ep; capLen = length;
    if (setup) memcpy(capSetup, setup, 8);
    if (kind == 0) {                       /* control */
        UCHAR rt = setup[0];
        if (rt & 0x80) {                   /* IN: hand back canned regs */
            USHORT off = (USHORT)(setup[2] | (setup[3] << 8));
            if (off == 0xE610) {
                memset(buffer, 0, length);
                if (length == 4) {
                    ((PUCHAR)buffer)[2] = (UCHAR)hwVersion;
                    ((PUCHAR)buffer)[3] = (UCHAR)(hwVersion >> 8);
                }
            } else if (off == 0xDC48) {
                unsigned j;
                if (packageFailure) return NDIS_STATUS_FAILURE;
                for(j=0;j<length;j++)((PUCHAR)buffer)[j]=(UCHAR)(packageId>>(8*j));
            } else if (off == (RTLU_PLA_CR & ~3) || off == R8150_CR) {
                memset(buffer, stuckReset ? 0xFF : 0, length);
            } else if (off >= RTLU_PLA_IDR && off < RTLU_PLA_IDR + 8) {
                memcpy(buffer, regReturn + off - RTLU_PLA_IDR, length);
            } else if (buffer && length <= sizeof(regReturn))
                memcpy(buffer, regReturn, length);
        } else {                           /* OUT: capture written data */
            if (buffer && length <= sizeof(capData)) memcpy(capData, buffer, length);
        }
    } else {                               /* bulk */
        if (buffer && length <= sizeof(capData)) memcpy(capData, buffer, length);
    }
    if (done) *done = length - ((shortTransfer && length) ? 1 : 0);
    return NDIS_STATUS_SUCCESS;
}

static USBNT_OSVER OS(unsigned maj, unsigned min)
{ USBNT_OSVER o; o.Major = (USHORT)maj; o.Minor = (USHORT)min; return o; }

int main(void)
{
    /* ---- 1. routing policy truth table -------------------------- */
    /* NT 3.1 / 3.50 / 3.51 / 4.0: USBNT on every controller tier */
    USBNT_HC_KIND tiers[] = { UsbHcUhci, UsbHcOhci, UsbHcEhci, UsbHcXhci, UsbHcUsb4 };
    int i;
    USBNT_OSVER pre[] = { {3,10},{3,50},{3,51},{4,0} };
    for (i = 0; i < 4; i++) {
        int t, all = 1;
        for (t = 0; t < 5; t++) if (!UsbntUseForNic(pre[i], tiers[t])) all = 0;
        CHECK(all, "NT<=4: USBNT used on UHCI/OHCI/EHCI/xHCI/USB4");
    }
    /* Windows 2000 / XP / XP x64: USBNT only for xHCI + USB4 */
    {
        USBNT_OSVER w2k = OS(5,0), xp = OS(5,1), xp64 = OS(5,2);
        CHECK(!UsbntUseForNic(w2k, UsbHcUhci), "2000: UHCI -> in-box (not USBNT)");
        CHECK(!UsbntUseForNic(w2k, UsbHcOhci), "2000: OHCI -> in-box");
        CHECK(!UsbntUseForNic(w2k, UsbHcEhci), "2000: EHCI -> in-box");
        CHECK( UsbntUseForNic(w2k, UsbHcXhci), "2000: xHCI (USB3) -> USBNT");
        CHECK( UsbntUseForNic(w2k, UsbHcUsb4), "2000: USB4 -> USBNT");
        CHECK(!UsbntUseForNic(xp,  UsbHcEhci), "XP: EHCI -> in-box");
        CHECK( UsbntUseForNic(xp,  UsbHcXhci), "XP: xHCI (USB3) -> USBNT");
        CHECK( UsbntUseForNic(xp64,UsbHcXhci), "XP x64: xHCI (USB3) -> USBNT");
        CHECK(!UsbntUseForNic(xp64,UsbHcEhci), "XP x64: EHCI -> in-box");
    }
    /* classify */
    CHECK(UsbntClassifyHc(0x30) == UsbHcXhci, "prog-if 0x30 -> xHCI");
    CHECK(UsbntClassifyHc(0x40) == UsbHcUsb4, "prog-if 0x40 -> USB4");
    CHECK(UsbntClassifyHc(0x20) == UsbHcEhci, "prog-if 0x20 -> EHCI");

    /* ---- 2. chip match ------------------------------------------ */
    CHECK(RtlUsbMatch(0x0BDA, 0x8153) != 0, "RTL8153 recognized");
    CHECK(RtlUsbMatch(0x0BDA, 0x8154) != 0, "RTL8154 (USB2.0 GbE) recognized");
    CHECK(RtlUsbMatch(0x0BDA, 0x8157) != 0, "RTL8157 recognized");
    CHECK(RtlUsbMatch(0x0BDA, 0x8159) != 0, "RTL8159 (USB 10G) recognized");
    CHECK(RtlUsbMatch(0x0BDA, 0x8159)->MaxSpeed == 10000, "RTL8159 ceiling is 10G");
    CHECK(RtlUsbMatch(0x10EC, 0x8153) == 0, "wrong vendor rejected");
    CHECK(RtlUsbMatch(0x0BDA, 0x8150) != 0, "legacy RTL8150 supported (own backend)");
    CHECK(RtlUsbMatch(0x0BDA, 0x8150)->MinUsbGen == 1, "RTL8150 marked USB 1.1");

    /* Silicon capability ceilings, checked against Linux rtl_ops_init:
       versions 12/13 are 2.5G; versions 14/15 are Gigabit, despite shared
       firmware/initialization families and reused USB product IDs. */
    {
        static const USHORT sig[] = {0x4c00,0x4c10,0x5c00,0x5c10,0x5c20,
            0x5c30,0x4800,0x6000,0x6010,0x7020,0x7030,0x7400,0x7410,
            0x6400,0x7420,0x1030,0x2020};
        static const USHORT speed[] = {100,100,1000,1000,1000,1000,100,
            1000,1000,2500,2500,2500,2500,1000,1000,5000,10000};
        USBNIC u;
        for(i=0;i<17;i++) {
            hwVersion=sig[i];
            CHECK(UsbNicAttach(&u,OS(4,0),UsbHcEhci,0x0bda,0x8153,
                              (PVOID)1,mockSubmit)==NDIS_STATUS_SUCCESS &&
                  u.Nic.Version==i+1 && u.Nic.MaxSpeedMbit==speed[i],
                  "silicon signature selects the source-verified media ceiling");
        }
        shortTransfer=1;
        CHECK(RtlUsbDetectVersion(&u.Nic)!=NDIS_STATUS_SUCCESS &&
              !u.Nic.Version && !u.Nic.MaxSpeedMbit && !u.Nic.DescriptorV2,
              "failed redetection clears stale silicon capabilities");
        shortTransfer=0;hwVersion=0x5c10;
    }

    /* RTL8159's signature alone cannot select a package-specific PHY path. */
    {
        USBNIC u;unsigned package;int ok=1;
        hwVersion=0x2020;
        for(package=0;package<256;package++){
            unsigned masked=package&0x1e;
            int valid=masked==0x18||masked==0x1a||masked==0x1c||masked==0x1e;
            NDIS_STATUS status;
            packageId=package;
            status=UsbNicAttach(&u,OS(4,0),UsbHcEhci,0x0bda,0x8159,(PVOID)1,mockSubmit);
            if(valid)ok&=status==NDIS_STATUS_SUCCESS&&u.Nic.PackageId==masked&&u.Nic.Version==17;
            else ok&=status!=NDIS_STATUS_SUCCESS&&!u.Nic.PackageId&&!u.Nic.Version&&!u.Nic.DescriptorV2;
        }
        CHECK(ok,"RTL8159 all package-byte masks select QFN68/QFN100 or reject before startup");
        packageId=0xffffffffUL;
        CHECK(RtlUsbDetectVersion(&u.Nic)!=NDIS_STATUS_SUCCESS&&!u.Nic.Version&&!u.Nic.PackageId,
              "RTL8159 all-ones package register is not accepted as QFN68");
        packageFailure=1;packageId=0x18;
        CHECK(RtlUsbDetectVersion(&u.Nic)!=NDIS_STATUS_SUCCESS&&!u.Nic.Version&&!u.Nic.PackageId,
              "RTL8159 package transfer failure clears old capabilities");
        packageFailure=0;packageId=0x1e;hwVersion=0x5c10;
    }

    /* ---- 3. attach + register request bytes --------------------- */
    {
        USBNIC u;
        NDIS_STATUS st;
        /* canned MAC for the IDR read during attach */
        UCHAR mac[6] = {0x00,0xE0,0x4C,0x11,0x22,0x33};
        memcpy(regReturn, mac, 6);
        st = UsbNicAttach(&u, OS(4,0), UsbHcUnknown, 0x0BDA, 0x8153,
                          (PVOID)0x1234, mockSubmit);
        CHECK(st == NDIS_STATUS_SUCCESS, "attach RTL8153 on NT4 succeeds");
        CHECK(u.Route == UsbRouteUsbnt, "NT4 route = USBNT");
        CHECK(memcmp(u.Nic.Mac, mac, 6) == 0, "MAC read back from IDR");

        /* the last control during attach (RCR dword write in Start) is OUT */
        /* re-issue a known register write and inspect the setup packet */
        {
            UCHAR v = 0x0C;
            RtlUsbWriteRegs(&u.Nic, RTLU_MCU_TYPE_PLA, RTLU_PLA_CR,
                            RTLU_BYTE_EN_BYTE, &v, 1);
            CHECK(capKind == 0, "reg write is a control transfer");
            CHECK(capSetup[0] == RTLU_RT_WRITE, "bmRequestType = host-to-device vendor");
            CHECK(capSetup[1] == RTLU_REQ_SET_REGS, "bRequest = SET_REGS");
            CHECK(((USHORT)capSetup[2] | (capSetup[3]<<8)) == (RTLU_PLA_CR & ~3),
                  "wValue = aligned command-register DWORD");
            CHECK(((USHORT)capSetup[4] | (capSetup[5]<<8))
                  == (RTLU_MCU_TYPE_PLA | 0x88),
                  "wIndex = MCU_TYPE_PLA | byte-enable");
            CHECK(capData[3] == 0x0C && capLen == 4, "written byte reached the data stage");
        }

        /* read path forms an IN request */
        {
            UCHAR buf[4];
            memset(regReturn, 0xAB, 4);
            RtlUsbReadRegs(&u.Nic, RTLU_MCU_TYPE_PLA, RTLU_PLA_RCR, buf, 4);
            CHECK(capSetup[0] == RTLU_RT_READ, "read uses device-to-host vendor");
            CHECK(buf[0] == 0xAB && buf[3] == 0xAB, "read returned register bytes");
        }

        /* ---- 4. TX framing -------------------------------------- */
        {
            UCHAR frame[60]; int k; ULONG total;
            for (k = 0; k < 60; k++) frame[k] = (UCHAR)k;
            total = UsbNicSend(&u, frame, 60);
            (void)total;
            CHECK(capKind == 1 && capEp == RTLU_EP_BULK_OUT, "TX on bulk-OUT");
            CHECK(capLen == (ULONG)(RTLU_TX_DESC_SIZE + 60), "TX length = desc + frame");
            CHECK((capData[0] | (capData[1]<<8)) == 60, "tx_desc opts1 low = frame len");
            CHECK(capData[RTLU_TX_DESC_SIZE] == 0 &&
                  capData[RTLU_TX_DESC_SIZE+59] == 59, "frame body copied after desc");
            shortTransfer = 1;
            CHECK(UsbNicSend(&u, frame, 60) != NDIS_STATUS_SUCCESS,
                  "short USB transmit is not reported as successful");
            CHECK(RtlUsbReadMac(&u.Nic) != NDIS_STATUS_SUCCESS,
                  "short register read is rejected");
            shortTransfer = 0;
        }
    }

    /* ---- 5. RX parse: two aligned packets in one bulk buffer ----- */
    {
        UCHAR buf[128]; PUCHAR f; ULONG fl, next;
        memset(buf, 0, sizeof(buf));
        /* packet 1: len 20 */
        buf[0] = 24;                      /* opts1 low = 20 */
        buf[RTLU_RX_DESC_SIZE] = 0xAA;
        /* next packet 8-byte aligned after 24+20 = 44 -> 48 */
        buf[48] = 18;                     /* opts1 low = 14 */
        buf[48 + RTLU_RX_DESC_SIZE] = 0xBB;
        next = RtlUsbNextRx(buf, sizeof(buf), 0, &f, &fl);
        CHECK(fl == 20 && f && f[0] == 0xAA, "RX packet 1 parsed (len 20)");
        CHECK(next == 48, "RX next offset is 8-byte aligned (48)");
        next = RtlUsbNextRx(buf, sizeof(buf), next, &f, &fl);
        CHECK(fl == 14 && f && f[0] == 0xBB, "RX packet 2 parsed (len 14)");
    }

    /* ---- 6. RTL8150 legacy backend framing ---------------------- */
    {
        UCHAR frame[64], out[72], rx[24]; PUCHAR f; ULONG fl, n2, k;

        CHECK(RtlUsbMatch(0x0BDA, 0x8150) != 0, "RTL8150 recognized");

        /* TX: raw frame, no descriptor; 60 bytes is not a 64 multiple */
        for (k = 0; k < 60; k++) frame[k] = (UCHAR)k;
        n2 = Rtl8150BuildTx(frame, 60, out);
        CHECK(n2 == 60, "8150 TX = frame length (no tx descriptor)");
        CHECK(out[0] == 0 && out[59] == 59, "8150 TX copies raw frame");
        memset(out, 0xa5, sizeof(out));
        n2 = Rtl8150BuildTx(frame, 42, out);
        CHECK(n2 == 60, "8150 short Ethernet frames are padded to ETH_ZLEN");
        for (k = 42; k < 60; k++) if (out[k]) break;
        CHECK(k == 60 && out[60] == 0xa5,
              "8150 minimum-frame padding is initialized and bounded");

        /* TX: a 64-byte frame gets one pad byte on full-speed bulk-OUT */
        n2 = Rtl8150BuildTx(frame, 64, out);
        CHECK(n2 == 65 && out[64] == 0, "8150 TX pads a 64-byte multiple");

        /* RX: frame + 4-byte trailing status; frame len = buf - 4 */
        memset(rx, 0, sizeof(rx));
        rx[0] = 0xCC;
        n2 = Rtl8150NextRx(rx, 24, &f, &fl);
        CHECK(fl == 20 && f && f[0] == 0xCC, "8150 RX strips 4-byte status");
        CHECK(n2 == 0, "8150 RX is one frame per buffer (no coalescing)");
    }

    /* Independent v2 wire vectors, not values generated by the emulator. */
    {
        UCHAR frame[60], out[76], rx[112];
        PUCHAR f; ULONG fl, next;
        memset(frame, 0x5A, sizeof(frame));
        CHECK(Rtl8157BuildTx(frame, 60, out) == 76, "8157 TX uses 16-byte descriptor");
        CHECK(out[8] == 0xC0 && out[9] == 3 && out[15] == 0xA8 &&
              out[0] == 60 && out[3] == 0xC0 && out[16] == 0x5A && out[75] == 0x5A,
              "8157 TX length and signature match the wire format");
        memset(rx, 0, sizeof(rx));
        rx[2] = 0x80; /* 64 bytes including FCS, shifted left 17 */
        memset(rx + 16, 0x5A, 60);
        next = Rtl8157NextRx(rx, sizeof(rx), 0, &f, &fl);
        CHECK(fl == 60 && f == rx + 16 && next == 80,
              "8157 RX strips FCS and advances on a 16-byte boundary");
        Rtl8157NextRx(rx, 79, 0, &f, &fl);
        CHECK(f == 0 && fl == 0, "8157 RX rejects truncated packet");
        Rtl8157NextRx(rx, sizeof(rx), 0xFFFFFFF8UL, &f, &fl);
        CHECK(f == 0 && fl == 0, "8157 RX rejects overflowing offset");
    }

    /* Reset must fail when the device never clears its reset bit. */
    {
        USBNIC u;
        NDIS_STATUS st;
        stuckReset = 1;
        memset(regReturn, 0xFF, sizeof(regReturn));
        st = UsbNicAttach(&u, OS(4,0), UsbHcUhci, 0x0BDA, 0x8150,
                          (PVOID)0x1234, mockSubmit);
        CHECK(st != NDIS_STATUS_SUCCESS && !u.Nic.Running,
              "8150 stuck reset fails without enabling traffic");
        st = UsbNicAttach(&u, OS(4,0), UsbHcEhci, 0x0BDA, 0x8153,
                          (PVOID)0x1234, mockSubmit);
        CHECK(st != NDIS_STATUS_SUCCESS && !u.Nic.Running,
              "8153 stuck reset fails without enabling traffic");
        stuckReset = 0;
        memset(regReturn, 0, sizeof(regReturn));
        st = UsbNicAttach(&u, OS(4,0), UsbHcUhci, 0x0BDA, 0x8150,
                          (PVOID)0x1234, mockSubmit);
        CHECK(st == NDIS_STATUS_SUCCESS, "8150 attaches after reset clears");
        UsbNicDetach(&u);
        CHECK(!u.Nic.Running && u.Route == UsbRouteNone,
              "8150 detach clears running state");
        CHECK((capSetup[2] | (capSetup[3] << 8)) == R8150_CR &&
              capSetup[4] == 0 && capSetup[5] == 0 && capData[0] == 0,
              "8150 detach uses the legacy stop register");
    }

    {
        USBNIC u;
        hwVersion = 0x1030;
        CHECK(USB_OK(UsbNicAttach(&u, OS(4,0), UsbHcEhci, 0x0BDA, 0x8156,
                                 (PVOID)1, mockSubmit)) && u.Nic.DescriptorV2 &&
              u.Nic.MaxSpeedMbit == 5000,
              "8157 silicon behind PID 8156 selects v2 descriptors");
        hwVersion = 0x2020;
        CHECK(USB_OK(UsbNicAttach(&u, OS(4,0), UsbHcEhci, 0x0BDA, 0x8159,
                                 (PVOID)1, mockSubmit)) && u.Nic.DescriptorV2 &&
              u.Nic.MaxSpeedMbit == 10000,
              "8159 silicon selects v2 descriptors and 10G ceiling");
        hwVersion = 0x7cf0;
        CHECK(!USB_OK(UsbNicAttach(&u, OS(4,0), UsbHcEhci, 0x0BDA, 0x8153,
                                  (PVOID)1, mockSubmit)) && !u.Nic.Running,
              "unknown silicon revision rejected before enabling traffic");
        hwVersion = 0x5c10;
    }
    /* Decode media independently of attachment; reject bus-error values,
       conflicting speed bits, short reads and speeds beyond the silicon. */
    {
        static const USHORT bits[]={4,8,0x100,0x10,0x200,0x400,0x1000,0x4000};
        static const ULONG rates[]={10,100,500,1000,1250,2500,5000,10000};
        USBNIC u;BOOLEAN up,duplex;ULONG speed;USHORT status;int j,good=1;
        hwVersion=0x2020;
        CHECK(USB_OK(UsbNicAttach(&u,OS(4,0),UsbHcEhci,0x0bda,0x8159,(PVOID)1,mockSubmit)),"media fixture attached");
        for(i=0;i<8;i++)for(j=0;j<2;j++){
            status=(USHORT)(bits[i]|2|j);regReturn[0]=(UCHAR)status;regReturn[1]=(UCHAR)(status>>8);
            if(!USB_OK(UsbNicReadMedia(&u,&up,&speed,&duplex))||!up||speed!=rates[i]||duplex!=j)good=0;
        }
        CHECK(good,"all eight media rates and duplex states decoded");
        regReturn[0]=0xff;regReturn[1]=0xff;
        CHECK(!USB_OK(UsbNicReadMedia(&u,&up,&speed,&duplex))&&!up&&!speed&&!duplex,"removed-device media value fails closed");
        regReturn[0]=0x1b;regReturn[1]=0;
        CHECK(!USB_OK(UsbNicReadMedia(&u,&up,&speed,&duplex))&&!up&&!speed,"conflicting media rates rejected");
        regReturn[0]=3;regReturn[1]=0;
        CHECK(!USB_OK(UsbNicReadMedia(&u,&up,&speed,&duplex))&&!up&&!speed,"link without speed rejected");
        regReturn[0]=0x13;shortTransfer=1;
        CHECK(!USB_OK(UsbNicReadMedia(&u,&up,&speed,&duplex))&&!up&&!speed,"short media read fails closed");shortTransfer=0;
        regReturn[0]=3;regReturn[1]=0x40;u.Nic.MaxSpeedMbit=2500;
        CHECK(!USB_OK(UsbNicReadMedia(&u,&up,&speed,&duplex))&&!up&&!speed,"media exceeds silicon ceiling rejected");
        regReturn[0]=0;regReturn[1]=0;
        CHECK(USB_OK(UsbNicReadMedia(&u,&up,&speed,&duplex))&&!up&&!speed,"carrier down is successful disconnected state");
        u.Nic.Running=FALSE;regReturn[0]=0xff;regReturn[1]=0xff;
        CHECK(USB_OK(UsbNicReadMedia(&u,&up,&speed,&duplex))&&!up&&!speed,"stopped device does not read bus");
        hwVersion=0x5c10;memset(regReturn,0,sizeof(regReturn));
        CHECK(USB_OK(UsbNicAttach(&u,OS(4,0),UsbHcUhci,0x0bda,0x8150,(PVOID)1,mockSubmit)),"legacy media fixture attached");
        good=1;
        for(i=0;i<4;i++){
            regReturn[0]=(UCHAR)(4|((i&1)?8:0)|((i&2)?16:0));
            if(!USB_OK(UsbNicReadMedia(&u,&up,&speed,&duplex))||!up||speed!=((i&1)?100u:10u)||duplex!=!!(i&2))good=0;
        }
        CHECK(good,"RTL8150 10/100 and duplex states decoded");
        regReturn[0]=0xff;
        CHECK(!USB_OK(UsbNicReadMedia(&u,&up,&speed,&duplex))&&!up&&!speed,"legacy removed-device read fails closed");
    }
    printf("\n%s (%d failures)\n", fails ? "TEST FAILED" : "ALL TESTS PASSED", fails);
    return fails ? 1 : 0;
}
