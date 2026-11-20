/* DisplayPort AUX controllers and two attachable monitor sinks.
 * Sources: Nouveau gm200_i2c_aux_xfer, AMD dce_aux and DCN 4.1 register headers.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "hw/display/edid.h"

#define WXP_AUX_BASE 0xd930
#define WXP_AUX_STRIDE 0x50
#define WXP_AUX_CHANNELS 8
#define WXP_DP_SINKS 2
#define DCN_AUX_BASE ((0x34c0 + 0x16b2) * 4)
#define DCN_AUX_STRIDE (0x1c * 4)
#define DCN_AUX_CHANNELS 4

typedef struct {
    uint32_t enable, control, arbitration, interrupt, status;
    uint8_t fifo[32], index;
    bool reading;
} WxpDcnAux;

typedef struct {
    uint8_t tx[16], rx[16], edid[256];
    uint32_t address, control, reply, autodpcd;
    uint8_t offset, segment, power;
    WxpDcnAux dcn;
} WxpAux;

static void wxp_aux_reset(WxpAux *a, unsigned channel)
{
    memset(a, 0, sizeof(*a));
    a->autodpcd = 0x10000;
    a->power = 1;
    if (channel < WXP_DP_SINKS) {
        qemu_edid_info info = {
            .name = channel ? "WINXPDM DP-2" : "WINXPDM DP-1",
            .serial = channel ? "2" : "1",
            .prefx = channel ? 1280 : 1920,
            .prefy = channel ? 1024 : 1080,
            .refresh_rate = 60000
        };
        qemu_edid_generate(a->edid, sizeof(a->edid), &info);
    }
}

static bool wxp_aux_reg(hwaddr offset)
{
    if (offset < WXP_AUX_BASE || offset >= WXP_AUX_BASE + WXP_AUX_CHANNELS * WXP_AUX_STRIDE) {
        return false;
    }
    unsigned reg = (offset - WXP_AUX_BASE) % WXP_AUX_STRIDE;
    return reg <= 0x28 || reg == 0x38;
}

static uint32_t wxp_aux_read(WxpAux *a, unsigned reg, bool connected)
{
    if (reg < 0x10) {
        return ldl_le_p(a->tx + reg);
    }
    if (reg < 0x20) {
        return ldl_le_p(a->rx + reg - 0x10);
    }
    switch (reg) {
    case 0x20: return a->address;
    case 0x24: return a->control;
    case 0x28: return (connected ? 0x10000000 : 0) | a->reply;
    case 0x38: return a->autodpcd;
    default: return 0;
    }
}

/* Model receiver capabilities and EDID reads. Lane status is supplied only
 * by a trained physical link or a separately implemented link-training model. */
static unsigned wxp_aux_transfer(WxpAux *a, unsigned channel, unsigned command, unsigned size)
{
    unsigned addr = a->address;
    switch (command) {
    case 8: /* Native AUX write */
        if (addr != 0x600 || size != 1 || (a->tx[0] != 1 && a->tx[0] != 2)) {
            return 1; /* Native NACK */
        }
        a->power = a->tx[0];
        return 0;
    case 9: { /* Native AUX read */
        uint8_t caps[16] = {channel ? 0x12 : 0x14, channel ? 0x14 : 0x1e,
                            0xc4, channel ? 0 : 0x80, 0, 0, 1, 0, 2};
        if (addr == 0x600 && size == 1) {
            a->rx[0] = a->power;
        } else if (addr <= sizeof(caps) && size <= sizeof(caps) - addr) {
            memcpy(a->rx, caps + addr, size);
        } else {
            return 1;
        }
        return 0;
    }
    case 0: case 4: /* I2C write, with optional middle-of-transaction flag */
        if ((addr != 0x50 && addr != 0x30) || size > 1) {
            return 4; /* I2C NACK */
        }
        if (size && addr == 0x50) {
            a->offset = a->tx[0];
        } else if (size) {
            a->segment = a->tx[0];
        }
        return 0;
    case 1: case 5: /* I2C read */
        if (addr != 0x50 || a->segment) {
            return 4;
        }
        for (unsigned i = 0; i < size; ++i) {
            a->rx[i] = a->edid[a->offset++];
        }
        return 0;
    default:
        return 1;
    }
}

static void wxp_aux_write(WxpAux *a, unsigned channel, unsigned reg, uint32_t value, bool connected)
{
    if (reg < 0x10) {
        stl_le_p(a->tx + reg, value);
    } else if (reg == 0x20) {
        a->address = value;
    } else if (reg == 0x38) {
        a->autodpcd = value & 0x10000;
    } else if (reg == 0x24) {
        unsigned owner = value & 0x00700000;
        a->control = (value & ~0x87010000U) | (owner << 4);
        if (value & 0x80000000) {
            a->reply = 0;
        } else if (value & 0x10000) {
            unsigned length = value & 0x100 ? 0 : 1 + (value & 255);
            unsigned cmd = (value >> 12) & 15;
            memset(a->rx, 0, sizeof(a->rx));
            if (!connected || !owner) {
                a->reply = 0x100; /* Controller timeout */
            } else if (length > sizeof(a->tx)) {
                a->reply = 0x200;
            } else {
                unsigned response = wxp_aux_transfer(a, channel, cmd, length);
                a->reply = (response << 16) | (!response && (cmd & 1) ? length : 0);
            }
        }
    }
}

static bool wxp_dcn_aux_reg(hwaddr offset)
{
    if (offset < DCN_AUX_BASE || offset >= DCN_AUX_BASE + DCN_AUX_CHANNELS * DCN_AUX_STRIDE) {
        return false;
    }
    unsigned reg = (offset - DCN_AUX_BASE) % DCN_AUX_STRIDE;
    return reg <= 0x10 || reg == 0x18;
}

static uint32_t wxp_dcn_read(WxpDcnAux *d, unsigned reg)
{
    switch (reg) {
    case 0: return d->enable;
    case 4: return d->control;
    case 8: return d->arbitration;
    case 12: return d->interrupt;
    case 16: return d->status;
    case 24: {
        uint8_t index = d->index;
        if (d->reading) {
            d->index = (d->index + 1) & 31;
        }
        return (index << 16) | (d->fifo[index] << 8) | d->reading;
    }
    default: return 0;
    }
}

static void wxp_dcn_submit(WxpAux *a, unsigned channel, bool connected)
{
    WxpDcnAux *d = &a->dcn;
    unsigned count = (d->control >> 16) & 31;
    unsigned cmd = d->fifo[0] >> 4;
    unsigned length = count == 3 ? 0 : d->fifo[3] + 1;
    d->status = 1; /* AUX_SW_DONE */
    d->interrupt |= 1;
    if (!connected) {
        d->status |= 0x200; /* HPD disconnected */
        return;
    }
    if (!(d->enable & 1) || (d->arbitration & 12) != 4 || count < 3 || length > 16 ||
        count != (length ? 4 : 3) + (cmd & 1 ? 0 : length)) {
        d->status |= 0x1000; /* Invalid byte count */
        return;
    }
    a->address = ((d->fifo[0] & 15) << 16) | (d->fifo[1] << 8) | d->fifo[2];
    if (!(cmd & 1)) {
        memcpy(a->tx, d->fifo + 4, length);
    }
    unsigned response = wxp_aux_transfer(a, channel, cmd, length);
    unsigned payload = (cmd & 1) && !response ? length : 0;
    d->fifo[0] = response << 4;
    memcpy(d->fifo + 1, a->rx, payload);
    d->status |= (payload + 1) << 24;
}

static void wxp_dcn_write(WxpAux *a, unsigned channel, unsigned reg, uint32_t value, bool connected)
{
    WxpDcnAux *d = &a->dcn;
    switch (reg) {
    case 0:
        d->enable = (value & ~0x20U) | ((value & 0x10) << 1);
        if (value & 0x10) {
            d->status = d->interrupt = d->arbitration = d->control = 0;
            d->index = 0;
            d->reading = false;
        }
        break;
    case 4:
        d->control = value & 0x001f00f0;
        if (value & 1) {
            wxp_dcn_submit(a, channel, connected);
        }
        break;
    case 8:
        d->arbitration = value & 1;
        if ((d->enable & 1) && (value & 0x10000) && !(value & 0x20000)) {
            d->arbitration |= 0x10004;
        }
        break;
    case 12:
        d->interrupt = (d->interrupt & 1) | (value & 4);
        if (value & 2) {
            d->interrupt &= ~1U;
            d->status = 0;
        }
        break;
    case 24:
        d->reading = value & 1;
        if (value & 0x80000000U) {
            d->index = (value >> 16) & 31;
        } else if (!d->reading) {
            d->index = (d->index + 1) & 31;
        }
        if (!d->reading) {
            d->fifo[d->index] = value >> 8;
        }
        break;
    }
}
