#ifndef WDM_KIT_PROTOCOL_H
#define WDM_KIT_PROTOCOL_H
/* Private VxD identifier for this development kit. */
#define WK_VXD_ID 0x7bd1
#define WK_SIGNATURE 0x574b
#define WK_DEVICE_TYPE 0x8337
#define WK_IOCTL ((0x8337UL << 16) | (0x800UL << 2))
#define WK_IOCTL_DEFERRED (WK_IOCTL + 4)
#define WK_IOCTL_STACK (WK_IOCTL + 12)
#define WK_IOCTL_PCI (WK_IOCTL + 16)
/* Internal-only stack depth diagnostic. */
#define WK_IOCTL_STACK_DEPTH (WK_IOCTL + 24)
/* FILE_WRITE_ACCESS. Uninstall detaches the kit's private filter before the
 * legacy I/O manager tests the device stack for outstanding attachments. */
#define WK_IOCTL_QUIESCE ((WK_IOCTL + 20) | (2UL << 14))
#define WK_IOCTL_TARGET_BASE (WK_IOCTL + 0x400UL)
#define WK_VXD_TARGET 0x5744
#define WK_VXD_PCI 0x5743
#define WK_STACK_BASE 1
#define WK_STACK_FILTER 2
#endif
