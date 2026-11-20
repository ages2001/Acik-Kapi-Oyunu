/*
 * A QEMU test object for a subset of the Maxwell and Pascal 3D class methods.
 * Method and packet encodings follow NVIDIA's clb097.h, clb197.h, clc197.h
 * and cl906f.h. It has one pitch-linear RGBA8 color target in local memory.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "qemu/osdep.h"
#include <math.h>
#include "qapi/error.h"
#include "qapi/visitor.h"
#include "qapi/qapi-builtin-visit.h"
#include "qom/object.h"
#include "qom/object_interfaces.h"

#define TYPE_WXP_NV_COLOR "winxpdm-nv-color"
OBJECT_DECLARE_SIMPLE_TYPE(WxpNvColor, WXP_NV_COLOR)

enum {
    SET_OBJECT = 0, TARGET_HI = 0x800, TARGET_LO = 0x804,
    TARGET_PITCH = 0x808, TARGET_HEIGHT = 0x80c, TARGET_FORMAT = 0x810,
    TARGET_MEMORY = 0x814, RECT_X = 0xd6c, RECT_Y = 0xd70,
    CLEAR_COLOR = 0xd80, CLEAR_CONTROL = 0x10f8, CLEAR_SURFACE = 0x19d0,
    A8R8G8B8 = 0xcf, A8B8G8R8 = 0xd5
};

struct WxpNvColor {
    Object parent_obj;
    uint32_t vram[16384];
    uint32_t cls, method, count, addr_hi, addr, pitch, height, format, memory;
    uint32_t rect_x, rect_y, control, color[4], read_addr;
    uint32_t dst_format, layout, dst_pitch, width, dst_height, dst_hi, dst_lo;
    uint32_t operation, primitive, solid_format, solid;
    int16_t px[3], py[3];
};

static bool color_clear(WxpNvColor *s, uint32_t action, Error **errp)
{
    unsigned x, y, c, first_x = 0, first_y = 0, end_x, end_y;
    uint32_t packed = 0, mask = 0;

    if (!s->pitch || (s->pitch & 127) || !s->height || s->addr_hi ||
        (s->addr & 127) || s->addr >= sizeof(s->vram) ||
        s->height > (sizeof(s->vram) - s->addr) / s->pitch ||
        s->memory != (1U << 12) ||
        (s->format != A8R8G8B8 && s->format != A8B8G8R8) ||
        (action & ~0x3cU) || (s->control & ~0x10U)) {
        error_setg(errp, "Unsupported color target or clear state");
        return false;
    }
    end_x = s->pitch / 4;
    end_y = s->height;
    if (s->control & 0x10) {
        first_x = s->rect_x & 0xffff;
        first_y = s->rect_y & 0xffff;
        end_x = MIN(end_x, s->rect_x >> 16);
        end_y = MIN(end_y, s->rect_y >> 16);
    }
    for (c = 0; c < 4; ++c) {
        unsigned channel = c;
        float f;
        if (!(action & (4U << c))) {
            continue;
        }
        memcpy(&f, &s->color[c], sizeof(f));
        if (!isfinite(f)) {
            error_setg(errp, "The color-clear fixture accepts finite UNORM values");
            return false;
        }
        if (s->format == A8R8G8B8 && !(c & 1)) {
            channel = 2 - c;
        }
        packed |= (uint32_t)(MIN(MAX(f, 0.0f), 1.0f) * 255.0f + 0.5f) << (8 * channel);
        mask |= 0xffU << (8 * channel);
    }
    for (y = first_y; y < end_y; ++y) {
        for (x = first_x; x < end_x; ++x) {
            uint32_t *word = &s->vram[(s->addr + y * s->pitch) / 4 + x];
            *word = (*word & ~mask) | packed;
        }
    }
    return true;
}

/* NV902D solid primitives use integer window coordinates. Test pixel centers
 * at twice their coordinate and apply a top/left rule on shared edges. */
static int64_t edge(int ax, int ay, int bx, int by, int x2, int y2)
{
    return (int64_t)(bx - ax) * (y2 - 2 * ay) -
           (int64_t)(by - ay) * (x2 - 2 * ax);
}

static bool solid_triangle(WxpNvColor *s, Error **errp)
{
    int a = 0, b = 1, c = 2;
    int64_t area = edge(s->px[a], s->py[a], s->px[b], s->py[b], 2*s->px[c], 2*s->py[c]);
    if (s->dst_hi || s->dst_lo >= sizeof(s->vram) || (s->dst_lo & 3) ||
        !s->dst_pitch || (s->dst_pitch & 3) || s->width > s->dst_pitch / 4 ||
        s->dst_height > (sizeof(s->vram) - s->dst_lo) / s->dst_pitch ||
        s->dst_format != A8B8G8R8 || s->solid_format != A8B8G8R8 ||
        s->layout != 1 || s->operation != 3 || s->primitive != 3) {
        error_setg(errp, "Unsupported NV902D solid triangle target or operation");
        return false;
    }
    if (!area) {
        return true;
    }
    if (area < 0) {
        b = 2; c = 1;
    }
    int order[] = {a,b,c,a};
    for (unsigned y = 0; y < s->dst_height; ++y) {
        for (unsigned x = 0; x < s->width; ++x) {
            bool covered = true;
            for (unsigned i = 0; i < 3; ++i) {
                int start = order[i], end = order[i+1];
                int dx = s->px[end] - s->px[start], dy = s->py[end] - s->py[start];
                int64_t side = edge(s->px[start], s->py[start], s->px[end], s->py[end], 2*x+1, 2*y+1);
                if (side < 0 || (!side && !(dy < 0 || (!dy && dx > 0)))) {
                    covered = false;
                    break;
                }
            }
            if (covered) {
                s->vram[(s->dst_lo + y*s->dst_pitch)/4 + x] = s->solid;
            }
        }
    }
    return true;
}

static bool solid_method(WxpNvColor *s, uint32_t value, Error **errp)
{
    switch (s->method) {
    case 0x200: s->dst_format = value; break;
    case 0x204: s->layout = value; break;
    case 0x214: s->dst_pitch = value; break;
    case 0x218: s->width = value; break;
    case 0x21c: s->dst_height = value; break;
    case 0x220: s->dst_hi = value; break;
    case 0x224: s->dst_lo = value; break;
    case 0x2ac: s->operation = value; break;
    case 0x580: s->primitive = value; break;
    case 0x584: s->solid_format = value; break;
    case 0x588: s->solid = value; break;
    case 0x600 ... 0x614: {
        unsigned index = (s->method - 0x600) / 8;
        if ((int32_t)value != (int16_t)value) {
            error_setg(errp, "Solid vertex outside signed 16-bit fixture limits");
            return false;
        }
        if (s->method & 4) {
            s->py[index] = (int16_t)value;
            if (index == 2) {
                return solid_triangle(s, errp);
            }
        } else {
            s->px[index] = (int16_t)value;
        }
        break;
    }
    default:
        error_setg(errp, "Unsupported NV902D method 0x%x", s->method);
        return false;
    }
    return true;
}

static bool write_method(WxpNvColor *s, uint32_t value, Error **errp)
{
    if (s->method == SET_OBJECT) {
        if (value != 0xb097 && value != 0xb197 && value != 0xc197 && value != 0x902d) {
            error_setg(errp, "Unsupported NVIDIA 3D class 0x%x", value);
            return false;
        }
        s->cls = value;
        return true;
    }
    if (!s->cls) {
        error_setg(errp, "Bind a 3D class before programming a target");
        return false;
    }
    if (s->cls == 0x902d) {
        return solid_method(s, value, errp);
    }
    switch (s->method) {
    case TARGET_HI: s->addr_hi = value; break;
    case TARGET_LO: s->addr = value; break;
    case TARGET_PITCH: s->pitch = value; break;
    case TARGET_HEIGHT: s->height = value; break;
    case TARGET_FORMAT: s->format = value; break;
    case TARGET_MEMORY: s->memory = value; break;
    case RECT_X: s->rect_x = value; break;
    case RECT_Y: s->rect_y = value; break;
    case CLEAR_CONTROL: s->control = value; break;
    case CLEAR_COLOR ... CLEAR_COLOR + 12:
        s->color[(s->method - CLEAR_COLOR) / 4] = value;
        break;
    case CLEAR_SURFACE:
        return color_clear(s, value, errp);
    default:
        error_setg(errp, "Unsupported 3D method 0x%x", s->method);
        return false;
    }
    return true;
}

static void push_word(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    WxpNvColor *s = WXP_NV_COLOR(obj);
    uint32_t word;
    if (!visit_type_uint32(v, name, &word, errp)) {
        return;
    }
    if (!s->count) {
        uint32_t count = (word >> 16) & 0x1fff;
        if ((word >> 29) != 1 || (word & 0xe000) || !count) {
            error_setg(errp, "Expected an incrementing method packet on subchannel zero");
            return;
        }
        s->method = (word & 0x1fff) * 4;
        s->count = count;
    } else if (write_method(s, word, errp)) {
        --s->count;
        s->method += 4;
    } else {
        s->count = 0;
    }
}

static void select_word(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    WxpNvColor *s = WXP_NV_COLOR(obj);
    uint32_t address;
    if (!visit_type_uint32(v, name, &address, errp)) {
        return;
    }
    if ((address & 3) || address >= sizeof(s->vram)) {
        error_setg(errp, "Readback address outside the color target memory");
        return;
    }
    s->read_addr = address / 4;
}

static void read_word(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    WxpNvColor *s = WXP_NV_COLOR(obj);
    visit_type_uint32(v, name, &s->vram[s->read_addr], errp);
}

static void nv_color_class_init(ObjectClass *oc, void *data)
{
    object_class_property_add(oc, "push-word", "uint32", NULL, push_word, NULL, NULL);
    object_class_property_add(oc, "read-address", "uint32", NULL, select_word, NULL, NULL);
    object_class_property_add(oc, "read-value", "uint32", read_word, NULL, NULL, NULL);
}

static const TypeInfo nv_color_type = {
    .name = TYPE_WXP_NV_COLOR,
    .parent = TYPE_OBJECT,
    .instance_size = sizeof(WxpNvColor),
    .class_init = nv_color_class_init,
    .interfaces = (InterfaceInfo[]) { { TYPE_USER_CREATABLE }, {} },
};

static void nv_color_register(void)
{
    type_register_static(&nv_color_type);
}
type_init(nv_color_register)
