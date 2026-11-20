/* A system-start test driver for a disposable Windows 2000 x86 guest.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#define _X86_ 1
#include <ntddk.h>

extern ULONG NTAPI WxpCreateEvent(PVOID, ULONG, PVOID, PVOID *) __asm__("_VideoPortCreateEvent@16");
extern ULONG NTAPI WxpWait(PVOID, PVOID, PLARGE_INTEGER) __asm__("_VideoPortWaitForSingleObject@12");
extern ULONG NTAPI WxpDeleteEvent(PVOID, PVOID) __asm__("_VideoPortDeleteEvent@8");
extern NTSTATUS NTAPI WxpGetVersion(PRTL_OSVERSIONINFOW) __asm__("_RtlGetVersion@4");
extern ULONG __cdecl WxpDebugPrint(ULONG, ULONG, PCSTR, ...) __asm__("_DbgPrintEx");

static int verify_event(ULONG flags)
{
    PVOID event = NULL;
    LARGE_INTEGER poll;
    ULONG first, second;
    poll.QuadPart = 0;
    if (WxpCreateEvent(NULL, flags, NULL, &event) || !event) return 0;
    first = WxpWait(NULL, event, &poll);
    second = WxpWait(NULL, event, &poll);
    /* An auto-reset event consumes its one signal. A manual-reset event retains it. */
    return !WxpDeleteEvent(NULL, event) && first == ((flags & 2) ? 0UL : 258UL)
           && second == (flags == 3 ? 0UL : 258UL);
}

static void output(const char *message)
{
    while (*message)
        __asm__ volatile("outb %0, $0xe9" : : "a"((unsigned char)*message++));
}

NTSTATUS NTAPI ProbeEntry(PDRIVER_OBJECT driver, PUNICODE_STRING service)
{
    RTL_OSVERSIONINFOEXW version;
    ULONG i, major, minor, build;
    UNREFERENCED_PARAMETER(driver);
    UNREFERENCED_PARAMETER(service);
    output("WXP2K companion loaded\n");
    for (i = 0; i < 4; ++i) {
        if (!verify_event(i)) {
            output("WXP2K event failure\n");
            return STATUS_UNSUCCESSFUL;
        }
    }
    if (WxpWait(NULL, NULL, NULL) != 87 || WxpDeleteEvent(NULL, NULL) != 87) return STATUS_UNSUCCESSFUL;
    output("WXP2K notification and synchronization event checks passed\n");
    RtlZeroMemory(&version, sizeof(version));
    version.dwOSVersionInfoSize = sizeof(version);
    PsGetVersion(&major, &minor, &build, NULL);
    if (!NT_SUCCESS(WxpGetVersion((PRTL_OSVERSIONINFOW)&version)) ||
        version.dwMajorVersion != major || version.dwMinorVersion != minor ||
        version.dwBuildNumber != build || major != 5 || minor != 0 || !version.wProductType) {
        output("WXP2K version check failed\n");
        return STATUS_UNSUCCESSFUL;
    }
    output("WXP2K version agrees with running Windows 2000 kernel\n");
    WxpDebugPrint(0, 0, "WINXPDM Windows 2000 kernel build %lu\n", build);
    output("WXP2K debug printing returned\nWXP2K PASS\n");
    return STATUS_SUCCESS;
}
