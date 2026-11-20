#ifndef WINWDM_PORTCLS_DISPATCH_H
#define WINWDM_PORTCLS_DISPATCH_H

typedef struct {
    LIST_ENTRY Link;
    PINTERRUPTSYNCROUTINE Callback;
    PVOID Context;
} SERVICE_ROUTINE;

/* The caller holds the interrupt object's spin lock throughout the dispatch. */
static BOOLEAN serviceInterrupt(IInterruptSync *iface, LIST_ENTRY *routines, INTERRUPTSYNCMODE mode)
{
    LIST_ENTRY *entry;
    BOOLEAN handled = FALSE, again;
    do {
        again = FALSE;
        for (entry = routines->Flink; entry != routines; entry = entry->Flink) {
            SERVICE_ROUTINE *service = CONTAINING_RECORD(entry, SERVICE_ROUTINE, Link);
            if (service->Callback(iface, service->Context) == STATUS_SUCCESS) {
                handled = TRUE;
                if (mode == InterruptSyncModeNormal) return TRUE;
                again = mode == InterruptSyncModeRepeat;
            }
        }
    } while (again);
    return handled;
}
#endif
