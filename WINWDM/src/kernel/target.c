#include "wdm_target.h"
#include "protocol.h"

#ifndef WK_TARGET_INDEX
#define WK_TARGET_INDEX WK_INDEX_600
#endif

unsigned WkTargetVersion(void)
{
#define WK_VERSION(id, code, label, file) code,
    static const unsigned versions[] = { WK_WDM_TARGETS(WK_VERSION) };
#undef WK_VERSION
    return versions[WK_TARGET_INDEX];
}

int WkMatchTarget(unsigned long controlCode)
{
    return controlCode == WK_IOCTL_TARGET_BASE + 4UL * WK_TARGET_INDEX;
}
