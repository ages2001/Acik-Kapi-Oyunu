/* Run inside a disposable Windows 3.x guest with an existing C:\WDMQA folder.
 * /install=<version> operates the actual wizard. After restarting Windows,
 * /check=<version> uses the same live VxD entry points as WINWDM Info. */
#include "kit.h"
#include <stdio.h>
#include <string.h>

static HWND waitForButton(const char *caption)
{
    DWORD start = GetTickCount();
    HWND window, button;
    MSG message;
    char text[32];
    do {
        while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }
        Yield();
        window = FindWindow(NULL, "WINWDM Setup");
        button = window ? GetDlgItem(window, 111) : NULL;
        if (button && GetWindowText(button, text, sizeof(text)) && !lstrcmp(text, caption))
            return window;
    } while (GetTickCount() - start < 30000UL);
    return NULL;
}

static int install(int target, FILE *report)
{
    char command[] = "A:\\WIN3\\SETUP.EXE";
    char text[16], marker[PATH_CAP];
    OFSTRUCT info;
    HWND window, box;
    int i;
    UINT length = GetWindowsDirectory(marker, sizeof(marker));
    if (!length || length > sizeof(marker) - sizeof("\\WINWDM\\KIT.ID")) return 0;
    strcat(marker, "\\WINWDM\\KIT.ID");
    if (OpenFile(marker, &info, OF_EXIST) != HFILE_ERROR) {
        fputs("An earlier installation is present.\n", report);
        return 0;
    }
    if (OpenFile(command, &info, OF_EXIST) == HFILE_ERROR) command[0] = 'B';
    if (WinExec(command, SW_SHOW) <= 31) return 0;
    window = waitForButton("&Next >");
    if (!window) return 0;
    PostMessage(window, WM_COMMAND, 111, 0);
    window = waitForButton("&Install");
    if (!window) return 0;
    box = GetDlgItem(window, 114);
    if (!box || !IsWindowVisible(box) || SendMessage(box, CB_GETCOUNT, 0, 0) != WK_TARGET_COUNT ||
        SendMessage(box, CB_GETCURSEL, 0, 0) != WK_INDEX_600) return 0;
    for (i = 0; i < WK_TARGET_COUNT; ++i) {
        if (SendMessage(box, CB_GETLBTEXTLEN, i, 0) >= sizeof(text) ||
            SendMessage(box, CB_GETLBTEXT, i, (LPARAM)(LPSTR)text) == CB_ERR ||
            lstrcmp(text, WkTargets[i].label)) return 0;
    }
    if (SendMessage(box, CB_SETCURSEL, target, 0) != target) return 0;
    PostMessage(window, WM_COMMAND, 111, 0);
    window = waitForButton("&Finish");
    if (!window || OpenFile(marker, &info, OF_EXIST) == HFILE_ERROR) return 0;
    PostMessage(window, WM_COMMAND, 111, 0);
    fputs("All six labels and the default passed. Setup installed the selected target.\n", report);
    return 1;
}

int PASCAL WinMain(HINSTANCE app, HINSTANCE previous, LPSTR args, int show)
{
    const char *version;
    FILE *report;
    int target, active, ok, installing;
    (void)app; (void)previous; (void)show;
    installing = !strncmp(args, "/install=", 9);
    if (!installing && strncmp(args, "/check=", 7)) return 2;
    version = args + (installing ? 9 : 7);
    for (target = 0; target < WK_TARGET_COUNT; ++target)
        if (!strcmp(version, WkTargets[target].label)) break;
    if (target == WK_TARGET_COUNT) return 2;
    report = fopen(installing ? "C:\\WDMQA\\INSTALL.TXT" : "C:\\WDMQA\\LIVE.TXT", "w");
    if (!report) return 2;
    fprintf(report, "Target %s, GetWinFlags 0x%08lX\n", version, GetWinFlags());
    if (installing) {
        ok = install(target, report);
    } else {
        active = WkActiveTarget();
        fprintf(report, "Active target %s\n", active < 0 ? "unknown" : WkTargets[active].label);
        ok = (GetWinFlags() & WF_ENHANCED) && active == target && WkKernelReady() && WkPciCheck();
        if (ok) fputs("Enhanced mode, filter, deferred I/O and PCI counter passed.\n", report);
    }
    fprintf(report, "%s\n", ok ? "PASS" : "FAIL");
    fclose(report);
    return ok ? 0 : 1;
}
