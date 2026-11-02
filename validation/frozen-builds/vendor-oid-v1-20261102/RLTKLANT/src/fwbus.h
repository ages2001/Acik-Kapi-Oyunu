/* Serialized PCI firmware register transport. The caller owns the adapter
 * exclusively, supplies passive-level waits, and keeps DMA stopped. */
#ifndef RLTK_FWBUS_H
#define RLTK_FWBUS_H
#include "fwvm.h"
enum RTL_FW_PHY_ACCESS { RtlFwPhyMdio, RtlFwPhyMdioDp, RtlFwPhyOcp };
typedef struct {
    void *Context;
    RTL_FW_U32 (*Read32)(void *,RTL_FW_U16);
    void (*Write32)(void *,RTL_FW_U16,RTL_FW_U32);
    int (*DelayUs)(void *,RTL_FW_U32);
    int (*DelayMs)(void *,RTL_FW_U32);
    int (*Cancelled)(void *);
    enum RTL_FW_PHY_ACCESS Access;
    RTL_FW_U32 Base;
    RTL_FW_U32 PollLimit,MaxCommands,MaxDelayMs;
    int Removed; /* latched bus removal; zero only when creating a new owner */
} RTL_FW_BUS;
int RtlFwBusRead(void *,int,RTL_FW_U16,RTL_FW_U16 *);
int RtlFwBusWrite(void *,int,RTL_FW_U16,RTL_FW_U16);
int RtlFwBusOcp(RTL_FW_BUS *,int mac,int write,RTL_FW_U16 address,RTL_FW_U16 *);
enum RTL_FW_RESULT RtlFwBusUpload(RTL_FW_BUS *,const RTL_FW_U8 *,RTL_FW_U32,RTL_FW_REPORT *);
#endif
