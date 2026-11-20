#ifndef WINWDM_SCSI_H
#define WINWDM_SCSI_H

/* Include the native NT DDK and srb.h first. Link the x86 WINWDM archive
 * and the system SCSIPORT import library. */
#ifdef __cplusplus
extern "C" {
#endif
ULONG NTAPI WkScsiPortInitialize(PVOID driver, PVOID registryPath,
    PHW_INITIALIZATION_DATA initialization, PVOID context);
#ifdef __cplusplus
}
#endif
#endif
