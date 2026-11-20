#ifndef WINWDM_RESIDENT_H
#define WINWDM_RESIDENT_H
#include <wdm.h>

BOOLEAN WkStartResidentPool(void);
VOID WkStopResidentPool(void);
PVOID WkResidentAllocate(SIZE_T bytes);
VOID WkResidentFree(PVOID address);
#endif
