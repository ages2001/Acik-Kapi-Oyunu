/* Windows NT 3.x and NT 4.0 video service installation.
 * The Video group and Device0 values follow the WINVBE NT setup and
 * Windows NT OEM display INF. Keep the original VGA driver for recovery.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "platform.h"
#include "files.h"

static const char services[] = "SYSTEM\\CurrentControlSet\\Services";
static const char group_order[] = "SYSTEM\\CurrentControlSet\\Control\\GroupOrderList";
static const char *const names[] = {"WXPDM.SYS", "WXPDM.DLL"};
static const char *const file_keys[] = {"MiniportFile", "DisplayFile"};
static const char group[] = "Video", modules[] = "wxpdm\0";
static const char image[] = "\\SystemRoot\\System32\\drivers\\wxpdm.sys";

static int save(const char *key,const char *value)
{
    return WritePrivateProfileString("NTVideo",key,value,wxp_state);
}
static int recorded(const char *key)
{
    return GetPrivateProfileInt("NTVideo",key,0,wxp_state)!=0;
}
static int put_dword(HKEY key,const char *name,DWORD value)
{
    return RegSetValueEx(key,name,0,REG_DWORD,(const BYTE*)&value,sizeof(value))==ERROR_SUCCESS;
}
static int get_dword(HKEY key,const char *name,DWORD *value)
{
    DWORD type,size=sizeof(*value);
    return RegQueryValueEx(key,name,NULL,&type,(BYTE*)value,&size)==ERROR_SUCCESS &&
           type==REG_DWORD && size==sizeof(*value);
}
static LONG open_service_key(const char *name, REGSAM access, HKEY *key)
{
    char path[PATH_CAP];
    if (!path_join(path,services,name)) return ERROR_INVALID_NAME;
    return RegOpenKeyEx(HKEY_LOCAL_MACHINE,path,0,access,key);
}

static int system_file(int index,char *out)
{
    char sys[PATH_CAP],drivers[PATH_CAP];
    UINT n=GetSystemDirectory(sys,sizeof(sys));
    if (!n || n>=sizeof(sys)) return 0;
    if (index==1) return path_join(out,sys,names[index]);
    return path_join(drivers,sys,"drivers") && path_join(out,drivers,names[index]);
}
static int copy_driver_files(void)
{
    int i;
    char src[PATH_CAP],dst[PATH_CAP];
    for (i=0;i<2;++i) {
        if (!system_file(i,dst) || !path_join(src,wxp_folder,names[i])) return 0;
        if (recorded(file_keys[i]) && exists(dst)) continue;
        if (!copy_new(src,dst)) return fail("Cannot create the display driver files in System32. Check for an existing driver or insufficient access.");
        if (!save(file_keys[i],"1")) { delete_owned(dst); return fail("Cannot record the display driver files."); }
    }
    return 1;
}

/* A group tag before the current VGA tag gives WINXPDM the first display.
   Preserve all unrelated tags, including tags added after installation. */
static int video_tag(HKEY service,int install)
{
    DWORD tags[256],type,n=sizeof(tags),count,tag,i,pos,highest=0,vga_tag=0;
    HKEY order,vga;
    char number[16];
    int ok=0;
    if (!install && !recorded("Tag")) return 1;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,group_order,0,KEY_QUERY_VALUE|KEY_SET_VALUE,&order)) return 0;
    if (RegQueryValueEx(order,group,NULL,&type,(BYTE*)tags,&n) || type!=REG_BINARY ||
        n<sizeof(DWORD) || n%sizeof(DWORD) || tags[0]!=n/sizeof(DWORD)-1) goto done;
    count=tags[0];
    tag=GetPrivateProfileInt("NTVideo","Tag",0,wxp_state);
    if (!tag && install) {
        for (i=1;i<=count;++i) if (tags[i]>highest) highest=tags[i];
        if (highest>=0x7ffffffeUL) goto done;
        tag=highest+1;
        wsprintf(number,"%lu",tag);
        if (!save("Tag",number)) goto done;
    }
    for (pos=1;pos<=count && tags[pos]!=tag;++pos) {}
    if (install) {
        if (!put_dword(service,"Tag",tag)) goto done;
        if (pos<=count) { ok=1; goto done; }
        if (count+2>sizeof(tags)/sizeof(tags[0])) goto done;
        if (open_service_key("Vga",KEY_QUERY_VALUE,&vga)==ERROR_SUCCESS) {
            get_dword(vga,"Tag",&vga_tag); RegCloseKey(vga);
        }
        for (pos=1;pos<=count && tags[pos]!=vga_tag;++pos) {}
        if (pos>count) pos=(count && tags[1]==1) ? 2 : 1;
        for (i=count+1;i>pos;--i) tags[i]=tags[i-1];
        tags[pos]=tag; ++count;
    } else {
        if (pos>count) { ok=1; goto done; }
        for (i=pos;i<count;++i) tags[i]=tags[i+1];
        --count;
    }
    tags[0]=count;
    ok=RegSetValueEx(order,group,0,REG_BINARY,(const BYTE*)tags,(count+1)*sizeof(DWORD))==ERROR_SUCCESS;
done:
    RegCloseKey(order);
    return ok;
}

static int configure_service(void)
{
    HKEY root,svc,dev;
    DWORD disp;
    int ok=0;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,services,0,KEY_CREATE_SUB_KEY,&root)) return 0;
    if (!RegCreateKeyEx(root,"wxpdm",0,NULL,0,KEY_ALL_ACCESS,NULL,&svc,&disp)) {
        if (disp==REG_CREATED_NEW_KEY) {
            if (!save("Service","1")) {
                RegCloseKey(svc); RegDeleteKey(root,"wxpdm"); RegCloseKey(root); return 0;
            }
        } else if (!recorded("Service")) {
            RegCloseKey(svc); RegCloseKey(root); return 0;
        }
        ok=put_dword(svc,"Type",1) && put_dword(svc,"ErrorControl",1) &&
           RegSetValueEx(svc,"Group",0,REG_SZ,(const BYTE*)group,sizeof(group))==ERROR_SUCCESS &&
           RegSetValueEx(svc,"ImagePath",0,REG_EXPAND_SZ,(const BYTE*)image,sizeof(image))==ERROR_SUCCESS;
        if (ok) {
            ok=RegCreateKeyEx(svc,"Device0",0,NULL,0,KEY_SET_VALUE,NULL,&dev,&disp)==ERROR_SUCCESS;
            if (ok) {
                ok=RegSetValueEx(dev,"InstalledDisplayDrivers",0,REG_MULTI_SZ,(const BYTE*)modules,sizeof(modules))==ERROR_SUCCESS &&
                   put_dword(dev,"VgaCompatible",0) && put_dword(dev,"DefaultSettings.XResolution",800) &&
                   put_dword(dev,"DefaultSettings.YResolution",600) && put_dword(dev,"DefaultSettings.BitsPerPel",16) &&
                   put_dword(dev,"DefaultSettings.VRefresh",60);
                RegCloseKey(dev);
            }
        }
        if (ok) ok=video_tag(svc,1) && put_dword(svc,"Start",1);
        RegCloseKey(svc);
    }
    RegCloseKey(root);
    return ok;
}

static LONG remove_key(HKEY parent,const char *name)
{
    HKEY key;
    char child[256];
    LONG status=RegOpenKeyEx(parent,name,0,KEY_ALL_ACCESS,&key);
    if (status==ERROR_FILE_NOT_FOUND) return ERROR_SUCCESS;
    if (status) return status;
    while ((status=RegEnumKey(key,0,child,sizeof(child)))==ERROR_SUCCESS) {
        status=remove_key(key,child);
        if (status) break;
    }
    RegCloseKey(key);
    if (status!=ERROR_NO_MORE_ITEMS) return status;
    return RegDeleteKey(parent,name);
}
int install_driver(HWND parent)
{
    (void)parent;
    if (!copy_driver_files()) return 0;
    if (!configure_service()) return fail("Cannot register the WINXPDM video service and its load order. Run Setup as an administrator.");
    return 1;
}
int remove_driver(HWND parent)
{
    char dst[PATH_CAP];
    HKEY root;
    LONG status;
    int i;
    (void)parent;
    if (recorded("Service")) {
        if (!video_tag(NULL,0) || RegOpenKeyEx(HKEY_LOCAL_MACHINE,services,0,KEY_ALL_ACCESS,&root))
            return fail("Cannot restore the Video service load order.");
        status=remove_key(root,"wxpdm"); RegCloseKey(root);
        if (status) return fail("Cannot unregister the WINXPDM video service.");
    }
    for (i=0;i<2;++i) if (recorded(file_keys[i])) {
        if (!system_file(i,dst) || (!delete_owned(dst) && !MoveFileEx(dst,NULL,MOVEFILE_DELAY_UNTIL_REBOOT)))
            return fail("Cannot remove the WINXPDM driver files. Restart Windows and run Uninstall again.");
    }
    return 1;
}
