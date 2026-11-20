/* DPCD addresses and SCDC bits follow Linux drm_dp.h and drm_scdc.h.
 * The transmitter's actual lane programming is supplied by the card port.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "links.h"

static int byte_out(WxpLink *l, unsigned bus, unsigned addr, unsigned v)
{
    uint8_t value = (uint8_t)v;
    return l->send(l->context,bus,addr,&value,1);
}
static int training(WxpLink *l, unsigned pattern, unsigned interval, uint8_t drive[4])
{
    uint8_t status[6];
    unsigned attempt, lane, ready, cr;
    unsigned mode = pattern == 1 ? 0x21 : 7; /* TPS1 without scrambling, TPS4 with scrambling */
    if (!byte_out(l,WXP_DPCD,0x102,mode)) return 0;
    for (attempt=0; attempt<5; ++attempt) {
        if (!l->dp_source(l->context,0x1e,4,pattern,drive) ||
            !l->send(l->context,WXP_DPCD,0x103,drive,4)) return 0;
        l->wait_us(interval ? 4000U*interval : pattern==1 ? 100 : 400);
        if (!l->receive(l->context,WXP_DPCD,0x202,status,6)) return 0;
        ready = 1; cr = 1;
        for (lane=0; lane<4; ++lane) {
            unsigned shift = (lane & 1)*4;
            unsigned bits = (status[lane/2] >> shift) & 7;
            unsigned request = (status[4+lane/2] >> shift) & 15;
            unsigned swing = request & 3, pre = request >> 2;
            if (!(bits & 1)) cr = 0;
            if (bits != 7) ready = 0;
            if (pre > 3-swing) pre = 3-swing;
            drive[lane] = (uint8_t)(swing | (pre << 3) | (swing==3 ? 4 : 0) | (pre==3 ? 32 : 0));
        }
        if (pattern==1 && cr) return 1;
        if (pattern==7 && !cr) return 0;
        if (pattern==7 && ready && (status[2] & 1)) return 1;
    }
    return 0;
}

int wxp_dp_hbr3(WxpLink *l)
{
    uint8_t caps[15], rate_lanes[2], drive[4] = {0,0,0,0};
    unsigned interval;
    int ok;
    if (!l->receive(l->context,WXP_DPCD,0,caps,sizeof(caps)) || caps[1]<0x1e ||
        (caps[2]&31)<4 || !(caps[3]&0x80)) return 0;
    interval = caps[14] & 0x7f;
    if (interval > 4) return 0;
    rate_lanes[0] = 0x1e; rate_lanes[1] = (uint8_t)(4 | (caps[2]&0x80));
    if (!l->send(l->context,WXP_DPCD,0x100,rate_lanes,2)) return 0;
    ok = training(l,1,interval,drive) && training(l,7,interval,drive);
    if (!byte_out(l,WXP_DPCD,0x102,0)) ok=0;
    if (!l->dp_source(l->context,0x1e,4,0,drive)) ok=0;
    return ok;
}

int wxp_hdmi_tmds(WxpLink *l, uint32_t tmds_khz)
{
    uint8_t version, flags, scrambling;
    unsigned attempt, config;
    if (!tmds_khz || tmds_khz>600000UL ||
        !l->receive(l->context,WXP_SCDC,1,&version,1) || version<1) return 0;
    config = tmds_khz>340000UL ? 3 : 0; /* 1:40 TMDS bit-clock ratio and scrambling */
    if (!byte_out(l,WXP_SCDC,2,1) || !byte_out(l,WXP_SCDC,0x20,config)) return 0;
    if (l->hdmi_source(l->context,tmds_khz,config)) {
        for (attempt=0; attempt<20; ++attempt) {
            l->wait_us(1000);
            if (!l->receive(l->context,WXP_SCDC,0x21,&scrambling,1) ||
                !l->receive(l->context,WXP_SCDC,0x40,&flags,1)) break;
            if ((scrambling&1)==(config&1) && (flags&15)==15) return 1;
        }
    }
    l->hdmi_source(l->context,0,0);
    byte_out(l,WXP_SCDC,0x20,0);
    return 0;
}
