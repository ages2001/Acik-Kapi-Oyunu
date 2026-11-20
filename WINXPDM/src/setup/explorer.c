/* SPDX-License-Identifier: GPL-3.0-or-later */
#define COBJMACROS
#include "platform.h"
#include "files.h"
#include <shlobj.h>

static const char uninstall[] = "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\WINXPDM";
static int desktop_path(char *out)
{
    LPITEMIDLIST pidl=NULL;
    char folder[PATH_CAP];
    int ok;
    if (FAILED(SHGetSpecialFolderLocation(NULL,CSIDL_DESKTOPDIRECTORY,&pidl))) return 0;
    ok=SHGetPathFromIDList(pidl,folder) && path_join(out,folder,"WINXPDM Info.lnk");
    CoTaskMemFree(pidl);
    return ok;
}
static int create_link(const char *path,const char *exe)
{
    IShellLinkA *link=NULL;
    IPersistFile *file=NULL;
    WCHAR wide[PATH_CAP];
    HRESULT hr=CoCreateInstance(&CLSID_ShellLink,NULL,CLSCTX_INPROC_SERVER,&IID_IShellLinkA,(void**)&link);
    if (FAILED(hr)) return 0;
    hr=IShellLinkA_SetPath(link,exe);
    if (SUCCEEDED(hr)) hr=IShellLinkA_SetIconLocation(link,exe,0);
    if (SUCCEEDED(hr)) hr=IShellLinkA_SetDescription(link,"WINXPDM display information");
    if (SUCCEEDED(hr)) hr=IShellLinkA_QueryInterface(link,&IID_IPersistFile,(void**)&file);
    if (SUCCEEDED(hr)) {
        if (MultiByteToWideChar(CP_ACP,0,path,-1,wide,PATH_CAP))
            hr=IPersistFile_Save(file,wide,TRUE);
        else hr=E_FAIL;
        IPersistFile_Release(file);
    }
    IShellLinkA_Release(link);
    return SUCCEEDED(hr);
}
static int set_string(HKEY k, const char *name, const char *data)
{
    return RegSetValueEx(k,name,0,REG_SZ,(const BYTE*)data,lstrlen(data)+1)==ERROR_SUCCESS;
}
int change_shortcuts(HWND parent,int create)
{
    HKEY k;
    LONG status;
    DWORD disp;
    HRESULT hr;
    char dest[PATH_CAP], exe[PATH_CAP], cmd[PATH_CAP+16], icon[PATH_CAP+6];
    int ok;
    (void)parent;
    hr=CoInitialize(NULL);
    if (FAILED(hr)) return fail("The Windows shell could not initialize.");
    if (!desktop_path(dest) || !path_join(exe,wxp_folder,"INFO.EXE")) {
        CoUninitialize(); return fail("Cannot locate the desktop.");
    }
    if (create) {
        if (exists(dest)) { CoUninitialize(); return fail("A WINXPDM Info shortcut already exists on the desktop."); }
        status=RegCreateKeyEx(HKEY_LOCAL_MACHINE,uninstall,0,NULL,0,KEY_WRITE,NULL,&k,&disp);
        ok=(status==ERROR_SUCCESS);
        if (ok) {
            wsprintf(cmd,"\"%s\" /remove",exe); wsprintf(icon,"%s,1",exe);
            ok=disp==REG_CREATED_NEW_KEY && set_string(k,"DisplayName","WINXPDM") &&
               set_string(k,"DisplayIcon",icon) && set_string(k,"UninstallString",cmd) &&
               create_link(dest,exe);
            RegCloseKey(k);
            if (!ok && disp==REG_CREATED_NEW_KEY) RegDeleteKey(HKEY_LOCAL_MACHINE,uninstall);
        }
    } else {
        /* The installer remembers which user's desktop received the link. */
        GetPrivateProfileString("Shell","Shortcut",dest,exe,sizeof(exe),wxp_state);
        ok=delete_owned(exe);
        status=RegDeleteKey(HKEY_LOCAL_MACHINE,uninstall);
        ok=ok && (status==ERROR_SUCCESS || status==ERROR_FILE_NOT_FOUND);
    }
    if (create && ok) WritePrivateProfileString("Shell","Shortcut",dest,wxp_state);
    CoUninitialize();
    return ok ? 1 : fail("Windows could not update the desktop shortcut or the Add/Remove Programs entry.");
}
