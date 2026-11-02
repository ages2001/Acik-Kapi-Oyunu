/* Realtek PCI PHY firmware command format. No OS or allocation dependency.
 * The image must remain immutable throughout validation and execution.
 * USB r8152 firmware is a different format and must not use this interface.
 */
#ifndef RLTK_FWVM_H
#define RLTK_FWVM_H
typedef unsigned int RTL_FW_U32;
typedef unsigned short RTL_FW_U16;
typedef unsigned char RTL_FW_U8;
typedef char RTL_FW_U32_MUST_BE_32_BITS[sizeof(RTL_FW_U32)==4?1:-1];
enum RTL_FW_RESULT {
    RtlFwOk, RtlFwInvalidImage, RtlFwInvalidCommand, RtlFwIoError,
    RtlFwBudgetExceeded, RtlFwCancelled, RtlFwInvalidArgument
};
typedef struct {
    void *Context;
    /* mac=0: PHY MDIO; mac=1: MAC MCU. Return nonzero on success.
       The device owner supplies page translation and serialized access. */
    int (*Read)(void *,int mac,RTL_FW_U16 reg,RTL_FW_U16 *value);
    int (*Write)(void *,int mac,RTL_FW_U16 reg,RTL_FW_U16 value);
    int (*DelayMs)(void *,RTL_FW_U16 milliseconds);
    int (*Cancelled)(void *);
    RTL_FW_U32 MaxCommands,MaxDelayMs;
} RTL_FW_IO;
typedef struct {
    RTL_FW_U32 CommandCount,Executed,DelayMs,FailedIndex;
    char Version[33];
} RTL_FW_REPORT;
enum RTL_FW_RESULT RtlFwInspect(const RTL_FW_U8 *,RTL_FW_U32,RTL_FW_REPORT *);
enum RTL_FW_RESULT RtlFwExecute(const RTL_FW_U8 *,RTL_FW_U32,const RTL_FW_IO *,RTL_FW_REPORT *);
#endif
