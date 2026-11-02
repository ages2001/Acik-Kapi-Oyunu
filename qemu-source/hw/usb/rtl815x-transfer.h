/* USB packet/transfer boundaries, independent of the Ethernet protocol core. */
#ifndef RTL815X_TRANSFER_H
#define RTL815X_TRANSFER_H
#include <stdint.h>
#include <string.h>
#define RTL_USB_TRANSFER_MAX 2048
typedef struct {
    uint8_t bytes[RTL_USB_TRANSFER_MAX];
    uint32_t count;
    int discard;
} RtlUsbTxAssembly;
static inline uint32_t rtl_usb_tx_packet(RtlUsbTxAssembly *t, int legacy,
        int v2, unsigned maxpacket, const uint8_t *data, uint32_t n)
{
    uint32_t expected=0,header=v2?16:8,length;
    int end=!n||n%maxpacket;
    if(t->discard){if(end)t->discard=0;return 0;}
    if(n>sizeof(t->bytes)-t->count){t->count=0;t->discard=!end;return 0;}
    if(n)memcpy(t->bytes+t->count,data,n);
    t->count+=n;
    if(legacy){
        if(!end)return 0;
        length=t->count;t->count=0;
        return length>=60&&length<=1514?length:0;
    }
    if(t->count>=header){
        const uint8_t *p=t->bytes+(v2?8:0);
        length=(uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
        length=v2?length>>4:length&0x3ffff;
        if(!length||length>1514){t->count=0;t->discard=!end;return 0;}
        expected=header+length;
        if(t->count>=expected){length=t->count;t->count=0;return length==expected?length:0;}
    }
    if(end)t->count=0; /* truncated descriptor/frame cannot join the next URB */
    return 0;
}
/* Return -1 for NAK; otherwise the bytes copied, including a terminating ZLP.
   The owner must flush queued network packets only after length AND zlp clear. */
static inline int rtl_usb_rx_packet(const uint8_t *data,uint32_t *length,
        uint32_t *offset,int *zlp,unsigned maxpacket,uint8_t *out,uint32_t cap)
{
    uint32_t n;
    if(*zlp){*zlp=0;return 0;}
    if(!*length)return -1;
    n=*length-*offset;if(n>cap)n=cap;
    if(n)memcpy(out,data+*offset,n);
    *offset+=n;
    if(*offset==*length){
        *zlp=n==cap&&n&&!(n%maxpacket);
        *offset=*length=0;
    }
    return (int)n;
}
#endif
