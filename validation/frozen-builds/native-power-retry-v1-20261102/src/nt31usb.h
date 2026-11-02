/* ABI-neutral bridge between the NT3.1 MAC and the common USB queues.
   Include after the caller's NDIS scalar definitions. */
#ifndef RTL_NT31_USB_H
#define RTL_NT31_USB_H
VOID N31UsbInitialize(VOID);
int N31UsbCreate(PVOID config,PVOID owner,PVOID *context,PUCHAR mac);
VOID N31UsbDestroy(PVOID context);
VOID N31UsbReset(PVOID context);
VOID N31UsbFilter(PVOID context);
VOID N31UsbPoll(PVOID context);
int N31UsbSend(PVOID context,PUCHAR frame,UINT length);
int N31UsbLink(PVOID context,PULONG speed);
VOID N31UsbName(PVOID context,char *name,UINT capacity);
VOID RtlUsbNtClientStart(PVOID driver);
VOID N31UsbReceive(PVOID owner,PUCHAR frame,UINT length);
VOID N31UsbReceiveComplete(PVOID owner);
#endif
