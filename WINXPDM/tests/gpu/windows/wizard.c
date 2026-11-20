/* Native XP Explorer integration test. Run the named phases in a disposable guest.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>
#include "../../../src/setup/ui.h"

static const char uninstall_key[] = "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\WINXPDM";
static const char service_key[] = "SYSTEM\\CurrentControlSet\\Services\\WxpGpu";
static char desktop[MAX_PATH], state[MAX_PATH], kernel_image[MAX_PATH];

static HWND page(const char *title, const char *expected)
{
    unsigned i;
    for (i = 0; i < 200; ++i) {
        char caption[80];
        HWND dialog = FindWindowA("#32770", title);
        if (dialog && GetDlgItemTextA(dialog, ID_TITLE, caption, sizeof(caption)) &&
            !strcmp(caption, expected)) return dialog;
        Sleep(150);
    }
    printf("Missing window %s, page %s\n", title, expected);
    return NULL;
}

static int click(HWND dialog, int id)
{
    HWND control = GetDlgItem(dialog, id);
    return control && IsWindowVisible(control) && IsWindowEnabled(control) &&
           PostMessage(control, BM_CLICK, 0, 0);
}

static int uninstall_command(char *command)
{
    HKEY key;
    DWORD size = MAX_PATH + 32, type;
    LONG rc = RegOpenKeyExA(HKEY_LOCAL_MACHINE, uninstall_key, 0, KEY_QUERY_VALUE, &key);
    if (rc != ERROR_SUCCESS) return 0;
    rc = RegQueryValueExA(key, "UninstallString", NULL, &type, (BYTE *)command, &size);
    RegCloseKey(key);
    return rc == ERROR_SUCCESS && type == REG_SZ && size > 1 && command[size - 1] == 0;
}

static int install(void)
{
    HWND dialog;
    char command[MAX_PATH + 32];
    if (WinExec(".\\SETUP.EXE", SW_SHOW) < 32) return 0;
    dialog = page("WINXPDM Setup", "Welcome to WINXPDM");
    if (!dialog || !click(dialog, IDOK)) return 0;
    dialog = page("WINXPDM Setup", "Ready to install");
    if (!dialog || !click(dialog, ID_EXISTING)) return 0;
    for (unsigned i = 0; i < 40 && IsDlgButtonChecked(dialog, ID_EXISTING) != BST_CHECKED; ++i) Sleep(50);
    if (IsDlgButtonChecked(dialog, ID_EXISTING) != BST_CHECKED || !click(dialog, IDOK)) return 0;
    dialog = page("WINXPDM Setup", "Installation complete");
    if (!dialog || !click(dialog, IDOK)) return 0;
    return GetPrivateProfileIntA("Package", "KeepDisplay", 0, state) == 1 &&
           GetPrivateProfileIntA("Package", "Complete", 0, state) == 1 &&
           GetFileAttributesA(desktop) != INVALID_FILE_ATTRIBUTES && uninstall_command(command);
}

/* Capture the visible native window while its verified image is still present. */
static int capture(HWND window, const char *path)
{
    RECT rect;
    BITMAPINFO info = {0};
    BYTE *pixels = NULL;
    GetWindowRect(window, &rect);
    int width = rect.right - rect.left, height = rect.bottom - rect.top;
    if (width < 1 || height < 1 || width > 4096 || height > 4096) return 0;
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 24;
    HDC screen = GetDC(NULL), dc = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, (void **)&pixels, NULL, 0);
    HGDIOBJ old = bitmap && dc ? SelectObject(dc, bitmap) : NULL;
    int ok = old && BitBlt(dc, 0, 0, width, height, screen, rect.left, rect.top, SRCCOPY);
    if (ok) {
        GdiFlush();
        FILE *file = fopen(path, "wb");
        ok = file && fprintf(file, "P6\n%d %d\n255\n", width, height) > 0;
        for (int y = 0; ok && y < height; ++y) {
            BYTE *line = pixels + y * ((width * 3 + 3) & ~3);
            for (int x = 0; x < width; ++x) {
                BYTE b = line[x*3];
                line[x*3] = line[x*3+2];
                line[x*3+2] = b;
            }
            ok = fwrite(line, 3, width, file) == (size_t)width;
        }
        if (file && fclose(file)) ok = 0;
    }
    if (old) SelectObject(dc, old);
    if (bitmap) DeleteObject(bitmap);
    if (dc) DeleteDC(dc);
    if (screen) ReleaseDC(NULL, screen);
    return ok;
}

static int viewer(const char *capture_path)
{
    HWND dialog, window = NULL;
    unsigned attempt;
    if ((INT_PTR)ShellExecuteA(NULL, "open", desktop, NULL, NULL, SW_SHOW) <= 32) return 0;
    dialog = page("WINXPDM Info", "Display information");
    if (!dialog || !click(dialog, ID_3D)) return 0;
    for (attempt = 0; attempt < 250 && !window; ++attempt) {
        window = FindWindowA("WxpGpuView", "WINXPDM 3D");
        if (!window) Sleep(100);
    }
    if (!window) return 0;
    ShowWindow(window, SW_RESTORE);
    SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    for (attempt = 0; attempt < 10; ++attempt) {
        RECT r;
        int black = 0, green = 0, red = 0, blue = 0;
        GetClientRect(window, &r);
        int side = min(r.bottom - 48, r.right - 16);
        if (side <= 0) return 0;
        Sleep(250);
        HDC dc = GetDC(window);
        if (!dc) continue;
        for (int y = 0; y < 32; ++y) for (int x = 0; x < 32; ++x) {
            COLORREF c = GetPixel(dc, (r.right - side) / 2 + (2*x+1)*side/64, 8 + (2*y+1)*side/64);
            if (c == CLR_INVALID) continue;
            int rr = GetRValue(c), gg = GetGValue(c), bb = GetBValue(c);
            if (!(rr | gg | bb)) ++black;
            if (gg > 100 && gg > rr + 60 && gg > bb + 60) ++green;
            if (rr > bb + 60 && rr > gg + 60) ++red;
            if (bb > rr + 60 && bb > gg + 60) ++blue;
        }
        ReleaseDC(window, dc);
        if (black > 500 && green >= 16 && red > 16 && blue > 16) {
            printf("Viewer samples: black=%d green=%d red=%d blue=%d\n", black, green, red, blue);
            return !capture_path || capture(window, capture_path);
        }
    }
    return 0;
}

static int remove_installed(void)
{
    char command[MAX_PATH + 32];
    HWND dialog, view = FindWindowA("WxpGpuView", NULL), info = FindWindowA("#32770", "WINXPDM Info");
    if (view) PostMessage(view, WM_CLOSE, 0, 0);
    if (info) PostMessage(info, WM_CLOSE, 0, 0);
    Sleep(500);
    if (!uninstall_command(command) || WinExec(command, SW_SHOW) < 32) return 0;
    dialog = page("Uninstall WINXPDM", "Remove WINXPDM");
    if (!dialog || !click(dialog, IDOK)) return 0;
    dialog = page("Uninstall WINXPDM", "Confirm removal");
    if (!dialog || !click(dialog, IDOK)) return 0;
    dialog = page("Uninstall WINXPDM", "Removal complete");
    return dialog && click(dialog, IDOK);
}

static int absent_key(const char *name)
{
    HKEY key;
    LONG status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, name, 0, KEY_QUERY_VALUE, &key);
    if (status == ERROR_SUCCESS) RegCloseKey(key);
    return status == ERROR_FILE_NOT_FOUND;
}

int main(int argc, char **argv)
{
    char win[MAX_PATH], system[MAX_PATH], directory[MAX_PATH];
    int ok;
    if ((argc != 2 && !(argc == 3 && !strcmp(argv[1], "viewer"))) || !GetWindowsDirectoryA(win, sizeof(win)) || !GetSystemDirectoryA(system, sizeof(system)) ||
        !SHGetSpecialFolderPathA(NULL, directory, CSIDL_DESKTOPDIRECTORY, FALSE)) return 2;
    if (snprintf(desktop, sizeof(desktop), "%s\\WINXPDM Info.lnk", directory) >= (int)sizeof(desktop) ||
        snprintf(state, sizeof(state), "%s\\WINXPDM\\STATE.INI", win) >= (int)sizeof(state) ||
        snprintf(kernel_image, sizeof(kernel_image), "%s\\drivers\\WXPGPU.SYS", system) >= (int)sizeof(kernel_image)) return 2;
    if (!strcmp(argv[1], "install")) ok = install();
    else if (!strcmp(argv[1], "viewer")) ok = viewer(argc == 3 ? argv[2] : NULL);
    else if (!strcmp(argv[1], "remove")) ok = remove_installed();
    else if (!strcmp(argv[1], "cleanup")) {
        ok = GetFileAttributesA(desktop) == INVALID_FILE_ATTRIBUTES &&
             GetFileAttributesA(state) == INVALID_FILE_ATTRIBUTES &&
             GetFileAttributesA(kernel_image) == INVALID_FILE_ATTRIBUTES &&
             absent_key(uninstall_key) && absent_key(service_key);
    } else return 2;
    printf("XP wizard %s %s\n", argv[1], ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
