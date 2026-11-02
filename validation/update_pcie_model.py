from pathlib import Path
p=Path('/workspace/rltklant-work/RLTKLANT/qemu/hw-net/rtl8169.c')
s=p.read_text().replace('#include "hw/pci/pci_device.h"','#include "hw/pci/pci_device.h"\n#include "hw/pci/pcie.h"\n#include "qapi/error.h"\n#include <zlib.h>')
s=s.replace('#define PHYS_5000M   0x800','#define PHYS_5000M   0x1000').replace('#define PHYS_10000M  0x1000','#define PHYS_10000M  0x4000')
s=s.replace('    uint16_t phy[32];','    uint16_t phy[32];\n    uint32_t phyocp;\n    uint16_t phy_ocp[32768];\n    uint8_t multicast[8];')
s=s.replace('    uint8_t buf[2048];','    uint8_t buf[16384];')
s=s.replace('        pci_dma_read(d, dptr, desc, 16);','        if (pci_dma_read(d, dptr, desc, 16) != MEMTX_OK) break;')
s=s.replace('        if (len > sizeof(buf)) len = sizeof(buf);\n        pci_dma_read(d, bufaddr, buf, len);', '''        /* Only complete, non-offloaded frames are supported by this model. */
        if (!len || len > sizeof(buf) ||
            (opts1 & (DESC_FS | DESC_LS)) != (DESC_FS | DESC_LS) ||
            pci_dma_read(d, bufaddr, buf, len) != MEMTX_OK) {
            break;
        }''')
s=s.replace('    uint8_t crc[4] = {0,0,0,0};','    uint32_t crc;')
s=s.replace('    pci_dma_read(d, dptr, desc, 16);','    if (pci_dma_read(d, dptr, desc, 16) != MEMTX_OK) return size;')
s=s.replace('    if (len > 2044) len = 2044;\n    pci_dma_write(d, bufaddr, buf, len);\n    pci_dma_write(d, bufaddr + len, crc, 4);   /* fake CRC the driver strips */', '''    if (len + 4 > (opts1 & DESC_LEN) || len + 4 > s->rms ||
        len + 4 > DESC_LEN) {
        s->isr |= INT_RDU;
        rtl8169_update_irq(s);
        return size;
    }
    crc = cpu_to_le32(crc32(0, buf, len));
    if (pci_dma_write(d, bufaddr, buf, len) != MEMTX_OK ||
        pci_dma_write(d, bufaddr + len, &crc, 4) != MEMTX_OK) return size;''')
s=s.replace('    s->phystatus = s->phystatus;   /* link stays up */','''    s->tnpds_lo = s->tnpds_hi = s->rdsar_lo = s->rdsar_hi = 0;
    s->rms = 0;
    s->phyocp = 0;
    memset(s->multicast, 0, sizeof(s->multicast));''')
s=s.replace('    switch (addr) {\n    case R_IDR0:', '''    if (addr < 6) {
        for (unsigned i = 0; i < size && addr + i < 6; i++)
            s->mac[addr + i] = val >> (i * 8);
        return;
    }
    if (addr >= R_MAR0 && addr + size <= R_MAR0 + 8) {
        for (unsigned i = 0; i < size; i++)
            s->multicast[addr - R_MAR0 + i] = val >> (i * 8);
        return;
    }
    switch (addr) {
    case R_IDR0:''',1)
s=s.replace('    case R_TCR: s->tcr = val; return;', '    case R_TCR: s->tcr = (val & ~0x7cf00000u) | (s->xid << 20); return;')
s=s.replace('    case R_PHYAR: rtl8169_phyar_write(s, val); return;', '''    case R_PHYAR:
        if (!s->is8125) rtl8169_phyar_write(s, val);
        return;
    case 0xB8:
        if (s->is8125) {
            unsigned index = (val >> 16) & 0x7fff;
            if (val & PHYAR_FLAG) {
                s->phy_ocp[index] = val;
                s->phyocp = val & ~PHYAR_FLAG;
            } else {
                s->phyocp = PHYAR_FLAG | (index << 16) | s->phy_ocp[index];
            }
        }
        return;''')
s=s.replace('case R_TPPOLL8125: if (val & TPPOLL_NPQ) rtl8169_tx(s); return;', 'case R_TPPOLL8125: if (size == 4 && (val & 1)) rtl8169_tx(s); return;')
s=s.replace('    RTL8169State *s = opaque;\n\n    switch (addr) {','''    RTL8169State *s = opaque;
    uint32_t result = 0;
    if (addr < 6) {
        for (unsigned i = 0; i < size && addr + i < 6; i++)
            result |= (uint32_t)s->mac[addr + i] << (i * 8);
        return result;
    }
    if (addr >= R_MAR0 && addr + size <= R_MAR0 + 8) {
        for (unsigned i = 0; i < size; i++)
            result |= (uint32_t)s->multicast[addr - R_MAR0 + i] << (i * 8);
        return result;
    }
    switch (addr) {''',1)
s=s.replace('    case R_PHYSTATUS: return s->phystatus & 0xFF;', '    case R_PHYSTATUS: return size >= 2 ? s->phystatus : s->phystatus & 0xFF;\n    case 0xB8: return s->phyocp;\n    case R_TNPDS: return s->tnpds_lo;\n    case R_TNPDS+4: return s->tnpds_hi;\n    case R_RDSAR: return s->rdsar_lo;\n    case R_RDSAR+4: return s->rdsar_hi;')
s=s.replace('    if (addr == R_PHYSTATUS && size == 2) return s->phystatus;\n','')
s=s.replace('static NetClientInfo net_rtl8169_info = {','''static void rtl8169_link_changed(NetClientState *nc)
{
    RTL8169State *s = qemu_get_nic_opaque(nc);
    if (nc->link_down) s->phystatus &= ~PHYS_LINKSTS;
    else s->phystatus |= PHYS_LINKSTS;
    s->isr |= INT_LINKCHG;
    rtl8169_update_irq(s);
}

static NetClientInfo net_rtl8169_info = {''')
s=s.replace('    .receive = rtl8169_receive,','    .receive = rtl8169_receive,\n    .link_status_changed = rtl8169_link_changed,')
s=s.replace('    rtl8169_set_variant(s);','''    if (s->variant > VAR_8127) {
        error_setg(errp, "Unsupported Realtek variant %u", s->variant);
        return;
    }
    rtl8169_set_variant(s);
    if (s->variant != VAR_8169) {
        if (pcie_endpoint_cap_init(dev, 0x80) < 0) {
            error_setg(errp, "Cannot initialize Realtek PCIe capability");
            return;
        }
    }''')
s=s.replace('static const VMStateDescription vmstate_rtl8169 = {','''static void pci_rtl8169_exit(PCIDevice *dev)
{
    RTL8169State *s = RTL8169(dev);
    pci_set_irq(dev, 0);
    if (s->variant != VAR_8169) pcie_cap_exit(dev);
    qemu_del_nic(s->nic);
}

static const VMStateDescription vmstate_rtl8169 = {''')
s=s.replace('    .version_id = 1,','    .unmigratable = true,\n    .version_id = 1,')
s=s.replace('    k->realize = pci_rtl8169_realize;','    k->realize = pci_rtl8169_realize;\n    k->exit = pci_rtl8169_exit;')
s=s.replace('        { INTERFACE_CONVENTIONAL_PCI_DEVICE },','        { INTERFACE_CONVENTIONAL_PCI_DEVICE },\n        { INTERFACE_PCIE_DEVICE },')
s=s.replace('static void rtl8169_register_types(void)','''static void rtl8127_realize(PCIDevice *dev, Error **errp)
{
    RTL8169(dev)->variant = VAR_8127;
    pci_rtl8169_realize(dev, errp);
}

static void rtl8127_class_init(ObjectClass *klass, void *data)
{
    PCI_DEVICE_CLASS(klass)->realize = rtl8127_realize;
    PCI_DEVICE_CLASS(klass)->device_id = 0x8127;
}

static const TypeInfo rtl8127_info = {
    .name = "rtl8127",
    .parent = TYPE_RTL8169,
    .class_init = rtl8127_class_init,
};

static void rtl8169_register_types(void)''').replace('    type_register_static(&rtl8169_info);','    type_register_static(&rtl8169_info);\n    type_register_static(&rtl8127_info);')
p.write_text(s)
p=Path('/workspace/rltklant-work/RLTKLANT/src/rltkhw.h');s=p.read_text().replace('RCP_PHYS_5000M      0x800','RCP_PHYS_5000M      0x1000').replace('RCP_PHYS_10000M     0x1000','RCP_PHYS_10000M     0x4000');p.write_text(s)
p=Path('/workspace/rltklant-work/RLTKLANT/src/backend_cplus.c');s=p.read_text().replace('RtlWrite16(a, RCP_TPPOLL_8125, RCP_TPPOLL_NPQ)','RtlWrite32(a, RCP_TPPOLL_8125, 1)');p.write_text(s)
