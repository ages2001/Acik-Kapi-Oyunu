/* NT3.1 x86 full-MAC completion macros. NdisCompleteReset and status
 * indication are DDK macros, not exports from the supplied NDIS.SYS.
 * This public full-MAC prefix is checked against NT3.1's actual
 * NdisRegisterProtocol and NdisCompleteOpen/CloseAdapter implementations.
 * See validation/nt31-completion-abi-20261102/manifest.json for provenance.
 * Do not use these layouts for miniport drivers or another architecture.
 */
#ifndef RLTK_NT31_NDIS_ABI_H
#define RLTK_NT31_NDIS_ABI_H
typedef VOID (STDC *N31_RESET_COMPLETE)(NDIS_HANDLE,NDIS_STATUS);
typedef VOID (STDC *N31_STATUS_INDICATE)(NDIS_HANDLE,NDIS_STATUS,PVOID,UINT);
typedef VOID (STDC *N31_STATUS_COMPLETE)(NDIS_HANDLE);
typedef struct {
    UCHAR MajorVersion,MinorVersion;USHORT Padding;ULONG Reserved;
    PVOID OpenComplete,CloseComplete,SendComplete,TransferComplete;
    N31_RESET_COMPLETE ResetComplete;
    PVOID RequestComplete,Receive,ReceiveComplete;
    N31_STATUS_INDICATE Status;
    N31_STATUS_COMPLETE StatusComplete;
    NDIS_STRING Name;
} N31_PROTOCOL_CHARACTERISTICS;
typedef struct {
    PVOID OpenQueue;
    NDIS_SPIN_LOCK ReferenceLock;
    USHORT ReferenceCount;BOOLEAN Closing;UCHAR Padding;
    UINT Length;
    N31_PROTOCOL_CHARACTERISTICS Characteristics;
} N31_PROTOCOL_PREFIX;
typedef struct {
    PVOID MacHandle,MacBindingHandle,AdapterHandle;
    N31_PROTOCOL_PREFIX *Protocol;
    NDIS_HANDLE ProtocolContext;
} N31_OPEN_PREFIX;
typedef char N31ProtocolCharsOffset[__builtin_offsetof(N31_PROTOCOL_PREFIX,Characteristics)==0x14?1:-1];
typedef char N31ProtocolResetOffset[__builtin_offsetof(N31_PROTOCOL_PREFIX,Characteristics.ResetComplete)==0x2c?1:-1];
typedef char N31ProtocolStatusOffset[__builtin_offsetof(N31_PROTOCOL_PREFIX,Characteristics.Status)==0x3c?1:-1];
typedef char N31OpenProtocolOffset[__builtin_offsetof(N31_OPEN_PREFIX,Protocol)==0x0c?1:-1];
typedef char N31OpenContextOffset[__builtin_offsetof(N31_OPEN_PREFIX,ProtocolContext)==0x10?1:-1];

static VOID N31CompleteReset(NDIS_HANDLE binding,NDIS_STATUS status)
{
    N31_OPEN_PREFIX *open=binding;
    open->Protocol->Characteristics.ResetComplete(open->ProtocolContext,status);
}
static VOID N31IndicateResetStatus(NDIS_HANDLE binding,NDIS_STATUS status)
{
    N31_OPEN_PREFIX *open=binding;
    open->Protocol->Characteristics.Status(open->ProtocolContext,status,NULL,0);
    open->Protocol->Characteristics.StatusComplete(open->ProtocolContext);
}
#endif
