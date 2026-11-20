/* Register the optional NT GPU interface. Win16 loads its sibling DRV on demand.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "platform.h"
#include "files.h"
#if defined(WXP_NT3) || defined(WXP_NT4) || defined(WXP_NT5)
static const char service[] = "SYSTEM\\CurrentControlSet\\Services\\WxpGpu";
static const char image[] = "system32\\drivers\\wxpgpu.sys";

static int target_path(char *dst)
{
    char system[PATH_CAP], drivers[PATH_CAP];
    UINT n = GetSystemDirectory(system,sizeof(system));
    return n && n < sizeof(system) && path_join(drivers,system,"drivers") && path_join(dst,drivers,"WXPGPU.SYS");
}
static int number(HKEY key,const char *name,DWORD value)
{ return RegSetValueEx(key,name,0,REG_DWORD,(const BYTE *)&value,sizeof(value))==ERROR_SUCCESS; }
static int mark(const char *key) { return WritePrivateProfileString("VirtIO",key,"1",wxp_state); }
static int owned(const char *key) { return GetPrivateProfileInt("VirtIO",key,0,wxp_state)!=0; }

int install_accelerator(void)
{
    char dst[PATH_CAP],src[PATH_CAP];
    const char *name="WXPGPU.SYS";
    DWORD disposition;
    HKEY key;
    int ok;
#ifdef WXP_NT3
    if (HIBYTE(LOWORD(GetVersion())) < 50) name="WXPG31.SYS";
#endif
    if (!target_path(dst) || !path_join(src,wxp_folder,name)) return 0;
    if (!owned("File")) {
        if (!copy_new(src,dst)) return fail("Cannot install WXPGPU.SYS in the Windows drivers directory.");
        if (!mark("File")) { delete_owned(dst); return fail("Cannot record the installed 3D driver."); }
    }
    if (RegCreateKeyEx(HKEY_LOCAL_MACHINE,service,0,NULL,0,KEY_ALL_ACCESS,NULL,&key,&disposition))
        return fail("Cannot register the 3D driver. Run Setup as an administrator.");
    if (disposition==REG_CREATED_NEW_KEY) {
        if (!mark("Service")) {
            RegCloseKey(key); RegDeleteKey(HKEY_LOCAL_MACHINE,service);
            return fail("Cannot record ownership of the 3D service.");
        }
    } else if (!owned("Service")) { RegCloseKey(key); return fail("The WxpGpu service already exists."); }
    ok=RegSetValueEx(key,"ImagePath",0,REG_EXPAND_SZ,(const BYTE *)image,sizeof(image))==ERROR_SUCCESS &&
       number(key,"Type",1) && number(key,"ErrorControl",1) && number(key,"Start",1);
    RegCloseKey(key);
    return ok || fail("Cannot save the 3D service settings.");
}
int remove_accelerator(void)
{
    char dst[PATH_CAP];
    if (owned("Service")) {
        HKEY key;
        LONG rc=RegOpenKeyEx(HKEY_LOCAL_MACHINE,service,0,KEY_SET_VALUE,&key);
        if (rc==ERROR_SUCCESS) {
            number(key,"Start",4);
            RegCloseKey(key);
            /* PnP may create an Enum child on NT 5. */
            RegDeleteKey(HKEY_LOCAL_MACHINE,"SYSTEM\\CurrentControlSet\\Services\\WxpGpu\\Enum");
            rc=RegDeleteKey(HKEY_LOCAL_MACHINE,service);
        }
        if (rc && rc!=ERROR_FILE_NOT_FOUND) return fail("Cannot remove the 3D service.");
    }
    if (owned("File") && (!target_path(dst) || (!delete_owned(dst) && !MoveFileEx(dst,NULL,MOVEFILE_DELAY_UNTIL_REBOOT))))
        return fail("Restart Windows and run Uninstall again to remove the 3D driver.");
    return 1;
}
#else
int install_accelerator(void) { return 1; }
int remove_accelerator(void) { return 1; }
#endif
