/* GPL-2.0-only: bridge to the attributed Linux PHY initialization component. */
#ifndef RLTK_PHYSEQ_H
#define RLTK_PHYSEQ_H
#include "fwbus.h"
#include "fwimages.h"
typedef struct {
    enum RTL_FW_RESULT Result;
    RTL_FW_REPORT Firmware;
    RTL_FW_U32 Operations,FailedOperation;
    int FirmwareApplied;
} RTL_PHY_REPORT;
enum RTL_FW_RESULT RtlPciPhyConfigure(RTL_FW_BUS *,const RTL_FW_CHIP *,RTL_PHY_REPORT *);
/* Initial, unpublished adapter only: caller has stopped DMA and supplies
 * passive waits. Ordinary MAC reset must not call this at DISPATCH_LEVEL. */
enum RTL_FW_RESULT RtlPciPhyConfigureByXid(RTL_FW_BUS *,RTL_FW_U16,int,RTL_PHY_REPORT *);
#endif
