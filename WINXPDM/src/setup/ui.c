/* WINXPDM Setup and Info. SPDX-License-Identifier: GPL-3.0-or-later */
#include "platform.h"
#include "ui.h"
#include "files.h"

HINSTANCE wxp_instance;
static int page;
static int removing;
static int working;
static int info;
static HICON artwork;
static char description[550];

static void block(HDC dc, int x, int y, int w, int h, COLORREF color)
{
    RECT r;
    HBRUSH brush = CreateSolidBrush(color);
    SetRect(&r, x, y, x + w, y + h);
    FillRect(dc, &r, brush);
    DeleteObject(brush);
}

LRESULT FARCB art_proc(HWND window, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        RECT r;
        HDC dc = BeginPaint(window, &ps);
        GetClientRect(window, &r);
        block(dc, 0, 0, r.right, r.bottom, RGB(0,0,128));
        block(dc, 0, 0, r.right, 4, RGB(0,128,128));
        block(dc, 0, 4, r.right, 2, RGB(0,255,255));
        block(dc, 0, r.bottom-16, r.right, 5, RGB(0,128,128));
        block(dc, 0, r.bottom-9, r.right, 3, RGB(0,255,255));
        DrawIcon(dc, (r.right-GetSystemMetrics(SM_CXICON))/2,
                 (r.bottom-GetSystemMetrics(SM_CYICON))/2, artwork);
        EndPaint(window, &ps);
        return 0;
    }
    return DefWindowProc(window, msg, wp, lp);
}

static void render(HWND d)
{
    const char *title, *body, *action;
    if (info && !removing) {
        title = "Display information";
        describe_display(description);
        body = description;
        action = "Close";
    } else if (page == 0) {
        title = removing ? "Remove WINXPDM" : "Welcome to WINXPDM";
        body = removing ? "Remove the display package and its shortcuts from this computer."
                        : "Set up the WINXPDM display package and the Info application on this computer.";
        action = "&Next >";
    } else if (page == 1) {
        title = removing ? "Confirm removal" : "Ready to install";
        if (removing)
            wsprintf(description, "Remove WINXPDM from %s? Close other display utilities before continuing.", wxp_folder);
        else
            wsprintf(description, "Install to %s.\r\n\r\n%s", wxp_folder,
                wxp_program_manager ? "Program Manager will contain Info and Uninstall icons."
                                    : "Info will appear on the desktop. Use Add/Remove Programs to uninstall.");
        body = description;
        action = removing ? "&Remove" : "&Install";
    } else {
        title = removing ? "Removal complete" : "Installation complete";
        body = "Restart Windows to finish changing the display driver.";
        action = "&Finish";
    }
    SetDlgItemText(d, ID_TITLE, title);
    SetDlgItemText(d, ID_BODY, body);
    SetDlgItemText(d, IDOK, action);
    ShowWindow(GetDlgItem(d, ID_BACK), (info && !removing) || page == 2 ? SW_HIDE : SW_SHOW);
    EnableWindow(GetDlgItem(d, ID_BACK), page == 1);
    ShowWindow(GetDlgItem(d, IDCANCEL), (info && !removing) || page == 2 ? SW_HIDE : SW_SHOW);
    SetFocus(GetDlgItem(d, IDOK));
}

DLGRET FARCB dialog_proc(HWND d, UINT msg, WPARAM wp, LPARAM lp)
{
    (void)lp;
    switch (msg) {
    case WM_INITDIALOG:
        SetWindowText(d, removing ? "Uninstall WINXPDM" : info ? "WINXPDM Info" : "WINXPDM Setup");
        render(d);
        return FALSE;
    case WM_CLOSE:
        if (!working) EndDialog(d, 0);
        return TRUE;
    case WM_COMMAND:
        if (working) return TRUE;
        switch (LOWORD(wp)) {
        case IDCANCEL: EndDialog(d, 0); return TRUE;
        case ID_BACK:
            if (page == 1) { page = 0; render(d); }
            return TRUE;
        case IDOK:
            if ((info && !removing) || page == 2) { EndDialog(d, 1); return TRUE; }
            if (page == 1) {
                int ok;
                working = 1;
                EnableWindow(GetDlgItem(d, IDOK), FALSE);
                ok = removing ? remove_product(d) : install_product(d);
                working = 0;
                EnableWindow(GetDlgItem(d, IDOK), TRUE);
                if (!ok) {
                    MessageBox(d, wxp_error, "WINXPDM", MB_OK | MB_ICONEXCLAMATION);
                    return TRUE;
                }
            }
            ++page; render(d); return TRUE;
        }
    }
    return FALSE;
}

static int application(HINSTANCE instance, const char *args)
{
    WNDCLASS wc = {0};
    DLGPROC callback;
    int result;
    wxp_instance = instance;
#ifdef BUILD_INFO
    info = 1;
#endif
    removing = (lstrcmpi(args, "/remove") == 0);
    if (!platform_init()) {
        MessageBox(NULL, wxp_error, "WINXPDM", MB_OK | MB_ICONEXCLAMATION);
        return 1;
    }
#ifdef BUILD_INFO
    /* Run the removal wizard from a separate module so Info and Setup can
       both be removed from the product directory. */
    if (removing) {
        char src[PATH_CAP], tmp[PATH_CAP], exe[PATH_CAP], cmd[PATH_CAP+20];
        char *dot;
#ifdef _WIN32
        char tempdir[PATH_CAP];
        unsigned n=GetTempPath(sizeof(tempdir),tempdir);
        if (!n || n>=sizeof(tempdir) || !GetTempFileName(tempdir,"WXP",0,tmp)) return 1;
#else
        if (!GetTempFileName(0,"WXP",0,tmp)) return 1;
#endif
        lstrcpy(exe,tmp);
        for (dot=exe+lstrlen(exe); dot>exe && *dot!='.'; --dot) {}
        if (dot==exe) { delete_owned(tmp); return 1; }
        lstrcpy(dot,".EXE");
        if (!path_join(src,wxp_folder,"SETUP.EXE") || !copy_new(src,exe)) {
            delete_owned(tmp);
            MessageBox(NULL,"Cannot start the removal wizard. Run SETUP.EXE /remove from the package.","WINXPDM",MB_OK);
            return 1;
        }
        delete_owned(tmp);
#ifdef _WIN32
        wsprintf(cmd,"\"%s\" /remove",exe);
#else
        wsprintf(cmd,"%s /remove",exe);
#endif
        if (WinExec(cmd,SW_SHOWNORMAL)<32) {
            delete_owned(exe);
            MessageBox(NULL,"Windows could not start the removal wizard.","WINXPDM",MB_OK);
            return 1;
        }
        return 0;
    }
#endif
    artwork = LoadIcon(instance, MAKEINTRESOURCE(removing ? ID_REMOVE_ICON : ID_INFO_ICON));
    wc.hInstance = instance;
    wc.lpfnWndProc = art_proc;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "WXPArt";
    RegisterClass(&wc);
#ifdef _WIN32
    callback = dialog_proc;
#else
    callback = (DLGPROC)MakeProcInstance((FARPROC)dialog_proc, instance);
#endif
    result = DialogBox(instance, MAKEINTRESOURCE(ID_WIZARD), NULL, callback);
#ifndef _WIN32
    FreeProcInstance((FARPROC)callback);
#endif
    return result < 0;
}

#ifdef _WIN32
void WINAPI WinMainEntry(void)
{
    const char *p = GetCommandLine();
    if (*p == '"') { ++p; while (*p && *p != '"') ++p; if (*p) ++p; }
    else { while (*p && *p != ' ') ++p; }
    while (*p == ' ') ++p;
    ExitProcess(application(GetModuleHandle(NULL), p));
}
#else
int PASCAL WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR args, int show)
{
    (void)previous; (void)show;
    return application(instance, args);
}
#endif
