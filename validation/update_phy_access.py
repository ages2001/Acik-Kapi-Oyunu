from pathlib import Path
root=Path('/workspace/rltklant-work/RLTKLANT/src')
p=root/'rtlchip.h';s=p.read_text().replace('No Realtek or Microsoft source is used.','Register semantics are also checked against the Realtek r8127 Linux\n * driver; see validation source records for provenance.')
s=s.replace('#define RTL_F_FASTET', '#define RTL_F_PHY_OCP           0x0020  /* PHY uses the PHYOCP window       */\n#define RTL_F_PHY_DP            0x0040  /* RTL8168DP PHY access gate        */\n#define RTL_F_FASTET')
# The Linux switch uses MAC versions 40 and later for PHY OCP access.
new=[]
for line in s.splitlines():
 if any(f'0x{x:03x},' in line for x in (0x4c0,0x509,0x5c8,0x541,0x6c0,0x502,0x54b,0x54a,0x609,0x641,0x688,0x689,0x68a,0x68b,0x708,0x681,0x649,0x64a,0x6c9)) and 'RTL_SPEED_' in line:
  line=line.replace('RTL_F_EEE','RTL_F_EEE|RTL_F_PHY_OCP')
 if any(f'0x{x:03x},' in line for x in (0x28a,0x28b)) and 'RTL_SPEED_' in line:
  line=line.replace('RTL_SPEED_1000, 0,','RTL_SPEED_1000, RTL_F_PHY_DP,')
 new.append(line)
s='\n'.join(new)+'\n'
s=s.replace('    /* RTL8168 / 8111 (PCIe Gigabit) */','    { 0x7c8, 0x488, RTL_SPEED_1000, RTL_F_EEE, "Realtek RTL8411 PCIe Gigabit Ethernet" },\n\n    /* RTL8168 / 8111 (PCIe Gigabit) */')
p.write_text(s)
p=root/'rltklant.h';s=p.read_text().replace('    ULONG           PciSlot;', '    USHORT          PhyPage;\n    ULONG           PciSlot;')
s=s.replace('VOID   RtlConfigurePhy', 'USHORT RtlCpPhyOcpRead(PRLTK_ADAPTER a, USHORT address);\nBOOLEAN RtlCpPhyOcpWrite(PRLTK_ADAPTER a, USHORT address, USHORT value);\nVOID   RtlConfigurePhy');p.write_text(s)
p=root/'backend_cplus.c';s=p.read_text();pos=s.index('USHORT RtlCpMdioRead')
s=s[:pos]+'''/* PHYOCP uses word addresses, unlike the legacy PHYAR register selector. */
USHORT RtlCpPhyOcpRead(PRLTK_ADAPTER a, USHORT address)
{
    UINT i;
    if (address & 1) return 0xFFFF;
    RtlWrite32(a, 0xB8, (ULONG)(address >> 1) << 16);
    for (i = 0; i < 2000; i++) {
        ULONG v = RtlRead32(a, 0xB8);
        if (v & 0x80000000UL) return (USHORT)v;
        NdisStallExecution(20);
    }
    return 0xFFFF;
}
BOOLEAN RtlCpPhyOcpWrite(PRLTK_ADAPTER a, USHORT address, USHORT value)
{
    UINT i;
    if (address & 1) return FALSE;
    RtlWrite32(a, 0xB8, 0x80000000UL | ((ULONG)(address >> 1) << 16) | value);
    for (i = 0; i < 2000; i++) {
        if (!(RtlRead32(a, 0xB8) & 0x80000000UL)) return TRUE;
        NdisStallExecution(20);
    }
    return FALSE;
}
static USHORT CpPhyAddress(PRLTK_ADAPTER a, UINT reg)
{
    if (!a->PhyPage) return (USHORT)(0xA400 + (reg & 31) * 2);
    return (USHORT)((a->PhyPage << 4) + ((reg & 31) - 16) * 2);
}

'''+s[pos:]
s=s.replace('    RtlWrite32(a, RCP_PHYAR, (ULONG)(reg & 0x1F) << 16);','''    if ((a->Flags & RTL_F_PHY_OCP) || a->Is8125) {
        if ((reg & 31) == 31) return a->PhyPage;
        if (a->PhyPage && (reg & 31) < 16) return 0xFFFF;
        return RtlCpPhyOcpRead(a, CpPhyAddress(a, reg));
    }
    RtlWrite32(a, RCP_PHYAR, (ULONG)(reg & 0x1F) << 16);''')
s=s.replace('    RtlWrite32(a, RCP_PHYAR, RCP_PHYAR_FLAG | ((ULONG)(reg & 0x1F) << 16) | val);','''    if ((a->Flags & RTL_F_PHY_OCP) || a->Is8125) {
        if ((reg & 31) == 31) { a->PhyPage = val; return; }
        if (a->PhyPage && (reg & 31) < 16) return;
        RtlCpPhyOcpWrite(a, CpPhyAddress(a, reg), val);
        return;
    }
    RtlWrite32(a, RCP_PHYAR, RCP_PHYAR_FLAG | ((ULONG)(reg & 0x1F) << 16) | val);''')
s=s.replace('static VOID CpReset(PRLTK_ADAPTER a)','static BOOLEAN CpReset(PRLTK_ADAPTER a)').replace('if (!(RtlRead8(a, RCP_CR) & RCP_CR_RST)) break;', 'if (!(RtlRead8(a, RCP_CR) & RCP_CR_RST)) return TRUE;').replace('        NdisStallExecution(10);\n    }\n}', '        NdisStallExecution(10);\n    }\n    return FALSE;\n}',1).replace('    CpReset(a);','    if (!CpReset(a)) return FALSE;')
s=s.replace('framelen >= RLTK_HEADER_SIZE + 4)', 'framelen >= RLTK_HEADER_SIZE + 4 && framelen <= RCP_RX_BUF_SIZE)')
s=s.replace('    if (len > RCP_RX_BUF_SIZE) len = RCP_RX_BUF_SIZE;', '    if (len > RCP_RX_BUF_SIZE) return FALSE;')
p.write_text(s)
p=root/'phy.c';s=p.read_text().replace('    if (!a->Is8125)\n        RtlCpMdioWrite', '    RtlCpMdioWrite')
s=s.replace('    /* 2.5/5/10G advertisement on 8125+ lives in Realtek vendor MMD pages set\n       by firmware at reset; RtlCapSpeed() enforces the ceiling in software. */', '''    if (a->Is8125) {
        USHORT adv = RtlCpPhyOcpRead(a, 0xA5D4);
        USHORT wanted = 0;
        if (a->SpeedDuplex == RLTK_SD_2500F ||
            (!a->SpeedDuplex && a->MaxSpeedMbit >= 2500)) wanted |= 0x0080;
        if (a->SpeedDuplex == RLTK_SD_5000F ||
            (!a->SpeedDuplex && a->MaxSpeedMbit >= 5000)) wanted |= 0x0100;
        if (a->SpeedDuplex == RLTK_SD_10000F ||
            (!a->SpeedDuplex && a->MaxSpeedMbit >= 10000)) wanted |= 0x1000;
        if (adv != 0xFFFF)
            RtlCpPhyOcpWrite(a, 0xA5D4, (adv & ~0x1180) | wanted);
    }''')
s=s.replace('case RLTK_SD_10000F: *gb = GBCR_1000F; break;   /* multi-gig: vendor MMD */', 'case RLTK_SD_10000F: break; /* selected in vendor advertisement below */')
p.write_text(s)
