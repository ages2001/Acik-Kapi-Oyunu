/* Native NT5 USB transport ABI. This header can be included by either the
   self-contained NDIS headers or a separate DDK translation unit. */
#ifndef RTL_NATIVE_URB_H
#define RTL_NATIVE_URB_H
void *RtlUrbOpen(void *lower,unsigned short *vendor,unsigned short *product);
unsigned RtlUrbBusSpeed(void*);
int RtlUrbCancelled(void*);
int RtlUrbWait(void*,unsigned);
int RtlUrbTransfer(void *context,unsigned char kind,unsigned char endpoint,
                   unsigned char *setup,void *buffer,unsigned long length,
                   unsigned long *done);
void RtlUrbResetRx(void *context);
void RtlUrbObserveBusReset(void *context);
int RtlUrbSetCarrier(void *context,int up);
void RtlUrbCancel(void *context);
int RtlUrbStart(void *context,void (*service)(void *),void *owner);
void RtlUrbClose(void *context);
#endif
