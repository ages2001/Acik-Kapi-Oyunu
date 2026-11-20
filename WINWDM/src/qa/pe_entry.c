/* Read-only x86 PE loader control for QEMU (-debugcon file:pe.log).
 * Report the test service suffix, create no devices and decline the start. */
#include <ntddk.h>

static VOID debugByte(UCHAR value)
{
    __asm__ volatile ("outb %0, $0xe9" : : "a" (value));
}

NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING path)
{
    UNICODE_STRING prefix;
    USHORT i, length;
    (void)driver;
    RtlInitUnicodeString(&prefix, L"WINWDM PE ENTRY ");
    for (i = 0; i < prefix.Length / sizeof(WCHAR); ++i) debugByte((UCHAR)prefix.Buffer[i]);
    if (path && path->Buffer) {
        length = path->Length / sizeof(WCHAR);
        for (i = length; i && path->Buffer[i - 1] != L'\\'; --i) {
            /* Keep only the final service name. */
        }
        for (; i < length; ++i) debugByte((UCHAR)path->Buffer[i]);
    }
    debugByte('\n');
    return STATUS_NO_SUCH_DEVICE;
}
