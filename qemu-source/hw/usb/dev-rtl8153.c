/*
 * dev-rtl8153.c - QEMU device model for a Realtek RTL8153 USB Gigabit
 *                 Ethernet adapter (USB\VID_0BDA&PID_8153).
 *
 * It emulates the Realtek r8152-family USB vendor protocol: the register
 * file reached through the GET/SET REGS vendor control request, and the
 * bulk-OUT (8-byte tx descriptor) / bulk-IN (24-byte rx descriptor,
 * 8-byte aligned) frame framing.  The shared, QEMU-independent protocol
 * core lives in rtl815x-core.h so the same model can be linked into a
 * host interop test driven by the RLTKLANT driver's own rtl815x.c layer.
 *
 * These limited protocol profiles were compared with public Linux/vendor
 * driver register definitions. RTL8150 has a separate USB1 core; newer
 * profiles use the r8152-family core. Model/driver agreement does not prove
 * independent silicon, complete PHY/firmware, or guest NDIS networking.
 *
 * Copyright (C) 2026 ages2001.
 */
#include "qemu/osdep.h"
#include "hw/usb.h"
#include "migration/vmstate.h"
#include "qemu/timer.h"
#include "desc.h"
#include "net/net.h"
#include "qemu/module.h"
#include "qemu/iov.h"
#include "qom/object.h"
#include "hw/qdev-properties.h"
#include "rtl815x-core.h"
#include "rtl8150-core.h"
#include "rtl815x-transfer.h"

#define TYPE_USB_RTL8153 "usb-rtl8153"
OBJECT_DECLARE_SIMPLE_TYPE(USBRtl8153State, USB_RTL8153)

struct USBRtl8153State {
    USBDevice   dev;
    R815xCore   core;
    R8150Core   core8150;
    NICState   *nic;
    NICConf     conf;
    bool        v2;
    bool        legacy;
    uint8_t     package_id;
    uint16_t    hw_version;
    QEMUTimer  *phy_timer;
    uint32_t    phy_generation;
    RtlUsbTxAssembly tx;
    int rxzlp;
    uint8_t halted_endpoints; /* bits: bulk IN, bulk OUT, interrupt IN */

    /* one received frame pending delivery on bulk-IN, pre-encoded */
    uint8_t     rxbuf[R815C_RX_DESC + R815C_MAX_FRAME + R815C_RX_ALIGN];
    uint32_t    rxlen;              /* encoded bytes waiting (0 = empty)     */
    uint32_t    rxptr;             /* bytes already drained                 */
};

enum { STR_MANUFACTURER = 1, STR_PRODUCT, STR_SERIAL };

static const USBDescStrings rtl_strings = {
    [STR_MANUFACTURER] = "Realtek",
    [STR_PRODUCT]      = "RTL8153 USB 10/100/1000 Gigabit Ethernet",
    [STR_SERIAL]       = "0001",
};

/* vendor-specific interface, three endpoints matching the driver:
   bulk IN ep1 (0x81), bulk OUT ep2 (0x02), interrupt IN ep3 (0x83). */
static const USBDescIface desc_iface_rtl = {
    .bInterfaceNumber   = 0,
    .bNumEndpoints      = 3,
    .bInterfaceClass    = 0xFF,
    .bInterfaceSubClass = 0xFF,
    .bInterfaceProtocol = 0x00,
    .eps = (USBDescEndpoint[]) {
        {
            .bEndpointAddress = USB_DIR_IN | 0x01,
            .bmAttributes     = USB_ENDPOINT_XFER_BULK,
            .wMaxPacketSize   = 512,
        },{
            .bEndpointAddress = USB_DIR_OUT | 0x02,
            .bmAttributes     = USB_ENDPOINT_XFER_BULK,
            .wMaxPacketSize   = 512,
        },{
            .bEndpointAddress = USB_DIR_IN | 0x03,
            .bmAttributes     = USB_ENDPOINT_XFER_INT,
            .wMaxPacketSize   = 2,
            .bInterval        = 11,
        },
    },
};

static const USBDescDevice desc_device_rtl = {
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = 0x40,
    .bNumConfigurations = 1,
    .confs = (USBDescConfig[]) {
        {
            .bNumInterfaces      = 1,
            .bConfigurationValue = 1,
            .bmAttributes        = USB_CFG_ATT_ONE | USB_CFG_ATT_SELFPOWER,
            .bMaxPower           = 0x32,
            .nif = 1,
            .ifs = &desc_iface_rtl,
        },
    },
};

static const USBDescIface desc_iface_rtl_full = {
    .bInterfaceNumber   = 0,
    .bNumEndpoints      = 3,
    .bInterfaceClass    = 0xFF,
    .bInterfaceSubClass = 0xFF,
    .bInterfaceProtocol = 0x00,
    .eps = (USBDescEndpoint[]) {
        {
            .bEndpointAddress = USB_DIR_IN | 0x01,
            .bmAttributes     = USB_ENDPOINT_XFER_BULK,
            .wMaxPacketSize   = 64,
        },{
            .bEndpointAddress = USB_DIR_OUT | 0x02,
            .bmAttributes     = USB_ENDPOINT_XFER_BULK,
            .wMaxPacketSize   = 64,
        },{
            .bEndpointAddress = USB_DIR_IN | 0x03,
            .bmAttributes     = USB_ENDPOINT_XFER_INT,
            .wMaxPacketSize   = 2,
            .bInterval        = 11,
        },
    },
};

static const USBDescDevice desc_device_rtl_full = {
    .bcdUSB             = 0x0110,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = 0x40,
    .bNumConfigurations = 1,
    .confs = (USBDescConfig[]) {
        {
            .bNumInterfaces      = 1,
            .bConfigurationValue = 1,
            .bmAttributes        = USB_CFG_ATT_ONE | USB_CFG_ATT_SELFPOWER,
            .bMaxPower           = 0x32,
            .nif = 1,
            .ifs = &desc_iface_rtl_full,
        },
    },
};

static const USBDesc desc_rtl = {
    .id = {
        .idVendor      = 0x0BDA,
        .idProduct     = 0x8153,
        .bcdDevice     = 0x3000,
        .iManufacturer = STR_MANUFACTURER,
        .iProduct      = STR_PRODUCT,
        .iSerialNumber = STR_SERIAL,
    },
    .full = &desc_device_rtl_full,
    .high = &desc_device_rtl,          /* RTL8153 is a high-speed (USB2.0+) part */
    .str  = rtl_strings,
};

/* ---- net backend ------------------------------------------------------ */

static bool rtl_can_receive(NetClientState *nc)
{
    USBRtl8153State *s = qemu_get_nic_opaque(nc);
    bool active = s->legacy ? s->core8150.link_up && s->core8150.rx_enabled :
                             r815c_carrier(&s->core) && s->core.rx_enabled;
    return active && s->rxlen == 0 && !s->rxzlp;
}

static ssize_t rtl_receive(NetClientState *nc, const uint8_t *buf, size_t size)
{
    USBRtl8153State *s = qemu_get_nic_opaque(nc);

    if (s->legacy ? !s->core8150.link_up || !s->core8150.rx_enabled :
                    !r815c_carrier(&s->core) || !s->core.rx_enabled) return -1;
    if (s->rxlen != 0 || s->rxzlp) return 0;          /* busy, back-pressure     */
    if (size == 0 || size > R815C_MAX_FRAME) return size;

    if (s->legacy) {
        s->rxlen = r8150c_rx_encode(buf, size, s->rxbuf);
    } else if (s->v2) {
        uint32_t wirelen = size + 4;
        uint32_t opts = wirelen << 17;
        memset(s->rxbuf, 0, sizeof(s->rxbuf));
        stl_le_p(s->rxbuf, opts);
        memcpy(s->rxbuf + 16, buf, size);
        s->rxlen = (16 + wirelen + 15) & ~15U;
    } else {
        s->rxlen = r815c_rx_encode(buf, (uint32_t)size, s->rxbuf);
    }
    s->rxptr = 0;
    usb_wakeup(usb_ep_get(&s->dev, USB_TOKEN_IN, 1), 0);
    return size;
}

static void rtl_link_status_changed(NetClientState *nc)
{
    USBRtl8153State *s = qemu_get_nic_opaque(nc);
    r815c_set_link(&s->core, !nc->link_down);
    r8150c_set_link(&s->core8150, !nc->link_down);
    if (nc->link_down) {
        s->rxlen=s->rxptr=s->rxzlp=0;memset(&s->tx,0,sizeof(s->tx));
        qemu_purge_queued_packets(nc);
    }
}

static void rtl_phy_link_timer(void *opaque)
{
    USBRtl8153State *s = opaque;
    r815c_tick(&s->core,300);
    if(s->nic)qemu_flush_queued_packets(qemu_get_queue(s->nic));
}

static NetClientInfo net_rtl_info = {
    .type        = NET_CLIENT_DRIVER_NIC,
    .size        = sizeof(NICState),
    .can_receive = rtl_can_receive,
    .receive     = rtl_receive,
    .link_status_changed = rtl_link_status_changed,
};

/* ---- USB plumbing ----------------------------------------------------- */

static void rtl_handle_reset(USBDevice *dev)
{
    USBRtl8153State *s = USB_RTL8153(dev);
    if(s->phy_timer)timer_del(s->phy_timer);
    s->phy_generation=0;
    r815c_reset(&s->core);
    if (s->core.version == 0x2020) s->core.reg[0xdc48] = s->package_id;
    r8150c_reset(&s->core8150);
    s->rxlen = s->rxptr = s->rxzlp = 0;
    s->halted_endpoints = 0;
    memset(&s->tx,0,sizeof(s->tx));
    if(s->nic)qemu_purge_queued_packets(qemu_get_queue(s->nic));
}

/* A MAC stop/reset invalidates partial USB transfers and queued frames. */
static void rtl_check_stopped(USBRtl8153State *s)
{
    bool active=s->legacy?s->core8150.rx_enabled&&s->core8150.tx_enabled:
                          s->core.rx_enabled&&s->core.tx_enabled;
    if(!active){
        s->rxlen=s->rxptr=s->rxzlp=0;memset(&s->tx,0,sizeof(s->tx));
        qemu_purge_queued_packets(qemu_get_queue(s->nic));
    }
}

static bool rtl_endpoint_control(USBRtl8153State *s, USBPacket *p, int request,
                                 int value, int index, int length, uint8_t *data)
{
    unsigned bit;
    if (request != (EndpointRequest | USB_REQ_GET_STATUS) &&
        request != (EndpointOutRequest | USB_REQ_CLEAR_FEATURE) &&
        request != (EndpointOutRequest | USB_REQ_SET_FEATURE)) return false;
    if ((index == 0 || index == 0x80) && !value && length == 2 &&
        request == (EndpointRequest | USB_REQ_GET_STATUS)) {
        data[0] = data[1] = 0; p->actual_length = 2; return true;
    }
    bit = index == 0x81 ? 1 : index == 0x02 ? 2 : index == 0x83 ? 4 : 0;
    if (!s->dev.configuration || !bit || value ||
        length != (request == (EndpointRequest | USB_REQ_GET_STATUS) ? 2 : 0)) {
        p->status = USB_RET_STALL;
        return true;
    }
    if (request == (EndpointRequest | USB_REQ_GET_STATUS)) {
        data[0] = !!(s->halted_endpoints & bit); data[1] = 0;
        p->actual_length = 2;
    } else if (request == (EndpointOutRequest | USB_REQ_SET_FEATURE)) {
        s->halted_endpoints |= bit;
    } else {
        s->halted_endpoints &= ~bit;
        /* CLEAR_FEATURE resets endpoint halt/toggle, not the device's
           queued network frame or its vendor-register state. */
    }
    return true;
}

static void rtl_handle_control(USBDevice *dev, USBPacket *p, int request,
                               int value, int index, int length, uint8_t *data)
{
    USBRtl8153State *s = USB_RTL8153(dev);
    if (rtl_endpoint_control(s,p,request,value,index,length,data)) return;
    int ret = usb_desc_handle_control(dev, p, request, value, index,
                                      length, data);
    if (ret >= 0) {
        if (request == (DeviceOutRequest | USB_REQ_SET_CONFIGURATION) ||
            request == (InterfaceOutRequest | USB_REQ_SET_INTERFACE)) s->halted_endpoints = 0;
        return;
    }
    if (s->legacy) {
        if ((request == 0xc005 || request == 0x4005) &&
            !r8150c_control(&s->core8150, request == 0xc005,
                            value, index, data, length)) {
            p->actual_length = length;
            if(request==0x4005)rtl_check_stopped(s);
        } else {
            p->status = USB_RET_STALL;
        }
        return;
    }

    switch (request) {
    case (R815C_RT_READ << 8) | R815C_REQ_REGS:        /* 0xC005 GET REGS */
        if (r815c_control_read(&s->core, value, index, data, length)) {
            p->status = USB_RET_STALL;
            break;
        }
        p->actual_length = length;
        break;
    case (R815C_RT_WRITE << 8) | R815C_REQ_REGS:       /* 0x4005 SET REGS */
        if (r815c_control_write(&s->core, value, index, data, length)) {
            p->status = USB_RET_STALL;
            break;
        }
        p->actual_length = length;
        rtl_check_stopped(s);
        if(s->phy_timer&&s->phy_generation!=s->core.negotiation_generation){
            s->phy_generation=s->core.negotiation_generation;
            timer_mod(s->phy_timer,qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL)+s->core.negotiation_ms);
        }
        break;
    default:
        p->status = USB_RET_STALL;
        break;
    }
}

static void rtl_handle_data(USBDevice *dev, USBPacket *p)
{
    USBRtl8153State *s = USB_RTL8153(dev);
    unsigned bit = p->pid == USB_TOKEN_IN ? (p->ep->nr == 1 ? 1 : p->ep->nr == 3 ? 4 : 0) :
                   p->pid == USB_TOKEN_OUT && p->ep->nr == 2 ? 2 : 0;
    if (s->halted_endpoints & bit) { p->status = USB_RET_STALL; return; }

    switch (p->pid) {
    case USB_TOKEN_OUT:
        if (p->ep->nr == 2) {                          /* bulk-OUT: TX     */
            uint8_t packet[RTL_USB_TRANSFER_MAX];
            const uint8_t *frame, *tmp;
            uint32_t flen=0,n=p->iov.size,complete;
            if(n>sizeof(packet)){p->status=USB_RET_STALL;memset(&s->tx,0,sizeof(s->tx));return;}
            usb_packet_copy(p,packet,n);
            if (!(s->legacy ? s->core8150.link_up && s->core8150.tx_enabled :
                              r815c_carrier(&s->core) && s->core.tx_enabled)) {
                memset(&s->tx,0,sizeof(s->tx));return;
            }
            complete=rtl_usb_tx_packet(&s->tx,s->legacy,s->v2,
                        p->ep->max_packet_size,packet,n);
            if(!complete)return;
            tmp=s->tx.bytes;n=complete;
            if(s->legacy)flen=r8150c_tx_decode(tmp,n,&frame);
            else if(s->v2){
                frame=tmp+16;
                if(n>=16&&ldl_le_p(tmp+12)==0xA8000000U&&
                   (ldl_le_p(tmp)&0xC0000000U)==0xC0000000U){
                    uint32_t desc_len=ldl_le_p(tmp+8)>>4;
                    if(desc_len&&desc_len<=n-16)flen=desc_len;
                }
            }else flen=r815c_tx_decode(tmp,n,&frame);
            if(flen)qemu_send_packet(qemu_get_queue(s->nic),frame,flen);
            return;
        }
        break;

    case USB_TOKEN_IN:
        if (p->ep->nr == 1) {                          /* bulk-IN: RX      */
            uint8_t packet[RTL_USB_TRANSFER_MAX];
            int len=rtl_usb_rx_packet(s->rxbuf,&s->rxlen,&s->rxptr,&s->rxzlp,
                        p->ep->max_packet_size,packet,MIN(p->iov.size,sizeof(packet)));
            if(len<0){p->status=USB_RET_NAK;return;}
            usb_packet_copy(p,packet,len);
            if(!s->rxlen&&!s->rxzlp)qemu_flush_queued_packets(qemu_get_queue(s->nic));
            return;
        }
        if (p->ep->nr == 3) {                          /* interrupt status */
            p->status = USB_RET_NAK;
            return;
        }
        break;

    default:
        break;
    }
    p->status = USB_RET_STALL;
}

static void rtl_realize(USBDevice *dev, Error **errp)
{
    USBRtl8153State *s = USB_RTL8153(dev);

    switch (USB_DEVICE_GET_CLASS(dev)->usb_desc->id.idProduct) {
    case 0x8150: s->legacy = true; break;
    case 0x8156: s->core.version = 0x7020; break;
    case 0x8157: s->core.version = 0x1030; break;
    case 0x8159: s->core.version = 0x2020; break;
    default: s->core.version = 0x5c10; break;
    }
    if (s->hw_version) s->core.version = s->hw_version;
    s->v2 = s->core.version == 0x1030 || s->core.version == 0x2020;
    s->phy_timer=timer_new_ms(QEMU_CLOCK_VIRTUAL,rtl_phy_link_timer,s);
    usb_desc_create_serial(dev);
    usb_desc_init(dev);

    qemu_macaddr_default_if_unset(&s->conf.macaddr);
    r815c_set_mac(&s->core, s->conf.macaddr.a);
    r815c_reset(&s->core);
    if (s->core.version == 0x2020) s->core.reg[0xdc48] = s->package_id;
    memcpy(s->core8150.mac, s->conf.macaddr.a, 6);
    r8150c_reset(&s->core8150);

    s->nic = qemu_new_nic(&net_rtl_info, &s->conf,
                          object_get_typename(OBJECT(s)), dev->qdev.id,
                          &dev->qdev.mem_reentrancy_guard, s);
    qemu_format_nic_info_str(qemu_get_queue(s->nic), s->conf.macaddr.a);
    rtl_link_status_changed(qemu_get_queue(s->nic));
    s->rxlen = s->rxptr = s->rxzlp = 0;
    memset(&s->tx,0,sizeof(s->tx));
    if(s->nic)qemu_purge_queued_packets(qemu_get_queue(s->nic));
}

static void rtl_unrealize(USBDevice *dev)
{
    USBRtl8153State *s = USB_RTL8153(dev);
    timer_del(s->phy_timer);timer_free(s->phy_timer);s->phy_timer=NULL;
    qemu_del_nic(s->nic);
}

static const VMStateDescription vmstate_usb_rtl8153 = {
    .name = "usb-rtl8153",
    .unmigratable = 1,
};

static Property rtl_properties[] = {
    DEFINE_NIC_PROPERTIES(USBRtl8153State, conf),
    DEFINE_PROP_UINT8("package-id", USBRtl8153State, package_id, 0x1e),
    DEFINE_PROP_UINT16("silicon-version", USBRtl8153State, hw_version, 0),
    DEFINE_PROP_UINT32("phy-faults", USBRtl8153State, core.phy_faults, 0),
    DEFINE_PROP_END_OF_LIST(),
};

/* Explicit test fault: a guest must clear the endpoint through USB control
   requests before data resumes. This is not a silicon revision property. */
static bool rtl_rx_halted_get(Object *obj,Error **errp)
{
    (void)errp;return !!(USB_RTL8153(obj)->halted_endpoints & 1);
}
static void rtl_rx_halted_set(Object *obj,bool value,Error **errp)
{
    USBRtl8153State *s=USB_RTL8153(obj);(void)errp;
    if(value)s->halted_endpoints|=1;else s->halted_endpoints&=~1;
    if(s->dev.attached)usb_wakeup(usb_ep_get(&s->dev,USB_TOKEN_IN,1),0);
}

static void rtl_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);

    uc->realize        = rtl_realize;
    uc->unrealize      = rtl_unrealize;
    uc->product_desc   = "Realtek RTL8153 USB Gigabit Ethernet";
    uc->usb_desc       = &desc_rtl;
    uc->handle_reset   = rtl_handle_reset;
    uc->handle_control = rtl_handle_control;
    uc->handle_data    = rtl_handle_data;
    set_bit(DEVICE_CATEGORY_NETWORK, dc->categories);
    dc->fw_name = "network";
    dc->vmsd = &vmstate_usb_rtl8153;
    device_class_set_props(dc, rtl_properties);
    object_class_property_add_bool(klass,"rx-halted",rtl_rx_halted_get,rtl_rx_halted_set);
}

static const TypeInfo rtl_info = {
    .name          = TYPE_USB_RTL8153,
    .parent        = TYPE_USB_DEVICE,
    .instance_size = sizeof(USBRtl8153State),
    .class_init    = rtl_class_init,
};

/* RTL8157 protocol test device. Implements v2 framing; the full silicon
 * PHY/firmware/power state machine is not yet modeled. */
static const USBDescStrings rtl8157_strings = {
    [STR_MANUFACTURER] = "Realtek",
    [STR_PRODUCT] = "RTL8157 USB 5G Ethernet (protocol test model)",
    [STR_SERIAL] = "0007",
};
static const USBDesc desc_rtl8157 = {
    .id = {
        .idVendor = 0x0BDA, .idProduct = 0x8157, .bcdDevice = 0x7000,
        .iManufacturer = STR_MANUFACTURER, .iProduct = STR_PRODUCT,
        .iSerialNumber = STR_SERIAL,
    },
    .full = &desc_device_rtl_full,
    .high = &desc_device_rtl,
    .str = rtl8157_strings,
};
static void rtl8157_class_init(ObjectClass *klass, void *data)
{
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);
    uc->product_desc = "Realtek RTL8157 USB 5G Ethernet protocol test model";
    uc->usb_desc = &desc_rtl8157;
}
static const TypeInfo rtl8157_info = {
    .name = "usb-rtl8157",
    .parent = TYPE_USB_RTL8153,
    .class_init = rtl8157_class_init,
};

/* Register/descriptor test profile; complete silicon behavior is not modeled. */
static const USBDescStrings rtl8156_strings = {
    [STR_MANUFACTURER] = "Realtek",
    [STR_PRODUCT] = "RTL8156 USB 2.5G Ethernet (protocol test model)",
    [STR_SERIAL] = "0006",
};
static const USBDesc desc_rtl8156 = {
    .id = {
        .idVendor = 0x0BDA, .idProduct = 0x8156, .bcdDevice = 0x6000,
        .iManufacturer = STR_MANUFACTURER, .iProduct = STR_PRODUCT,
        .iSerialNumber = STR_SERIAL,
    },
    .full = &desc_device_rtl_full,
    .high = &desc_device_rtl,
    .str = rtl8156_strings,
};
static void rtl8156_class_init(ObjectClass *klass, void *data)
{
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);
    uc->product_desc = "Realtek RTL8156 USB 2.5G Ethernet protocol test model";
    uc->usb_desc = &desc_rtl8156;
}
static const TypeInfo rtl8156_info = {
    .name = "usb-rtl8156",
    .parent = TYPE_USB_RTL8153,
    .class_init = rtl8156_class_init,
};

/* Register/descriptor test profile; complete silicon behavior is not modeled. */
static const USBDescStrings rtl8159_strings = {
    [STR_MANUFACTURER] = "Realtek",
    [STR_PRODUCT] = "RTL8159 USB 10G Ethernet (protocol test model)",
    [STR_SERIAL] = "0009",
};
static const USBDesc desc_rtl8159 = {
    .id = {
        .idVendor = 0x0BDA, .idProduct = 0x8159, .bcdDevice = 0x9000,
        .iManufacturer = STR_MANUFACTURER, .iProduct = STR_PRODUCT,
        .iSerialNumber = STR_SERIAL,
    },
    .full = &desc_device_rtl_full,
    .high = &desc_device_rtl,
    .str = rtl8159_strings,
};
static void rtl8159_class_init(ObjectClass *klass, void *data)
{
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);
    uc->product_desc = "Realtek RTL8159 USB 10G Ethernet protocol test model";
    uc->usb_desc = &desc_rtl8159;
}
static const TypeInfo rtl8159_info = {
    .name = "usb-rtl8159",
    .parent = TYPE_USB_RTL8153,
    .class_init = rtl8159_class_init,
};

/* USB1 full-speed legacy profile; no high-speed alternate descriptor. */
static const USBDescStrings rtl8150_strings = {
    [STR_MANUFACTURER] = "Realtek",
    [STR_PRODUCT] = "RTL8150 USB 10/100 Ethernet (protocol test model)",
    [STR_SERIAL] = "0000",
};
static const USBDescIface desc_iface_8150 = {
    .bInterfaceNumber = 0, .bNumEndpoints = 3,
    .bInterfaceClass = 0xff, .bInterfaceSubClass = 0xff,
    .eps = (USBDescEndpoint[]) {
        {.bEndpointAddress = 0x81, .bmAttributes = USB_ENDPOINT_XFER_BULK,
         .wMaxPacketSize = 64},
        {.bEndpointAddress = 0x02, .bmAttributes = USB_ENDPOINT_XFER_BULK,
         .wMaxPacketSize = 64},
        {.bEndpointAddress = 0x83, .bmAttributes = USB_ENDPOINT_XFER_INT,
         .wMaxPacketSize = 8, .bInterval = 100},
    },
};
static const USBDescDevice desc_device_8150 = {
    .bcdUSB = 0x0110, .bMaxPacketSize0 = 8, .bNumConfigurations = 1,
    .confs = (USBDescConfig[]) {
        {.bNumInterfaces = 1, .bConfigurationValue = 1,
         .bmAttributes = USB_CFG_ATT_ONE | USB_CFG_ATT_SELFPOWER,
         .bMaxPower = 0x32, .nif = 1, .ifs = &desc_iface_8150},
    },
};
static const USBDesc desc_rtl8150 = {
    .id = {.idVendor = 0x0bda, .idProduct = 0x8150, .bcdDevice = 0x0100,
           .iManufacturer = STR_MANUFACTURER, .iProduct = STR_PRODUCT,
           .iSerialNumber = STR_SERIAL},
    .full = &desc_device_8150, .str = rtl8150_strings,
};
static void rtl8150_class_init(ObjectClass *klass, void *data)
{
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);
    uc->product_desc = "Realtek RTL8150 USB1 Fast Ethernet protocol test model";
    uc->usb_desc = &desc_rtl8150;
}
static const TypeInfo rtl8150_info = {
    .name = "usb-rtl8150", .parent = TYPE_USB_RTL8153,
    .class_init = rtl8150_class_init,
};

#include "rtl8153-hid-test.h"

static void rtl_register_types(void)
{
    type_register_static(&rtl_info);
    type_register_static(&rtl8157_info);
    type_register_static(&rtl8156_info);
    type_register_static(&rtl8159_info);
    type_register_static(&rtl8150_info);
    type_register_static(&mixed_info);
}

type_init(rtl_register_types)
