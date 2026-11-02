/*
 * test_qemu_multi.c - multiple *different* Realtek USB NICs at once, driven
 * by the real RLTKLANT USB front end against the QEMU usb-rtl8153 device
 * core (one emulated device instance per adapter, each with its own MAC).
 *
 * It shows the NT 3.1 picture the user asked for - several distinct USB
 * Ethernet adapters (RTL8152 100M, RTL8153 1G, RTL8156 2.5G) all attached
 * at the same time, each bringing up on its own device and reading its own
 * station address - and then re-runs the attach across every other OS so
 * the per-OS routing (USBNT vs in-box) is exercised too.  The MACs it
 * prints are exactly what DHCPNT31 is given, one per adapter, to obtain a
 * separate DHCP lease for each USB NIC on NT 3.1.
 *
 * Copyright (C) 2026 ages2001.
 */
#include <stdio.h>
#include <string.h>
#include "usbnic.h"
#include "usbchip.h"
#include "rtl815x-core.h"

static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); fails++; } } while (0)

/* one emulated device (the QEMU device core) per adapter */
typedef struct {
    R815xCore core;
    UCHAR     rxwire[R815C_RX_DESC + R815C_MAX_FRAME + R815C_RX_ALIGN];
    ULONG     rxlen;
    UCHAR     txseen[R815C_MAX_FRAME];
    ULONG     txlen;
} DevSim;

static NDIS_STATUS submit(PVOID devObject, UCHAR kind, UCHAR ep, PUCHAR setup,
                          PVOID buffer, ULONG length, PULONG done)
{
    DevSim *d = (DevSim *)devObject;
    if (done) *done = 0;
    if (kind == 0) {
        UCHAR rt = setup[0], req = setup[1];
        USHORT val = (USHORT)(setup[2] | (setup[3] << 8));
        if (req != R815C_REQ_REGS) return NDIS_STATUS_FAILURE;
        if (rt == R815C_RT_READ) { if (r815c_control_read(&d->core, val, (uint16_t)(setup[4] | (setup[5]<<8)), buffer, (uint16_t)length)) return NDIS_STATUS_FAILURE; }
        else                      if (r815c_control_write(&d->core, val,
                (USHORT)(setup[4] | (setup[5] << 8)), buffer, (uint16_t)length))
                return NDIS_STATUS_FAILURE;
        if (done) *done = length;
        return NDIS_STATUS_SUCCESS;
    }
    if (ep == RTLU_EP_BULK_OUT) {
        const uint8_t *f; uint32_t fl;
        if (!d->core.tx_enabled) return NDIS_STATUS_FAILURE;
        fl = r815c_tx_decode(buffer, length, &f);
        if (fl) { memcpy(d->txseen, f, fl); d->txlen = fl; }
        if (done) *done = length;
        return NDIS_STATUS_SUCCESS;
    }
    if (ep == RTLU_EP_BULK_IN) {
        ULONG n;
        if (!d->core.rx_enabled) { if (done) *done = 0; return NDIS_STATUS_SUCCESS; }
        n = d->rxlen; if (n > length) n = length;
        memcpy(buffer, d->rxwire, n); d->rxlen = 0;
        if (done) *done = n;
        return NDIS_STATUS_SUCCESS;
    }
    return NDIS_STATUS_FAILURE;
}

static UCHAR g_sink[R815C_MAX_FRAME]; static ULONG g_sinklen;
static VOID sink(PVOID ctx, PUCHAR f, ULONG n){ (void)ctx; if(n<=sizeof g_sink){memcpy(g_sink,f,n);g_sinklen=n;} }
static USBNT_OSVER OS(unsigned a,unsigned b){ USBNT_OSVER v; v.Major=(USHORT)a; v.Minor=(USHORT)b; return v; }

/* three different Realtek USB parts, each with its own station address */
static USHORT   g_id [3] = { 0x8152, 0x8153, 0x8156 };
static UCHAR    g_mac[3][6] = {
    { 0x52,0x54,0x00,0x11,0x11,0x01 },
    { 0x52,0x54,0x00,0x11,0x11,0x02 },
    { 0x52,0x54,0x00,0x11,0x11,0x03 },
};

int main(void)
{
    DevSim dev[3];
    USBNIC u[3];
    int i;
    const char *osn[] = {"NT 3.1","NT 3.50","NT 3.51","NT 4.0",
                         "2000 EHCI","2000 xHCI","XP EHCI","XP x64 xHCI"};
    USBNT_OSVER osv[] = {OS(3,1),OS(3,50),OS(3,51),OS(4,0),OS(5,0),OS(5,0),OS(5,1),OS(5,2)};
    USBNT_HC_KIND hcv[] = {UsbHcEhci,UsbHcEhci,UsbHcEhci,UsbHcEhci,
                           UsbHcEhci,UsbHcXhci,UsbHcEhci,UsbHcXhci};

    printf("== NT 3.1: three different USB Ethernet adapters at once ==\n\n");
    for (i = 0; i < 3; i++) {
        NDIS_STATUS st;
        UCHAR frame[80]; ULONG k;
        memset(&dev[i], 0, sizeof(DevSim));
        r815c_set_mac(&dev[i].core, g_mac[i]);
        r815c_reset(&dev[i].core);

        st = UsbNicAttach(&u[i], OS(3,1), UsbHcEhci, 0x0BDA, g_id[i],
                          &dev[i], submit);
        CHECK(st == NDIS_STATUS_SUCCESS, "attach");
        CHECK(u[i].Route == UsbRouteUsbnt, "NT 3.1 routes USB NIC via USBNT");
        CHECK(memcmp(u[i].Nic.Mac, g_mac[i], 6) == 0, "own MAC read back");

        for (k = 0; k < 64; k++) frame[k] = (UCHAR)(i*0x10 + k);
        st = UsbNicSend(&u[i], frame, 64);
        CHECK(st == NDIS_STATUS_SUCCESS && dev[i].txlen == 64 &&
              memcmp(dev[i].txseen, frame, 64) == 0, "independent TX");

        dev[i].rxlen = r815c_rx_encode(frame, 48, dev[i].rxwire);
        g_sinklen = 0;
        UsbNicPoll(&u[i], sink, 0);
        CHECK(g_sinklen == 48, "independent RX");

        printf("  adapter %d: %-42s route=USBNT  MAC=%02X:%02X:%02X:%02X:%02X:%02X  %u Mbit\n",
               i+1, u[i].Nic.Chip->Name,
               u[i].Nic.Mac[0],u[i].Nic.Mac[1],u[i].Nic.Mac[2],
               u[i].Nic.Mac[3],u[i].Nic.Mac[4],u[i].Nic.Mac[5],
               (unsigned)u[i].Nic.Chip->MaxSpeed);
    }

    printf("\n  All three attach simultaneously with distinct MACs ->\n");
    printf("  DHCPNT31 gets one MAC per adapter:\n");
    printf("    DHCPNT31 %02X-%02X-%02X-%02X-%02X-%02X %02X-%02X-%02X-%02X-%02X-%02X %02X-%02X-%02X-%02X-%02X-%02X /apply\n",
           g_mac[0][0],g_mac[0][1],g_mac[0][2],g_mac[0][3],g_mac[0][4],g_mac[0][5],
           g_mac[1][0],g_mac[1][1],g_mac[1][2],g_mac[1][3],g_mac[1][4],g_mac[1][5],
           g_mac[2][0],g_mac[2][1],g_mac[2][2],g_mac[2][3],g_mac[2][4],g_mac[2][5]);

    printf("\n== same multi-adapter bring-up on every other OS (routing) ==\n\n");
    for (i = 0; i < (int)(sizeof osn / sizeof osn[0]); i++) {
        int j, ok = 1; USBNIC_ROUTE r = UsbRouteNone;
        for (j = 0; j < 3; j++) {
            DevSim d; USBNIC v; NDIS_STATUS st;
            memset(&d, 0, sizeof d); r815c_set_mac(&d.core, g_mac[j]); r815c_reset(&d.core);
            st = UsbNicAttach(&v, osv[i], hcv[i], 0x0BDA, g_id[j], &d, submit);
            if (st != NDIS_STATUS_SUCCESS || memcmp(v.Nic.Mac, g_mac[j], 6)) ok = 0;
            r = v.Route;
        }
        CHECK(ok, "all three attach");
        printf("  %-12s -> 3/3 adapters up, route=%s\n", osn[i],
               r == UsbRouteUsbnt ? "USBNT" : "in-box USB");
    }

    printf("\n%s (%d failures)\n",
           fails ? "MULTI-ADAPTER TEST FAILED" : "MULTI-ADAPTER USB TESTS PASSED", fails);
    return fails ? 1 : 0;
}
