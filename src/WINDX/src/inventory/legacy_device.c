#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>

/* Direct3D 8 and 9 use different structures with the same type names.
 * Compile this routine against each API's own SDK declarations. */
#ifdef WX_PROBE_D3D8
#include <d3d8.h>
#define factory_type IDirect3D8
#define device_type IDirect3DDevice8
#define create_device IDirect3D8_CreateDevice
#define release_factory IDirect3D8_Release
#define release_device IDirect3DDevice8_Release
#define factory_name "Direct3DCreate8"
#define probe wx_probe_legacy8
#else
#include <d3d9.h>
#define factory_type IDirect3D9
#define device_type IDirect3DDevice9
#define create_device IDirect3D9_CreateDevice
#define release_factory IDirect3D9_Release
#define release_device IDirect3DDevice9_Release
#define factory_name "Direct3DCreate9"
#define probe wx_probe_legacy9
#endif

BOOL probe(HMODULE library)
{
    factory_type *(WINAPI *create)(UINT) = (void *)GetProcAddress(library, factory_name);
    factory_type *factory = create ? create(D3D_SDK_VERSION) : NULL;
    if (!factory) return FALSE;
    HWND window = CreateWindowExA(0, "STATIC", "", WS_POPUP, 0, 0, 8, 8, NULL, NULL, GetModuleHandleA(NULL), NULL);
    device_type *device = NULL;
    D3DPRESENT_PARAMETERS p = {0};
    p.Windowed = TRUE;
    p.SwapEffect = D3DSWAPEFFECT_DISCARD;
    p.BackBufferWidth = 8;
    p.BackBufferHeight = 8;
    p.BackBufferFormat = D3DFMT_UNKNOWN;
    p.hDeviceWindow = window;
    BOOL result = window && SUCCEEDED(create_device(factory, 0, D3DDEVTYPE_HAL, window,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING, &p, &device)) && device;
    if (device) release_device(device);
    if (window) DestroyWindow(window);
    release_factory(factory);
    return result;
}
