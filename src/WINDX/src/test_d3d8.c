#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <d3d8.h>
#include "test_output.h"

BOOL test_d3d8(HWND window)
{
    typedef IDirect3D8 *(WINAPI *create_fn)(UINT);
    IDirect3D8 *api = NULL;
    IDirect3DDevice8 *device = NULL;
    IDirect3DSurface8 *target = NULL;
    D3DPRESENT_PARAMETERS params = {0};
    D3DLOCKED_RECT mapped;
    HMODULE dll = LoadLibraryA("d3d8.dll");
    create_fn create = dll ? (create_fn)GetProcAddress(dll,"Direct3DCreate8") : NULL;
    HRESULT hr = E_NOINTERFACE;
    BOOL pixels = FALSE;
    if (!create || !(api = create(D3D_SDK_VERSION))) goto done;
    params.Windowed = TRUE; params.hDeviceWindow = window;
    params.SwapEffect = D3DSWAPEFFECT_DISCARD;
    D3DDISPLAYMODE mode;
    if (FAILED(hr = IDirect3D8_GetAdapterDisplayMode(api, 0, &mode))) goto done;
    params.BackBufferWidth = 4; params.BackBufferHeight = 3; params.BackBufferFormat = mode.Format;
    hr = IDirect3D8_CreateDevice(api, 0, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING,
            &params, &device);
    if (FAILED(hr)) goto done;
    hr = IDirect3DDevice8_CreateRenderTarget(device, 4, 3, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, TRUE, &target);
    if (FAILED(hr)) goto done;
    if (FAILED(hr = IDirect3DDevice8_SetRenderTarget(device, target, NULL))) goto done;
    hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_TARGET, 0xff3579bd, 1, 0);
    if (FAILED(hr)) goto done;
    hr = IDirect3DSurface8_LockRect(target, &mapped, NULL, D3DLOCK_READONLY);
    if (FAILED(hr)) goto done;
    pixels = mapped.pBits && mapped.Pitch >= 16;
    for (unsigned y = 0; pixels && y < 3; ++y) {
        const DWORD *row = (const DWORD *)((BYTE *)mapped.pBits + y * mapped.Pitch);
        for (unsigned x = 0; x < 4; ++x) if (row[x] != 0xff3579bd) pixels = FALSE;
    }
    hr = IDirect3DSurface8_UnlockRect(target);
    pixels = pixels && SUCCEEDED(hr);
done:
    if (target) IDirect3DSurface8_Release(target);
    if (device) IDirect3DDevice8_Release(device);
    if (api) IDirect3D8_Release(api);
    if (dll) FreeLibrary(dll);
    report(pixels ? "Direct3D 8 clear and 12-pixel readback PASS\r\n" : "Direct3D 8 clear/readback FAIL ");
    if (!pixels) { report_hex((DWORD)hr); report("\r\n"); }
    return pixels;
}
