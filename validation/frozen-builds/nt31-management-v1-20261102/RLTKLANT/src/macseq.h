/* SPDX-License-Identifier: GPL-2.0-only
 * Serialized MAC configuration subtree. Not a complete adapter startup API.
 * The owner keeps DMA stopped and excludes interrupts, reset and other I/O.
 * The caller must also supply PCI power/ASPM, PHY, DMA and lifecycle
 * ordering; do not invoke this passive transaction from an NDIS DPC.
 */
#ifndef RLTK_MACSEQ_H
#define RLTK_MACSEQ_H
#include "fwbus.h"
#include "fwimages.h"
typedef struct {
    void *Context;
    int (*Read)(void *,RTL_FW_U16 offset,unsigned width,RTL_FW_U32 *);
    int (*Write)(void *,RTL_FW_U16 offset,unsigned width,RTL_FW_U32);
    int (*PciRead)(void *,RTL_FW_U16 offset,unsigned width,RTL_FW_U32 *);
    int (*PciWrite)(void *,RTL_FW_U16 offset,unsigned width,RTL_FW_U32);
    int (*DelayUs)(void *,RTL_FW_U32);
    int (*Cancelled)(void *);
    /* Valid mapped extent, not the size a caller wishes the BAR had.
       RTL8125+ touches EEE timing through 0x6049; legacy needs 0x100. */
    RTL_FW_U32 RegisterBytes,MaxOperations,MaxDelayUs;
    RTL_FW_U16 PcieCapability;
    RTL_FW_U8 PciFunction;
    enum RTL_FW_PHY_ACCESS PhyAccess;
} RTL_MAC_IO;
typedef struct {
    enum RTL_FW_RESULT Result;
    RTL_FW_U32 Operations,FailedOperation,DelayUs;
    int FirmwareApplied;
    unsigned DashType,DashEnabled;
    RTL_FW_REPORT Firmware;
} RTL_MAC_REPORT;
enum RTL_FW_RESULT RtlPciMacConfigure(const RTL_MAC_IO *,const RTL_FW_CHIP *,RTL_MAC_REPORT *);
/* Separate source-ordered lifecycle transactions. The owner must run
 * Initialize before the first MAC reset, then Acquire before PHY/MAC setup.
 * Release requires drained DMA/interrupts and precedes device power-down.
 * These are hardware transactions, not OS PCI D-state transitions. */
enum RTL_MAC_HARDWARE_STAGE {
    RtlMacHardwareInitialize,
    RtlMacHardwareAcquire,
    RtlMacHardwareRelease
};
enum RTL_FW_RESULT RtlPciMacHardware(const RTL_MAC_IO *,const RTL_FW_CHIP *,
                                    enum RTL_MAC_HARDWARE_STAGE,RTL_MAC_REPORT *);
#endif
