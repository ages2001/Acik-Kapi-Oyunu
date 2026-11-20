/* Observe the real NT 3.1 loader's result for a local Forceware miniport.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <ntddk.h>

static WORK_QUEUE_ITEM work;

static void msg(const char *s)
{
    while (*s) __asm__ volatile("outb %0,$0xe9" : : "a"(*s++));
}

static void hex(ULONG value, unsigned width)
{
    char digits[9];
    unsigned i;
    for (i = width; i; --i) {
        digits[i - 1] = "0123456789abcdef"[value & 15];
        value >>= 4;
    }
    digits[width] = 0;
    msg(digits);
}

static void report(const char *what, NTSTATUS status)
{
    msg(what); msg(" NTSTATUS "); hex((ULONG)status, 8); msg("\n");
}

static NTSTATUS value(HANDLE key, PCWSTR name, ULONG type, PVOID data, ULONG size)
{
    UNICODE_STRING n;
    RtlInitUnicodeString(&n, name);
    return ZwSetValueKey(key, &n, 0, type, data, size);
}

static NTSTATUS load(PCWSTR path, PCWSTR image, ULONG pci)
{
    ULONG type = 1, start = 3, error = 1;
    UNICODE_STRING service, file;
    OBJECT_ATTRIBUTES oa;
    NTSTATUS status;
    HANDLE key;
    RtlInitUnicodeString(&service, path);
    RtlInitUnicodeString(&file, image);
    InitializeObjectAttributes(&oa, &service, OBJ_CASE_INSENSITIVE, NULL, NULL);
    status = ZwCreateKey(&key, KEY_SET_VALUE, &oa, 0, NULL, REG_OPTION_NON_VOLATILE, NULL);
    if (!NT_SUCCESS(status)) { report("Create service key", status); return status; }
    status = value(key, L"Type", REG_DWORD, &type, sizeof(type));
    if (NT_SUCCESS(status)) status = value(key, L"Start", REG_DWORD, &start, sizeof(start));
    if (NT_SUCCESS(status)) status = value(key, L"ErrorControl", REG_DWORD, &error, sizeof(error));
    if (NT_SUCCESS(status)) status = value(key, L"WinxpdmPciId", REG_DWORD, &pci, sizeof(pci));
    if (NT_SUCCESS(status)) status = value(key, L"ImagePath", REG_EXPAND_SZ, file.Buffer, file.Length + sizeof(WCHAR));
    ZwClose(key);
    if (!NT_SUCCESS(status)) { report("Write service settings", status); return status; }
    return ZwLoadDriver(&service);
}

static VOID NTAPI run(PVOID unused)
{
    NTSTATUS status;
    LARGE_INTEGER delay;
    ULONG id = 0;
    UNREFERENCED_PARAMETER(unused);
    delay.QuadPart = -20000000;
    KeDelayExecutionThread(KernelMode, FALSE, &delay);
    if (HalGetBusDataByOffset(PCIConfiguration, 0, 6, &id, 0, sizeof(id)) != sizeof(id) ||
        (id & 0xffff) != 0x10de) { msg("TEST COMPLETE NVIDIA board missing\n"); return; }
    msg("PCI "); hex(id & 0xffff, 4); msg(":"); hex(id >> 16, 4); msg(" at bus 0 slot 6\n");
    msg("Calling ZwLoadDriver for the control image\n");
    status = load(L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\WxpControl",
                  L"\\SystemRoot\\System32\\Drivers\\W3CTRL.SYS", id);
    report("Control ZwLoadDriver", status);
    if (!NT_SUCCESS(status)) { msg("TEST COMPLETE control failed\n"); return; }
    status = load(L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\WxpSmall",
                  L"\\SystemRoot\\System32\\Drivers\\W3SMALL.SYS", id);
    report("32-byte aligned control ZwLoadDriver", status);
    status = load(L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\WxpAligned",
                  L"\\SystemRoot\\System32\\Drivers\\W3ALIGN.SYS", id);
    report("Repacked control ZwLoadDriver", status);
    if (!NT_SUCCESS(status)) { msg("TEST COMPLETE repacked control failed\n"); return; }
    status = load(L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\WxpMiss",
                  L"\\SystemRoot\\System32\\Drivers\\W3MISS.SYS", id);
    report("Missing import control ZwLoadDriver", status);
    msg("Calling ZwLoadDriver for the private NVIDIA miniport\n");
    status = load(L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\WxpNvLab",
                  L"\\SystemRoot\\System32\\Drivers\\NV4_MINI.SYS", id);
    report("Forceware ZwLoadDriver", status);
    msg("TEST COMPLETE loader returned\n");
}

NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING key)
{
    UNREFERENCED_PARAMETER(driver);
    UNREFERENCED_PARAMETER(key);
    ExInitializeWorkItem(&work, run, NULL);
    ExQueueWorkItem(&work, DelayedWorkQueue);
    msg("NT 3.1 deferred driver-load test registered\n");
    return STATUS_SUCCESS;
}
