#ifndef WINWDM_VMM_EVENT_H
#define WINWDM_VMM_EVENT_H
#include <wdm.h>

/* Single-waiter, one-shot completion for resident VMM objects. */
void WkInitVmmEvent(PKEVENT event);
void WkSignalVmmEvent(PKEVENT event);
void WkWaitVmmEvent(PKEVENT event);
#endif
