/* Test-only NT 3.x-compatible USBNT log capture; no driver configuration changes. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#define GET_LOG CTL_CODE(FILE_DEVICE_UNKNOWN,0x905,METHOD_BUFFERED,FILE_ANY_ACCESS)
static char buffer[32768];
void Entry(void)
{
    HANDLE device, file;
    DWORD n = 0, written = 0;
    device = CreateFileA("\\\\.\\UsbNt", 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                         0, OPEN_EXISTING, 0, 0);
    if (device == INVALID_HANDLE_VALUE) ExitProcess(1);
    if (!DeviceIoControl(device, GET_LOG, 0, 0, buffer, sizeof(buffer), &n, 0)) {
        CloseHandle(device);
        ExitProcess(2);
    }
    CloseHandle(device);
    file = CreateFileA("C:\\RLTTEST\\USBLOG.TXT", GENERIC_WRITE, FILE_SHARE_READ,
                       0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE) ExitProcess(3);
    if (!WriteFile(file, buffer, n, &written, 0) || written != n) {
        CloseHandle(file);
        ExitProcess(4);
    }
    CloseHandle(file);
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buffer, n, &written, 0);
    ExitProcess(0);
}
