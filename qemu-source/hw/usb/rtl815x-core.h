/*
 * rtl815x-core.h - self-contained Realtek r8152-family USB-LAN protocol core.
 *
 * This is the device-side model of the Realtek USB vendor protocol: the
 * register file reached through the GET/SET REGS vendor request, and the
 * bulk TX (8-byte descriptor) / RX (24-byte descriptor, 8-byte aligned)
 * framing.  It has no QEMU dependency so it can be compiled both into the
 * QEMU usb-rtl8153 device and into a host interop test that drives it with
 * the RLTKLANT driver's own rtl815x.c protocol layer.
 *
 * Offsets/bits mirror the RLTKLANT USBNT/src/usbchip.h definitions, which
 * follow the public RTL8152/8153 register map and the Linux r8152 driver.
 *
 * Copyright (C) 2026 ages2001.
 */
#ifndef RTL815X_CORE_H
#define RTL815X_CORE_H

#include <stdint.h>
#include <string.h>
#include "rtl815x-fwchecks.h"

/* vendor request + register layout (must match usbchip.h) */
#define R815C_RT_READ       0xC0
#define R815C_RT_WRITE      0x40
#define R815C_REQ_REGS      0x05
#define R815C_MCU_PLA       0x0100
#define R815C_PLA_IDR       0xC000
#define R815C_PLA_CR        0xE813
#define R815C_PLA_RCR       0xC010
#define R815C_CR_RE         0x08
#define R815C_CR_TE         0x04
#define R815C_CR_RST        0x10

#define R815C_TX_DESC       8
#define R815C_RX_DESC       24
#define R815C_TX_LEN_MASK   0x0003FFFFu
#define R815C_RX_LEN_MASK   0x00007FFFu
#define R815C_RX_ALIGN      8
#define R815C_MAX_FRAME     1514
#define R815C_REGFILE       0x10000      /* flat offset space (covers 0xC3FF) */

typedef struct R815xCore {
    uint8_t  reg[R815C_REGFILE];         /* PLA register bank */
    uint8_t  usb_reg[R815C_REGFILE];     /* independent USB register bank */
    uint8_t  mac[6];
    uint16_t version;
    int      rx_enabled;
    int      tx_enabled;
    int      link_up;
    /* Register-transport fixture. Firmware completion below validates bytes;
       it does not execute a PHY MCU or simulate analog link training. */
    uint16_t phy[32768],sram[65536],sram2[65536],sram_address,sram2_address;
    uint32_t advanced[2][65536];
    uint32_t phy_faults,fw_chunk,fw_bytes,fw_bad,negotiation_ms,negotiation_generation;
    uint64_t fw_hash;
} R815xCore;

#define R815C_FAULT_AUTOLOAD 1u
#define R815C_FAULT_PHY_STATUS 2u
#define R815C_FAULT_PATCH 4u
#define R815C_FAULT_SPEEDUP 8u
#define R815C_FAULT_TGPHY 16u
#define R815C_FAULT_ADVANCED 32u
#define R815C_FAULT_RESET 64u
#define R815C_FAULT_FIRMWARE 128u
static inline void r815c_set_link(R815xCore*,int);
static inline uint16_t r815c_word(const uint8_t*p)
{return (uint16_t)(p[0]|(uint16_t)p[1]<<8);}
static inline void r815c_putword(uint8_t*p,uint16_t value)
{p[0]=(uint8_t)value;p[1]=(uint8_t)(value>>8);}
static inline unsigned r815c_stable_version(const R815xCore*c)
{
    switch(c->version){case 0x7410:case 0x7420:return 13;
    case 0x1030:return 16;case 0x2020:return 17;default:return 0;}
}
static inline uint16_t r815c_phy_read(R815xCore*c,unsigned reg)
{
    if(reg==0xa438)return c->sram[c->sram_address];
    if(reg==0xb87e)return c->sram2[c->sram2_address];
    if(reg==0xa420&&(c->phy_faults&R815C_FAULT_PHY_STATUS))return 0;
    return c->phy[(reg&0xffff)/2];
}
static inline void r815c_phy_write(R815xCore*c,unsigned reg,uint16_t value)
{
    unsigned i;uint16_t old=c->phy[(reg&0xffff)/2];
    c->phy[(reg&0xffff)/2]=value;
    if(reg==0xa436)c->sram_address=value;
    if(reg==0xb87c)c->sram2_address=value;
    /* SRAM reads retain the selected word; stream writes advance it. */
    if(reg==0xa438)c->sram[c->sram_address++]=value;
    if(reg==0xb87e)c->sram2[c->sram2_address++]=value;
    if(reg==0xa400){
        if(!(c->phy_faults&R815C_FAULT_RESET))c->phy[reg/2]&=~0x8000u;
        if(value&0x8200){c->negotiation_ms=300;c->negotiation_generation++;}
        c->phy[0xa420/2]=(value&0x0800)?5:3;
        r815c_set_link(c,c->link_up);
    }
    if(reg==0xb820){
        if((value&0x10)&&!(old&0x10)){
            c->fw_bytes=c->fw_chunk=c->fw_bad=0;
            c->fw_hash=UINT64_C(14695981039346656037);
        }
        if(!(c->phy_faults&R815C_FAULT_PATCH))c->phy[0xb800/2]=(value&0x10)?0x40:0;
        if(!(value&0x10)&&(old&0x10)&&!c->fw_bad&&!(c->phy_faults&R815C_FAULT_FIRMWARE)){
            for(i=0;i<sizeof(r815c_firmware_checks)/sizeof(r815c_firmware_checks[0]);i++)
                if(r815c_firmware_checks[i].version==r815c_stable_version(c)&&
                   c->fw_bytes==r815c_firmware_checks[i].size&&
                   c->fw_hash==r815c_firmware_checks[i].hash)
                    c->sram[0x801e]=(uint16_t)r815c_firmware_checks[i].code_version;
        }
    }
}
static inline void r815c_usb_command(R815xCore*c,unsigned off)
{
    if(off<=0xd634&&off+4>0xd634&&(c->usb_reg[0xd634]&1)&&!(c->phy_faults&R815C_FAULT_TGPHY)){
        unsigned reg=r815c_word(c->usb_reg+0xd630);
        if(c->usb_reg[0xd634]&2)r815c_phy_write(c,reg,r815c_word(c->usb_reg+0xd632));
        else r815c_putword(c->usb_reg+0xd632,r815c_phy_read(c,reg));
        c->usb_reg[0xd634]&=~1u;
    }
    if(off<=0xd5dc&&off+4>0xd5dc&&(c->usb_reg[0xd5dc]&1)&&!(c->phy_faults&R815C_FAULT_ADVANCED)){
        unsigned bank=!!(c->usb_reg[0xd5dc]&4),address=r815c_word(c->usb_reg+0xd5d6);
        if(c->usb_reg[0xd5dc]&2)c->advanced[bank][address]=r815c_word(c->usb_reg+0xd5d8)|(uint32_t)r815c_word(c->usb_reg+0xd5da)<<16;
        else{r815c_putword(c->usb_reg+0xd5d8,(uint16_t)c->advanced[bank][address]);
             r815c_putword(c->usb_reg+0xd5da,(uint16_t)(c->advanced[bank][address]>>16));}
        c->usb_reg[0xd5dc]&=~1u;
    }
}

/* PLA_PHYSTATUS encoding from the vendor r8152 register definitions.
 * This models cable state at a fixed profile speed, not PHY negotiation. */
static inline void r815c_set_link(R815xCore *c, int up)
{
    uint16_t status = 0;
    c->link_up = !!up;
    if (up&&!c->negotiation_ms&&!(c->phy[0xa400/2]&0x0800)) {
        status = 0x0003; /* full duplex and carrier */
        switch (c->version) {
        case 0x4c00: case 0x4c10: case 0x4800: status |= 0x0008; break;
        case 0x7020: case 0x7030: case 0x7400: case 0x7410:
            status |= 0x0400; break;
        case 0x1030: status |= 0x1000; break;
        case 0x2020: status |= 0x4000; break;
        default: status |= 0x0010; break;
        }
    }
    c->reg[0xe908] = status;
    c->reg[0xe909] = status >> 8;
}

/* Fixed 300ms fixture interval after a BMCR reset/restart, not analog
   autonegotiation timing. QEMU advances this on its virtual clock. */
static inline void r815c_tick(R815xCore*c,unsigned ms)
{
    if(!c->negotiation_ms)return;
    c->negotiation_ms=ms>=c->negotiation_ms?0:c->negotiation_ms-ms;
    r815c_set_link(c,c->link_up);
}
static inline int r815c_carrier(const R815xCore*c)
{return c->link_up&&!c->negotiation_ms&&!(c->phy[0xa400/2]&0x0800);}

static inline void r815c_reset(R815xCore *c)
{
    memset(c->reg, 0, sizeof(c->reg));
    memset(c->usb_reg, 0, sizeof(c->usb_reg));
    memset(c->phy,0,sizeof(c->phy));memset(c->sram,0,sizeof(c->sram));
    memset(c->sram2,0,sizeof(c->sram2));memset(c->advanced,0,sizeof(c->advanced));
    c->sram_address=c->sram2_address=0;c->fw_chunk=c->fw_bytes=c->fw_bad=c->negotiation_ms=0;
    c->negotiation_generation=0;
    c->fw_hash=UINT64_C(14695981039346656037);
    c->phy[0xa420/2]=3;c->reg[0xe001]=1;c->reg[0xe84f]=2;
    c->reg[0xe004]=(c->phy_faults&R815C_FAULT_AUTOLOAD)?0:2;
    memcpy(&c->reg[R815C_PLA_IDR], c->mac, 6);
    if (!c->version) c->version = 0x5c10;
    c->reg[0xe612] = c->version;
    c->reg[0xe613] = c->version >> 8;
    /* Default RTL8159 test profile is QFN68. The QEMU device can override
       this after reset to exercise QFN100 and unknown-package rejection. */
    if (c->version == 0x2020) c->reg[0xdc48] = 0x1e;
    c->rx_enabled = 0;
    c->tx_enabled = 0;
    r815c_set_link(c, c->link_up);
}

static inline void r815c_set_mac(R815xCore *c, const uint8_t mac[6])
{
    memcpy(c->mac, mac, 6);
    memcpy(&c->reg[R815C_PLA_IDR], mac, 6);
}

/* Internal PLA-byte write and modeled command-register side effects.
   The vendor-request entry points below select banks and byte enables. */
static inline void r815c_set_regs(R815xCore *c, uint16_t offset,
                                  const uint8_t *data, uint16_t len)
{
    uint32_t i;
    for (i = 0; i < len && (offset + i) < R815C_REGFILE; i++)
        c->reg[offset + i] = data[i];

    if (offset == R815C_PLA_CR && len >= 1) {
        uint8_t cr = c->reg[R815C_PLA_CR];
        c->rx_enabled = (cr & R815C_CR_RE) ? 1 : 0;
        c->tx_enabled = (cr & R815C_CR_TE) ? 1 : 0;
        if ((cr & R815C_CR_RST)&&!(c->phy_faults&R815C_FAULT_RESET)) {
            c->reg[R815C_PLA_CR] = (uint8_t)(cr & ~R815C_CR_RST);
            memcpy(&c->reg[R815C_PLA_IDR], c->mac, 6);
            c->rx_enabled = c->tx_enabled = 0;
        }
    }
}

/* Aligned vendor writes use one byte-enable bit per lane, duplicated in
 * each nibble for the first/last DWORD. Reject malformed requests. */
static inline int r815c_control_write(R815xCore *c, uint16_t offset,
                                      uint16_t index, const uint8_t *data,
                                      uint16_t len)
{
    uint32_t i;
    if ((offset & 3) || !len || (len & 3) ||
        (uint32_t)offset + len > R815C_REGFILE) return -1;
    if ((index & 0xFF00) != 0 && (index & 0xFF00) != R815C_MCU_PLA) return -1;
    for (i = 0; i < len; i++) {
        uint8_t mask = i < 4 ? index & 15 :
                       i >= (uint32_t)len - 4 ? (index >> 4) & 15 : 15;
        if (mask & (1u << (i & 3))) {
            if ((index & R815C_MCU_PLA)&&offset+i>=0xb000&&offset+i<0xc000){
                /* Apply both enabled byte lanes as one PHY word operation;
                   SRAM selectors/data must not auto-increment twice. */
                unsigned wordoff=(offset+i)&~1u,shift=((offset+i)&1)*8;
                unsigned reg=(r815c_word(c->reg+0xe86c)&0xf000)|(wordoff&0xfff);
                uint16_t value=r815c_phy_read(c,reg);
                value=(uint16_t)((value&~(0xffu<<shift))|((unsigned)data[i]<<shift));
                if(!(i&1)&&i+1<len&&(mask&(1u<<((i+1)&3)))){
                    value=(uint16_t)((value&255)|((unsigned)data[++i]<<8));
                }
                r815c_phy_write(c,reg,value);
            }else if (index & R815C_MCU_PLA)
                r815c_set_regs(c, offset + i, data + i, 1);
            else{
                c->usb_reg[offset + i] = data[i];
                if(offset+i>=0x9a00&&offset+i<0xa200){
                    if((unsigned)(offset+i-0x9a00)!=c->fw_chunk)c->fw_bad=1;
                    c->fw_chunk++;
                }
            }
        }
    }
    if(index&R815C_MCU_PLA){
        if(offset<=0xdc6a&&(unsigned)offset+len>0xdc6a&&(c->reg[0xdc6a]&0x10)&&!(c->phy_faults&R815C_FAULT_SPEEDUP)){
            if(!c->fw_chunk||c->fw_chunk>2048)c->fw_bad=1;
            else for(i=0;i<c->fw_chunk;i++)c->fw_hash=(c->fw_hash^c->usb_reg[0x9a00+i])*UINT64_C(1099511628211);
            c->fw_bytes+=c->fw_chunk;c->fw_chunk=0;c->reg[0xdc6a]&=~0x10u;
        }
    }else for(i=0;i<len;i+=4)r815c_usb_command(c,offset+i);
    return 0;
}

/* Vendor reads select the USB or PLA bank; malformed/unknown requests stall.
   USB-bank addresses must never trigger PLA command-register behavior. */
static inline int r815c_control_read(R815xCore *c, uint16_t offset,
                                     uint16_t index, uint8_t *data, uint16_t len)
{
    const uint8_t *bank;
    if ((index != 0 && index != R815C_MCU_PLA) || (offset & 3) ||
        !len || (len & 3) || (uint32_t)offset + len > R815C_REGFILE) return -1;
    bank = index == R815C_MCU_PLA ? c->reg : c->usb_reg;
    if(index==R815C_MCU_PLA){
        unsigned i;
        for(i=0;i<len;i++){
            unsigned off=offset+i;
            if(off>=0xb000&&off<0xc000){
                unsigned reg=(r815c_word(c->reg+0xe86c)&0xf000)|(off&0xffe);
                data[i]=(uint8_t)(r815c_phy_read(c,reg)>>((off&1)*8));
            }else data[i]=bank[off];
        }
    }else memcpy(data, bank + offset, len);
    return 0;
}

/* Decode one bulk-OUT transfer: strip the 8-byte tx descriptor, return the
   ethernet frame length (0 if malformed) and point *frame at the payload. */
static inline uint32_t r815c_tx_decode(const uint8_t *buf, uint32_t buflen,
                                       const uint8_t **frame)
{
    uint32_t opts1, len;
    *frame = 0;
    if (buflen < R815C_TX_DESC) return 0;
    opts1 = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
            ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
    if ((opts1 & 0xC0000000u) != 0xC0000000u) return 0;
    len = opts1 & R815C_TX_LEN_MASK;
    if (len == 0 || R815C_TX_DESC + len > buflen) return 0;
    *frame = buf + R815C_TX_DESC;
    return len;
}

/* Encode a received ethernet frame into a bulk-IN buffer: 24-byte rx
   descriptor (opts1 low 15 bits = length) + payload, padded to the 8-byte
   alignment the next descriptor expects.  Returns total bytes written. */
static inline uint32_t r815c_rx_encode(const uint8_t *frame, uint32_t len,
                                       uint8_t *out)
{
    uint32_t total;
    memset(out, 0, R815C_RX_DESC);
    out[0] = (uint8_t)((len + 4) & 0xFF);
    out[1] = (uint8_t)(((len + 4) >> 8) & 0x7F);       /* low 15 bits of opts1 */
    memcpy(out + R815C_RX_DESC, frame, len);
    total = R815C_RX_DESC + len + 4;
    memset(out + R815C_RX_DESC + len, 0, 4);
    while (total & (R815C_RX_ALIGN - 1)) out[total++] = 0;
    return total;
}

#endif /* RTL815X_CORE_H */
