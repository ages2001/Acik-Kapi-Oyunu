/*
 * Thunderbolt / USB4 host interface (NHI) with routers behind it - test
 * model for the USBNT tbt.c driver.
 *
 *   -device usb4-nhi,id=tb[,mode=usb4|legacy|fr|ar|tr|icl|tgl][,security=0..2]
 *        [,device-id=0x....][,caps-version=0x20][,v2=on][,apple=on|off]
 *
 * Ring 0 (control channel, raw mode) is emulated with descriptor and CRC-32C
 * checks.  mode=usb4: a USB4 host router (route 0) with two USB4 ports
 * (lanes 1+2, 3+4), the NHI adapter, two PCIe and two USB 3 downstream
 * adapters; the driver must act as connection manager.  Other modes: Intel
 * firmware connection manager (ICM) with the message formats of Falcon /
 * Alpine Ridge (fr, ar), Titan Ridge (tr) or Ice Lake and later (icl: force
 * power through PCI config 0xFC first; tgl: firmware already running).
 * v2=on (mode=usb4): USB4 v2 / Thunderbolt 5 host router (Barlow Ridge id,
 * caps version 0x40): a link to a USB4 v2 device router comes up at Gen 4,
 * bonded by the hardware (2 x 40 Gb/s); the CM must program CM version 2 and
 * PCIe extended encapsulation on both PCIe adapters.  mode=legacy: Thunderbolt
 * 1 / 2 / 3 host router without firmware connection manager (device-id: 0x1547
 * Cactus Ridge (default, Thunderbolt 1), 0x156C Falcon Ridge, 0x15EB Titan
 * Ridge); apple=on (default) gives it Apple's subsystem ids 2222:1111, the
 * driver must be the connection manager then (plug events of the host router,
 * link controller from Thunderbolt 2 on); apple=off: a PC, the BIOS runs it
 * and the driver must keep its hands off (any packet is a violation).
 *
 * Run time properties (qom-set / qom-get):
 *   plug0, plug1     device router 0 at host port 1; router 1 behind its port 3
 *   kind0, kind1     "usb4", "usb4v2", "tb3" or "tb1" (before plugging)
 *   devkey0          "clear" (device forgets its key), "bad" (other key),
 *                    "hex:<64 digits>" (key from an earlier pairing)
 *   pcie0/1, usb3-0/1  tunnel to router 0 / 1 up (read only)
 *   approved0        firmware connected router 0 (read only)
 *   host-clean       no tunnel of router 0 left in the host router
 *   gbps0, gbps1     link speed to router 0 / 1 (20 one lane, 40 bonded, 80 Gen 4)
 *   ee0              PCIe tunnel 0 with extended encapsulation (read only)
 *   violations, vlog protocol errors seen (count, last messages)
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 */

#include "qemu/osdep.h"
#include "qemu/module.h"
#include "qemu/timer.h"
#include "qapi/error.h"
#include "qapi/visitor.h"
#include "hw/qdev-properties.h"
#include "hw/pci/pci_device.h"
#include "sysemu/dma.h"
#include "qom/object.h"

#define TYPE_USB4_NHI "usb4-nhi"
OBJECT_DECLARE_SIMPLE_TYPE(Usb4NhiState, USB4_NHI)

#define NHI_LEN         0x40000
#define REG_CAPS        0x39640
#define REG_RESET       0x39898
#define REG_INMAIL_DATA 0x39900
#define REG_INMAIL_CMD  0x39904
#define REG_OUTMAIL_CMD 0x3990c
#define REG_FW_STS      0x39944
#define HOPS            12

#define RING_ENABLE     0x80000000u
#define RING_RAW        0x40000000u
#define D_COMPLETED     (0x2u << 20)
#define D_POSTED        (0x4u << 20)

enum { P_READ = 1, P_WRITE, P_ERROR, P_NOTIFY_ACK, P_EVENT, P_XREQ, P_XRESP, P_OVERRIDE, P_RESET,
       P_ICM_EVENT, P_ICM_CMD, P_ICM_RESP };

#define T_LANE          0x000001u
#define T_NHI           0x000002u
#define T_PCIE_DOWN     0x100101u
#define T_PCIE_UP       0x100102u
#define T_USB3_DOWN     0x200101u
#define T_USB3_UP       0x200102u

#define NR              3               /* host, router 0, router 1 */
#define NP              10
#define CAP_PHY_OFF     0x10
#define CAP_USB4_OFF    0x20
#define CAP_ADAP_OFF    0x10
#define SW_PE_VSE       0x30            /* Thunderbolt 3 plug events capability */
#define SW_LC_VSE       0x40            /* Thunderbolt 3 link controller capability */

enum { M_USB4, M_FR, M_AR, M_TR, M_ICL, M_TGL, M_LEGACY };

typedef struct Router {
    bool present;
    bool usb4;
    int ver;                            /* USB4 version (0: Thunderbolt) */
    int gen;                            /* Thunderbolt generation of a version 0 router (1 .. 3) */
    int depth, up, maxport;
    uint32_t route;                     /* where it is */
    bool enumerated;
    uint32_t sw[512];
    uint32_t port[NP][64];
    uint32_t hop[NP][128];
    int64_t rr_at, cr_at;
} Router;

typedef struct Pending {
    uint8_t type;
    uint8_t n;
    uint32_t d[63];
} Pending;

typedef struct PlugAck {
    bool used;
    uint32_t route;
    int port;
    bool unplug;
    int64_t sent;
    int tries;
} PlugAck;

typedef struct IcmDev {
    uint8_t uuid[16];
    bool has_key;
    uint8_t key[32];
    bool reported, approved, keyok;
    uint8_t conn_key, conn_id;
} IcmDev;

struct Usb4NhiState {
    PCIDevice parent_obj;
    MemoryRegion mmio;
    QEMUTimer *tick;
    char *mode_str;
    int mode;
    uint8_t security;
    uint16_t device_id;
    uint32_t caps_version;
    bool v2, apple;
    uint32_t ring[HOPS * 2][4];         /* tx 0..11, rx 12..23 */
    uint32_t opt[HOPS * 2][2];
    uint16_t tx_cons, rx_prod;          /* hardware indexes */
    uint32_t fw_sts, inmail_data, inmail_cmd, outmail_cmd, reset;
    Pending q[64];
    int qh, qn;
    Router r[NR];
    char *kind[2];
    bool plug[2];
    int64_t up_at[2];                   /* link training done */
    int64_t bond_at[2];                 /* lane bonding done */
    PlugAck acks[8];
    /* firmware */
    bool icm_ready, powered;
    int64_t power_at;
    IcmDev dev[2];
    uint32_t violations;
    char vlog[2048];
    uint32_t frames;
};

/* ------------------------------------------------------------------ */

static void violation(Usb4NhiState *s, const char *fmt, ...) G_GNUC_PRINTF(2, 3);

static void violation(Usb4NhiState *s, const char *fmt, ...)
{
    char msg[256];
    va_list ap;
    size_t l;

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    s->violations++;
    fprintf(stderr, "usb4-nhi: VIOLATION %s\n", msg);
    l = strlen(s->vlog);
    if (l + strlen(msg) + 2 >= sizeof(s->vlog)) {
        s->vlog[0] = 0;
        l = 0;
    }
    snprintf(s->vlog + l, sizeof(s->vlog) - l, "%s; ", msg);
}

static uint32_t crc32c_calc(const uint8_t *p, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    int k;

    while (n--) {
        c ^= *p++;
        for (k = 0; k < 8; k++) {
            c = (c >> 1) ^ (0x82F63B78u & (0u - (c & 1)));
        }
    }
    return ~c;
}

static int64_t now_ms(void)
{
    return qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL);
}

static void kick(Usb4NhiState *s)
{
    if (s->tick != NULL && !timer_pending(s->tick)) {
        timer_mod(s->tick, now_ms() + 1);
    }
}

/* ------------------------------------------------------------------ */
/* receive ring: frames to the driver                                   */
/* ------------------------------------------------------------------ */

static void rx_flush(Usb4NhiState *s)
{
    PCIDevice *pd = PCI_DEVICE(s);
    uint32_t *rg = s->ring[HOPS];
    uint32_t size = rg[3] & 0xFFFF, fsize = rg[3] >> 16;

    if (!(s->opt[HOPS][0] & RING_ENABLE) || size == 0) {
        return;
    }
    while (s->qn > 0) {
        uint16_t head = rg[2] & 0xFFFF;         /* first descriptor not given to us */
        Pending *p = &s->q[s->qh];
        uint64_t base = ((uint64_t)rg[1] << 32) | rg[0];
        uint64_t da = base + (uint64_t)s->rx_prod * 16;
        uint8_t desc[16], buf[256];
        uint32_t ctl, len = p->n * 4 + 4;
        uint64_t phys;
        int i;

        if (s->rx_prod == head) {
            return;                             /* no buffer posted */
        }
        pci_dma_read(pd, da, desc, 16);
        phys = ldq_le_p(desc);
        ctl = ldl_le_p(desc + 8);
        if (!(ctl & D_POSTED)) {
            violation(s, "rx descriptor %u given to the hardware without POSTED", s->rx_prod);
        }
        if (fsize != 0 && fsize < len) {
            violation(s, "rx frame size %u too small", fsize);
        }
        for (i = 0; i < p->n; i++) {
            stl_be_p(buf + 4 * i, p->d[i]);
        }
        stl_be_p(buf + 4 * p->n, crc32c_calc(buf, p->n * 4));
        pci_dma_write(pd, phys, buf, len);
        ctl = len | ((uint32_t)p->type << 12) | ((uint32_t)p->type << 16) | D_COMPLETED | D_POSTED;
        stl_le_p(desc + 8, ctl);
        pci_dma_write(pd, da + 8, desc + 8, 4);
        s->rx_prod = (s->rx_prod + 1) % size;
        s->qh = (s->qh + 1) % 64;
        s->qn--;
    }
}

static void rx_push(Usb4NhiState *s, uint8_t type, const uint32_t *d, int n)
{
    Pending *p;

    if (s->qn >= 64) {
        violation(s, "driver does not take control frames (receive queue full)");
        return;
    }
    p = &s->q[(s->qh + s->qn) % 64];
    p->type = type;
    p->n = n;
    memcpy(p->d, d, n * 4);
    s->qn++;
    rx_flush(s);
}

/* ------------------------------------------------------------------ */
/* routers                                                              */
/* ------------------------------------------------------------------ */

/* lane adapter LANE_ADP_CS_1: state 29:26, current width 25:20, current speed 19:16, LB 15, target width 5:4 */
#define LANE_DEFAULT    ((0x4u << 16) | (1u << 20) | (1u << 4))

static void lane_state(Router *r, int p, uint32_t st)
{
    uint32_t *v = &r->port[p][CAP_PHY_OFF + 1];

    if (st != 2) {
        *v = LANE_DEFAULT | (st << 26);         /* link down: single lane again, bonding request gone */
    } else {
        *v = (*v & ~0x3C000000u) | (st << 26);
    }
}

enum { K_USB4, K_V2, K_TB3, K_TB2, K_TB1 };

static bool swcm(Usb4NhiState *s)
{
    return s->mode == M_USB4 || s->mode == M_LEGACY;
}

static int kind_of(const char *k)
{
    if (k == NULL || !strcmp(k, "usb4")) {
        return K_USB4;
    }
    if (!strcmp(k, "usb4v2")) {
        return K_V2;
    }
    if (!strcmp(k, "tb1")) {
        return K_TB1;
    }
    return K_TB3;
}

static int host_kind(Usb4NhiState *s)
{
    switch (s->mode) {
    case M_USB4:
        return s->v2 ? K_V2 : K_USB4;
    case M_ICL:
    case M_TGL:
        return K_USB4;
    case M_LEGACY:
        if (s->device_id == 0x1513 || s->device_id == 0x1547 || s->device_id == 0x1548) {
            return K_TB1;
        }
        if (s->device_id == 0x156A || s->device_id == 0x156C) {
            return K_TB2;
        }
        return K_TB3;
    default:
        return K_TB3;
    }
}

/* Gen 4: both routers USB4 v2, the two lanes come up as one 80 Gb/s link */
static void gen4_link(Usb4NhiState *s, int c)
{
    Router *pr = &s->r[c - 1], *r = &s->r[c];
    int l0 = (c == 1) ? 1 : 3, k;

    if (pr->ver < 2 || r->ver < 2) {
        return;
    }
    for (k = 0; k < 2; k++) {
        uint32_t *a = &pr->port[l0 + k][CAP_PHY_OFF + 1], *b = &r->port[r->up + k][CAP_PHY_OFF + 1];
        *a = (*a & ~((0x3Fu << 20) | (0xFu << 16))) | (2u << 20) | (0x2u << 16);
        *b = (*b & ~((0x3Fu << 20) | (0xFu << 16))) | (2u << 20) | (0x2u << 16);
    }
}

static bool gen4(Router *r, int lane)
{
    return ((r->port[lane][CAP_PHY_OFF + 1] >> 16) & 0xF) == 0x2;
}

static void port_init(Router *r, int p, uint32_t type, bool usb4, int pnum)
{
    uint32_t *pt = r->port[p];

    memset(pt, 0, sizeof(r->port[p]));
    pt[0] = 0x8086 | (0x1234u << 16);
    pt[2] = type | (usb4 ? 0x20u << 24 : 0);
    pt[3] = (uint32_t)pnum << 20;
    pt[5] = 63 | (63u << 11);
    if (type == T_LANE) {
        pt[1] = CAP_PHY_OFF;
        /* supported: single and dual width, Gen 2 and Gen 3 */
        pt[CAP_PHY_OFF] = 0x01u << 8 | (usb4 ? CAP_USB4_OFF : 0) | (3u << 20) | ((r->ver >= 2 ? 0xEu : 0xCu) << 16);
        pt[CAP_PHY_OFF + 1] = LANE_DEFAULT | (7u << 26);       /* unplugged */
        if (usb4) {
            pt[CAP_USB4_OFF] = 0x06u << 8;
            pt[5] |= 0x80000000u;                       /* hot plug disabled */
        }
    } else if (type == T_PCIE_DOWN || type == T_PCIE_UP || type == T_USB3_DOWN || type == T_USB3_UP) {
        pt[1] = CAP_ADAP_OFF;
        pt[CAP_ADAP_OFF] = 0x04u << 8;
        if (type == T_USB3_DOWN || type == T_USB3_UP) {
            pt[CAP_ADAP_OFF + 4] = 1u << 12;            /* 20 Gb/s */
        }
    }
}

static void router_init(Usb4NhiState *s, int i, int kind)
{
    Router *r = &s->r[i];
    bool usb4 = kind == K_USB4 || kind == K_V2;
    int p;

    memset(r, 0, sizeof(*r));
    r->usb4 = usb4;
    r->ver = kind == K_V2 ? 2 : usb4 ? 1 : 0;
    r->gen = kind == K_TB1 ? 1 : kind == K_TB2 ? 2 : 3;
    if (i == 0) {
        r->maxport = 9;
        r->depth = 0;
        r->up = 0;
        r->route = 0;
        port_init(r, 0, 0, usb4, 0);
        for (p = 1; p <= 4; p++) {
            port_init(r, p, T_LANE, usb4, p);
        }
        port_init(r, 5, T_NHI, usb4, 5);
        port_init(r, 6, T_PCIE_DOWN, usb4, 6);
        port_init(r, 7, T_PCIE_DOWN, usb4, 7);
        port_init(r, 8, usb4 ? T_USB3_DOWN : 0, usb4, 8);
        port_init(r, 9, usb4 ? T_USB3_DOWN : 0, usb4, 9);
        r->sw[0] = PCI_DEVICE(s)->config[0] | (PCI_DEVICE(s)->config[1] << 8) |
                   ((uint32_t)s->device_id << 16);
        r->present = true;
    } else {
        r->maxport = 8;
        r->depth = i;
        r->up = 1;
        r->route = (i == 1) ? 0x01 : 0x0301;
        port_init(r, 0, 0, usb4, 0);
        for (p = 1; p <= 4; p++) {
            port_init(r, p, T_LANE, usb4, p);
        }
        port_init(r, 5, T_PCIE_UP, usb4, 5);
        port_init(r, 6, T_PCIE_DOWN, usb4, 6);
        if (usb4) {
            port_init(r, 7, T_USB3_UP, usb4, 7);
            port_init(r, 8, T_USB3_DOWN, usb4, 8);
        } else {
            port_init(r, 7, 0, usb4, 7);
            port_init(r, 8, 0, usb4, 8);
        }
        /* upstream lanes are up */
        lane_state(r, 1, 2);
        lane_state(r, 2, 2);
        if (usb4) {
            r->port[1][CAP_USB4_OFF + 0x12] |= 0x100u;          /* PORT_CS_18.BE: lane bonding possible */
        }
        r->port[1][5] &= ~0x80000000u;
        r->port[2][5] &= ~0x80000000u;
        r->sw[0] = 0x8086 | ((kind == K_V2 ? 0x5786u : usb4 ? 0x0B26u : kind == K_TB1 ? 0x1549u : 0x15EFu) << 16);
        /* downstream lanes of a USB4 router are locked until the CM unlocks them */
        if (usb4) {
            r->port[3][4] |= 0x80000000u;
            r->port[4][4] |= 0x80000000u;
        }
        r->sw[6] = 0x00040000u;                         /* HCI: internal xHCI */
    }
    r->sw[1] = (usb4 ? 0 : SW_PE_VSE) | ((uint32_t)r->maxport << 14) | (1u << 24);
    r->sw[4] = r->ver == 2 ? 0x40u << 24 : usb4 ? 0x20u << 24 : 0x02u << 24;
    if (!usb4) {
        r->sw[6] = 0;
        /* plug events (short header) and link controller (long header; not on Thunderbolt 1) */
        r->sw[SW_PE_VSE] = (r->gen >= 2 ? SW_LC_VSE : 0) | (0x05u << 8) | (0x01u << 16) | (2u << 24);
        r->sw[SW_PE_VSE + 1] = 0x7C;
    }
    if (!usb4 && r->gen >= 2) {
        r->sw[SW_LC_VSE] = (0x05u << 8) | (0x06u << 16);
        r->sw[SW_LC_VSE + 1] = 0 | (0x100u << 16);     /* long header: next 0, length */
        /* descriptor: 2 link controllers, start 0x10, 0x20 dwords each */
        r->sw[SW_LC_VSE + 2] = 2 | (0x10u << 8) | (0x20u << 16);
        r->sw[SW_LC_VSE + 0x10 + 0x8D] = 1u << 12;     /* port attributes of LC 0: lane bonding possible */
    }
}

/* Thunderbolt 3 link controller register of a lane: lc + start + n * size + 0x96 */
static int tb3_lc_ctrl(Router *r, int lane)
{
    return SW_LC_VSE + 0x10 + ((lane - 1) / 2) * 0x20 + 0x96;
}

static bool hop_ok(Usb4NhiState *s, Router *r, int in, int inhop, int out, int next, bool lane_in,
                   const char *what)
{
    uint32_t d0 = r->hop[in][inhop * 2], d1 = r->hop[in][inhop * 2 + 1];

    if (!(d0 & 0x80000000u)) {
        return false;
    }
    if ((int)((d0 >> 11) & 0x3F) != out || (int)(d0 & 0x7FF) != next) {
        violation(s, "%s: hop %u of adapter %u goes to adapter %u hop %u, not %u hop %u", what, inhop, in,
                  (d0 >> 11) & 0x3F, d0 & 0x7FF, out, next);
        return false;
    }
    if (lane_in && ((d0 >> 17) & 0x7F) == 0) {
        violation(s, "%s: hop %u of lane %u without initial credits", what, inhop, in);
        return false;
    }
    if (lane_in && ((r->port[in][CAP_PHY_OFF + 1] >> 20) & 0x3F) == 2 && ((d0 >> 17) & 0x7F) < 32) {
        violation(s, "%s: hop %u of bonded lane %u with %u credits (32 expected)", what, inhop, in, (d0 >> 17) & 0x7F);
        return false;
    }
    if (((d1 >> 8) & 7) == 0) {
        violation(s, "%s: hop %u of adapter %u with priority 0", what, inhop, in);
    }
    return true;
}

/* tunnel between the parent (pr, adapter pd, lane pl) and child (r, adapter cu, lane cl) */
static bool tunnel_ok(Usb4NhiState *s, Router *pr, int pd, int pl, Router *r, int cu, int cl, int hopid,
                      const char *what, bool report)
{
    uint32_t d0;
    int ld, lu;

    d0 = pr->hop[pd][hopid * 2];
    if (!(d0 & 0x80000000u)) {
        if (report) {
            violation(s, "%s: no path at the downstream adapter %u hop %u", what, pd, hopid);
        }
        return false;
    }
    ld = d0 & 0x7FF;
    d0 = r->hop[cu][hopid * 2];
    if (!(d0 & 0x80000000u)) {
        if (report) {
            violation(s, "%s: no path at the upstream adapter %u hop %u", what, cu, hopid);
        }
        return false;
    }
    lu = d0 & 0x7FF;
    if (ld < 8 || lu < 8) {
        if (report) {
            violation(s, "%s: link HopID below 8 (%d / %d)", what, ld, lu);
        }
        return false;
    }
    if (!hop_ok(s, pr, pd, hopid, pl, ld, false, what) || !hop_ok(s, r, cl, ld, cu, hopid, true, what) ||
        !hop_ok(s, r, cu, hopid, cl, lu, false, what) || !hop_ok(s, pr, pl, lu, pd, hopid, true, what)) {
        if (report) {
            violation(s, "%s: paths incomplete", what);
        }
        return false;
    }
    return true;
}

static bool configured(Usb4NhiState *s, Router *r, const char *what, bool report)
{
    if (!r->enumerated) {
        if (report) {
            violation(s, "%s: router not enumerated", what);
        }
        return false;
    }
    if (r->usb4) {
        if (!(r->sw[6] & (1u << 25))) {
            if (report) {
                violation(s, "%s: configuration not valid (ROUTER_CS_5.CV) before tunnelling", what);
            }
            return false;
        }
        if (!(r->port[r->up][CAP_USB4_OFF + 0x13] & 8)) {
            if (report) {
                violation(s, "%s: upstream USB4 port not configured (PORT_CS_19.PC)", what);
            }
            return false;
        }
    }
    return true;
}

static bool parent_pc(Usb4NhiState *s, Router *pr, int pl, const char *what, bool report)
{
    if (pr->usb4 && !(pr->port[pl][CAP_USB4_OFF + 0x13] & 8)) {
        if (report) {
            violation(s, "%s: downstream USB4 port not configured (PORT_CS_19.PC)", what);
        }
        return false;
    }
    return true;
}

static bool pcie_up(Usb4NhiState *s, int i, bool report)
{
    Router *pr = &s->r[i - 1], *r = &s->r[i];
    int pd = 6, pl = (i == 1) ? 1 : 3;
    char what[32];

    snprintf(what, sizeof(what), "PCIe tunnel %d", i - 1);
    if (!r->present || !(pr->port[pd][CAP_ADAP_OFF] & 0x80000000u) || !(r->port[5][CAP_ADAP_OFF] & 0x80000000u)) {
        return false;
    }
    if (!configured(s, r, what, report) || !parent_pc(s, pr, pl, what, report)) {
        return false;
    }
    if (r->usb4 && !(r->sw[5] & (1u << 24))) {
        if (report) {
            violation(s, "%s: PCIe tunnelling not switched on (ROUTER_CS_5.PTO)", what);
        }
        return false;
    }
    if (!r->usb4 && r->gen >= 2 && !(r->sw[tb3_lc_ctrl(r, 1)] & (1u << 30))) {
        if (report) {
            violation(s, "%s: Thunderbolt 3 lane not configured (LC SX_CTRL)", what);
        }
        return false;
    }
    if (!pr->usb4 && pr->gen >= 2 && !(pr->sw[tb3_lc_ctrl(pr, pl)] & (pl & 1 ? 1u << 16 : 1u << 20))) {
        if (report) {
            violation(s, "%s: downstream Thunderbolt lane of router %d not configured (LC SX_CTRL)", what, i - 1);
        }
        return false;
    }
    {
        bool want = pr->ver >= 2 && r->ver >= 2 && gen4(r, 1);
        bool ee_d = pr->port[pd][CAP_ADAP_OFF + 1] & 1, ee_u = r->port[5][CAP_ADAP_OFF + 1] & 1;
        if (want && (!ee_d || !ee_u)) {
            if (report) {
                violation(s, "%s: USB4 v2 Gen 4 link without PCIe extended encapsulation (down %d up %d)", what,
                          ee_d, ee_u);
            }
            return false;
        }
        if (!want && (ee_d || ee_u)) {
            if (report) {
                violation(s, "%s: PCIe extended encapsulation on a link that does not have it", what);
            }
            return false;
        }
    }
    return tunnel_ok(s, pr, pd, pl, r, 5, 1, 8, what, report);
}

static bool usb3_up(Usb4NhiState *s, int i, bool report)
{
    Router *pr = &s->r[i - 1], *r = &s->r[i];
    int pd = 8, pl = (i == 1) ? 1 : 3;
    char what[32];

    snprintf(what, sizeof(what), "USB 3 tunnel %d", i - 1);
    if (!r->present || !r->usb4 || !(pr->port[pd][CAP_ADAP_OFF] & 0x80000000u) ||
        !(r->port[7][CAP_ADAP_OFF] & 0x80000000u)) {
        return false;
    }
    if (!configured(s, r, what, report) || !parent_pc(s, pr, pl, what, report)) {
        return false;
    }
    if (!(r->sw[5] & (1u << 25))) {
        if (report) {
            violation(s, "%s: USB 3 tunnelling not switched on (ROUTER_CS_5.UTO)", what);
        }
        return false;
    }
    if (i == 1 && (pr->port[pd][CAP_ADAP_OFF + 2] & 0xFFFFFF) == 0) {
        if (report) {
            violation(s, "%s: no bandwidth allocated", what);
        }
        return false;
    }
    return tunnel_ok(s, pr, pd, pl, r, 7, 1, 8, what, report);
}

static bool host_clean(Usb4NhiState *s)
{
    Router *h = &s->r[0];
    int k;

    if (h->port[6][CAP_ADAP_OFF] & 0x80000000u || h->port[8][CAP_ADAP_OFF] & 0x80000000u ||
        (h->port[1][CAP_PHY_OFF + 1] & 0x8000u)) {
        return false;
    }
    for (k = 0; k < 64; k++) {
        if ((h->hop[6][2 * k] | h->hop[8][2 * k] | h->hop[1][2 * k]) & 0x80000000u) {
            return false;
        }
    }
    return true;
}

static void send_plug(Usb4NhiState *s, uint32_t route, int port, bool unplug)
{
    uint32_t d[3];
    int i;

    d[0] = 0;
    d[1] = route;
    d[2] = (uint32_t)port | (unplug ? 0x80000000u : 0);
    rx_push(s, P_EVENT, d, 3);
    for (i = 0; i < 8; i++) {
        PlugAck *a = &s->acks[i];
        if (a->used && a->route == route && a->port == port && a->unplug == unplug) {
            a->sent = now_ms();
            return;
        }
    }
    for (i = 0; i < 8; i++) {
        PlugAck *a = &s->acks[i];
        if (!a->used) {
            a->used = true;
            a->route = route;
            a->port = port;
            a->unplug = unplug;
            a->sent = now_ms();
            a->tries = 0;
            kick(s);
            return;
        }
    }
}

/* where a lane of router pr leads */
static int child_of(Usb4NhiState *s, int pi, int lane)
{
    if (pi == 0 && (lane == 1 || lane == 2)) {
        return s->r[1].present ? 1 : -1;
    }
    if (pi == 1 && (lane == 3 || lane == 4)) {
        return s->r[2].present ? 2 : -1;
    }
    return -1;
}

static void error_pkt(Usb4NhiState *s, uint32_t route, int err, int port)
{
    uint32_t d[3];

    d[0] = 0x80000000u;
    d[1] = route;
    d[2] = (uint32_t)err | ((uint32_t)port << 8);
    rx_push(s, P_ERROR, d, 3);
}

static void sw_write(Usb4NhiState *s, int ri, int off, uint32_t v)
{
    Router *r = &s->r[ri];
    uint32_t old = r->sw[off];

    switch (off) {
    case 0:
        return;
    case 1:
        if (ri != 0) {
            if ((int)((v >> 8) & 0x3F) != r->up) {
                violation(s, "router %d: upstream adapter written as %u, it is %d", ri, (v >> 8) & 0x3F, r->up);
            }
            if ((int)((v >> 20) & 7) != r->depth) {
                violation(s, "router %d: depth written as %u", ri, (v >> 20) & 7);
            }
        }
        r->sw[1] = (old & ~((0x3Fu << 8) | (7u << 20))) | (v & ((0x3Fu << 8) | (7u << 20)));
        return;
    case 2:
        if (ri != 0 && v != r->route) {
            violation(s, "router %d: route written as %x, it is %x", ri, v, r->route);
        }
        r->sw[2] = v;
        return;
    case 3:
        r->sw[3] = v;
        if (v & 0x80000000u) {
            if (r->sw[2] != r->route || (int)((r->sw[1] >> 20) & 7) != r->depth) {
                violation(s, "router %d enabled with route %x depth %u", ri, r->sw[2], (r->sw[1] >> 20) & 7);
            }
            r->enumerated = true;
        }
        return;
    case 4:
        if (r->ver == 1 && ((v >> 8) & 0xFF) != 0x10) {
            violation(s, "router %d: CM version %02x written (USB4 v1 router: 10)", ri, (v >> 8) & 0xFF);
        }
        if (r->ver == 2 && ((v >> 8) & 0xFF) != 0x20) {
            violation(s, "router %d: USB4 v2 router programmed with CM version %02x (v2 features stay hidden)", ri,
                      (v >> 8) & 0xFF);
        }
        r->sw[4] = (old & 0xFF000000u) | (v & 0x00FFFFFFu);
        return;
    case 5:
        if (r->usb4 && ri != 0) {
            if (!(old & 0x80000000u) && (v & 0x80000000u)) {
                if (!(r->sw[6] & (1u << 24))) {
                    violation(s, "router %d: configuration valid before router ready", ri);
                }
                r->cr_at = now_ms() + 5;
                kick(s);
            } else if ((v ^ old) & 0x07800000u) {
                r->sw[6] &= ~(1u << 24);
                r->rr_at = now_ms() + 10;
                kick(s);
            }
            if (v & (1u << 23)) {
                violation(s, "router %d: CNS left set (Thunderbolt 3 not supported by the CM)", ri);
            }
        }
        r->sw[5] = v;
        return;
    case 6:
        return;                                         /* status */
    default:
        if (getenv("NHI_DEBUG")) {
            fprintf(stderr, "usb4-nhi: router %d sw[%x] = %08x\n", ri, off, v);
        }
        r->sw[off] = v;
        return;
    }
}

/* LB written on the downstream lane adapter (router ri, lane p): bond the link once both ends of both lanes
   are set to dual width */
static void lane_bond_request(Usb4NhiState *s, int ri, int p)
{
    int c = child_of(s, ri, p), k;
    Router *pr = &s->r[ri], *r;

    if (c < 0 || (p != 1 && p != 3)) {
        violation(s, "lane bonding requested on router %d adapter %d (not a connected lane 0)", ri, p);
        return;
    }
    r = &s->r[c];
    if (gen4(pr, p)) {
        violation(s, "lane bonding requested on a Gen 4 link to router %d (it is bonded already)", c);
        return;
    }
    if (pr->gen < 2 || r->gen < 2) {
        violation(s, "lane bonding requested with a Thunderbolt 1 router (router %d)", c);
        return;
    }
    for (k = 0; k < 2; k++) {
        if (((pr->port[p + k][CAP_PHY_OFF + 1] >> 4) & 3) != 3 || ((r->port[r->up + k][CAP_PHY_OFF + 1] >> 4) & 3) != 3) {
            violation(s, "lane bonding to router %d requested before both lanes of both ends were set to dual", c);
            return;
        }
    }
    if (r->usb4 ? !(r->port[r->up][CAP_USB4_OFF + 0x12] & 0x100u) : !(r->sw[SW_LC_VSE + 0x10 + 0x8D] & (1u << 12))) {
        violation(s, "lane bonding to router %d, which does not support it", c);
        return;
    }
    s->bond_at[c - 1] = now_ms() + 5;
    kick(s);
}

static void port_write(Usb4NhiState *s, int ri, int p, int off, uint32_t v)
{
    Router *r = &s->r[ri];
    uint32_t *pt = r->port[p];
    uint32_t type = pt[2] & 0xFFFFFF;
    uint32_t old = pt[off];

    if (off < 4 || off == 6 || off == 7) {
        return;                                         /* read only header */
    }
    if (off == 4) {
        pt[4] = (old & 0x7FFFFFFFu) | (v & 0x80000000u);
        return;
    }
    if (off == 5) {
        pt[5] = (old & 0x7FFFFFFFu) | (v & 0x80000000u);
        return;
    }
    if (type == T_LANE && off == CAP_PHY_OFF + 1) {
        pt[off] = (old & 0x3FFF0000u) | (v & ~0x3FFF0000u);
        if ((v & 0x8000u) && !(old & 0x8000u)) {
            lane_bond_request(s, ri, p);
        }
        return;
    }
    if (type == T_LANE && (off == CAP_PHY_OFF || off == CAP_USB4_OFF || off == CAP_USB4_OFF + 0x12)) {
        return;
    }
    if (type == T_PCIE_DOWN || type == T_PCIE_UP) {
        if (off == CAP_ADAP_OFF + 1) {
            if (((v ^ old) & 1) && (pt[CAP_ADAP_OFF] & 0x80000000u)) {
                violation(s, "router %d adapter %d: extended encapsulation changed while the adapter is enabled", ri, p);
            }
            if ((v & 1) && r->ver < 2) {
                violation(s, "router %d adapter %d: extended encapsulation on a router before USB4 v2", ri, p);
            }
            pt[off] = v & 1;
            return;
        }
        if (off == CAP_ADAP_OFF) {
            pt[off] = (old & 0x7FFFFFFFu) | (v & 0x80000000u);
            if (!(old & 0x80000000u) && (v & 0x80000000u)) {
                if (ri == 0 && p == 6) {
                    if (!(s->r[1].port[5][CAP_ADAP_OFF] & 0x80000000u)) {
                        violation(s, "PCIe tunnel 0: downstream adapter enabled before the upstream one");
                    }
                    pcie_up(s, 1, true);
                } else if (ri == 1 && p == 6) {
                    pcie_up(s, 2, true);
                }
            }
        }
        return;
    }
    if (type == T_USB3_DOWN || type == T_USB3_UP) {
        int o = off - CAP_ADAP_OFF;
        if (o == 0) {
            pt[off] = (old & 0x3FFFFFFFu) | (v & 0xC0000000u);
            if (type == T_USB3_UP && !(old & 0x80000000u) && (v & 0x80000000u)) {
                usb3_up(s, ri, true);
            }
        } else if (o == 1 || o == 4) {
            /* read only */
        } else if (o == 2) {
            if (ri == 0 && ((v ^ old) & 0xFFFFFF) && !(old & 0x80000000u)) {
                violation(s, "USB 3 bandwidth written without the CM request (CMR) handshake");
            }
            if (ri == 0 && ((v ^ old) & 0xFFFFFF) && !(pt[CAP_ADAP_OFF + 1] & 0x80000000u)) {
                violation(s, "USB 3 bandwidth written before the host acknowledged (HCA)");
            }
            pt[off] = v;
            if (v & 0x80000000u) {
                pt[CAP_ADAP_OFF + 1] |= 0x80000000u;
            } else {
                pt[CAP_ADAP_OFF + 1] &= ~0x80000000u;
            }
        } else {
            pt[off] = v;
        }
        return;
    }
    pt[off] = v;
}

static void cfg_packet(Usb4NhiState *s, bool write, const uint32_t *d, int n)
{
    uint32_t route = d[1], addr = d[2];
    int off = addr & 0x1FFF, len = (addr >> 13) & 0x3F, port = (addr >> 19) & 0x3F, space = (addr >> 25) & 3;
    int ri = 0, depth = 0, i;
    uint32_t resp[3 + 63];
    Router *r;

    if (d[0] & 0x3FFFFF) {
        violation(s, "route above 32 bits");
    }
    /* follow the route */
    while (depth < 4 && ((route >> (8 * depth)) & 0xFF)) {
        int lane = (route >> (8 * depth)) & 0xFF;
        Router *cur = &s->r[ri];
        int c;
        if (ri != 0 && !cur->enumerated) {
            violation(s, "packet through router %d before it is enumerated", ri);
            error_pkt(s, cur->route, 0, lane);
            return;
        }
        if (lane > cur->maxport || (cur->port[lane][2] & 0xFFFFFF) != T_LANE) {
            error_pkt(s, cur->route, 4, lane);
            return;
        }
        c = child_of(s, ri, lane);
        if (c < 0 || (s->up_at[c - 1] != 0)) {
            error_pkt(s, cur->route, 0, lane);
            return;
        }
        if (cur->usb4 && (cur->port[lane][4] & 0x80000000u)) {
            error_pkt(s, cur->route, 15, lane);         /* locked */
            return;
        }
        ri = c;
        depth++;
    }
    if (route >> (8 * depth)) {
        error_pkt(s, route, 0, 0);
        return;
    }
    r = &s->r[ri];
    if (len == 0 || (write && n < 3 + len)) {
        violation(s, "config %s with length %d (%d dwords)", write ? "write" : "read", len, n);
        return;
    }
    if (space == 2) {
        if (off + len > 512) {
            error_pkt(s, route, 2, port);
            return;
        }
    } else if (space == 1 || space == 0) {
        if (port > r->maxport) {
            error_pkt(s, route, 4, port);
            return;
        }
        if (off + len > (space == 1 ? 64 : 128)) {
            error_pkt(s, route, 2, port);
            return;
        }
    } else {
        error_pkt(s, route, 2, port);
        return;
    }
    resp[0] = 0x80000000u;
    resp[1] = route;
    resp[2] = (addr & ~(0x3Fu << 19)) | ((uint32_t)(ri == 0 ? port : r->up) << 19);
    if (write) {
        for (i = 0; i < len; i++) {
            uint32_t v = d[3 + i];
            if (space == 2) {
                sw_write(s, ri, off + i, v);
            } else if (space == 1) {
                port_write(s, ri, port, off + i, v);
            } else {
                r->hop[port][off + i] = v;
            }
        }
        rx_push(s, P_WRITE, resp, 3);
    } else {
        for (i = 0; i < len; i++) {
            resp[3 + i] = space == 2 ? r->sw[off + i] : space == 1 ? r->port[port][off + i] : r->hop[port][off + i];
        }
        rx_push(s, P_READ, resp, 3 + len);
    }
}

/* ------------------------------------------------------------------ */
/* firmware connection manager                                          */
/* ------------------------------------------------------------------ */

static bool fr_msgs(Usb4NhiState *s)
{
    return s->mode == M_FR || s->mode == M_AR;
}

static void icm_reply(Usb4NhiState *s, uint8_t code, uint8_t flags, const uint8_t *body, int blen)
{
    uint32_t d[63];
    uint8_t b[252];
    int n = (4 + blen + 3) / 4, i;

    memset(b, 0, sizeof(b));
    b[0] = code;
    b[1] = flags;
    b[2] = 0;
    b[3] = 1;
    memcpy(b + 4, body, blen);
    for (i = 0; i < n; i++) {
        d[i] = ldl_le_p(b + 4 * i);
    }
    rx_push(s, P_ICM_RESP, d, n);
}

static void icm_connected(Usb4NhiState *s, int i)
{
    IcmDev *dv = &s->dev[i];
    uint8_t b[244];
    uint32_t d[61];
    uint16_t link = 1 | (1u << 4);
    int k;

    memset(b, 0, sizeof(b));
    b[0] = 0x03;
    b[1] = (uint8_t)((s->security & 3) << 3) | 0x20 | 0x80;
    b[3] = 1;
    memcpy(b + 4, dv->uuid, 16);
    if (s->security == 0 || s->mode == M_ICL || s->mode == M_TGL) {
        dv->approved = true;
    }
    if (dv->approved) {
        link |= 0x100;
    }
    if (fr_msgs(s)) {
        b[20] = dv->conn_key;
        b[21] = dv->conn_id;
        stw_le_p(b + 22, link);
        k = 24;
    } else {
        stl_le_p(b + 20, 0);
        stl_le_p(b + 24, 0x01);
        b[28] = dv->conn_id;
        stw_le_p(b + 30, link);
        k = 32;
    }
    /* Intel vendor specific entry: vendor 8086 model 15ef */
    b[k] = 14;
    b[k + 1] = 0x10;
    stw_le_p(b + k + 2, 0x8086);
    stw_le_p(b + k + 4, 0x15EF);
    for (k = 0; k < 61; k++) {
        d[k] = ldl_le_p(b + 4 * k);
    }
    rx_push(s, P_ICM_EVENT, d, 61);
    dv->reported = true;
}

static void icm_disconnected(Usb4NhiState *s, int i)
{
    uint8_t b[12];
    uint32_t d[3];

    memset(b, 0, sizeof(b));
    b[0] = 0x04;
    b[3] = 1;
    if (fr_msgs(s)) {
        stw_le_p(b + 6, 1 | (1u << 4));
        d[0] = ldl_le_p(b);
        d[1] = ldl_le_p(b + 4);
        rx_push(s, P_ICM_EVENT, d, 2);
    } else {
        stl_le_p(b + 4, 0);
        stl_le_p(b + 8, 0x01);
        d[0] = ldl_le_p(b);
        d[1] = ldl_le_p(b + 4);
        d[2] = ldl_le_p(b + 8);
        rx_push(s, P_ICM_EVENT, d, 3);
    }
    s->dev[i].reported = false;
    s->dev[i].approved = false;
    s->dev[i].keyok = false;
}

/* checks the device part of a request; returns the offset after it */
static int icm_who(Usb4NhiState *s, const uint8_t *b, int len, const char *what)
{
    IcmDev *dv = &s->dev[0];
    int end = fr_msgs(s) ? 24 : 32;

    if (len < end) {
        violation(s, "%s: request too short (%d)", what, len);
        return -1;
    }
    if (!s->plug[0] || !dv->reported || memcmp(b + 4, dv->uuid, 16) != 0) {
        violation(s, "%s: unknown device", what);
        return -1;
    }
    if (fr_msgs(s)) {
        if (b[20] != dv->conn_key || b[21] != dv->conn_id) {
            violation(s, "%s: connection key / id %02x %02x, not %02x %02x", what, b[20], b[21], dv->conn_key,
                      dv->conn_id);
            return -1;
        }
    } else {
        if (ldl_le_p(b + 20) != 0 || ldl_le_p(b + 24) != 0x01 || b[28] != dv->conn_id) {
            violation(s, "%s: route %08x%08x / id %02x wrong", what, ldl_le_p(b + 20), ldl_le_p(b + 24), b[28]);
            return -1;
        }
    }
    return end;
}

static void icm_cmd(Usb4NhiState *s, const uint32_t *d, int n)
{
    uint8_t b[252], body[248];
    IcmDev *dv = &s->dev[0];
    int i, len = n * 4, off;

    for (i = 0; i < n; i++) {
        stl_le_p(b + 4 * i, d[i]);
    }
    if (s->mode == M_ICL && !s->powered) {
        return;                                         /* firmware not running */
    }
    memset(body, 0, sizeof(body));
    switch (b[0]) {
    case 0x03:                                          /* driver ready */
        switch (s->mode) {
        case M_FR:
            stw_le_p(body + 2, s->security);
            icm_reply(s, 0x03, 0, body, 4);
            break;
        case M_AR:
            stw_le_p(body + 2, s->security);
            icm_reply(s, 0x03, 0, body, 4);
            break;
        default:
            stw_le_p(body + 2, s->security | (2u << 4));
            stl_le_p(body + 4, 0x2000);
            stw_le_p(body + 8, s->device_id);
            icm_reply(s, 0x03, 0, body, 12);
            break;
        }
        s->icm_ready = true;
        kick(s);
        break;
    case 0x04:                                          /* approve */
        off = icm_who(s, b, len, "approve");
        if (off < 0) {
            icm_reply(s, 0x04, 1, b + 4, (fr_msgs(s) ? 24 : 32) - 4);
            break;
        }
        if (s->mode == M_ICL || s->mode == M_TGL) {
            violation(s, "approve sent to an Ice Lake style firmware");
        }
        if (s->security == 2 && !dv->keyok) {
            violation(s, "approve in secure mode without a key");
            icm_reply(s, 0x04, 1, b + 4, off - 4);
            break;
        }
        dv->approved = true;
        icm_reply(s, 0x04, 0, b + 4, off - 4);
        break;
    case 0x06:                                          /* add key */
        off = icm_who(s, b, len, "add key");
        if (off < 0 || len < off + 32) {
            icm_reply(s, 0x06, 1, b + 4, (fr_msgs(s) ? 24 : 32) - 4);
            break;
        }
        if (s->security != 2) {
            violation(s, "key added in security level %u", s->security);
        }
        memcpy(dv->key, b + off, 32);
        dv->has_key = true;
        dv->keyok = true;
        icm_reply(s, 0x06, 0, b + 4, off - 4);
        break;
    case 0x05:                                          /* challenge */
        off = icm_who(s, b, len, "challenge");
        if (off < 0 || len < off + 32) {
            icm_reply(s, 0x05, 1, b + 4, (fr_msgs(s) ? 24 : 32) - 4);
            break;
        }
        memcpy(body, b + 4, off - 4);
        memcpy(body + off - 4, b + off, 32);
        if (!dv->has_key) {
            icm_reply(s, 0x05, 0x03, body, off - 4 + 64);
            break;
        }
        {
            GHmac *hm = g_hmac_new(G_CHECKSUM_SHA256, dv->key, 32);
            gsize ml = 32;
            g_hmac_update(hm, b + off, 32);
            g_hmac_get_digest(hm, body + off - 4 + 32, &ml);
            g_hmac_unref(hm);
        }
        dv->keyok = true;
        icm_reply(s, 0x05, 0, body, off - 4 + 64);
        break;
    default:
        violation(s, "unknown firmware request %02x", b[0]);
        break;
    }
}

/* ------------------------------------------------------------------ */
/* transmit ring: frames from the driver                                */
/* ------------------------------------------------------------------ */

static void handle_frame(Usb4NhiState *s, uint8_t type, const uint8_t *buf, int len)
{
    uint32_t d[64];
    int n = (len - 4) / 4, i;

    s->frames++;
    if (len < 8 || (len & 3)) {
        violation(s, "transmit frame of %d bytes", len);
        return;
    }
    if (ldl_be_p(buf + len - 4) != crc32c_calc(buf, len - 4)) {
        violation(s, "transmit frame type %u with a bad CRC", type);
        return;
    }
    for (i = 0; i < n; i++) {
        d[i] = ldl_be_p(buf + 4 * i);
    }
    switch (type) {
    case P_READ:
    case P_WRITE:
        if (n < 3) {
            violation(s, "config packet too short");
            return;
        }
        if (s->mode == M_LEGACY && !s->apple) {
            violation(s, "control packet to a Thunderbolt controller the BIOS runs");
            return;
        }
        if (!swcm(s)) {
            /* the firmware lets the driver read the host router only */
            if (d[1] != 0 || type == P_WRITE || !s->icm_ready) {
                if (!s->icm_ready) {
                    return;
                }
                error_pkt(s, d[1], 0, 0);
                return;
            }
        }
        cfg_packet(s, type == P_WRITE, d, n);
        break;
    case P_ERROR:
        /* plug event acknowledgement */
        if (n >= 3 && (d[2] & 0xFF) == 7) {
            int port = (d[2] >> 8) & 0x3F, pg = d[2] >> 30;
            for (i = 0; i < 8; i++) {
                PlugAck *a = &s->acks[i];
                if (a->used && a->route == d[1] && a->port == port) {
                    if (pg != (a->unplug ? 3 : 2)) {
                        violation(s, "plug event of %x:%d acknowledged with pg %d", d[1], port, pg);
                    }
                    a->used = false;
                    return;
                }
            }
            violation(s, "acknowledgement for no plug event (%x:%d)", d[1], port);
        } else {
            violation(s, "error packet %08x from the driver", n >= 3 ? d[2] : 0);
        }
        break;
    case P_NOTIFY_ACK:
        break;
    case P_ICM_CMD:
        if (swcm(s)) {
            violation(s, "firmware request to a host without firmware connection manager");
            return;
        }
        icm_cmd(s, d, n);
        break;
    default:
        violation(s, "transmit frame of type %u", type);
        break;
    }
}

static void tx_run(Usb4NhiState *s)
{
    PCIDevice *pd = PCI_DEVICE(s);
    uint32_t *rg = s->ring[0];
    uint32_t size = rg[3] & 0xFFFF;
    uint16_t prod = rg[2] >> 16;
    uint64_t base = ((uint64_t)rg[1] << 32) | rg[0];

    if (!(s->opt[0][0] & RING_ENABLE)) {
        violation(s, "transmit producer moved while ring 0 is off");
        return;
    }
    if (!(s->opt[0][0] & RING_RAW)) {
        violation(s, "control ring not in raw mode");
    }
    if (size == 0 || prod >= size) {
        violation(s, "transmit producer %u, ring size %u", prod, size);
        return;
    }
    while (s->tx_cons != prod) {
        uint8_t desc[16], buf[256];
        uint32_t ctl, len, eof, sof;
        uint64_t da = base + (uint64_t)s->tx_cons * 16, phys;

        pci_dma_read(pd, da, desc, 16);
        phys = ldq_le_p(desc);
        ctl = ldl_le_p(desc + 8);
        len = ctl & 0xFFF;
        eof = (ctl >> 12) & 0xF;
        sof = (ctl >> 16) & 0xF;
        if (!(ctl & D_POSTED)) {
            violation(s, "transmit descriptor %u without POSTED", s->tx_cons);
        }
        if (eof != sof) {
            violation(s, "transmit descriptor with eof %u / sof %u", eof, sof);
        }
        if (len > 256 || len == 0) {
            violation(s, "transmit length %u", len);
        } else {
            pci_dma_read(pd, phys, buf, len);
            handle_frame(s, (uint8_t)eof, buf, len);
        }
        ctl |= D_COMPLETED;
        stl_le_p(desc + 8, ctl);
        pci_dma_write(pd, da + 8, desc + 8, 4);
        s->tx_cons = (s->tx_cons + 1) % size;
    }
}

/* ------------------------------------------------------------------ */
/* time: link training, router ready, plug event repeats, firmware      */
/* ------------------------------------------------------------------ */

static void link_up(Usb4NhiState *s, int c)
{
    Router *pr = &s->r[c - 1];
    int l0 = (c == 1) ? 1 : 3;

    lane_state(pr, l0, 2);
    lane_state(pr, l0 + 1, 2);
    gen4_link(s, c);
    if (pr->usb4) {
        if (s->r[c].usb4) {
            pr->port[l0][CAP_USB4_OFF + 0x12] &= ~0x200u;
        } else {
            pr->port[l0][CAP_USB4_OFF + 0x12] |= 0x200u;       /* Thunderbolt 3 compatible mode */
        }
    }
    if (swcm(s)) {
        if (!pr->usb4 && (c == 1 || pr->enumerated) && (pr->sw[SW_PE_VSE + 1] & 0x78)) {
            violation(s, "router %d: device plugged in, but its plug events are off (Thunderbolt plug events "
                      "capability)", c - 1);
            return;
        }
        if ((c == 1 || pr->enumerated) && !(pr->port[l0][5] & 0x80000000u)) {
            send_plug(s, pr->route, l0, false);
            send_plug(s, pr->route, l0 + 1, false);
        }
    } else if (s->icm_ready && c == 1) {
        icm_connected(s, 0);
    }
}

static void tick(void *opaque)
{
    Usb4NhiState *s = opaque;
    int64_t t = now_ms();
    bool busy = false;
    int i;

    for (i = 0; i < 2; i++) {
        if (s->up_at[i] != 0) {
            if (t >= s->up_at[i]) {
                s->up_at[i] = 0;
                link_up(s, i + 1);
            } else {
                busy = true;
            }
        }
    }
    for (i = 0; i < 2; i++) {
        if (s->bond_at[i] != 0) {
            if (t >= s->bond_at[i]) {
                Router *pr = &s->r[i], *r = &s->r[i + 1];
                int l0 = i == 0 ? 1 : 3, k;
                s->bond_at[i] = 0;
                if (r->present) {
                    for (k = 0; k < 2; k++) {
                        pr->port[l0 + k][CAP_PHY_OFF + 1] = (pr->port[l0 + k][CAP_PHY_OFF + 1] & ~(0x3Fu << 20)) |
                                                            (2u << 20);
                        r->port[r->up + k][CAP_PHY_OFF + 1] = (r->port[r->up + k][CAP_PHY_OFF + 1] & ~(0x3Fu << 20)) |
                                                              (2u << 20);
                    }
                }
            } else {
                busy = true;
            }
        }
    }
    for (i = 1; i < NR; i++) {
        Router *r = &s->r[i];
        if (r->rr_at != 0) {
            if (t >= r->rr_at) {
                r->rr_at = 0;
                r->sw[6] |= 1u << 24;
            } else {
                busy = true;
            }
        }
        if (r->cr_at != 0) {
            if (t >= r->cr_at) {
                r->cr_at = 0;
                r->sw[6] |= 1u << 25;
            } else {
                busy = true;
            }
        }
    }
    for (i = 0; i < 8; i++) {
        PlugAck *a = &s->acks[i];
        if (a->used) {
            if (t - a->sent > 300) {
                if (++a->tries > 3) {
                    violation(s, "plug event %x:%d not acknowledged", a->route, a->port);
                    a->used = false;
                    continue;
                }
                send_plug(s, a->route, a->port, a->unplug);
            }
            busy = true;
        }
    }
    if (s->mode == M_ICL && s->power_at != 0) {
        if (t >= s->power_at) {
            s->power_at = 0;
            s->powered = true;
            pci_set_long(PCI_DEVICE(s)->config + 0xC8, 0x80000000u);
            s->fw_sts |= 0x80000000u | 1;
        } else {
            busy = true;
        }
    }
    if (!swcm(s) && s->icm_ready && s->plug[0] && !s->dev[0].reported && s->up_at[0] == 0) {
        icm_connected(s, 0);
    }
    rx_flush(s);
    if (busy || s->qn > 0) {
        timer_mod(s->tick, t + 1);
    }
}

/* ------------------------------------------------------------------ */
/* registers                                                            */
/* ------------------------------------------------------------------ */

static uint64_t nhi_read(void *opaque, hwaddr a, unsigned size)
{
    Usb4NhiState *s = opaque;
    int k;

    if (a < 0x08000 + HOPS * 16 && (a < HOPS * 16 || a >= 0x08000)) {
        int rx = a >= 0x08000;
        int ring = (int)((a & 0x7FFF) / 16) + (rx ? HOPS : 0);
        int reg = (a & 15) / 4;
        uint32_t v = s->ring[ring][reg];
        if (reg == 2 && ring == 0) {
            v = (v & 0xFFFF0000u) | s->tx_cons;
        } else if (reg == 2 && ring == HOPS) {
            v = (v & 0xFFFF) | ((uint32_t)s->rx_prod << 16);
        }
        return v;
    }
    if (a >= 0x19800 && a < 0x19800 + HOPS * 32) {
        k = (a - 0x19800) / 32;
        return (a & 31) < 8 ? s->opt[k][(a & 7) / 4] : 0;
    }
    if (a >= 0x29800 && a < 0x29800 + HOPS * 32) {
        k = (a - 0x29800) / 32 + HOPS;
        return (a & 31) < 8 ? s->opt[k][(a & 7) / 4] : 0;
    }
    switch (a) {
    case REG_CAPS:
        return HOPS | (s->caps_version << 16);
    case REG_RESET:
        return s->reset;
    case REG_INMAIL_DATA:
        return s->inmail_data;
    case REG_INMAIL_CMD:
        return s->inmail_cmd;
    case REG_OUTMAIL_CMD:
        return s->outmail_cmd;
    case REG_FW_STS:
        return s->fw_sts;
    default:
        return 0;
    }
}

static void nhi_write(void *opaque, hwaddr a, uint64_t val, unsigned size)
{
    Usb4NhiState *s = opaque;
    uint32_t v = val;
    int k;

    if (a < 0x08000 + HOPS * 16 && (a < HOPS * 16 || a >= 0x08000)) {
        int rx = a >= 0x08000;
        int ring = (int)((a & 0x7FFF) / 16) + (rx ? HOPS : 0);
        int reg = (a & 15) / 4;
        s->ring[ring][reg] = v;
        if (ring == 0 && reg == 2) {
            if (v & 0xFFFF) {
                violation(s, "driver wrote the transmit consumer index");
            }
            if ((v >> 16) == 0 && s->tx_cons == 0 && !(s->opt[0][0] & RING_ENABLE)) {
                return;                                 /* reset while off */
            }
            tx_run(s);
        } else if (ring == HOPS && reg == 2) {
            if (v >> 16) {
                violation(s, "driver wrote the receive producer index");
            }
            rx_flush(s);
        }
        return;
    }
    if (a >= 0x19800 && a < 0x19800 + HOPS * 32) {
        k = (a - 0x19800) / 32;
        if ((a & 31) < 8) {
            s->opt[k][(a & 7) / 4] = v;
            if (k == 0 && (a & 31) == 0 && !(v & RING_ENABLE)) {
                s->tx_cons = 0;
            }
        }
        return;
    }
    if (a >= 0x29800 && a < 0x29800 + HOPS * 32) {
        k = (a - 0x29800) / 32 + HOPS;
        if ((a & 31) < 8) {
            s->opt[k][(a & 7) / 4] = v;
            if (k == HOPS && (a & 31) == 0) {
                if (!(v & RING_ENABLE)) {
                    s->rx_prod = 0;
                } else {
                    if ((s->opt[HOPS][1] & 0xFFFF) == 0) {
                        violation(s, "receive ring enabled with an empty end of frame mask");
                    }
                    rx_flush(s);
                }
            }
        }
        return;
    }
    switch (a) {
    case REG_RESET:
        if (v & 1) {
            router_init(s, 0, host_kind(s));
            lane_state(&s->r[0], 1, s->plug[0] ? 2 : 7);
            lane_state(&s->r[0], 2, s->plug[0] ? 2 : 7);
        }
        break;
    case REG_INMAIL_DATA:
        s->inmail_data = v;
        break;
    case REG_INMAIL_CMD:
        if (v & 0x80000000u) {
            uint8_t cmd = v & 0xFF;
            if (swcm(s) || (cmd != 0x23 && cmd != 0x05)) {
                violation(s, "mailbox command %02x", cmd);
                s->inmail_cmd = (v & ~0x80000000u) | 0x40000000u;
            } else {
                s->inmail_cmd = v & ~0xC0000000u;
            }
        } else {
            s->inmail_cmd = v;
        }
        break;
    case REG_FW_STS:
        break;
    default:
        break;
    }
}

static const MemoryRegionOps nhi_ops = {
    .read = nhi_read,
    .write = nhi_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = { .min_access_size = 4, .max_access_size = 4 },
    .impl = { .min_access_size = 4, .max_access_size = 4 },
};

static void nhi_config_write(PCIDevice *pd, uint32_t addr, uint32_t val, int len)
{
    Usb4NhiState *s = USB4_NHI(pd);

    pci_default_write_config(pd, addr, val, len);
    if (addr == 0xFC && s->mode == M_ICL) {
        uint32_t v = pci_get_long(pd->config + 0xFC);
        if (v & 2) {
            if (((v >> 24) & 0xFF) != 0x22) {
                violation(s, "force power without the DMA delay 0x22");
            }
            if (!s->powered && s->power_at == 0) {
                s->power_at = now_ms() + 30;
                kick(s);
            }
        } else {
            s->powered = false;
            s->icm_ready = false;
            pci_set_long(pd->config + 0xC8, 0);
        }
    }
}

/* ------------------------------------------------------------------ */
/* properties                                                           */
/* ------------------------------------------------------------------ */

static void do_plug(Usb4NhiState *s, int i, bool on)
{
    if (on == s->plug[i]) {
        return;
    }
    if (!DEVICE(s)->realized) {
        s->plug[i] = on;                        /* there from the start (see reset) */
        return;
    }
    if (i == 1 && on && !s->plug[0]) {
        return;
    }
    s->plug[i] = on;
    if (!swcm(s)) {
        if (i == 0) {
            if (on) {
                s->up_at[0] = now_ms() + 20;
                s->dev[0].conn_key = (uint8_t)(s->dev[0].conn_key + 0x11);
                s->dev[0].conn_id = (uint8_t)(s->dev[0].conn_id + 1);
            } else {
                s->up_at[0] = 0;
                if (s->dev[0].reported) {
                    icm_disconnected(s, 0);
                }
                s->dev[0].approved = false;
            }
        }
        kick(s);
        return;
    }
    if (on) {
        const char *k = s->kind[i] ? s->kind[i] : "usb4";
        router_init(s, i + 1, kind_of(k));
        s->r[i + 1].present = true;
        {
            Router *pr = &s->r[i];
            int l0 = i == 0 ? 1 : 3;
            lane_state(pr, l0, 1);
            lane_state(pr, l0 + 1, 1);
        }
        s->up_at[i] = now_ms() + 30;
    } else {
        int j;
        for (j = 1; j >= i; j--) {
            Router *pr;
            int l0 = j == 0 ? 1 : 3;
            bool was = s->r[j + 1].present;
            s->r[j + 1].present = false;
            s->r[j + 1].enumerated = false;
            s->plug[j] = false;
            s->up_at[j] = 0;
            s->bond_at[j] = 0;
            pr = &s->r[j];
            lane_state(pr, l0, 7);
            lane_state(pr, l0 + 1, 7);
            if (was && j == i && (j == 0 || pr->enumerated) && !(pr->port[l0][5] & 0x80000000u)) {
                send_plug(s, pr->route, l0, true);
                send_plug(s, pr->route, l0 + 1, true);
            }
        }
    }
    kick(s);
}

static bool get_plug0(Object *o, Error **e) { return USB4_NHI(o)->plug[0]; }
static bool get_plug1(Object *o, Error **e) { return USB4_NHI(o)->plug[1]; }
static void set_plug0(Object *o, bool v, Error **e) { do_plug(USB4_NHI(o), 0, v); }
static void set_plug1(Object *o, bool v, Error **e) { do_plug(USB4_NHI(o), 1, v); }
static bool get_pcie0(Object *o, Error **e)
{
    Usb4NhiState *s = USB4_NHI(o);
    return swcm(s) ? pcie_up(s, 1, false) : (s->plug[0] && s->dev[0].approved);
}
static bool get_pcie1(Object *o, Error **e) { return pcie_up(USB4_NHI(o), 2, false); }
static bool get_usb30(Object *o, Error **e) { return usb3_up(USB4_NHI(o), 1, false); }
static bool get_usb31(Object *o, Error **e) { return usb3_up(USB4_NHI(o), 2, false); }
static void get_gbps(Object *o, Visitor *v, const char *name, void *opaque, Error **errp)
{
    Usb4NhiState *s = USB4_NHI(o);
    Router *r = &s->r[name[4] == '0' ? 1 : 2];
    uint32_t cs1 = r->port[r->up][CAP_PHY_OFF + 1], n = 0;

    if (r->present) {
        uint32_t sp = (cs1 >> 16) & 0xF;
        n = (sp == 0x2 ? 40 : sp == 0x4 ? 20 : 10) * ((cs1 >> 20) & 0x3F);
    }
    visit_type_uint32(v, name, &n, errp);
}
static bool get_ee0(Object *o, Error **e)
{
    Usb4NhiState *s = USB4_NHI(o);
    return (s->r[0].port[6][CAP_ADAP_OFF + 1] & 1) && (s->r[1].port[5][CAP_ADAP_OFF + 1] & 1);
}
static bool get_approved0(Object *o, Error **e) { return USB4_NHI(o)->dev[0].approved; }
static bool get_clean(Object *o, Error **e) { return host_clean(USB4_NHI(o)); }
static char *get_vlog(Object *o, Error **e) { return g_strdup(USB4_NHI(o)->vlog); }
static char *get_kind0(Object *o, Error **e) { return g_strdup(USB4_NHI(o)->kind[0] ?: "usb4"); }
static char *get_kind1(Object *o, Error **e) { return g_strdup(USB4_NHI(o)->kind[1] ?: "usb4"); }
static void set_kind0(Object *o, const char *v, Error **e) { g_free(USB4_NHI(o)->kind[0]); USB4_NHI(o)->kind[0] = g_strdup(v); }
static void set_kind1(Object *o, const char *v, Error **e) { g_free(USB4_NHI(o)->kind[1]); USB4_NHI(o)->kind[1] = g_strdup(v); }
static char *get_devkey(Object *o, Error **e) { return g_strdup(USB4_NHI(o)->dev[0].has_key ? "set" : "none"); }
static void set_devkey(Object *o, const char *v, Error **e)
{
    Usb4NhiState *s = USB4_NHI(o);
    if (!strcmp(v, "clear")) {
        s->dev[0].has_key = false;
    } else if (!strcmp(v, "bad")) {
        s->dev[0].key[0] ^= 0x5A;
        s->dev[0].has_key = true;
    } else if (!strncmp(v, "hex:", 4) && strlen(v) == 4 + 64) {
        /* the key the device keeps from an earlier pairing */
        int i;
        for (i = 0; i < 32; i++) {
            unsigned int b;
            if (sscanf(v + 4 + 2 * i, "%2x", &b) != 1) {
                error_setg(e, "devkey0: bad hex");
                return;
            }
            s->dev[0].key[i] = b;
        }
        s->dev[0].has_key = true;
    } else {
        error_setg(e, "devkey0: clear, bad or hex:<64 hex digits>");
    }
}
static void get_violations(Object *o, Visitor *v, const char *name, void *opaque, Error **errp)
{
    uint32_t n = USB4_NHI(o)->violations;
    visit_type_uint32(v, name, &n, errp);
}
static void get_frames(Object *o, Visitor *v, const char *name, void *opaque, Error **errp)
{
    uint32_t n = USB4_NHI(o)->frames;
    visit_type_uint32(v, name, &n, errp);
}

static void nhi_init(Object *obj)
{
    object_property_add_bool(obj, "plug0", get_plug0, set_plug0);
    object_property_add_bool(obj, "plug1", get_plug1, set_plug1);
    object_property_add_bool(obj, "pcie0", get_pcie0, NULL);
    object_property_add_bool(obj, "pcie1", get_pcie1, NULL);
    object_property_add_bool(obj, "usb3-0", get_usb30, NULL);
    object_property_add_bool(obj, "usb3-1", get_usb31, NULL);
    object_property_add_bool(obj, "approved0", get_approved0, NULL);
    object_property_add_bool(obj, "ee0", get_ee0, NULL);
    object_property_add_bool(obj, "host-clean", get_clean, NULL);
    object_property_add_str(obj, "vlog", get_vlog, NULL);
    object_property_add_str(obj, "kind0", get_kind0, set_kind0);
    object_property_add_str(obj, "kind1", get_kind1, set_kind1);
    object_property_add_str(obj, "devkey0", get_devkey, set_devkey);
    object_property_add(obj, "violations", "uint32", get_violations, NULL, NULL, NULL);
    object_property_add(obj, "frames", "uint32", get_frames, NULL, NULL, NULL);
    object_property_add(obj, "gbps0", "uint32", get_gbps, NULL, NULL, NULL);
    object_property_add(obj, "gbps1", "uint32", get_gbps, NULL, NULL, NULL);
}

static void nhi_reset(DeviceState *dev)
{
    Usb4NhiState *s = USB4_NHI(dev);

    memset(s->ring, 0, sizeof(s->ring));
    memset(s->opt, 0, sizeof(s->opt));
    s->tx_cons = s->rx_prod = 0;
    s->qh = s->qn = 0;
    memset(s->acks, 0, sizeof(s->acks));
    memset(s->bond_at, 0, sizeof(s->bond_at));
    s->icm_ready = false;
    s->powered = false;
    s->power_at = 0;
    s->inmail_cmd = s->inmail_data = 0;
    switch (s->mode) {
    case M_USB4:
    case M_LEGACY:
        s->fw_sts = 0;
        break;
    case M_ICL:
        s->fw_sts = 0;
        break;
    default:
        s->fw_sts = 0x80000000u | 1;
        break;
    }
    s->outmail_cmd = 3u << 8;
    router_init(s, 0, host_kind(s));
    s->r[1].present = s->r[2].present = false;
    /* devices plugged in from the start: links up, no events */
    if (s->plug[0]) {
        if (swcm(s)) {
            int i;
            for (i = 0; i < 2 && s->plug[i]; i++) {
                router_init(s, i + 1, kind_of(s->kind[i]));
                s->r[i + 1].present = true;
                s->up_at[i] = 0;
                lane_state(&s->r[i], i == 0 ? 1 : 3, 2);
                lane_state(&s->r[i], i == 0 ? 2 : 4, 2);
                gen4_link(s, i + 1);
                if (!s->r[i + 1].usb4 && s->r[i].usb4) {
                    s->r[i].port[i == 0 ? 1 : 3][CAP_USB4_OFF + 0x12] |= 0x200u;
                }
            }
        } else {
            s->up_at[0] = 0;
            lane_state(&s->r[0], 1, 2);
        }
    }
}

static void nhi_realize(PCIDevice *pd, Error **errp)
{
    Usb4NhiState *s = USB4_NHI(pd);
    const char *m = s->mode_str ? s->mode_str : "usb4";
    uint16_t id;
    uint32_t cls;
    static const uint8_t uuid0[16] = { 0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe,
                                       0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef };

    if (!strcmp(m, "usb4")) {
        s->mode = M_USB4; id = s->v2 ? 0x5781 : 0x1668; cls = 0x0C0340;
        if (s->v2 && s->caps_version == 0x20) {
            s->caps_version = 0x40;
        }
    } else if (!strcmp(m, "legacy")) {
        s->mode = M_LEGACY; id = 0x1547; cls = 0x088000;
    } else if (!strcmp(m, "fr")) {
        s->mode = M_FR; id = 0x156C; cls = 0x088000;
    } else if (!strcmp(m, "ar")) {
        s->mode = M_AR; id = 0x15D2; cls = 0x088000;
    } else if (!strcmp(m, "tr")) {
        s->mode = M_TR; id = 0x15EB; cls = 0x088000;
    } else if (!strcmp(m, "icl")) {
        s->mode = M_ICL; id = 0x8A17; cls = 0x0C0340;
    } else if (!strcmp(m, "tgl")) {
        s->mode = M_TGL; id = 0x9A1B; cls = 0x0C0340;
    } else {
        error_setg(errp, "usb4-nhi: mode is usb4, legacy, fr, ar, tr, icl or tgl");
        return;
    }
    if (s->device_id == 0) {
        s->device_id = id;
    }
    pci_config_set_vendor_id(pd->config, s->mode == M_USB4 && s->device_id == 0x1668 ? 0x1022 : 0x8086);
    pci_config_set_device_id(pd->config, s->device_id);
    pci_config_set_class(pd->config, cls >> 8);
    if (s->mode == M_LEGACY && s->apple) {
        pci_set_word(pd->config + PCI_SUBSYSTEM_VENDOR_ID, 0x2222);
        pci_set_word(pd->config + PCI_SUBSYSTEM_ID, 0x1111);
    }
    pd->config[PCI_CLASS_PROG] = cls & 0xFF;
    pd->config[PCI_INTERRUPT_PIN] = 1;
    pci_set_long(pd->wmask + 0xE0, 0xFFFFFFFFu);
    pci_set_long(pd->wmask + 0xFC, 0xFFFFFFFFu);
    pci_set_long(pd->config + 0xE4, 0x1003);
    memcpy(s->dev[0].uuid, uuid0, 16);
    s->dev[0].conn_key = 0x40;
    s->dev[0].conn_id = 0x10;
    memory_region_init_io(&s->mmio, OBJECT(s), &nhi_ops, s, "usb4-nhi", NHI_LEN);
    pci_register_bar(pd, 0, PCI_BASE_ADDRESS_SPACE_MEMORY | PCI_BASE_ADDRESS_MEM_TYPE_64, &s->mmio);
    s->tick = timer_new_ms(QEMU_CLOCK_VIRTUAL, tick, s);
}

static Property nhi_props[] = {
    DEFINE_PROP_STRING("mode", Usb4NhiState, mode_str),
    DEFINE_PROP_UINT8("security", Usb4NhiState, security, 1),
    DEFINE_PROP_UINT16("device-id", Usb4NhiState, device_id, 0),
    DEFINE_PROP_UINT32("caps-version", Usb4NhiState, caps_version, 0x20),
    DEFINE_PROP_BOOL("v2", Usb4NhiState, v2, false),
    DEFINE_PROP_BOOL("apple", Usb4NhiState, apple, true),
    DEFINE_PROP_END_OF_LIST(),
};

static void nhi_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->realize = nhi_realize;
    k->config_write = nhi_config_write;
    k->vendor_id = 0x8086;
    k->device_id = 0x1668;
    k->class_id = 0x0C03;
    dc->reset = nhi_reset;
    dc->desc = "Thunderbolt / USB4 host interface (test model)";
    device_class_set_props(dc, nhi_props);
    set_bit(DEVICE_CATEGORY_BRIDGE, dc->categories);
}

static const TypeInfo nhi_info = {
    .name = TYPE_USB4_NHI,
    .parent = TYPE_PCI_DEVICE,
    .instance_size = sizeof(Usb4NhiState),
    .instance_init = nhi_init,
    .class_init = nhi_class_init,
    .interfaces = (InterfaceInfo[]) {
        { INTERFACE_PCIE_DEVICE },
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { },
    },
};

static void nhi_register(void)
{
    type_register_static(&nhi_info);
}

type_init(nhi_register)
