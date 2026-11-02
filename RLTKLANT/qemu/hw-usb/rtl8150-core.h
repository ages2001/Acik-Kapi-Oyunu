/* Minimal RTL8150 USB1 register/framing model, independent of r8152 banks.
 * Protocol reference: Linux rtl8150.c vendor request 5, enable_net_traffic,
 * rtl8150_start_xmit and MSR/CSCR link definitions. Does not model EEPROM,
 * MII management, PHY negotiation, error interrupts or power management.
 */
#ifndef RTL8150_CORE_H
#define RTL8150_CORE_H
#include <stdint.h>
#include <string.h>
typedef struct R8150Core {
    uint8_t reg[0x200], mac[6];
    int link_up, rx_enabled, tx_enabled;
} R8150Core;
static inline void r8150c_set_link(R8150Core *c, int up)
{
    c->link_up=!!up;
    c->reg[0x137]=up?0x1c:0; /* 100M, full duplex, carrier */
    c->reg[0x14c]=up?0x08:0; /* CSCR link status */
}
static inline void r8150c_reset(R8150Core *c)
{
    memset(c->reg,0,sizeof(c->reg));
    memcpy(c->reg+0x120,c->mac,6);
    c->rx_enabled=c->tx_enabled=0;
    r8150c_set_link(c,c->link_up);
}
static inline int r8150c_control(R8150Core *c, int read, uint16_t off,
                                 uint16_t index, uint8_t *data, uint16_t len)
{
    unsigned i;
    if(index||!len||(uint32_t)off+len>sizeof(c->reg))return -1;
    if(read){memcpy(data,c->reg+off,len);return 0;}
    for(i=0;i<len;i++) {
        unsigned at=off+i;
        if(at==0x137||at==0x14c||at==0x14d)continue; /* status is read-only */
        c->reg[at]=data[i];
        if(at==0x12e){
            if(data[i]&0x10)r8150c_reset(c);
            else{
                c->rx_enabled=!!(data[i]&8);
                c->tx_enabled=!!(data[i]&4);
            }
        }
    }
    return 0;
}
static inline uint32_t r8150c_tx_decode(const uint8_t *buf,uint32_t len,
                                       const uint8_t **frame)
{
    *frame=0;
    if(len<60||len>1514)return 0;
    /* A transfer of 64*n+1 bytes may include the terminating pad byte.
       Ethernet padding may also be real data, so retain it on the wire. */
    *frame=buf;
    return len;
}
static inline uint32_t r8150c_rx_encode(const uint8_t *frame,uint32_t len,
                                       uint8_t *out)
{
    if(!len||len>1514)return 0;
    memcpy(out,frame,len);
    memset(out+len,0,4); /* error-free RX trailer */
    return len+4;
}
#endif
