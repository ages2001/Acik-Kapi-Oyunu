/* Shared test scene for all Windows ABIs. Values are IEEE 754 binary32. */
#ifndef WINXPDM_TEST_SCENE_H
#define WINXPDM_TEST_SCENE_H
#define WXP_DEMO_FLAGS WXP_DRAW_TEST_OUTPUTS
#include "../../../src/drivers/virtio/demo_scene.h"
static int check_scene(const WxpFrame *r)
{
    unsigned x, y, black = 0, green = 0, colored = 0;
    if (r->status || !r->completed || !r->fence || r->pixels[0] != 0xff000000UL ||
        r->pixels[16 * WXP_SIDE + 16] != 0xff00ff00UL) return 0;
    if (r->outputs >= 2 && r->tested_outputs != 3) return 0;
    for (y = 0; y < WXP_SIDE; ++y) {
        for (x = 0; x < WXP_SIDE; ++x) {
            uint32_t c = r->pixels[y * WXP_SIDE + x];
            if (c == 0xff000000UL) ++black;
            else if (c == 0xff00ff00UL) ++green;
            else {
                unsigned red = (unsigned)(c & 255), blue = (unsigned)((c >> 16) & 255);
                if ((c & 0xff00ff00UL) != 0xff000000UL || red + blue < 254 || red + blue > 256)
                    return 0;
                if ((x < 12 && red <= blue) || (x > 20 && blue <= red)) return 0;
                ++colored;
            }
        }
    }
    return black == 736 && green == 32 && colored == 256;
}

#define FM1 0xbf800000UL
#define WHITE {F1,F1,F1,F1}
static const WxpDraw textured = {
    WXP_GPU_ABI, 6, {0,0,0,F1}, {
        {{FM1,FM1,0,F1},WHITE,{0,0}}, {{F1,FM1,0,F1},WHITE,{F1,0}}, {{F1,F1,0,F1},WHITE,{F1,F1}},
        {{FM1,FM1,0,F1},WHITE,{0,0}}, {{F1,F1,0,F1},WHITE,{F1,F1}}, {{FM1,F1,0,F1},WHITE,{0,F1}}
    }, WXP_DRAW_TEXTURED, {0xff0000ffUL,0xff00ff00UL,0xffff0000UL,0xff00ffffUL}
};
static int check_texture(const WxpFrame *r)
{
    unsigned x, y;
    if (r->status || !r->completed) return 0;
    for (y=0; y<WXP_SIDE; ++y) for (x=0; x<WXP_SIDE; ++x) {
        unsigned index = (y >= WXP_SIDE/2 ? 2 : 0) + (x >= WXP_SIDE/2 ? 1 : 0);
        if (r->pixels[y*WXP_SIDE+x] != textured.texture[index]) return 0;
    }
    return 1;
}
#endif
