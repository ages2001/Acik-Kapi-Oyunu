/* DisplayPort 8b/10b link training and HDMI 2.0 SCDC.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef WINXPDM_LINKS_H
#define WINXPDM_LINKS_H
#include <stdint.h>
enum { WXP_DPCD, WXP_SCDC };
typedef struct {
    void *context;
    int (*receive)(void *, unsigned bus, unsigned address, uint8_t *data, unsigned length);
    int (*send)(void *, unsigned bus, unsigned address, const uint8_t *data, unsigned length);
    int (*dp_source)(void *, unsigned rate, unsigned lanes, unsigned pattern, const uint8_t drive[4]);
    int (*hdmi_source)(void *, uint32_t tmds_khz, unsigned config);
    void (*wait_us)(unsigned microseconds);
} WxpLink;
int wxp_dp_hbr3(WxpLink *link);
int wxp_hdmi_tmds(WxpLink *link, uint32_t tmds_khz);
#endif
