/* Exercise the public multimedia interfaces and record which DLL answered. */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#define INITGUID
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <mmsystem.h>
#include <dinput.h>
#include <dsound.h>
#include <dplay.h>
#include <dmusicc.h>
#include <dxdiag.h>
#include <xinput.h>
#include "test_output.h"
#include "identity/wx_identity.h"

/* ABI prefixes used by the June 2010 SDK, which predates the system XAudio 2.8 ABI. */
typedef struct Audio27 Audio27;
typedef struct {
    HRESULT (WINAPI *query)(Audio27 *, REFIID, void **);
    ULONG (WINAPI *addref)(Audio27 *);
    ULONG (WINAPI *release)(Audio27 *);
    HRESULT (WINAPI *device_count)(Audio27 *, UINT32 *);
    HRESULT (WINAPI *device_details)(Audio27 *, UINT32, void *);
    HRESULT (WINAPI *initialize)(Audio27 *, UINT32, UINT32);
} Audio27Vtbl;
struct Audio27 { const Audio27Vtbl *vt; };
typedef struct Xact37 Xact37;
typedef struct {
    HRESULT (WINAPI *query)(Xact37 *, REFIID, void **);
    ULONG (WINAPI *addref)(Xact37 *);
    ULONG (WINAPI *release)(Xact37 *);
    HRESULT (WINAPI *renderer_count)(Xact37 *, WORD *);
} Xact37Vtbl;
struct Xact37 { const Xact37Vtbl *vt; };
static const GUID audio_class = {0x5a508685,0xa254,0x4fba,{0x9b,0x82,0x9a,0x24,0xb0,0x03,0x06,0xaf}};
static const GUID audio_iface = {0x8bcf1f58,0x9fe7,0x4583,{0x8a,0xc6,0xe2,0xad,0xc4,0x65,0xc8,0xbb}};
static const GUID xact_class = {0xbcc782bc,0x6492,0x4c22,{0x8c,0x35,0xf5,0xd7,0x2f,0xe7,0x3c,0x6e}};
static const GUID xact_iface = {0xb1ee676a,0xd9cd,0x4d2a,{0x89,0xa8,0xfa,0x53,0xeb,0x9e,0x48,0x0b}};

static HMODULE component(const char *name)
{
    char path[MAX_PATH];
    HMODULE dll;
    DWORD len = GetModuleFileNameA(NULL, path, sizeof(path));
    if (!len || len >= sizeof(path)) return NULL;
    while (len && path[len-1] != '\\' && path[len-1] != '/') --len;
    if (len + lstrlenA(name) >= sizeof(path)) return NULL;
    lstrcpyA(path + len, name);
    dll = LoadLibraryA(GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES ? path : name);
    if (dll && GetModuleFileNameA(dll, path, sizeof(path))) {
        report("Module "); report(path); report("\r\n");
    } else { report("Cannot load "); report(name); report(" "); report_hex(GetLastError()); report("\r\n"); }
    return dll;
}
static BOOL result(const char *label, HRESULT hr)
{
    report(label);
    if (SUCCEEDED(hr)) { report(" PASS\r\n"); return TRUE; }
    report(" FAIL "); report_hex((DWORD)hr); report("\r\n"); return FALSE;
}
static HRESULT object(HMODULE dll, REFCLSID clsid, REFIID iid, void **out)
{
    typedef HRESULT (WINAPI *factory_fn)(REFCLSID, REFIID, void **);
    factory_fn get = dll ? (factory_fn)GetProcAddress(dll, "DllGetClassObject") : NULL;
    IClassFactory *factory = NULL;
    HRESULT hr = get ? get(clsid, &IID_IClassFactory, (void **)&factory) : E_NOINTERFACE;
    if (SUCCEEDED(hr) && factory) {
        hr = IClassFactory_CreateInstance(factory, NULL, iid, out);
        IClassFactory_Release(factory);
    }
    return hr;
}
static BOOL CALLBACK input_device(const DIDEVICEINSTANCEA *device, void *context)
{
    DWORD *flags = context;
    if (GET_DIDEVICE_TYPE(device->dwDevType) == DI8DEVTYPE_KEYBOARD) *flags |= 1;
    if (GET_DIDEVICE_TYPE(device->dwDevType) == DI8DEVTYPE_MOUSE) *flags |= 2;
    return DIENUM_CONTINUE;
}
static BOOL input(void)
{
    typedef HRESULT (WINAPI *create_fn)(HINSTANCE,DWORD,REFIID,void **,IUnknown *);
    HMODULE dll = component("dinput8.dll");
    create_fn create = dll ? (create_fn)GetProcAddress(dll, "DirectInput8Create") : NULL;
    IDirectInput8A *di = NULL;
    IDirectInputDevice8A *keyboard = NULL;
    DIDEVCAPS caps = {sizeof(caps)};
    DWORD devices = 0;
    HRESULT hr = create ? create(GetModuleHandleA(NULL), DIRECTINPUT_VERSION, &IID_IDirectInput8A, (void **)&di, NULL) : E_NOINTERFACE;
    if (SUCCEEDED(hr)) hr = IDirectInput8_EnumDevices(di, DI8DEVCLASS_ALL, input_device, &devices, DIEDFL_ATTACHEDONLY);
    if (SUCCEEDED(hr) && devices != 3) hr = DIERR_NOTFOUND;
    if (SUCCEEDED(hr)) hr = IDirectInput8_CreateDevice(di, &GUID_SysKeyboard, &keyboard, NULL);
    if (SUCCEEDED(hr)) hr = IDirectInputDevice8_GetCapabilities(keyboard, &caps);
    if (SUCCEEDED(hr) && !caps.dwButtons) hr = E_FAIL;
    if (keyboard) IDirectInputDevice8_Release(keyboard);
    if (di) IDirectInput8_Release(di);
    if (dll) FreeLibrary(dll);
    return result("DirectInput 8 mouse, keyboard and keyboard capabilities", hr);
}
static BOOL CALLBACK sound_device(GUID *guid, const char *name, const char *module, void *context)
{
    DWORD *devices = context;
    if (name && *name) ++*devices;
    return TRUE;
}
static BOOL sound(void)
{
    typedef HRESULT (WINAPI *enum_fn)(LPDSENUMCALLBACKA,void *);
    HMODULE dll = component("dsound.dll");
    enum_fn enumerate = dll ? (enum_fn)GetProcAddress(dll, "DirectSoundEnumerateA") : NULL;
    DWORD devices = 0;
    HRESULT hr = enumerate ? enumerate(sound_device, &devices) : E_NOINTERFACE;
    report("DirectSound playback descriptions "); report_hex(devices); report("\r\n");
    if (dll) FreeLibrary(dll);
    return result("DirectSound playback enumeration", hr);
}
static BOOL controllers(void)
{
    typedef DWORD (WINAPI *state_fn)(DWORD,XINPUT_STATE *);
    HMODULE dll = component("xinput1_3.dll");
    state_fn state = dll ? (state_fn)GetProcAddress(dll, "XInputGetState") : NULL;
    XINPUT_STATE pad;
    HRESULT hr = state ? S_OK : E_NOINTERFACE;
    DWORD count = 0, disconnected = 0;
    for (DWORD i = 0; state && i < XUSER_MAX_COUNT; ++i) {
        DWORD code = state(i, &pad);
        if (!code) ++count;
        else if (code == ERROR_DEVICE_NOT_CONNECTED) ++disconnected;
        else hr = HRESULT_FROM_WIN32(code);
    }
    report("XInput connected "); report_hex(count); report(", disconnected "); report_hex(disconnected); report("\r\n");
    if (dll) FreeLibrary(dll);
    return result("XInput 1.3 controller slots", hr);
}
static BOOL xaudio(void)
{
    HMODULE dll = component("XAudio2_7.dll");
    Audio27 *audio = NULL;
    UINT32 count = 0;
    HRESULT hr = object(dll, &audio_class, &audio_iface, (void **)&audio);
    if (SUCCEEDED(hr)) hr = audio->vt->initialize(audio, 0, 0xffffffffu);
    if (SUCCEEDED(hr)) hr = audio->vt->device_count(audio, &count);
    report("XAudio 2.7 playback devices "); report_hex(count); report("\r\n");
    if (audio) audio->vt->release(audio);
    if (dll) FreeLibrary(dll);
    return result("XAudio 2.7 engine initialization and device enumeration", hr);
}
static BOOL xact(void)
{
    HMODULE dll = component("XACTEngine3_7.dll");
    Xact37 *engine = NULL;
    WORD count = 0;
    HRESULT hr = object(dll, &xact_class, &xact_iface, (void **)&engine);
    if (SUCCEEDED(hr)) hr = engine->vt->renderer_count(engine, &count);
    report("XACT 3.7 renderers "); report_hex(count); report("\r\n");
    if (engine) engine->vt->release(engine);
    if (dll) FreeLibrary(dll);
    return result("XACT 3.7 engine creation and renderer enumeration", hr);
}
static HRESULT diagnostic_version(IDxDiagContainer *system, const WCHAR *property, DWORD expected)
{
    VARIANT value;
    HRESULT hr;
    VariantInit(&value);
    hr = IDxDiagContainer_GetProp(system, property, &value);
    if (SUCCEEDED(hr) && ((V_VT(&value) != VT_UI4 && V_VT(&value) != VT_I4) || V_UI4(&value) != expected))
        hr = E_FAIL;
    VariantClear(&value);
    return hr;
}

static HRESULT diagnostic_text(IDxDiagContainer *system, const WCHAR *property, const char *label)
{
    VARIANT value;
    char text[768];
    HRESULT hr;
    VariantInit(&value);
    hr = IDxDiagContainer_GetProp(system, property, &value);
    if (SUCCEEDED(hr)) {
        if (V_VT(&value) != VT_BSTR || !V_BSTR(&value) || !*V_BSTR(&value) ||
                !WideCharToMultiByte(CP_ACP, 0, V_BSTR(&value), -1, text, sizeof(text), NULL, NULL)) {
            hr = E_FAIL;
        } else { report(label); report(text); report("\r\n"); }
    }
    VariantClear(&value);
    return hr;
}

static BOOL diagnostic(void)
{
    HMODULE dll = component("dxdiagn.dll");
    DXDIAG_INIT_PARAMS params = {sizeof(params), DXDIAG_DX9_SDK_VERSION, FALSE, NULL};
    IDxDiagProvider *provider = NULL;
    IDxDiagContainer *root = NULL, *system = NULL;
    DWORD count = 0;
    struct wx_system_identity expected = {0};
    HRESULT hr = object(dll, &CLSID_DxDiagProvider, &IID_IDxDiagProvider, (void **)&provider);
    if (SUCCEEDED(hr) && !wx_identity_version(&expected)) hr = E_FAIL;
    if (SUCCEEDED(hr)) hr = IDxDiagProvider_Initialize(provider, &params);
    if (SUCCEEDED(hr)) hr = IDxDiagProvider_GetRootContainer(provider, &root);
    if (SUCCEEDED(hr)) hr = IDxDiagContainer_GetNumberOfChildContainers(root, &count);
    if (SUCCEEDED(hr) && !count) hr = E_FAIL;
    if (SUCCEEDED(hr)) hr = IDxDiagContainer_GetChildContainer(root, L"DxDiag_SystemInfo", &system);
    if (SUCCEEDED(hr)) hr = diagnostic_version(system, L"dwOSMajorVersion", expected.major);
    if (SUCCEEDED(hr)) hr = diagnostic_version(system, L"dwOSMinorVersion", expected.minor);
    if (SUCCEEDED(hr)) hr = diagnostic_version(system, L"dwOSBuildNumber", expected.build);
    if (SUCCEEDED(hr)) hr = diagnostic_version(system, L"dwOSPlatformID", expected.platform);
    if (SUCCEEDED(hr)) hr = diagnostic_text(system, L"szOSExLongEnglish", "DxDiag operating system: ");
    if (SUCCEEDED(hr)) hr = diagnostic_text(system, L"szProcessorEnglish", "DxDiag processor: ");
    if (SUCCEEDED(hr)) hr = diagnostic_text(system, L"szSystemManufacturerEnglish", "DxDiag system manufacturer: ");
    if (SUCCEEDED(hr)) hr = diagnostic_text(system, L"szSystemModelEnglish", "DxDiag system model: ");
    if (SUCCEEDED(hr)) hr = diagnostic_text(system, L"szBIOSEnglish", "DxDiag BIOS: ");
    if (SUCCEEDED(hr)) hr = diagnostic_text(system, L"szSetupParamEnglish", "DxDiag setup parameters: ");
    if (SUCCEEDED(hr)) hr = diagnostic_text(system, L"szDxDiagVersion", "DxDiag executable version: ");
    if (SUCCEEDED(hr)) hr = diagnostic_text(system, L"szDirectXVersionLongEnglish", "DxDiag DirectX inventory: ");
    report("DxDiag root containers "); report_hex(count); report("\r\n");
    if (system) IDxDiagContainer_Release(system);
    if (root) IDxDiagContainer_Release(root);
    if (provider) IDxDiagProvider_Release(provider);
    if (dll) FreeLibrary(dll);
    return result("DxDiag COM system information matches the running OS", hr);
}
static BOOL music(void)
{
    HMODULE dll = component("dmusic.dll");
    IDirectMusic8 *engine = NULL;
    DMUS_PORTCAPS caps = {sizeof(caps)};
    HRESULT hr = object(dll, &CLSID_DirectMusic, &IID_IDirectMusic8, (void **)&engine);
    if (SUCCEEDED(hr)) hr = IDirectMusic8_EnumPort(engine, 0, &caps);
    if (hr == S_FALSE) report("DirectMusic reported no ports\r\n");
    else if (hr == S_OK) { report("DirectMusic first port type "); report_hex(caps.dwType); report("\r\n"); }
    if (engine) IDirectMusic8_Release(engine);
    if (dll) FreeLibrary(dll);
    return result("DirectMusic 8 port enumeration", hr);
}
static BOOL CALLBACK connection(LPCGUID id, void *blob, DWORD bytes, LPCDPNAME name, DWORD flags, void *context)
{
    if (id && blob && bytes && name) ++*(DWORD *)context;
    return TRUE;
}
static BOOL play(void)
{
    HMODULE dll = component("dplayx.dll");
    IDirectPlay4A *dp = NULL;
    DWORD count = 0;
    HRESULT hr = object(dll, &CLSID_DirectPlay, &IID_IDirectPlay4A, (void **)&dp);
    if (SUCCEEDED(hr)) hr = dp->lpVtbl->EnumConnections(dp, NULL, connection, &count, DPCONNECTION_DIRECTPLAY);
    report("DirectPlay registered local service providers "); report_hex(count); report("\r\n");
    if (dp) dp->lpVtbl->Release(dp);
    if (dll) FreeLibrary(dll);
    return result("DirectPlay 4 service provider enumeration", hr);
}
static const struct { const char *name; BOOL (*run)(void); } checks[] = {
    {"input", input}, {"sound", sound}, {"xinput", controllers}, {"xaudio", xaudio},
    {"xact", xact}, {"music", music}, {"play", play}, {"diag", diagnostic}
};
void WINAPI media_entry(void)
{
    const char *args = GetCommandLineA();
    BOOL passed = TRUE, found = FALSE;
    HRESULT hr;
    char selection[16];
    unsigned length = 0;
    initialize_test();
    if (*args == '"') { for (++args; *args && *args != '"'; ++args) {} if (*args) ++args; }
    else while (*args && *args != ' ') ++args;
    while (*args == ' ' || *args == '\t') ++args;
    while (*args && *args != ' ' && *args != '\t' && length + 1 < sizeof(selection)) selection[length++] = *args++;
    selection[length] = 0;
    while (*args == ' ' || *args == '\t') ++args;
    if (*args) { report("Choose one test name.\r\n"); ExitProcess(1); }
    hr = CoInitialize(NULL);
    if (FAILED(hr)) { result("COM initialization", hr); ExitProcess(1); }
    for (unsigned i = 0; i < sizeof(checks)/sizeof(checks[0]); ++i) {
        if (*selection && lstrcmpiA(selection, checks[i].name)) continue;
        found = TRUE;
        if (!checks[i].run()) passed = FALSE;
    }
    CoUninitialize();
    if (!found) report("Choose input, sound, xinput, xaudio, xact, music, play or diag.\r\n");
    ExitProcess(found && passed ? 0 : 1);
}
