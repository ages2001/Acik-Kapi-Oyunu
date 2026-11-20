/* XP video-port / kernel calls used by the i386 Forceware 382.69 miniport.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#define _X86_ 1
#include <ntddk.h>
#include <stdarg.h>
int __cdecl _vsnprintf(char *buffer, size_t size, const char *format, va_list args);

NTSTATUS NTAPI WxpGetVersion(PRTL_OSVERSIONINFOW info)
{
    UNICODE_STRING service_pack;
    ULONG length;
    if (!info || (info->dwOSVersionInfoSize != sizeof(*info) &&
        info->dwOSVersionInfoSize != sizeof(RTL_OSVERSIONINFOEXW)))
        return STATUS_INVALID_PARAMETER;
    PsGetVersion(&info->dwMajorVersion, &info->dwMinorVersion, &info->dwBuildNumber, &service_pack);
    info->dwPlatformId = 2; /* VER_PLATFORM_WIN32_NT */
    length = min(service_pack.Length, sizeof(info->szCSDVersion) - sizeof(WCHAR));
    RtlZeroMemory(info->szCSDVersion, sizeof(info->szCSDVersion));
    if (length && service_pack.Buffer) RtlCopyMemory(info->szCSDVersion, service_pack.Buffer, length);
    if (info->dwOSVersionInfoSize == sizeof(RTL_OSVERSIONINFOEXW)) {
        PRTL_OSVERSIONINFOEXW extended = (PRTL_OSVERSIONINFOEXW)info;
        RTL_QUERY_REGISTRY_TABLE query[2] = {{0}};
        ULONG service_version = 0, default_version = 0;
        const volatile KUSER_SHARED_DATA *shared = (const volatile KUSER_SHARED_DATA *)0xffdf0000;
        query[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
        query[0].Name = L"CSDVersion";
        query[0].EntryContext = &service_version;
        query[0].DefaultType = REG_DWORD;
        query[0].DefaultData = &default_version;
        query[0].DefaultLength = sizeof(default_version);
        NTSTATUS result = RtlQueryRegistryValues(RTL_REGISTRY_CONTROL, L"Windows", query, NULL, NULL);
        if (!NT_SUCCESS(result)) return result;
        extended->wServicePackMajor = (USHORT)(service_version >> 8);
        extended->wServicePackMinor = (USHORT)(service_version & 0xff);
        extended->wSuiteMask = (USHORT)shared->SuiteMask;
        extended->wProductType = (UCHAR)shared->NtProductType;
        extended->wReserved = 0;
    }
    return STATUS_SUCCESS;
}

ULONG __cdecl WxpDebugPrint(ULONG component, ULONG level, PCSTR format, ...)
{
    char output[512];
    va_list args;
    UNREFERENCED_PARAMETER(component);
    UNREFERENCED_PARAMETER(level);
    va_start(args, format);
    _vsnprintf(output, sizeof(output) - 1, format, args);
    va_end(args);
    output[sizeof(output) - 1] = 0;
    return DbgPrint("%s", output);
}

NTSTATUS NTAPI WxpEntry(PDRIVER_OBJECT driver, PUNICODE_STRING path)
{
    UNREFERENCED_PARAMETER(driver);
    UNREFERENCED_PARAMETER(path);
    return STATUS_SUCCESS;
}
