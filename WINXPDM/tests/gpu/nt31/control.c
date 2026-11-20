/* Known-good native image to qualify the same loader and module search path.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <ntddk.h>
#ifdef WXP_MISSING_IMPORT
__declspec(dllimport) NTSTATUS NTAPI WxpMissingProbeEntry(void);
#endif

NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING key)
{
    ULONG major, minor, build;
    const char *s = "Control DriverEntry: kernel debugger data and W3PORT version PASS\n";
    UNREFERENCED_PARAMETER(driver);
    UNREFERENCED_PARAMETER(key);
#ifdef WXP_MISSING_IMPORT
    return WxpMissingProbeEntry();
#endif
    PsGetVersion(&major, &minor, &build, NULL);
    if (major != 3 || minor != 10 || !build || KD_DEBUGGER_ENABLED > 1) return STATUS_DATA_ERROR;
    while (*s) __asm__ volatile("outb %0,$0xe9" : : "a"(*s++));
    return STATUS_SUCCESS;
}
