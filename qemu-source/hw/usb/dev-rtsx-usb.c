/*
 * Realtek USB card reader with the vendor protocol (RTS5129 "USB2.0-CRW"
 * 0BDA:0129, RTS5139, RTS5179) - test model for the USBNT rtsxusb driver.
 *
 *   -device usb-rtsx,id=crw[,pid=0x0129][,lqfp=on]
 *   -drive if=none,id=c,file=card.img,format=raw -device sd-card,drive=c,bus=crw.0
 *
 * Bulk OUT 2 takes "RTCR" packets: batch register commands (read, write,
 * check; the reads and checks answer on bulk IN 1), sequential register
 * reads / writes (the ping-pong buffer).  A batch with a data stage moves
 * ring buffer data on the bulk pipes; the batch continues (and answers)
 * after the data.  Control requests (vendor, 0) write single registers.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 */

#include "qemu/osdep.h"
#include "qemu/module.h"
#include "qapi/error.h"
#include "hw/qdev-properties.h"
#include "hw/usb.h"
#include "desc.h"
#include "qom/object.h"
#include "rtsx-engine.h"

#define TYPE_USB_RTSX "usb-rtsx"
OBJECT_DECLARE_SIMPLE_TYPE(USBRtsxState, USB_RTSX)

#define R(a)            (0xC000 | ((a) & 0x3FFF))
#define U_CARD_EXIST    0xFD6F
#define U_OCPSTAT       0xFD83
#define U_HW_VERSION    0xFC01
#define U_CFG_MODE      0xFC0E
#define U_CFG_MODE_1    0xFC0F
#define U_CARD_SHARE    0xFD51
#define U_DATA_SOURCE   0xFD5D
#define U_CARD_STOP     0xFD53
#define U_MC_DMA_TC0    0xFF11

enum { ST_IDLE, ST_DATA_IN, ST_DATA_OUT };

struct USBRtsxState {
    USBDevice dev;
    RtsxEngine e;
    SDBus sdbus;
    uint16_t pid;
    bool lqfp;
    /* current batch */
    uint8_t cmd[1024];
    uint32_t ncmd, next;
    uint8_t flag;
    int state;
    uint32_t data_len, data_pos;
    uint8_t *wbuf;
    /* answer */
    uint8_t rsp[1024];
    uint32_t rsp_len;
    bool rsp_ready;
    uint32_t batches, bad;
    /* command packet being received (full speed: several 64 byte packets) */
    uint8_t obuf[1100];
    uint32_t olen;
};

enum { STR_MANUFACTURER = 1, STR_PRODUCT, STR_SERIAL };

static const USBDescStrings desc_strings = {
    [STR_MANUFACTURER] = "Generic",
    [STR_PRODUCT]      = "USB2.0-CRW",
    [STR_SERIAL]       = "20071114173400000",
};

static const USBDescIface desc_iface_full = {
    .bInterfaceNumber = 0, .bNumEndpoints = 3,
    .bInterfaceClass = 0xFF, .bInterfaceSubClass = 0x06, .bInterfaceProtocol = 0x50,
    .eps = (USBDescEndpoint[]) {
        { .bEndpointAddress = USB_DIR_IN | 0x01, .bmAttributes = USB_ENDPOINT_XFER_BULK, .wMaxPacketSize = 64 },
        { .bEndpointAddress = USB_DIR_OUT | 0x02, .bmAttributes = USB_ENDPOINT_XFER_BULK, .wMaxPacketSize = 64 },
        { .bEndpointAddress = USB_DIR_IN | 0x03, .bmAttributes = USB_ENDPOINT_XFER_INT, .wMaxPacketSize = 2,
          .bInterval = 10 },
    }
};

static const USBDescIface desc_iface_high = {
    .bInterfaceNumber = 0, .bNumEndpoints = 3,
    .bInterfaceClass = 0xFF, .bInterfaceSubClass = 0x06, .bInterfaceProtocol = 0x50,
    .eps = (USBDescEndpoint[]) {
        { .bEndpointAddress = USB_DIR_IN | 0x01, .bmAttributes = USB_ENDPOINT_XFER_BULK, .wMaxPacketSize = 512 },
        { .bEndpointAddress = USB_DIR_OUT | 0x02, .bmAttributes = USB_ENDPOINT_XFER_BULK, .wMaxPacketSize = 512 },
        { .bEndpointAddress = USB_DIR_IN | 0x03, .bmAttributes = USB_ENDPOINT_XFER_INT, .wMaxPacketSize = 2,
          .bInterval = 7 },
    }
};

static const USBDescDevice desc_device_full = {
    .bcdUSB = 0x0200, .bMaxPacketSize0 = 64, .bNumConfigurations = 1,
    .confs = (USBDescConfig[]) {
        { .bNumInterfaces = 1, .bConfigurationValue = 1, .bmAttributes = USB_CFG_ATT_ONE, .bMaxPower = 250,
          .nif = 1, .ifs = &desc_iface_full },
    },
};

static const USBDescDevice desc_device_high = {
    .bcdUSB = 0x0200, .bMaxPacketSize0 = 64, .bNumConfigurations = 1,
    .confs = (USBDescConfig[]) {
        { .bNumInterfaces = 1, .bConfigurationValue = 1, .bmAttributes = USB_CFG_ATT_ONE, .bMaxPower = 250,
          .nif = 1, .ifs = &desc_iface_high },
    },
};

static USBDesc desc_rtsx = {
    .id = {
        .idVendor = 0x0BDA, .idProduct = 0x0129, .bcdDevice = 0x3960,
        .iManufacturer = STR_MANUFACTURER, .iProduct = STR_PRODUCT, .iSerialNumber = STR_SERIAL,
    },
    .full = &desc_device_full,
    .high = &desc_device_high,
    .str = desc_strings,
};

static void rtsx_live_regs(USBRtsxState *s)
{
    RtsxEngine *e = &s->e;
    bool in = sdbus_get_inserted(&s->sdbus);

    e->regs[R(U_CARD_EXIST)] = (in ? 0x04 : 0) | (in && sdbus_get_readonly(&s->sdbus) ? 0x20 : 0);
    e->regs[R(U_OCPSTAT)] = 0;
}

static void rsp_put(USBRtsxState *s, uint8_t v)
{
    if (s->rsp_len < sizeof(s->rsp)) {
        s->rsp[s->rsp_len++] = v;
    }
}

/* runs the batch from s->next; stops while the engine waits for data */
static void run_batch(USBRtsxState *s)
{
    while (s->next < s->ncmd) {
        uint8_t *c = s->cmd + 8 + s->next * 4;
        int type = c[0] >> 6;
        uint16_t reg = R(((c[0] & 0x3F) << 8) | c[1]);
        uint8_t mask = c[2], data = c[3];

        if (s->e.pending) {
            return;
        }
        rtsx_live_regs(s);
        switch (type) {
        case 0:
            rsp_put(s, rtsx_reg_read(&s->e, reg));
            break;
        case 1:
            rtsx_reg_write(&s->e, reg, mask, data);
            if (s->e.pending == 1 && (s->flag & 0x02)) {
                s->state = ST_DATA_IN;
                s->data_len = s->e.ring_len;
                s->data_pos = 0;
            } else if (s->e.pending == 2 && (s->flag & 0x04)) {
                s->state = ST_DATA_OUT;
                s->data_len = s->e.pend_bytes;
                s->data_pos = 0;
            }
            break;
        case 2:
            if ((rtsx_reg_read(&s->e, reg) & mask) != (data & mask)) {
                s->bad++;
            }
            rsp_put(s, rtsx_reg_read(&s->e, reg));
            break;
        default:
            break;
        }
        s->next++;
    }
    if (s->flag & 0x01) {
        s->rsp_ready = true;
    }
    usb_wakeup(usb_ep_get(&s->dev, USB_TOKEN_IN, 1), 0);
}

static void rtsx_packet(USBRtsxState *s, uint8_t *p, uint32_t len)
{
    uint32_t n, addr, i;

    if (len < 8 || memcmp(p, "RTCR", 4) != 0) {
        s->bad++;
        return;
    }
    s->rsp_len = 0;
    s->rsp_ready = false;
    switch (p[4]) {
    case 0:                                         /* batch */
        n = (p[5] << 8) | p[6];
        if (8 + n * 4 > len) {
            n = (len - 8) / 4;
        }
        memcpy(s->cmd, p, MIN(len, sizeof(s->cmd)));
        s->ncmd = MIN(n, (sizeof(s->cmd) - 8) / 4);
        s->next = 0;
        s->flag = p[7];
        s->state = ST_IDLE;
        s->batches++;
        run_batch(s);
        break;
    case 1:                                         /* sequential read */
        n = (p[5] << 8) | p[6];
        addr = (p[8] << 8) | p[9];
        rtsx_live_regs(s);
        for (i = 0; i < n && i < sizeof(s->rsp); i++) {
            s->rsp[i] = s->e.regs[R(addr + i)];
        }
        s->rsp_len = n & ~3u;
        s->rsp_ready = true;
        usb_wakeup(usb_ep_get(&s->dev, USB_TOKEN_IN, 1), 0);
        break;
    case 2:                                         /* sequential write */
        n = (p[5] << 8) | p[6];
        addr = (p[8] << 8) | p[9];
        for (i = 0; i < n && 12 + i < len; i++) {
            s->e.regs[R(addr + i)] = p[12 + i];
        }
        break;
    default:
        s->bad++;
        break;
    }
}

static void rtsx_handle_data(USBDevice *dev, USBPacket *p)
{
    USBRtsxState *s = USB_RTSX(dev);
    uint8_t buf[1024];
    size_t n;

    if (p->pid == USB_TOKEN_OUT && p->ep->nr == 2) {
        if (s->state == ST_DATA_OUT) {
            n = MIN(p->iov.size, s->data_len - s->data_pos);
            usb_packet_copy(p, s->wbuf + s->data_pos, n);
            s->data_pos += n;
            if (s->data_pos >= s->data_len) {
                s->state = ST_IDLE;
                rtsx_dma_write(&s->e, s->wbuf, s->data_len);
                run_batch(s);
            }
            return;
        }
        n = MIN(p->iov.size, sizeof(s->obuf) - s->olen);
        usb_packet_copy(p, s->obuf + s->olen, n);
        s->olen += n;
        if (s->olen >= 8) {
            uint32_t need;
            uint32_t cnt = (s->obuf[5] << 8) | s->obuf[6];
            if (memcmp(s->obuf, "RTCR", 4) != 0) {
                s->bad++;
                s->olen = 0;
                return;
            }
            need = s->obuf[4] == 0 ? 8 + cnt * 4 : s->obuf[4] == 2 ? ((12 + cnt + 3) & ~3u) : 12;
            if (need > sizeof(s->obuf)) {
                need = sizeof(s->obuf);
            }
            if (s->olen >= need || n < p->ep->max_packet_size) {
                uint32_t l = s->olen;
                s->olen = 0;
                rtsx_packet(s, s->obuf, l);
            }
        }
        return;
    }
    if (p->pid == USB_TOKEN_IN && p->ep->nr == 1) {
        if (s->state == ST_DATA_IN) {
            n = MIN(p->iov.size, s->data_len - s->data_pos);
            usb_packet_copy(p, s->e.ring + s->data_pos, n);
            s->data_pos += n;
            if (s->data_pos >= s->data_len) {
                s->state = ST_IDLE;
                rtsx_dma_read_done(&s->e);
                run_batch(s);
            }
            return;
        }
        if (s->rsp_ready) {
            n = MIN(p->iov.size, sizeof(s->rsp));
            memset(buf, 0, sizeof(buf));
            memcpy(buf, s->rsp, MIN(n, s->rsp_len));
            usb_packet_copy(p, buf, n);
            s->rsp_ready = false;
            return;
        }
        p->status = USB_RET_NAK;
        return;
    }
    if (p->pid == USB_TOKEN_IN && p->ep->nr == 3) {
        p->status = USB_RET_NAK;
        return;
    }
    p->status = USB_RET_STALL;
}

static void rtsx_handle_control(USBDevice *dev, USBPacket *p, int request, int value, int index, int length,
                                uint8_t *data)
{
    USBRtsxState *s = USB_RTSX(dev);
    int ret = usb_desc_handle_control(dev, p, request, value, index, length, data);

    if (ret >= 0) {
        return;
    }
    if (request == ((USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE) << 8 | 0x00)) {
        uint16_t a = ((value & 0xFF) << 8) | (value >> 8);
        rtsx_reg_write(&s->e, R(a), index & 0xFF, index >> 8);
        p->actual_length = 0;
        return;
    }
    if (request == ((USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE) << 8 | 0x00) && length >= 1) {
        uint16_t a = ((value & 0xFF) << 8) | (value >> 8);
        rtsx_live_regs(s);
        data[0] = s->e.regs[R(a)];
        p->actual_length = 1;
        return;
    }
    p->status = USB_RET_STALL;
}

static void rtsx_handle_reset(USBDevice *dev)
{
    USBRtsxState *s = USB_RTSX(dev);

    rtsx_engine_init(&s->e, &s->sdbus, R(U_DATA_SOURCE), R(U_CARD_STOP));
    s->e.regs[R(U_HW_VERSION)] = 0x01;
    s->e.regs[R(U_CFG_MODE)] = 0x00;
    s->e.regs[R(U_CFG_MODE_1)] = s->pid == 0x0140 ? 0x02 : 0x00;
    s->e.regs[R(U_CARD_SHARE)] = s->lqfp ? 0x04 : 0x00;
    s->state = ST_IDLE;
    s->rsp_ready = false;
    s->ncmd = s->next = 0;
    s->olen = 0;
}

static void rtsx_realize(USBDevice *dev, Error **errp)
{
    USBRtsxState *s = USB_RTSX(dev);

    desc_rtsx.id.idProduct = s->pid;
    usb_desc_create_serial(dev);
    usb_desc_init(dev);
    qbus_init(&s->sdbus, sizeof(s->sdbus), TYPE_SD_BUS, DEVICE(dev), NULL);
    s->wbuf = g_malloc0(RX_RING_MAX);
    rtsx_handle_reset(dev);
}

static Property rtsx_props[] = {
    DEFINE_PROP_UINT16("pid", USBRtsxState, pid, 0x0129),
    DEFINE_PROP_BOOL("lqfp", USBRtsxState, lqfp, false),
    DEFINE_PROP_UINT32("batches", USBRtsxState, batches, 0),
    DEFINE_PROP_UINT32("bad", USBRtsxState, bad, 0),
    DEFINE_PROP_END_OF_LIST(),
};

static void rtsx_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);

    uc->realize = rtsx_realize;
    uc->product_desc = "Realtek USB card reader (test)";
    uc->usb_desc = &desc_rtsx;
    uc->handle_reset = rtsx_handle_reset;
    uc->handle_control = rtsx_handle_control;
    uc->handle_data = rtsx_handle_data;
    dc->desc = "Realtek RTS5129 USB card reader (test model)";
    device_class_set_props(dc, rtsx_props);
    set_bit(DEVICE_CATEGORY_STORAGE, dc->categories);
}

static const TypeInfo rtsx_info = {
    .name = TYPE_USB_RTSX,
    .parent = TYPE_USB_DEVICE,
    .instance_size = sizeof(USBRtsxState),
    .class_init = rtsx_class_init,
};

static void rtsx_register(void)
{
    type_register_static(&rtsx_info);
}

type_init(rtsx_register)
