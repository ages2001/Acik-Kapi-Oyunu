/* Native driver selection uses the guest's display class installer.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "platform.h"
#include "files.h"

#ifdef WXP_NT5
#include <setupapi.h>
#include <devguid.h>
typedef BOOL (WINAPI *update_fn)(HWND,LPCSTR,LPCSTR,DWORD,PBOOL);
typedef BOOL (WINAPI *uninstall_inf_fn)(LPCSTR,DWORD,PVOID);
static const char hardware[] = "PCI\\VEN_1234&DEV_1111";

/* Save the actual installed INF before binding our driver. */
static int adapter_info(char *service, char *inf, char *match)
{
    HDEVINFO devs;
    SP_DEVINFO_DATA dev;
    DWORD index, size;
    char ids[2048], *id, base[sizeof(hardware)];
    HKEY key;
    int found=0;
    service[0]=0; inf[0]=0; match[0]=0;
    devs=SetupDiGetClassDevs(NULL,"PCI",NULL,DIGCF_PRESENT|DIGCF_ALLCLASSES);
    if (devs==INVALID_HANDLE_VALUE) return 0;
    dev.cbSize=sizeof(dev);
    for (index=0; !found && SetupDiEnumDeviceInfo(devs,index,&dev); ++index) {
        if (!SetupDiGetDeviceRegistryProperty(devs,&dev,SPDRP_HARDWAREID,NULL,
                                               (BYTE*)ids,sizeof(ids)-2,&size) || size<2) continue;
        ids[size]=0; ids[size+1]=0;
        for (id=ids; *id; id+=lstrlen(id)+1) {
            int n=lstrlen(id);
            if (n<(int)sizeof(hardware)-1 || n>=PATH_CAP) continue;
            lstrcpyn(base,id,sizeof(base));
            if (!lstrcmpi(base,hardware) && (!id[sizeof(base)-1] || id[sizeof(base)-1]=='&')) {
                lstrcpy(match,id);
                found=1;
                break;
            }
        }
        if (!found) continue;
        SetupDiGetDeviceRegistryProperty(devs,&dev,SPDRP_SERVICE,NULL,(BYTE*)service,PATH_CAP,NULL);
        key=SetupDiOpenDevRegKey(devs,&dev,DICS_FLAG_GLOBAL,0,DIREG_DRV,KEY_READ);
        if (key!=INVALID_HANDLE_VALUE) {
            size=PATH_CAP;
            if (RegQueryValueEx(key,"InfPath",NULL,NULL,(BYTE*)inf,&size)!=ERROR_SUCCESS) inf[0]=0;
            inf[PATH_CAP-1]=0;
            RegCloseKey(key);
        }
    }
    SetupDiDestroyDeviceInfoList(devs);
    return found;
}
static int update(HWND owner,const char *id,const char *inf)
{
    HMODULE lib=LoadLibrary("newdev.dll");
    update_fn fn=lib ? (update_fn)(void*)GetProcAddress(lib,"UpdateDriverForPlugAndPlayDevicesA") : NULL;
    BOOL reboot=FALSE;
    int ok=fn && fn(owner,id,inf,1,&reboot);
    DWORD err=GetLastError();
    if (lib) FreeLibrary(lib);
    if (!ok) wsprintf(wxp_error,"Windows could not select the display driver (error %lu).",err);
    return ok;
}
int install_driver(HWND parent)
{
    char service[PATH_CAP],old[PATH_CAP],path[PATH_CAP],published[PATH_CAP],id[PATH_CAP];
    if (!adapter_info(service,old,id)) return fail("Windows could not find the QEMU Standard VGA adapter.");
    if (!GetPrivateProfileInt("Driver","Saved",0,wxp_state)) {
        if (!lstrcmpi(service,"wxpdm")) return fail("Another installation already owns the WINXPDM device binding.");
        if (!WritePrivateProfileString("Driver","PreviousInf",old,wxp_state) ||
            !WritePrivateProfileString("Driver","Saved","1",wxp_state))
            return fail("Cannot save the previous driver selection.");
    }
    if (lstrcmpi(service,"wxpdm") &&
        (!path_join(path,wxp_folder,"WXPDM.INF") || !update(parent,id,path))) return 0;
    if (!adapter_info(service,published,id) || lstrcmpi(service,"wxpdm"))
        return fail("Windows has not confirmed the WINXPDM device binding. Check Device Manager after restarting.");
    return WritePrivateProfileString("Driver","PublishedInf",published,wxp_state) ||
           fail("Could not save the installed INF name.");
}
int remove_driver(HWND parent)
{
    char service[PATH_CAP],inf[PATH_CAP],path[PATH_CAP],windows[PATH_CAP],folder[PATH_CAP],id[PATH_CAP];
    HMODULE setup;
    uninstall_inf_fn remove_inf;
    if (!adapter_info(service,inf,id)) return fail("Reconnect the QEMU display adapter before uninstalling.");
    if (!lstrcmpi(service,"wxpdm")) {
        GetPrivateProfileString("Driver","PreviousInf","",inf,sizeof(inf),wxp_state);
        if (!inf[0]) return fail("Select a replacement display driver in Device Manager, then run Uninstall again.");
        if (!GetWindowsDirectory(windows,sizeof(windows)) || !path_join(folder,windows,"INF") ||
            !path_join(path,folder,inf) || !update(parent,id,path)) return 0;
    }
    if (!adapter_info(service,inf,id) || !lstrcmpi(service,"wxpdm"))
        return fail("Restart with the replacement display driver, then run Uninstall again.");
    GetPrivateProfileString("Driver","PublishedInf","",inf,sizeof(inf),wxp_state);
    setup=GetModuleHandle("setupapi.dll");
    remove_inf=setup ? (uninstall_inf_fn)(void*)GetProcAddress(setup,"SetupUninstallOEMInfA") : NULL;
    if (remove_inf && inf[0] && !remove_inf(inf,0,NULL) && GetLastError()!=ERROR_FILE_NOT_FOUND)
        return fail("Windows still has the WINXPDM driver package in use. Restart and run Uninstall again.");
    return 1;
}
#else

/* On Windows 9x, the PnP node points to the live display class instance. */
static int selected(void)
{
    HKEY root,model,node,settings;
    DWORD i,j,size;
    char modelname[200],instance[200],key[PATH_CAP],classid[80],driver[80];
    int active=0;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,"Enum\\PCI",0,KEY_READ,&root)) return 0;
    for (i=0;RegEnumKey(root,i,modelname,sizeof(modelname))==ERROR_SUCCESS;++i) {
        char prefix[18];
        lstrcpyn(prefix,modelname,18);
        if (lstrcmpi(prefix,"VEN_1234&DEV_1111")) continue;
        if (RegOpenKeyEx(root,modelname,0,KEY_READ,&model)) continue;
        for (j=0;RegEnumKey(model,j,instance,sizeof(instance))==ERROR_SUCCESS;++j) {
            if (RegOpenKeyEx(model,instance,0,KEY_READ,&node)) continue;
            size=sizeof(classid);
            if (!RegQueryValueEx(node,"Driver",NULL,NULL,(BYTE*)classid,&size)) {
                wsprintf(key,"System\\CurrentControlSet\\Services\\Class\\%s\\DEFAULT",classid);
                if (!RegOpenKeyEx(HKEY_LOCAL_MACHINE,key,0,KEY_READ,&settings)) {
                    size=sizeof(driver);
                    if (!RegQueryValueEx(settings,"drv",NULL,NULL,(BYTE*)driver,&size) &&
                        !lstrcmpi(driver,"wxpdm.drv")) active=1;
                    RegCloseKey(settings);
                }
            }
            RegCloseKey(node);
        }
        RegCloseKey(model);
    }
    RegCloseKey(root);
    return active;
}
static int select_driver(HWND parent,int installing)
{
    char text[650], sys[PATH_CAP], exe[PATH_CAP], launch[PATH_CAP+35];
    const char *file="WXPDM.INF";
    const char *app="control.exe";
    if (selected()==installing) return 1;
    if (installing)
        wsprintf(text,"In Windows display setup, change the display adapter. Choose Have Disk or Other, then select %s\\%s. Select WINXPDM and return here before restarting.",wxp_folder,file);
    else lstrcpy(text,"In Windows display setup, select your previous display driver or Standard VGA. Return here before restarting.");
    if (MessageBox(parent,text,"Select the display driver",MB_OKCANCEL|MB_ICONINFORMATION)!=IDOK)
        return fail("The display selection was cancelled.");
    if (!GetWindowsDirectory(sys,sizeof(sys)) || !path_join(exe,sys,app)) return 0;
    wsprintf(launch,"\"%s\" desk.cpl",exe);
    if (WinExec(launch,SW_SHOWNORMAL)<32) return fail("Windows could not open its display setup.");
    if (MessageBox(parent,"Finish the change in Windows display setup, then choose OK.","WINXPDM",MB_OKCANCEL)!=IDOK ||
        selected()!=installing)
        return fail("Windows has not confirmed the display selection. Finish the change in display setup and retry.");
    return 1;
}
int install_driver(HWND parent) { return select_driver(parent,1); }
int remove_driver(HWND parent) { return select_driver(parent,0); }
#endif
