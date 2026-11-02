/*
 * test_qemu_interop.c - interoperation test between the RLTKLANT USB-NIC
 * driver protocol (the real rtl815x.c front end) and the QEMU usb-rtl8153
 * device model's protocol core (hw/usb/rtl815x-core.h, the very code the
 * emulated device runs).
 *
 * The driver's control/bulk transfers are routed into the device core, so
 * this exercises the complete bring-up and data path the way it would run
 * over USB in QEMU: SET/GET REGS register access, MAC read-back, RX/TX
 * enable, an 8-byte-descriptor TX frame decoded by the device, and a
 * 24-byte-descriptor RX frame encoded by the device and parsed by the
 * driver. This checks internal protocol interoperability; it is not
 * independent silicon or guest networking validation.
 *
 * Copyright (C) 2026 ages2001.
 */
#include <stdio.h>
#include <string.h>
#include "usbnic.h"
#include "usbchip.h"
#include "rtl815x-core.h"        /* the QEMU device model's protocol core */

static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); fails++; } \
                           else printf("ok  : %s\n", msg); } while (0)

/* ---- the emulated device (same core the QEMU usb-rtl8153 runs) -------- */
static R815xCore  g_dev;
static UCHAR      g_rx_wire[R815C_RX_DESC + R815C_MAX_FRAME + R815C_RX_ALIGN];
static ULONG      g_rx_wire_len;         /* device->host bytes staged      */
static UCHAR      g_tx_seen[R815C_MAX_FRAME];
static ULONG      g_tx_seen_len;         /* host->device frame captured    */

/* Bridge the driver's USBDI submit primitive to the device core, exactly
   as a real USB stack would carry the URB to the usb-rtl8153 endpoints. */
static NDIS_STATUS interopSubmit(PVOID devObject, UCHAR kind, UCHAR ep,
                                 PUCHAR setup, PVOID buffer,
                                 ULONG length, PULONG done)
{
    (void)devObject;
    if (done) *done = 0;

    if (kind == 0) {                     /* control: setup + data stage    */
        UCHAR  rt  = setup[0], req = setup[1];
        USHORT val = (USHORT)(setup[2] | (setup[3] << 8));
        if (req != R815C_REQ_REGS) return NDIS_STATUS_FAILURE;
        if (rt == R815C_RT_READ) {
            if (r815c_control_read(&g_dev, val, (uint16_t)(setup[4] | (setup[5]<<8)), buffer, (uint16_t)length)) return NDIS_STATUS_FAILURE;
        } else if (rt == R815C_RT_WRITE) {
            if (r815c_control_write(&g_dev, val,
                (USHORT)(setup[4] | (setup[5] << 8)), buffer, (uint16_t)length))
                return NDIS_STATUS_FAILURE;
        } else return NDIS_STATUS_FAILURE;
        if (done) *done = length;
        return NDIS_STATUS_SUCCESS;
    }

    /* bulk */
    if (ep == RTLU_EP_BULK_OUT) {        /* TX: device decodes the frame   */
        const uint8_t *frame;
        uint32_t flen;
        if (!g_dev.tx_enabled) return NDIS_STATUS_FAILURE;
        flen = r815c_tx_decode((const uint8_t *)buffer, length, &frame);
        if (flen) { memcpy(g_tx_seen, frame, flen); g_tx_seen_len = flen; }
        if (done) *done = length;
        return NDIS_STATUS_SUCCESS;
    }
    if (ep == RTLU_EP_BULK_IN) {         /* RX: device delivers staged buf */
        ULONG n;
        if (!g_dev.rx_enabled) { if (done) *done = 0; return NDIS_STATUS_SUCCESS; }
        n = g_rx_wire_len; if (n > length) n = length;
        memcpy(buffer, g_rx_wire, n);
        g_rx_wire_len = 0;               /* consumed */
        if (done) *done = n;
        return NDIS_STATUS_SUCCESS;
    }
    return NDIS_STATUS_FAILURE;
}

/* RX sink */
static UCHAR g_sink[R815C_MAX_FRAME];
static ULONG g_sink_len;
static VOID rxSink(PVOID ctx, PUCHAR frame, ULONG len)
{
    (void)ctx;
    if (len <= sizeof(g_sink)) { memcpy(g_sink, frame, len); g_sink_len = len; }
}

static USBNT_OSVER OS(unsigned maj, unsigned min)
{
    USBNT_OSVER v; v.Major = (USHORT)maj; v.Minor = (USHORT)min; return v;
}

int main(void)
{
    UCHAR mac[6] = { 0x52, 0x54, 0x00, 0xAA, 0xBB, 0xCC };
    USBNIC u;
    NDIS_STATUS st;
    UCHAR frame[128], rxf[96];
    ULONG i;

    printf("RLTKLANT driver  <->  QEMU usb-rtl8153 device core interop\n");

    CHECK(RTLU_PLA_CR == 0xE813 && R815C_PLA_CR == 0xE813,
          "command register matches the documented r8152 address");

    /* power on the emulated device with a known station address */
    memset(&g_dev, 0, sizeof(g_dev));
    r815c_set_mac(&g_dev, mac);
    r815c_reset(&g_dev);

    /* XP on an EHCI port: in-box stack carries it, driver drives the NIC */
    st = UsbNicAttach(&u, OS(5,1), UsbHcEhci, 0x0BDA, 0x8153,
                      (PVOID)0x1, interopSubmit);
    CHECK(st == NDIS_STATUS_SUCCESS, "driver attaches over emulated RTL8153");
    CHECK(memcmp(u.Nic.Mac, mac, 6) == 0, "driver read station MAC from device");
    CHECK(memcmp(&g_dev.reg[R815C_PLA_IDR], mac, 6) == 0,
          "starting traffic preserves all six station-address bytes");
    CHECK(g_dev.tx_enabled && g_dev.rx_enabled,
          "bring-up enabled device TX+RX (CR written through to device)");
    CHECK((g_dev.reg[R815C_PLA_RCR] & 0x0A) != 0,
          "driver programmed the device receive-config register");
    {
        BOOLEAN carrier = TRUE;
        CHECK(UsbNicReadLink(&u, &carrier) == NDIS_STATUS_SUCCESS && !carrier,
              "attached device with no cable reports link down");
        r815c_set_link(&g_dev, 1);
        CHECK(UsbNicReadLink(&u, &carrier) == NDIS_STATUS_SUCCESS && carrier,
              "driver reads modeled cable connect through vendor request");
        r815c_set_link(&g_dev, 0);
        CHECK(UsbNicReadLink(&u, &carrier) == NDIS_STATUS_SUCCESS && !carrier,
              "driver reads cable removal without USB detachment");
    }

    /* TX: driver frames it with an 8-byte tx descriptor; device decodes */
    for (i = 0; i < 100; i++) frame[i] = (UCHAR)(0x40 + i);
    st = UsbNicSend(&u, frame, 100);
    CHECK(st == NDIS_STATUS_SUCCESS, "driver TX submitted");
    CHECK(g_tx_seen_len == 100 && memcmp(g_tx_seen, frame, 100) == 0,
          "device received the exact 100-byte frame the driver sent");

    /* RX: device encodes a frame with a 24-byte rx descriptor; driver parses */
    for (i = 0; i < 64; i++) rxf[i] = (UCHAR)(0x80 ^ i);
    g_rx_wire_len = r815c_rx_encode(rxf, 64, g_rx_wire);
    g_sink_len = 0;
    st = UsbNicPoll(&u, rxSink, (PVOID)0);
    CHECK(st == NDIS_STATUS_SUCCESS, "driver RX poll ok");
    CHECK(g_sink_len == 64 && memcmp(g_sink, rxf, 64) == 0,
          "driver parsed the exact 64-byte frame the device delivered");

    /* Revision reset must work through the actual banked model transport. */
    {
        const USHORT signatures[] = {0x7020,0x7030,0x1030,0x2020};
        const USHORT products[] = {0x8156,0x8156,0x8157,0x8159};
        for(i=0;i<4;i++) {
            g_dev.version=signatures[i];r815c_reset(&g_dev);
            st=UsbNicAttach(&u,OS(4,0),UsbHcEhci,0x0bda,products[i],
                            (PVOID)1,interopSubmit);
            CHECK(st==NDIS_STATUS_SUCCESS && g_dev.rx_enabled && g_dev.tx_enabled,
                  "revision reset attaches through separate USB/PLA banks");
            if(i<2) {
                CHECK(g_dev.usb_reg[0xd4b0]==1 && g_dev.usb_reg[0xd406]==0 &&
                      g_dev.reg[0xd4b0]==0,
                      "8156 BMU/ECM reset touches USB bank only");
            }
            UsbNicDetach(&u);
        }
    }

    printf("\n%s (%d failures)\n",
           fails ? "INTEROP TEST FAILED" : "QEMU INTEROP TESTS PASSED", fails);
    return fails ? 1 : 0;
}
