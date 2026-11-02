/* Test-only Windows 2000 console observer for the fictional NIC/HID fixture.
 * QMP injects the HID report; this program requires Windows console key events.
 */
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0400
#include <windows.h>
static HANDLE log_file = INVALID_HANDLE_VALUE;
static void out(const char *text)
{
    DWORD n, len = lstrlenA(text);
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text, len, &n, 0);
    if (log_file != INVALID_HANDLE_VALUE) WriteFile(log_file, text, len, &n, 0);
}
void Entry(void)
{
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD old_mode = 0, started, n;
    INPUT_RECORD record;
    int down = 0, ok = 0;
    log_file = CreateFileA("C:\\MIXTEST\\KEY.LOG", GENERIC_WRITE, FILE_SHARE_READ,
                           0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (log_file == INVALID_HANDLE_VALUE || !GetConsoleMode(input, &old_mode)) goto done;
    SetFilePointer(log_file, 0, 0, FILE_END);
    out("RUN "); out(GetCommandLineA()); out("\r\n");
    if (!SetConsoleMode(input, 0)) goto done;
    FlushConsoleInputBuffer(input);
    out("READY: waiting for composite HID B down/up\r\n");
    started = GetTickCount();
    while (GetTickCount() - started < 30000) {
        if (WaitForSingleObject(input, 100) != WAIT_OBJECT_0) continue;
        if (!ReadConsoleInputA(input, &record, 1, &n) || n != 1) break;
        if (record.EventType != KEY_EVENT || record.Event.KeyEvent.wVirtualKeyCode != 'B' ||
            record.Event.KeyEvent.wVirtualScanCode != 0x30) continue;
        if (record.Event.KeyEvent.bKeyDown) {
            if (!down) out("KEY B DOWN SCAN 30\r\n");
            down = 1;
        } else if (down) {
            out("KEY B UP SCAN 30\r\n"); ok = 1; break;
        }
    }
    SetConsoleMode(input, old_mode);
done:
    out(ok ? "PASS WINDOWS COMPOSITE HID INPUT\r\n" : "FAIL WINDOWS COMPOSITE HID INPUT\r\n");
    if (log_file != INVALID_HANDLE_VALUE) CloseHandle(log_file);
    ExitProcess(ok ? 0 : 1);
}
