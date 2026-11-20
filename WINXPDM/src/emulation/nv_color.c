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

static bool write_method(WxpNvColor *s, uint32_t value, Error **errp)
{
    if (s->method == SET_OBJECT) {
        if (value != 0xb097 && value != 0xb197 && value != 0xc197) {
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
