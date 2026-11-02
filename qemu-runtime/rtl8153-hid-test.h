/* Fictional composite fixture: RTL8153 protocol interface plus a boot keyboard.
 * This is explicitly a USBNT mixed-owner reset test, not a Realtek product.
 * Included only by dev-rtl8153.c so the ordinary NIC protocol stays unchanged.
 */
#define TYPE_USB_RTL_HID_TEST "usb-rtl8153-hid-test"
OBJECT_DECLARE_SIMPLE_TYPE(USBRtlHidTestState, USB_RTL_HID_TEST)
struct USBRtlHidTestState {
    USBRtl8153State parent;
    bool key_down, key_pending, key_seen, key_halted;
    uint8_t protocol, idle, leds;
    unsigned resets;
};

static const uint8_t mixed_report[] = {
    0x05,0x01,0x09,0x06,0xa1,0x01,0x05,0x07,0x19,0xe0,0x29,0xe7,
    0x15,0x00,0x25,0x01,0x75,0x01,0x95,0x08,0x81,0x02,0x95,0x01,
    0x75,0x08,0x81,0x01,0x95,0x05,0x75,0x01,0x05,0x08,0x19,0x01,
    0x29,0x05,0x91,0x02,0x95,0x01,0x75,0x03,0x91,0x01,0x95,0x06,
    0x75,0x08,0x15,0x00,0x25,0x65,0x05,0x07,0x19,0x00,0x29,0x65,
    0x81,0x00,0xc0
};
static const USBDescIface mixed_keyboard = {
    .bInterfaceNumber = 1, .bNumEndpoints = 1,
    .bInterfaceClass = USB_CLASS_HID, .bInterfaceSubClass = 1,
    .bInterfaceProtocol = 1, .ndesc = 1,
    .descs = (USBDescOther[]) {{
        .data = (uint8_t[]) {9, 0x21, 0x11, 1, 0, 1,
                            0x22, sizeof(mixed_report), 0},
    }},
    .eps = (USBDescEndpoint[]) {{
        .bEndpointAddress = USB_DIR_IN | 4,
        .bmAttributes = USB_ENDPOINT_XFER_INT,
        .wMaxPacketSize = 8, .bInterval = 7,
    }},
};
static const USBDescStrings mixed_strings = {
    [STR_MANUFACTURER] = "USBNT Test Fixture",
    [STR_PRODUCT] = "Fictional RTL8153 protocol and HID composite reset fixture",
    [STR_SERIAL] = "MIXED-RESET-0001",
};
static USBDescIface mixed_interfaces[2][2];
static USBDescConfig mixed_configs[2];
static USBDescDevice mixed_devices[2];
static USBDesc mixed_desc;

static void mixed_reset(USBDevice *dev)
{
    USBRtlHidTestState *s = USB_RTL_HID_TEST(dev);
    rtl_handle_reset(dev);
    s->protocol = 1;
    s->idle = s->leds = 0;
    s->key_halted = false;
    s->key_pending = true;
    s->key_seen = false;
    fprintf(stderr, "usbnt-mixed-test: physical reset %u\n", ++s->resets);
}

static void mixed_control(USBDevice *dev, USBPacket *p, int request,
                          int value, int index, int length, uint8_t *data)
{
    USBRtlHidTestState *s = USB_RTL_HID_TEST(dev);
    if (index == 0x84 &&
        (request == (EndpointRequest | USB_REQ_GET_STATUS) ||
         request == (EndpointOutRequest | USB_REQ_CLEAR_FEATURE) ||
         request == (EndpointOutRequest | USB_REQ_SET_FEATURE))) {
        if (value || (request == (EndpointRequest | USB_REQ_GET_STATUS) ? length != 2 : length != 0)) {
            p->status = USB_RET_STALL;
        } else if (request == (EndpointRequest | USB_REQ_GET_STATUS)) {
            data[0] = s->key_halted; data[1] = 0; p->actual_length = 2;
        } else {
            s->key_halted = request == (EndpointOutRequest | USB_REQ_SET_FEATURE);
        }
        return;
    }
    if (index == 1) {
        if (request == (InterfaceRequest | USB_REQ_GET_DESCRIPTOR) && value == 0x2200) {
            p->actual_length = MIN(length, sizeof(mixed_report));
            memcpy(data, mixed_report, p->actual_length);
            return;
        }
        switch (request) {
        case 0xa101: /* GET_REPORT: current boot input report. */
            if (value != 0x0100 || length < 8) break;
            memset(data, 0, 8); data[2] = s->key_down ? 5 : 0;
            p->actual_length = 8; return;
        case 0x2109: /* SET_REPORT: keyboard LEDs. */
            if (value != 0x0200 || length != 1) break;
            s->leds = data[0]; p->actual_length = 1; return;
        case 0xa102: /* GET_IDLE */
            if (value || length != 1) break;
            data[0] = s->idle; p->actual_length = 1; return;
        case 0x210a: /* SET_IDLE */
            /* Only change-triggered reports are implemented by this fixture. */
            if (value || length) break;
            s->idle = value >> 8; return;
        case 0xa103: /* GET_PROTOCOL */
            if (value || length != 1) break;
            data[0] = s->protocol; p->actual_length = 1; return;
        case 0x210b: /* SET_PROTOCOL */
            if (value > 1 || length) break;
            s->protocol = value; return;
        default:
            rtl_handle_control(dev, p, request, value, index, length, data);
            return;
        }
        p->status = USB_RET_STALL;
        return;
    }
    rtl_handle_control(dev, p, request, value, index, length, data);
}

static void mixed_data(USBDevice *dev, USBPacket *p)
{
    USBRtlHidTestState *s = USB_RTL_HID_TEST(dev);
    if (p->pid == USB_TOKEN_IN && p->ep->nr == 4) {
        uint8_t report[8] = {0};
        if (s->key_halted) { p->status = USB_RET_STALL; return; }
        if (!s->key_pending) { p->status = USB_RET_NAK; return; }
        if (p->iov.size < sizeof(report)) { p->status = USB_RET_STALL; return; }
        report[2] = s->key_down ? 5 : 0; /* HID B */
        usb_packet_copy(p, report, sizeof(report));
        s->key_pending = false;
        s->key_seen = true;
        fprintf(stderr, "usbnt-mixed-test: keyboard %s delivered\n", s->key_down ? "down" : "up");
        return;
    }
    rtl_handle_data(dev, p);
}
static bool mixed_key_get(Object *obj, Error **errp)
{
    (void)errp;
    return USB_RTL_HID_TEST(obj)->key_down;
}
static bool mixed_seen_get(Object *obj, Error **errp)
{
    (void)errp;
    return USB_RTL_HID_TEST(obj)->key_seen;
}
static void mixed_key_set(Object *obj, bool value, Error **errp)
{
    USBRtlHidTestState *s = USB_RTL_HID_TEST(obj);
    USBDevice *dev = USB_DEVICE(obj);
    (void)errp;
    s->key_down = value; s->key_pending = true; s->key_seen = false;
    if (dev->attached) usb_wakeup(usb_ep_get(dev, USB_TOKEN_IN, 4), 0);
}
static const VMStateDescription mixed_vmstate = {
    .name = TYPE_USB_RTL_HID_TEST, .unmigratable = true,
};
static void mixed_class_init(ObjectClass *klass, void *data)
{
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);
    unsigned i;
    (void)data;
    for (i = 0; i < 2; i++) {
        mixed_devices[i] = i ? desc_device_rtl : desc_device_rtl_full;
        mixed_configs[i] = mixed_devices[i].confs[0];
        mixed_interfaces[i][0] = i ? desc_iface_rtl : desc_iface_rtl_full;
        mixed_interfaces[i][1] = mixed_keyboard;
        mixed_configs[i].bNumInterfaces = mixed_configs[i].nif = 2;
        mixed_configs[i].ifs = mixed_interfaces[i];
        mixed_devices[i].confs = &mixed_configs[i];
    }
    mixed_desc = desc_rtl;
    mixed_desc.full = &mixed_devices[0]; mixed_desc.high = &mixed_devices[1];
    mixed_desc.str = mixed_strings;
    uc->usb_desc = &mixed_desc;
    uc->product_desc = "USBNT fictional mixed-owner reset test fixture";
    uc->handle_reset = mixed_reset;
    uc->handle_control = mixed_control;
    uc->handle_data = mixed_data;
    object_class_property_add_bool(klass, "test-key-down", mixed_key_get, mixed_key_set);
    object_class_property_add_bool(klass, "test-key-seen", mixed_seen_get, NULL);
    /* Test-only state is intentionally not migratable. */
    DEVICE_CLASS(klass)->vmsd = &mixed_vmstate;
}
static const TypeInfo mixed_info = {
    .name = TYPE_USB_RTL_HID_TEST, .parent = TYPE_USB_RTL8153,
    .instance_size = sizeof(USBRtlHidTestState), .class_init = mixed_class_init,
};
