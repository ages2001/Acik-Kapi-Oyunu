#include "common.h"
#include <commdlg.h>
char osname[96], appdir[MAX_PATH], appfile[MAX_PATH], drivers[MAX_PATH], self[MAX_PATH];
unsigned minor;
typedef LONG(WINAPI *RtlVer)(OSVERSIONINFOW *);
static int system_version(void) {
    DWORD v = GetVersion(), major = LOBYTE(LOWORD(v)), build = HIWORD(v);
    HMODULE nt = GetModuleHandleA("ntdll.dll");
    RtlVer get = nt ? (RtlVer)(void *)GetProcAddress(nt, "RtlGetVersion") : 0;
    minor = HIBYTE(LOWORD(v));
    if (get) {
        OSVERSIONINFOW info;
        memset(&info, 0, sizeof(info));
        info.dwOSVersionInfoSize = sizeof(info);
        if (get(&info) == 0) {
            major = info.dwMajorVersion;
            minor = info.dwMinorVersion;
            build = info.dwBuildNumber;
        }
    }
    if (!nt || (!get && (v & 0x80000000u))) {
        const char *name =
            major == 3 ? "Win32s"
                       : (major == 4 && minor == 0
                              ? "Windows 95"
                              : (major == 4 && minor == 10
                                     ? "Windows 98"
                                     : (major == 4 && minor == 90 ? "Windows Me" : "Windows")));
        lstrcpyA(osname, name);
        return 0;
    }
    if (major == 3 && minor == 10)
        lstrcpyA(osname, "Windows NT 3.1");
    else if (major == 3 && (minor == 50 || minor == 51))
        wsprintfA(osname, "Windows NT 3.%u", minor);
    else
        wsprintfA(osname, "Windows NT %u.%u, build %u", (unsigned)major, minor, (unsigned)build);
    return major == 3 && (minor == 10 || minor == 50 || minor == 51);
}
static int paths(void) {
    char dir[MAX_PATH];
    UINT n = GetWindowsDirectoryA(dir, MAX_PATH);
    if (!n || n >= MAX_PATH || !join(appdir, dir, TITLE) || !join(appfile, appdir, "NT3SCPRT.EXE"))
        return 0;
    n = GetSystemDirectoryA(dir, MAX_PATH);
    if (!n || n >= MAX_PATH || !join(drivers, dir, "drivers"))
        return 0;
    n = GetModuleFileNameA(0, self, MAX_PATH);
    return n && n < MAX_PATH;
}
static void perform(HWND window, unsigned action) {
    const char *err = 0;
    int changed = 0, remove = action == REMOVE;
    int adding = action == ADD || action == ADDBOOT;
    HANDLE mutex;
    DWORD acquired;
    OPENFILENAMEA ofn;
    char selected[MAX_PATH] = {0}, id[12], msg[320];
    if (adding || action == OPTICAL) {
        memset(&ofn, 0, sizeof(ofn));
        ofn.lStructSize = OPENFILENAME_SIZE_VERSION_400;
        ofn.hwndOwner = window;
        ofn.lpstrFilter = action == OPTICAL ? "NT4 optical class (CDROM.SYS)\0CDROM.SYS\0\0"
                                            : "SCSI miniports (*.SYS)\0*.SYS\0\0";
        ofn.lpstrFile = selected;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = action == OPTICAL ? "Select NT4 CDROM.SYS" : "Select an NT4 miniport";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
        if (!GetOpenFileNameA(&ofn))
            return;
    }
    if (remove && MessageBoxA(window,
                              "Remove the registered miniports and NT3SCPRT? "
                              "Move any dependent disks to a supported controller first.",
                              TITLE, MB_OKCANCEL | MB_ICONQUESTION) != IDOK)
        return;
    mutex = CreateMutexA(0, FALSE, "NT3SCPRT.Setup");
    acquired = mutex ? WaitForSingleObject(mutex, 0) : WAIT_FAILED;
    if (acquired != WAIT_OBJECT_0 && acquired != WAIT_ABANDONED) {
        MessageBoxA(window, "Setup is busy or the installation lock is unavailable.", TITLE, MB_OK);
        if (mutex)
            CloseHandle(mutex);
        return;
    }
    if (!remove)
        err = install_program();
    if (remove && !err)
        err = check_system_disk();
    if (remove && !err)
        err = remove_class();
    if (remove && !err)
        err = remove_drivers();
    if (!err)
        err = port_action(remove, &changed);
    if (!err && action == OPTICAL)
        err = install_class(selected);
    if (!err && adding)
        err = add_driver(selected, id, action == ADDBOOT);
    if (!err && !icons(remove))
        err = "Program Manager did not acknowledge the icon update. Open Program Manager and run "
              "setup again.";
    if (!err && remove)
        err = remove_program();
    ReleaseMutex(mutex);
    CloseHandle(mutex);
    if (err)
        MessageBoxA(window, err, TITLE, MB_OK | MB_ICONEXCLAMATION);
    else {
        if (action == OPTICAL)
            lstrcpyA(
                msg,
                "NT4 optical class registered. Restart Windows NT to use the CD-ROM class driver.");
        else if (action == ADDBOOT)
            wsprintfA(msg,
                      "Boot-start miniport registered as %s. Restart and verify disk access before "
                      "moving the system volume.",
                      id);
        else if (adding)
            wsprintfA(msg, "Miniport registered as %s. Restart Windows NT to load the driver.", id);
        else if (remove)
            lstrcpyA(msg, "NT3SCPRT removed. Restart Windows NT to complete removal.");
        else if (changed)
            lstrcpyA(msg, "NT3SCPRT installed. The original system port was backed up. Add a "
                          "miniport and restart Windows NT.");
        else
            lstrcpyA(msg, "NT3SCPRT installed. Use Add driver to select an NT4 SCSI miniport.");
        MessageBoxA(window, msg, TITLE, MB_OK | MB_ICONINFORMATION);
    }
}
static INT_PTR CALLBACK dialog(HWND win, UINT msg, WPARAM wp, LPARAM lp) {
    (void)lp;
    if (msg == WM_INITDIALOG) {
        SetDlgItemTextA(win, FIELD_OS, osname);
        return TRUE;
    }
    if (msg == WM_COMMAND) {
        if (LOWORD(wp) == INSTALL || LOWORD(wp) == REMOVE || LOWORD(wp) == ADD ||
            LOWORD(wp) == ADDBOOT || LOWORD(wp) == OPTICAL) {
            perform(win, LOWORD(wp));
            return TRUE;
        }
        if (LOWORD(wp) == IDCANCEL || LOWORD(wp) == IDOK) {
            EndDialog(win, 0);
            return TRUE;
        }
    }
    if (msg == WM_CLOSE) {
        EndDialog(win, 0);
        return TRUE;
    }
    return FALSE;
}
void start(void) {
    char msg[140], *cmd = GetCommandLineA();
    int uninstall = 0;
    if (!system_version()) {
        wsprintfA(msg, "Unsupported OS (%s)", osname);
        MessageBoxA(0, msg, "NT3SCPRT Setup", MB_OK | MB_ICONSTOP);
        ExitProcess(1);
    }
    if (!paths()) {
        MessageBoxA(0, "The Windows directory path is too long.", TITLE, MB_OK);
        ExitProcess(2);
    }
    if (*cmd == '"') {
        ++cmd;
        while (*cmd && *cmd != '"')
            ++cmd;
        if (*cmd)
            ++cmd;
    } else
        while (*cmd && *cmd != ' ' && *cmd != '\t')
            ++cmd;
    while (*cmd == ' ' || *cmd == '\t')
        ++cmd;
    uninstall = !lstrcmpiA(cmd, "/remove");
    if (uninstall)
        perform(0, REMOVE);
    else if (DialogBoxParamA(GetModuleHandleA(0), MAKEINTRESOURCEA(DLG_MAIN), 0, dialog, 0) == -1) {
        MessageBoxA(0, "Cannot open the setup dialog.", TITLE, MB_OK | MB_ICONSTOP);
        ExitProcess(3);
    }
    ExitProcess(0);
}
