/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef RTL_USB_FW_EXEC_H
#define RTL_USB_FW_EXEC_H
#include "usbfw.h"
enum RTL_UFW_SPACE {RtlUsbFwUsb=0,RtlUsbFwPla=1,RtlUsbFwPhy=2,RtlUsbFwSram=3};
enum RTL_UFW_EXEC_RESULT {RtlUsbFwExecOk=0,RtlUsbFwExecArgument,
    RtlUsbFwExecInvalid,RtlUsbFwExecIo,RtlUsbFwExecCancelled,
    RtlUsbFwExecTimeout,RtlUsbFwExecBudget,RtlUsbFwExecVersion};
enum RTL_UFW_FLAGS {
    /* EXT_INIT after a power cut: the reference requests patch mode without
       waiting for the running-PHY acknowledgement. The caller detects it. */
    RtlUsbFwPowerCut=1,
    /* Vendor ram-code policy: bypass a detected flash when replacing an
       older PHY image. Distinct from the pinned Linux container policy. */
    RtlUsbFwVendorFlash=2
};
/* Caller owns exclusive PASSIVE-level device access and an immutable input
   for the complete call. MAC/DMA must be stopped and revision-specific PHY
   power/init prerequisites met. Control callbacks return 1 on full success.
   PHY/SRAM transfers are 16-bit; PLA/USB transfers can be 1, 2, 4 or a block.
   Cancellation is checked before each transfer or wait. On any failure no
   more device access is performed, including cleanup on a removed device.
   The owner must reset/reinitialize before retrying a partially applied image.
   Bounds are cumulative, including successful waits; no allocator is used. */
typedef struct {
    void *Context;
    int (*Read)(void*,enum RTL_UFW_SPACE,unsigned,RTL_UFW_U8*,unsigned);
    int (*Write)(void*,enum RTL_UFW_SPACE,unsigned,const RTL_UFW_U8*,unsigned);
    int (*DelayMs)(void*,unsigned);
    int (*Cancelled)(void*);
    unsigned MaxOperations,MaxDelayMs,Flags;
} RTL_UFW_BUS;
typedef struct {
    enum RTL_UFW_EXEC_RESULT Result;
    RTL_UFW_REPORT Validation;
    unsigned Operations,DelayMs,Blocks,Offset,PatchRequested;
} RTL_UFW_EXEC_REPORT;
enum RTL_UFW_EXEC_RESULT RtlUsbFwExecute(const RTL_UFW_U8*,RTL_UFW_U32,
    unsigned,const RTL_UFW_BUS*,RTL_UFW_EXEC_REPORT*);
#endif
