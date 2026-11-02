/* NT 3.1 poll ownership. Acquire under the shared list lock so removal
 * cannot free an adapter between lookup and reference acquisition.
 * Removal delists and drains packet/filter users before NDIS teardown.
 * PollLock is separate from the Ethernet filter's own spin lock. */
static BOOLEAN n31_poll_acquire(PN31_ADAPTER a)
{
    KIRQL old;
    BOOLEAN running;
    KeAcquireSpinLock(&a->PollLock, &old);
    running = a->Running;
    if (running) a->PollActive++;
    KeReleaseSpinLock(&a->PollLock, old);
    return running;
}

static VOID n31_poll_release(PN31_ADAPTER a)
{
    KIRQL old;
    BOOLEAN drained;
    KeAcquireSpinLock(&a->PollLock, &old);
    a->PollActive--;
    drained = !a->Running && !a->PollActive;
    if (drained) KeSetEvent(a->PollDrained, 0, FALSE);
    KeReleaseSpinLock(&a->PollLock, old);
}

static VOID n31_poll_stop(PN31_ADAPTER a)
{
    KIRQL old;
    BOOLEAN busy;
    KeAcquireSpinLock(&a->PollLock, &old);
    KeResetEvent(a->PollDrained);
    a->Running = FALSE;
    busy = a->PollActive != 0;
    KeReleaseSpinLock(&a->PollLock, old);
    if (busy) KeWaitForSingleObject(a->PollDrained, 0, 0, FALSE, NULL);
    /* Join the final release's unlock before event/adapter storage can go. */
    KeAcquireSpinLock(&a->PollLock, &old);
    KeReleaseSpinLock(&a->PollLock, old);
}
