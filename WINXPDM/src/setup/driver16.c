/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "platform.h"
#include "files.h"

static const char missing[] = "{absent}";
static const char unsaved[] = "{not recorded}";
static const char *const section[] = {"boot", "boot.description", "386Enh", "WINXPDM","WINXPDM","WINXPDM"};
static const char *const key[] = {"display.drv","display.drv","display","Width","Height","Depth"};
static const char *const value[] = {NULL,"WINXPDM QEMU Standard VGA","*vddvga","800","600","16"};
#define NUM_SETTINGS 6

/* The full module path keeps the previous display module available for rollback. */
static int configure(int installing)
{
    char ini[PATH_CAP], windows[PATH_CAP], module[PATH_CAP], current[PATH_CAP], old[PATH_CAP], slot[8];
    int i;
    if (!GetWindowsDirectory(windows,sizeof(windows)) || !path_join(ini,windows,"SYSTEM.INI") ||
        !path_join(module,wxp_folder,"WXPDM.DRV")) return 0;
    for (i=0;i<NUM_SETTINGS;++i) {
        const char *setting=i ? value[i] : module;
        wsprintf(slot,"Key%d",i);
        GetPrivateProfileString(section[i],key[i],missing,current,sizeof(current),ini);
        GetPrivateProfileString("Previous",slot,unsaved,old,sizeof(old),wxp_state);
        if (installing) {
            if ((!lstrcmp(old,unsaved) && !WritePrivateProfileString("Previous",slot,current,wxp_state)) ||
                !WritePrivateProfileString(section[i],key[i],setting,ini))
                return fail("Cannot update SYSTEM.INI. Use Uninstall to restore the previous settings.");
        } else if (!lstrcmpi(current,setting) && lstrcmp(old,unsaved)) {
            if (!WritePrivateProfileString(section[i],key[i],
                    lstrcmp(old,missing) ? old : NULL,ini))
                return fail("Cannot restore a saved SYSTEM.INI setting.");
        }
    }
    return 1;
}
int install_driver(HWND parent) { (void)parent; return configure(1); }
int remove_driver(HWND parent) { (void)parent; return configure(0); }
