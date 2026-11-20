/* Verify physical-memory reporting and a committed/writable Win32 allocation.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <windows.h>
#include <stdio.h>

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR args, int show)
{
    MEMORYSTATUS memory = {sizeof(memory)};
    OSVERSIONINFOA version = {sizeof(version)};
    const DWORD size = 128UL * 1024 * 1024;
    DWORD offset;
    FILE *log = fopen("C:\\WRLIVE.TXT", "w");
    (void)instance; (void)previous; (void)args; (void)show;
    if (!log) return 1;
    GlobalMemoryStatus(&memory);
    GetVersionExA(&version);
    fprintf(log, "Windows %lu.%lu.%lu\nphysical_KiB=%lu\navailable_KiB=%lu\n",
            version.dwMajorVersion, version.dwMinorVersion, version.dwBuildNumber & 65535UL,
            memory.dwTotalPhys / 1024UL, memory.dwAvailPhys / 1024UL);
    fflush(log);
    volatile DWORD *data = VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!data) {
        fprintf(log, "VirtualAlloc error=%lu\n", GetLastError());
        fclose(log);
        return 1;
    }
    for (offset = 0; offset < size; offset += 4096)
        data[offset / sizeof(*data)] = offset ^ 0x75612526UL;
    for (offset = 0; offset < size; offset += 4096) {
        if (data[offset / sizeof(*data)] != (offset ^ 0x75612526UL)) {
            fprintf(log, "page mismatch at byte %lu\n", offset);
            fclose(log);
            return 1;
        }
    }
    if (!VirtualFree((void *)data, 0, MEM_RELEASE)) {
        fprintf(log, "VirtualFree error=%lu\n", GetLastError());
        fclose(log);
        return 1;
    }
    fprintf(log, "committed_written_read_released_MiB=128\n");
    fclose(log);
    /* A lab-only QEMU debug console marker after the results file is closed. */
    const char *marker = "WINRAM live check complete\n";
    while (*marker) {
        __asm__ volatile("outb %0, $0xe9" : : "a"((unsigned char)*marker++));
    }
    /* Flush the FAT volume through Windows before the host reads the log. */
    ExitWindowsEx(EWX_SHUTDOWN, 0);
    return 0;
}
