/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "platform.h"
#include "files.h"
#ifdef _WIN32
#define new_dir(p) CreateDirectory(p,NULL)
#define drop_dir(p) RemoveDirectory(p)
#else
#include <dos.h>
#include <direct.h>
#include <io.h>
#define new_dir(p) (_mkdir(p)==0)
#define drop_dir(p) (_rmdir(p)==0)
#endif

char wxp_error[420], wxp_folder[PATH_CAP], wxp_source[PATH_CAP], wxp_state[PATH_CAP];
int wxp_program_manager, wxp_keep_display;
static const char *const payload[] = {
    "INFO.EXE", "SETUP.EXE", "GPUVIEW.EXE",
#if defined(WXP_WIN16)
    "WXPGPU.DRV", "WXPDM.DRV",
#elif defined(WXP_9X)
    "WXPGPU.DRV", "WXPDM.DRV", "WXPDM.386", "WVBDD32.DLL", "WXPDM.INF",
#elif defined(WXP_NT3)
    "WXPG31.SYS", "WXPGPU.SYS", "WXPDM.SYS", "WXPDM.DLL", "OEMSETUP.INF",
#else
    "WXPGPU.SYS", "WXPDM.SYS", "WXPDM.DLL", "WXPDM.INF",
#endif
    NULL
};

int fail(const char *message)
{
    lstrcpy(wxp_error, message);
    return 0;
}

int path_join(char *out, const char *dir, const char *name)
{
    int len = lstrlen(dir);
    if (!len || len + lstrlen(name) + 2 >= PATH_CAP)
        return fail("The installation path is too long.");
    lstrcpy(out, dir);
    if (out[len-1] != '\\') lstrcat(out, "\\");
    lstrcat(out, name);
    return 1;
}

static int this_platform(void)
{
    DWORD v = GetVersion();
#ifdef WXP_WIN16
    wxp_program_manager = 1;
    return LOBYTE(LOWORD(v)) == 3 && (GetWinFlags() & WF_ENHANCED) &&
#ifdef WXP_WIN30
        HIBYTE(LOWORD(v)) == 0;
#else
        HIBYTE(LOWORD(v)) >= 10 && HIBYTE(LOWORD(v)) < 90;
#endif
#elif defined(WXP_NT3)
    wxp_program_manager = 1;
    return !(v & 0x80000000UL) && LOBYTE(LOWORD(v)) == 3;
#elif defined(WXP_9X)
    return (v & 0x80000000UL) && LOBYTE(LOWORD(v)) == 4;
#elif defined(WXP_NT4)
    return !(v & 0x80000000UL) && LOBYTE(LOWORD(v)) == 4;
#elif defined(_WIN64)
    return !(v & 0x80000000UL) && LOBYTE(LOWORD(v)) == 5 && HIBYTE(LOWORD(v)) == 2;
#else
    typedef BOOL (WINAPI *check_wow64)(HANDLE,PBOOL);
    check_wow64 query;
    BOOL wow64=FALSE;
    if ((v & 0x80000000UL) || LOBYTE(LOWORD(v)) != 5) return 0;
    query=(check_wow64)(void*)GetProcAddress(GetModuleHandle("kernel32.dll"),"IsWow64Process");
    return !query || (query(GetCurrentProcess(),&wow64) && !wow64);
#endif
}

int platform_init(void)
{
    char windows[PATH_CAP], *end;
    unsigned n = GetWindowsDirectory(windows, sizeof(windows));
    if (!this_platform()) return fail("Open the WINXPDM package for this Windows version and processor.");
    if (!n || n >= sizeof(windows) || !path_join(wxp_folder, windows, "WINXPDM") ||
        !path_join(wxp_state, wxp_folder, "STATE.INI")) return fail("Cannot locate the Windows directory.");
    n = GetModuleFileName(wxp_instance, wxp_source, sizeof(wxp_source));
    if (!n || n >= sizeof(wxp_source)) return fail("Cannot locate the package folder.");
    for (end = wxp_source+n; end > wxp_source && end[-1] != '\\'; --end) {}
    if (end == wxp_source) return fail("Cannot locate the installation files.");
    *end = 0;
    return 1;
}

int exists(const char *path)
{
#ifdef _WIN32
    return GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES;
#else
    unsigned attr;
    return _dos_getfileattr(path, &attr) == 0;
#endif
}

int delete_owned(const char *path)
{
    if (!exists(path)) return 1;
#ifdef _WIN32
    return DeleteFile(path) != 0;
#else
    return _unlink(path) == 0;
#endif
}

int copy_new(const char *from, const char *to)
{
#ifdef _WIN32
    if (CopyFile(from, to, TRUE)) {
        SetFileAttributes(to, FILE_ATTRIBUTE_NORMAL);
        return 1;
    }
#else
    static char buffer[4096];
    HFILE in, out;
    UINT count;
    int ok = 1;
    if (exists(to)) return 0;
    in = _lopen(from, OF_READ | OF_SHARE_DENY_WRITE);
    if (in == HFILE_ERROR) return 0;
    out = _lcreat(to, 0);
    if (out != HFILE_ERROR) {
        while ((count = _lread(in, buffer, sizeof(buffer))) != 0) {
            if (count == (UINT)HFILE_ERROR || _lwrite(out,buffer,count) != count) { ok = 0; break; }
        }
        if (_lclose(out) == HFILE_ERROR) ok = 0;
        if (!ok) delete_owned(to);
    } else ok = 0;
    _lclose(in);
    return ok;
#endif
    return 0;
}

static void undo_copies(int count)
{
    char path[PATH_CAP];
    while (count > 0) {
        if (path_join(path, wxp_folder, payload[--count])) delete_owned(path);
    }
    delete_owned(wxp_state);
    drop_dir(wxp_folder);
}

static int configure_product(HWND parent)
{
    return install_accelerator() && (GetPrivateProfileInt("Package","KeepDisplay",0,wxp_state) || install_driver(parent));
}

int install_product(HWND parent)
{
    char src[PATH_CAP], dst[PATH_CAP], mark[20];
    int i;
    if (exists(wxp_folder)) {
        GetPrivateProfileString("Package", "Owner", "", mark, sizeof(mark), wxp_state);
        if (lstrcmp(mark,"WINXPDM") == 0) {
            if (GetPrivateProfileInt("Package","Complete",0,wxp_state))
                return fail("WINXPDM is already installed. Open Info to view the active display.");
            if (!configure_product(parent)) return 0;
            return WritePrivateProfileString("Package","Complete","1",wxp_state) ||
                   fail("Could not record the completed installation.");
        }
        return fail("The destination folder already exists. Setup cannot claim its contents.");
    }
    if (!new_dir(wxp_folder)) return fail("Cannot create the product folder. Check your access and disk space.");
    for (i=0; payload[i]; ++i) {
        if (!path_join(src,wxp_source,payload[i]) || !path_join(dst,wxp_folder,payload[i]) || !copy_new(src,dst)) {
            wsprintf(wxp_error,"Could not copy %s from the package.",payload[i]);
            undo_copies(i);
            return 0;
        }
    }
    if (!WritePrivateProfileString("Package","Owner","WINXPDM",wxp_state) ||
        !WritePrivateProfileString("Package","KeepDisplay",wxp_keep_display ? "1" : "0",wxp_state)) {
        undo_copies(i); return fail("Cannot save the uninstall record.");
    }
    if (!change_shortcuts(parent,1)) { undo_copies(i); return 0; }
    /* Keep the recovery entry when the native installer needs another run. */
    if (!configure_product(parent)) return 0;
    return WritePrivateProfileString("Package","Complete","1",wxp_state) ||
           fail("Could not record the completed installation.");
}

int remove_product(HWND parent)
{
    char mark[20], path[PATH_CAP];
    int i;
    GetPrivateProfileString("Package","Owner","",mark,sizeof(mark),wxp_state);
    if (lstrcmp(mark,"WINXPDM")) return fail("The WINXPDM installation record is missing.");
    if (!GetPrivateProfileInt("Package","KeepDisplay",0,wxp_state) && !remove_driver(parent)) return 0;
    if (!remove_accelerator()) return 0;
    /* Keep the entry points and the recovery record until the driver files are free. */
    for (i=2; payload[i]; ++i) {
        if (!path_join(path,wxp_folder,payload[i]) || !delete_owned(path))
            return fail("The display change is saved. Restart Windows, then run Uninstall again to remove the driver files.");
    }
    if (!change_shortcuts(parent,0)) return 0;
    for (i=0; i<2; ++i) {
        if (!path_join(path,wxp_folder,payload[i]) || !delete_owned(path))
            return fail("Close other WINXPDM programs and run the package's SETUP.EXE /remove again.");
    }
    delete_owned(wxp_state);
    drop_dir(wxp_folder);
    return 1;
}

void describe_display(char *text)
{
    HDC dc=GetDC(NULL);
    DWORD v = GetVersion();
    wsprintf(text, "Windows %u.%02u\r\n\r\nActive display: %d x %d pixels\r\nColor depth: %d bits per pixel\r\nLogical resolution: %d dpi",
             (unsigned)LOBYTE(LOWORD(v)), (unsigned)HIBYTE(LOWORD(v)),
             GetDeviceCaps(dc,HORZRES),GetDeviceCaps(dc,VERTRES),
             GetDeviceCaps(dc,BITSPIXEL)*GetDeviceCaps(dc,PLANES),GetDeviceCaps(dc,LOGPIXELSX));
    ReleaseDC(NULL,dc);
}
