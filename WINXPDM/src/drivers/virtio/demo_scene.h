/* Depth and interpolation demonstration. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef WXP_DEMO_SCENE_H
#define WXP_DEMO_SCENE_H
#include "gpu.h"
#define F1 0x3f800000UL
#define FH 0x3f000000UL
#define FN 0xbf400000UL
#define FP 0x3f400000UL
#ifndef WXP_DEMO_FLAGS
#define WXP_DEMO_FLAGS 0
#endif
static const WxpDraw scene = {
    WXP_GPU_ABI, 6, {0, 0, 0, F1}, {
        /* Smaller green triangle at near Z, submitted first. */
        {{0xbe800000UL, 0xbe800000UL, 0xbf000000UL, F1}, {0,F1,0,F1}, {0,0}},
        {{0x3e800000UL, 0xbe800000UL, 0xbf000000UL, F1}, {0,F1,0,F1}, {0,0}},
        {{0, 0x3e800000UL, 0xbf000000UL, F1}, {0,F1,0,F1}, {0,0}},
        /* Far triangle changes from red to blue along X. */
        {{FN,FN,FH,F1}, {F1,0,0,F1}, {0,0}},
        {{FP,FN,FH,F1}, {0,0,F1,F1}, {0,0}},
        {{0,FP,FH,F1}, {FH,0,FH,F1}, {0,0}}
    }, WXP_DEMO_FLAGS, {0,0,0,0}
};
#endif
