/* All x86 NT releases export the original stdcall names. Later DDK inline
 * aliases select fastcall entry points which the early kernels do not export. */
#ifndef WK_NT_LEGACY_H
#define WK_NT_LEGACY_H
#include <ntifs.h>
#undef IoCallDriver
#undef IoCompleteRequest
#undef KeAcquireSpinLock
#undef KeReleaseSpinLock
#undef ObDereferenceObject
#undef IoSetCancelRoutine
NTKERNELAPI NTSTATUS NTAPI IoCallDriver(PDEVICE_OBJECT device, PIRP irp);
NTKERNELAPI VOID NTAPI IoCompleteRequest(PIRP irp, CCHAR boost);
NTHALAPI VOID NTAPI KeAcquireSpinLock(PKSPIN_LOCK lock, PKIRQL oldIrql);
NTHALAPI VOID NTAPI KeReleaseSpinLock(PKSPIN_LOCK lock, KIRQL oldIrql);
NTKERNELAPI LONG_PTR NTAPI ObDereferenceObject(PVOID object);
NTSYSAPI NTSTATUS NTAPI NtWaitForSingleObject(HANDLE handle, BOOLEAN alertable, PLARGE_INTEGER timeout);
#define IoSetCancelRoutine(irp,routine) __atomic_exchange_n(&(irp)->CancelRoutine,(routine),__ATOMIC_ACQ_REL)
#endif
