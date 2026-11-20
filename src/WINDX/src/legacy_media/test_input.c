/* Capability and acquisition checks for the installed DirectInput devices. */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include "../test_output.h"

typedef HRESULT (WINAPI *create_fn)(HINSTANCE,DWORD,REFIID,void **,IUnknown *);

struct device_count {BOOL keyboard, mouse;};
static BOOL CALLBACK enumerate(const DIDEVICEINSTANCEA *device, void *param)
{
    struct device_count *counts=param;
    if (IsEqualGUID(&device->guidInstance,&GUID_SysKeyboard)) counts->keyboard=TRUE;
    if (IsEqualGUID(&device->guidInstance,&GUID_SysMouse)) counts->mouse=TRUE;
    return DIENUM_CONTINUE;
}

static HRESULT acquire_device(IDirectInput7A *input, REFGUID id, const DIDATAFORMAT *format,
        IDirectInputDevice7A **out)
{
    DIDEVCAPS caps={0};caps.dwSize=sizeof(caps);
    HRESULT hr=IDirectInput7_CreateDeviceEx(input,id,&IID_IDirectInputDevice7A,(void **)out,NULL);
    if (SUCCEEDED(hr)) hr=IDirectInputDevice7_GetCapabilities(*out,&caps);
    if (SUCCEEDED(hr)) hr=IDirectInputDevice7_SetDataFormat(*out,format);
    if (SUCCEEDED(hr)) hr=IDirectInputDevice7_SetCooperativeLevel(*out,GetDesktopWindow(),DISCL_NONEXCLUSIVE|DISCL_BACKGROUND);
    if (SUCCEEDED(hr)) hr=IDirectInputDevice7_Acquire(*out);
    return hr;
}

void WINAPI input_entry(void)
{
    HMODULE self=GetModuleHandleA(NULL);
    char path[MAX_PATH],*base;
    DWORD len;
    struct device_count count={0};
    IDirectInput7A *input=NULL; IDirectInputDevice7A *keyboard=NULL,*mouse=NULL;
    BYTE keys[256]={0}; DIMOUSESTATE pointer={0};
    initialize_test();
    len=GetModuleFileNameA(self,path,sizeof(path));
    if (!len || len>=sizeof(path)) ExitProcess(1);
    base=path+len;
    while(base>path && base[-1]!='\\' && base[-1]!='/')--base;
    if (base-path+sizeof("dinput.dll")>sizeof(path)) ExitProcess(1);
    lstrcpyA(base,"dinput.dll");
    HMODULE lib=LoadLibraryA(path);
    if (!lib){report("DirectInput load error ");report_hex(GetLastError());report("\r\n");ExitProcess(1);}
    report("DirectInput module ");report(path);report("\r\n");
    create_fn create=(create_fn)GetProcAddress(lib,"DirectInputCreateEx");
    HRESULT hr=create?create(self,0x0700,&IID_IDirectInput7A,(void **)&input,NULL):E_NOINTERFACE;
    if (SUCCEEDED(hr)) hr=IDirectInput7_EnumDevices(input,0,enumerate,&count,DIEDFL_ATTACHEDONLY);
    if (SUCCEEDED(hr) && (!count.keyboard || !count.mouse)) hr=E_FAIL;
    if (SUCCEEDED(hr)) hr=acquire_device(input,&GUID_SysKeyboard,&c_dfDIKeyboard,&keyboard);
    if (SUCCEEDED(hr)) hr=acquire_device(input,&GUID_SysMouse,&c_dfDIMouse,&mouse);
    if (SUCCEEDED(hr)) hr=IDirectInputDevice7_GetDeviceState(keyboard,sizeof(keys),keys);
    if (SUCCEEDED(hr)) hr=IDirectInputDevice7_GetDeviceState(mouse,sizeof(pointer),&pointer);
    if (SUCCEEDED(hr)) report("Keyboard and mouse enumeration, capabilities, acquisition and state PASS\r\n");
    const char *cmd=GetCommandLineA();BOOL interactive=FALSE;
    for (;*cmd;++cmd) if (*cmd=='/' && (cmd[1]=='i'||cmd[1]=='I')) interactive=TRUE;
    if (interactive && SUCCEEDED(hr)) {
        BOOL down=FALSE,up=FALSE,moved=FALSE;
        report("Press and release F7 and move the mouse within 20 seconds.\r\n");
        for (unsigned i=0;i<800 && !(down && up && moved);++i) {
            MSG msg;
            while (PeekMessageA(&msg,NULL,0,0,PM_REMOVE)) {TranslateMessage(&msg);DispatchMessageA(&msg);}
            hr=IDirectInputDevice7_GetDeviceState(keyboard,sizeof(keys),keys);
            if (FAILED(hr)) break;
            hr=IDirectInputDevice7_GetDeviceState(mouse,sizeof(pointer),&pointer);
            if (FAILED(hr)) break;
            if (keys[DIK_F7]&0x80) down=TRUE;else if (down) up=TRUE;
            if (pointer.lX || pointer.lY) moved=TRUE;
            Sleep(25);
        }
        if (SUCCEEDED(hr) && !(down && up && moved)) hr=E_FAIL;
        if (SUCCEEDED(hr)) report("F7 press and release and measured relative mouse movement PASS\r\n");
        else { report("Observed press, release and movement ");report_hex(down|(up<<1)|(moved<<2));report("\r\n");}
    }
    if (FAILED(hr)) {report("DirectInput test HRESULT ");report_hex((DWORD)hr);report("\r\n");}
    if (mouse) {IDirectInputDevice7_Unacquire(mouse);IDirectInputDevice7_Release(mouse);}
    if (keyboard) {IDirectInputDevice7_Unacquire(keyboard);IDirectInputDevice7_Release(keyboard);}
    if (input) IDirectInput7_Release(input);
    FreeLibrary(lib);ExitProcess(FAILED(hr));
}
