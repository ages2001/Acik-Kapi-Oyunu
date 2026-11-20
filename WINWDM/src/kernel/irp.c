#include "wdm_irp.h"

/* Reinitialize all stack locations and transient IRP fields, preserving the
 * original allocator's flags so its owner can release the packet normally. */
VOID NTAPI IoReuseIrp(PIRP irp, NTSTATUS status)
{
    UCHAR allocationFlags = irp->AllocationFlags;
    IoInitializeIrp(irp, irp->Size, irp->StackCount);
    irp->AllocationFlags = allocationFlags;
    irp->IoStatus.Status = status;
}
