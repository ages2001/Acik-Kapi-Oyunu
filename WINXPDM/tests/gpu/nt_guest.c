/* Native NT 5.x probe for a disposable guest. SPDX-License-Identifier: GPL-3.0-or-later */
#include <windows.h>
#include <setupapi.h>

typedef BOOL (WINAPI *update_t)(HWND,LPCSTR,LPCSTR,DWORD,PBOOL);
typedef DWORD (WINAPI *node_status_t)(DWORD*,DWORD*,DWORD,DWORD);
static HANDLE output;

static void log_line(const char *key, const char *value)
{
    char line[1200];
    DWORD wrote;
    wsprintfA(line,"%s=%s\r\n",key,value);
    WriteFile(output,line,lstrlenA(line),&wrote,0);
    FlushFileBuffers(output);
}

static int locate(const char *base,char *id,char *service)
{
    HDEVINFO devices=SetupDiGetClassDevsA(NULL,"PCI",NULL,DIGCF_PRESENT|DIGCF_ALLCLASSES);
    SP_DEVINFO_DATA device;
    DWORD i, size;
    char ids[4096], prefix[256], *entry;
    int n=lstrlenA(base),found=0;
    if (devices==INVALID_HANDLE_VALUE || n<1 || n>240) return 0;
    device.cbSize=sizeof(device);
    for (i=0; !found && SetupDiEnumDeviceInfo(devices,i,&device); ++i) {
        if (!SetupDiGetDeviceRegistryPropertyA(devices,&device,SPDRP_HARDWAREID,NULL,
             (BYTE*)ids,sizeof(ids)-2,&size)) continue;
        ids[size]=0; ids[size+1]=0;
        for (entry=ids; *entry; entry+=lstrlenA(entry)+1) {
            if (lstrlenA(entry)<n || lstrlenA(entry)>255) continue;
            lstrcpynA(prefix,entry,n+1);
            if (!lstrcmpiA(prefix,base) && (entry[n]==0 || entry[n]=='&')) {
                lstrcpyA(id,entry);
                service[0]=0;
                SetupDiGetDeviceRegistryPropertyA(devices,&device,SPDRP_SERVICE,NULL,(BYTE*)service,255,NULL);
                service[255]=0;
                {
                    HMODULE cfg=LoadLibraryA("cfgmgr32.dll");
                    node_status_t query=cfg?(node_status_t)(void*)GetProcAddress(cfg,"CM_Get_DevNode_Status"):NULL;
                    DWORD flags=0,problem=0;
                    char text[40];
                    if (query && !query(&flags,&problem,device.DevInst,0)) {
                        wsprintfA(text,"%lu",problem);
                        log_line("DeviceProblem",text);
                    }
                    if (cfg) FreeLibrary(cfg);
                }
                found=1;
                break;
            }
        }
    }
    SetupDiDestroyDeviceInfoList(devices);
    return found;
}

static void describe(void)
{
    DISPLAY_DEVICEA adapter;
    HKEY key;
    HDC dc;
    DWORD size=512,i;
    char value[512], text[128];
    ZeroMemory(&adapter,sizeof(adapter));
    adapter.cb=sizeof(adapter);
    for (i=0;EnumDisplayDevicesA(NULL,i,&adapter,0);++i) {
        if (adapter.StateFlags&DISPLAY_DEVICE_PRIMARY_DEVICE) {
            log_line("DisplayDevice",adapter.DeviceString);
            break;
        }
    }
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,"HARDWARE\\DEVICEMAP\\VIDEO",0,KEY_READ,&key)==ERROR_SUCCESS) {
        if (!RegQueryValueExA(key,"\\Device\\Video0",NULL,NULL,(BYTE*)value,&size)) {
            value[sizeof(value)-1]=0;
            log_line("Video0",value);
        }
        RegCloseKey(key);
    }
    dc=GetDC(NULL);
    wsprintfA(text,"%dx%dx%d",GetDeviceCaps(dc,HORZRES),GetDeviceCaps(dc,VERTRES),
              GetDeviceCaps(dc,BITSPIXEL)*GetDeviceCaps(dc,PLANES));
    log_line("Mode",text);
    {
        HDC src=CreateCompatibleDC(dc),dst=CreateCompatibleDC(dc);
        HBITMAP a=CreateCompatibleBitmap(dc,16,16),b=CreateCompatibleBitmap(dc,16,16);
        HGDIOBJ old_a=src&&a?SelectObject(src,a):NULL;
        HGDIOBJ old_b=dst&&b?SelectObject(dst,b):NULL;
        int ok=old_a && old_b && SetPixel(src,5,9,RGB(255,0,0))==RGB(255,0,0) &&
               BitBlt(dst,0,0,16,16,src,0,0,SRCCOPY) && GetPixel(dst,5,9)==RGB(255,0,0);
        log_line("GdiBitBlt",ok?"pass":"fail");
        if (old_a) SelectObject(src,old_a);
        if (old_b) SelectObject(dst,old_b);
        if (a) DeleteObject(a);
        if (b) DeleteObject(b);
        if (src) DeleteDC(src);
        if (dst) DeleteDC(dst);
    }
    ReleaseDC(NULL,dc);
}

int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,LPSTR args,int show)
{
    char root[MAX_PATH], cfg[MAX_PATH], path[MAX_PATH], base[256], inf[MAX_PATH];
    char id[256], service[256],status[40],*end;
    HMODULE lib;
    update_t update;
    BOOL reboot=FALSE,ok;
    int binding=!lstrcmpiA(args,"/bind");
    DWORD err;
    (void)instance; (void)previous; (void)show;
    if (!GetModuleFileNameA(NULL,root,sizeof(root)) || lstrlenA(root)>MAX_PATH-20) return 1;
    for (end=root+lstrlenA(root); end>root && end[-1]!='\\'; --end) {}
    *end=0;
    wsprintfA(cfg,"%sCASE.INI",root);
    wsprintfA(path,"%s%s",root,binding?"BIND.TXT":"PROBE.TXT");
    output=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if (output==INVALID_HANDLE_VALUE) return 1;
    describe();
    GetPrivateProfileStringA("Test","HardwareID","",base,sizeof(base),cfg);
    if (!locate(base,id,service)) { log_line("PCI","missing"); CloseHandle(output); return 2; }
    log_line("HardwareID",id); log_line("Service",service);
    if (!binding) { CloseHandle(output); return 0; }
    GetPrivateProfileStringA("Test","Inf","",inf,sizeof(inf),cfg);
    if (!inf[0] || lstrlenA(root)+lstrlenA(inf)>=MAX_PATH) { CloseHandle(output); return 3; }
    wsprintfA(path,"%s%s",root,inf);
    lib=LoadLibraryA("newdev.dll");
    update=lib?(update_t)(void*)GetProcAddress(lib,"UpdateDriverForPlugAndPlayDevicesA"):NULL;
    if (!update) { log_line("UpdateDriver","unavailable"); CloseHandle(output); return 4; }
    ok=update(NULL,id,path,1,&reboot);
    err=GetLastError();
    wsprintfA(status,"%lu",ok?0:err);
    log_line("UpdateError",status); log_line("RebootRequested",reboot?"yes":"no");
    if (ok && locate(base,id,service)) log_line("SelectedService",service);
    FreeLibrary(lib); CloseHandle(output);
    return ok?0:5;
}
