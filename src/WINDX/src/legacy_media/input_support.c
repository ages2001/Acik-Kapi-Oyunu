#define COBJMACROS
#include <stdarg.h>
#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "winuser.h"
#include "objbase.h"
#include "dinput.h"
#include "device_private.h"
#include "dinput_private.h"

BOOL wx_legacy_input(void)
{
    DWORD version=GetVersion();
    WORD low=LOWORD(version);
    return (LOBYTE(low)<4) || ((version & 0x80000000u) && LOBYTE(low)==4 && HIBYTE(low)<10);
}

HRESULT wx_action_map_guid(REFGUID id, WCHAR **out)
{
    extern int WINAPI wx_guid_string(REFGUID,WCHAR *,int);
    if (!out) return E_POINTER;
    WCHAR *name=HeapAlloc(GetProcessHeap(),0,39*sizeof(*name));
    if (!name) return E_OUTOFMEMORY;
    if (!wx_guid_string(id,name,39)) { HeapFree(GetProcessHeap(),0,name); return E_INVALIDARG; }
    *out=name;
    return S_OK;
}

static HRESULT register_class(const char *key, const char *label, const char *dll, BOOL remove)
{
    HKEY cls, server;
    LONG err;
    char subkey[96];
    lstrcpyA(subkey,key);lstrcatA(subkey,"\\InprocServer32");
    if (remove) {
        char installed[MAX_PATH];
        LONG size=sizeof(installed);
        err=RegQueryValueA(HKEY_CLASSES_ROOT,subkey,installed,&size);
        if (err==ERROR_FILE_NOT_FOUND) return S_OK;
        if (err) return HRESULT_FROM_WIN32(err);
        if (lstrcmpiA(installed,dll)) return S_OK;
        err=RegDeleteKeyA(HKEY_CLASSES_ROOT,subkey);
        if (err && err!=ERROR_FILE_NOT_FOUND) return HRESULT_FROM_WIN32(err);
        err=RegDeleteKeyA(HKEY_CLASSES_ROOT,key);
        return err==ERROR_FILE_NOT_FOUND ? S_OK : HRESULT_FROM_WIN32(err);
    }
    err=RegCreateKeyA(HKEY_CLASSES_ROOT,key,&cls);
    if (err) return HRESULT_FROM_WIN32(err);
    err=RegSetValueA(cls,NULL,REG_SZ,label,lstrlenA(label)+1);
    if (!err) {
        err=RegCreateKeyA(cls,"InprocServer32",&server);
        if (!err) {
            const char model[]="Both";
            err=RegSetValueA(server,NULL,REG_SZ,dll,lstrlenA(dll)+1);
            if (!err) err=RegSetValueExA(server,"ThreadingModel",0,REG_SZ,(BYTE *)model,sizeof(model));
            RegCloseKey(server);
        }
    }
    RegCloseKey(cls);
    return HRESULT_FROM_WIN32(err);
}

HRESULT wx_register_input(HMODULE module, BOOL remove, BOOL version8)
{
    const char *cls = version8 ? "CLSID\\{25e609e4-b259-11cf-bfc7-444553540000}" :
                                 "CLSID\\{25e609e0-b259-11cf-bfc7-444553540000}";
    const char *label = version8 ? "DirectInput8 Object" : "DirectInput Object";
    char path[MAX_PATH];
    DWORD length=GetModuleFileNameA(module,path,sizeof(path));
    if (!length || length>=sizeof(path)) return E_FAIL;
    return register_class(cls,label,path,remove);
}
