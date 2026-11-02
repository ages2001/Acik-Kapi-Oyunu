from pathlib import Path
r=Path('/workspace/rltklant-work/RLTKLANT')
p=r/'USBNT/src/rtl815x.h';s=p.read_text().replace('    BOOLEAN             Running;', '    USHORT              HwVersion;\n    USHORT              MaxSpeedMbit;\n    UCHAR               Version;\n    BOOLEAN             DescriptorV2;\n    BOOLEAN             Running;');s=s.replace('NDIS_STATUS RtlUsbReset (', 'NDIS_STATUS RtlUsbDetectVersion(PRTL_USB_NIC n);\nNDIS_STATUS RtlUsbReset (');p.write_text(s)
p=r/'USBNT/src/rtl815x.c';s=p.read_text();at=s.index('static NDIS_STATUS WriteByte');s=s[:at]+'''/* PID is not a silicon revision: newer chips can retain an older USB ID.
 * Signatures are PLA_TCR1 & 0x7cf0 (Linux rtl8152_get_version).
 * Unknown revisions must not silently use the legacy descriptor format. */
NDIS_STATUS RtlUsbDetectVersion(PRTL_USB_NIC n)
{
    static const struct { USHORT Signature, Speed; UCHAR Version; } versions[] = {
        {0x4c00,100,1}, {0x4c10,100,2},
        {0x5c00,1000,3}, {0x5c10,1000,4}, {0x5c20,1000,5}, {0x5c30,1000,6},
        {0x4800,100,7}, {0x6000,1000,8}, {0x6010,1000,9},
        {0x7020,2500,10}, {0x7030,2500,11}, {0x7400,1000,12},
        {0x7410,1000,13}, {0x6400,2500,14}, {0x7420,1000,15},
        {0x1030,5000,16}, {0x2020,10000,17}
    };
    UCHAR data[4];
    ULONG i;
    NDIS_STATUS st = RtlUsbReadRegs(n, RTLU_MCU_TYPE_PLA, 0xE610, data, 4);
    if (!USB_OK(st)) return st;
    n->HwVersion = (USHORT)((get32le(data) >> 16) & 0x7cf0);
    n->Version = 0;
    n->DescriptorV2 = FALSE;
    n->MaxSpeedMbit = 0;
    for (i = 0; i < sizeof(versions)/sizeof(versions[0]); i++) {
        if (n->HwVersion == versions[i].Signature) {
            n->Version = versions[i].Version;
            n->MaxSpeedMbit = versions[i].Speed;
            n->DescriptorV2 = n->Version >= 16;
            return NDIS_STATUS_SUCCESS;
        }
    }
    return NDIS_STATUS_ADAPTER_NOT_FOUND;
}

'''+s[at:];p.write_text(s)
p=r/'USBNT/src/usbnic.c';s=p.read_text().replace('    st = RtlUsbReset(&u->Nic);', '    st = RtlUsbDetectVersion(&u->Nic);\n    if (!USB_OK(st)) return st;\n    st = RtlUsbReset(&u->Nic);').replace('u->Nic.Chip->DeviceId == 0x8157','u->Nic.DescriptorV2');p.write_text(s)
p=r/'qemu/hw-usb/rtl815x-core.h';s=p.read_text().replace('    uint8_t  mac[6];', '    uint8_t  mac[6];\n    uint16_t version;');s=s.replace('    memcpy(&c->reg[R815C_PLA_IDR], c->mac, 6);\n    c->rx_enabled', '    memcpy(&c->reg[R815C_PLA_IDR], c->mac, 6);\n    if (!c->version) c->version = 0x5c10;\n    c->reg[0xe612] = c->version;\n    c->reg[0xe613] = c->version >> 8;\n    c->rx_enabled',1);p.write_text(s)
p=r/'qemu/hw-usb/dev-rtl8153.c';s=p.read_text().replace('    usb_desc_create_serial(dev);', '    s->core.version = s->v2 ? 0x1030 : 0x5c10;\n    usb_desc_create_serial(dev);');p.write_text(s)
p=r/'USBNT/test/test_usbnic.c';s=p.read_text().replace('static int stuckReset;', 'static USHORT hwVersion = 0x5c10;\nstatic int stuckReset;').replace('            if (off == (RTLU_PLA_CR & ~3)', '''            if (off == 0xE610) {
                memset(buffer, 0, length);
                if (length == 4) {
                    ((PUCHAR)buffer)[2] = (UCHAR)hwVersion;
                    ((PUCHAR)buffer)[3] = (UCHAR)(hwVersion >> 8);
                }
            } else if (off == (RTLU_PLA_CR & ~3)''')
# add revision regression just before final summary
pos=s.rfind('    printf(')
s=s[:pos]+'''    {
        USBNIC u;
        hwVersion = 0x1030;
        CHECK(USB_OK(UsbNicAttach(&u, OS(4,0), UsbHcEhci, 0x0BDA, 0x8156,
                                 (PVOID)1, mockSubmit)) && u.Nic.DescriptorV2 &&
              u.Nic.MaxSpeedMbit == 5000,
              "8157 silicon behind PID 8156 selects v2 descriptors");
        hwVersion = 0x2020;
        CHECK(USB_OK(UsbNicAttach(&u, OS(4,0), UsbHcEhci, 0x0BDA, 0x8159,
                                 (PVOID)1, mockSubmit)) && u.Nic.DescriptorV2 &&
              u.Nic.MaxSpeedMbit == 10000,
              "8159 silicon selects v2 descriptors and 10G ceiling");
        hwVersion = 0x7cf0;
        CHECK(!USB_OK(UsbNicAttach(&u, OS(4,0), UsbHcEhci, 0x0BDA, 0x8153,
                                  (PVOID)1, mockSubmit)) && !u.Nic.Running,
              "unknown silicon revision rejected before enabling traffic");
        hwVersion = 0x5c10;
    }
'''+s[pos:];p.write_text(s)
p=r/'USBNT/test/test_plug.c';s=p.read_text().replace('        memcpy(buffer, regReturn, length);','''    {
        USHORT off = (USHORT)(setup[2] | (setup[3] << 8));
        memcpy(buffer, regReturn, length);
        if (off == 0xE610 && length == 4) {
            ((PUCHAR)buffer)[2] = 0x30;
            ((PUCHAR)buffer)[3] = 0x70;
        }
    }''');p.write_text(s)
