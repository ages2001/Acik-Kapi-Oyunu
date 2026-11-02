/*
 * test_usbntdev.c - the RLTKLANT USBNT-client path end to end: a mock
 * USBNT_DEV_IF (the \Device\UsbNtDev interface) backed by the QEMU
 * usb-rtl8153 device core drives the real RLTKLANT USB stack
 * (usbcli -> usbbind -> usbnic -> rtl815x).  Proves that a Realtek USB NIC
 * reached through USBNT's generic device interface brings up and moves
 * frames byte-for-byte, the way it will on NT 3.1-4.0 over usbnt.sys.
 *
 * Copyright (C) 2026 ages2001.
 */
#include <stdio.h>
#include <string.h>
#include "usbnic.h"
#include "usbchip.h"
#include "usbntcli.h"
#include "rtl815x-core.h"

extern NDIS_STATUS RtlUsbNtSubmit(PVOID devObject, UCHAR kind, UCHAR ep,
                                  PUCHAR setup, PVOID buffer, ULONG length,
                                  PULONG done);

static int fails = 0;
#define CHECK(c, m) do { if(!(c)){printf("FAIL: %s\n",m);fails++;} else printf("ok  : %s\n",m);} while(0)

/* the emulated device (same core the QEMU usb-rtl8153 runs) */
static R815xCore g_dev;
static UCHAR g_rx[R815C_RX_DESC + R815C_MAX_FRAME + R815C_RX_ALIGN];
static ULONG g_rxlen;
static UCHAR g_tx[R815C_MAX_FRAME];
static ULONG g_txlen;

/* ---- mock USBNT_DEV_IF: what usbnt.sys's \Device\UsbNtDev provides ---- */
static int UC_CALL mkRegister(const UC_CLIENT *c){ (void)c; return UC_OK; }
static void UC_CALL mkUnregister(const UC_CLIENT *c){ (void)c; }
static int UC_CALL mkGetInfo(ULONG id, UC_DEVINFO *o){ (void)id; memset(o,0,sizeof(*o)); o->Id=id; o->Vid=0x0BDA; o->Pid=0x8153; return UC_OK; }
static int UC_CALL mkGetDesc(ULONG id,UCHAR t,UCHAR i,void*b,ULONG l,ULONG*a){ (void)id;(void)t;(void)i;(void)b;(void)l; if(a)*a=0; return UC_OK; }
static int UC_CALL mkControl(ULONG id, UCHAR rt, UCHAR req, USHORT val, USHORT idx,
                             void *buf, USHORT len, ULONG to, ULONG *act)
{
    (void)id; (void)idx; (void)to;
    if (req != R815C_REQ_REGS) return UC_ERR_IO;
    if (rt == R815C_RT_READ) { if (r815c_control_read(&g_dev, val, idx, buf, len)) return UC_ERR_IO; }
    else                      if (r815c_control_write(&g_dev, val, idx, buf, len)) return UC_ERR_IO;
    if (act) *act = len;
    return UC_OK;
}
static int UC_CALL mkTransfer(ULONG id, UCHAR ep, void *buf, ULONG len, ULONG to, ULONG *act)
{
    (void)id; (void)to;
    if (ep == RTLU_EP_BULK_OUT) {
        const uint8_t *f; uint32_t fl;
        if (!g_dev.tx_enabled) return UC_ERR_IO;
        fl = r815c_tx_decode((const uint8_t*)buf, len, &f);
        if (fl){ memcpy(g_tx, f, fl); g_txlen = fl; }
        if (act) *act = len;
        return UC_OK;
    }
    if (ep == RTLU_EP_BULK_IN) {
        ULONG n;
        if (!g_dev.rx_enabled){ if(act)*act=0; return UC_OK; }
        n = g_rxlen; if (n > len) n = len;
        memcpy(buf, g_rx, n); g_rxlen = 0;
        if (act) *act = n;
        return UC_OK;
    }
    return UC_ERR_IO;
}
static int UC_CALL mkClaim(ULONG id, int c){ (void)id;(void)c; return UC_OK; }

static const USBNT_DEV_IF g_If = {
    USBNT_DEV_IF_VERSION, sizeof(USBNT_DEV_IF),
    mkRegister, mkUnregister, mkGetInfo, mkGetDesc, mkControl, mkTransfer, mkClaim
};

/* the usbbind target usbcli.c expects: { const USBNT_DEV_IF *If; ULONG Id; } */
typedef struct { const USBNT_DEV_IF *If; ULONG Id; } Target;

static UCHAR g_sink[R815C_MAX_FRAME]; static ULONG g_sinklen;
static VOID sink(PVOID c, PUCHAR f, ULONG n){ (void)c; if(n<=sizeof g_sink){memcpy(g_sink,f,n);g_sinklen=n;} }
static USBNT_OSVER OS(unsigned a,unsigned b){ USBNT_OSVER v; v.Major=(USHORT)a; v.Minor=(USHORT)b; return v; }

int main(void)
{
    UCHAR mac[6] = { 0x52,0x54,0x00,0xBE,0xEF,0x01 };
    Target tgt; USBNIC u; NDIS_STATUS st; UCHAR frame[128]; ULONG i;

    printf("RLTKLANT  <->  USBNT \\Device\\UsbNtDev  <->  usb-rtl8153 core\n\n");
    memset(&g_dev,0,sizeof g_dev); r815c_set_mac(&g_dev, mac); r815c_reset(&g_dev);
    tgt.If = &g_If; tgt.Id = 1;

    st = UsbNicAttach(&u, OS(4,0), UsbHcEhci, 0x0BDA, 0x8153, &tgt, RtlUsbNtSubmit);
    CHECK(st == NDIS_STATUS_SUCCESS, "attach over USBNT generic device interface");
    CHECK(memcmp(u.Nic.Mac, mac, 6) == 0, "MAC read through USBNT_DEV_IF.Control");
    CHECK(g_dev.tx_enabled && g_dev.rx_enabled, "bring-up reached the device via the interface");

    for (i=0;i<100;i++) frame[i]=(UCHAR)(0x30+i);
    st = UsbNicSend(&u, frame, 100);
    CHECK(st==NDIS_STATUS_SUCCESS && g_txlen==100 && memcmp(g_tx,frame,100)==0,
          "TX frame delivered to the device through USBNT_DEV_IF.Transfer");

    for (i=0;i<64;i++) frame[i]=(UCHAR)(0xC0 ^ i);
    g_rxlen = r815c_rx_encode(frame, 64, g_rx); g_sinklen=0;
    st = UsbNicPoll(&u, sink, (PVOID)0);
    CHECK(st==NDIS_STATUS_SUCCESS && g_sinklen==64 && memcmp(g_sink,frame,64)==0,
          "RX frame received from the device through USBNT_DEV_IF.Transfer");

    printf("\n%s (%d failures)\n", fails?"USBNT-DEV PATH FAILED":"USBNT-DEV PATH TESTS PASSED", fails);
    return fails?1:0;
}
