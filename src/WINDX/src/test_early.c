#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>
#include "test_output.h"

static BOOL query_older_interfaces(IDirectDraw *dd)
{
    static const struct { const GUID *iid; const char *name; } interfaces[] = {
        {&IID_IDirectDraw, "DirectDraw"}, {&IID_IDirectDraw2, "DirectDraw2"},
        {&IID_IDirectDraw4, "DirectDraw4"}, {&IID_IDirectDraw7, "DirectDraw7"},
        {&IID_IDirect3D, "Direct3D"}, {&IID_IDirect3D2, "Direct3D2"},
        {&IID_IDirect3D3, "Direct3D3"}, {&IID_IDirect3D7, "Direct3D7"}
    };
    BOOL ok = TRUE;
    for (unsigned i = 0; i < sizeof(interfaces)/sizeof(interfaces[0]); ++i) {
        IUnknown *object = NULL;
        HRESULT hr = IDirectDraw_QueryInterface(dd, interfaces[i].iid, (void **)&object);
        report(interfaces[i].name);
        if (SUCCEEDED(hr) && object) {
            report(" interface PASS\r\n");
            IUnknown_Release(object);
        } else { report(" interface FAIL "); report_hex((DWORD)hr); report("\r\n"); ok = FALSE; }
    }
    return ok;
}

static BOOL test_ddraw(HWND window)
{
    typedef HRESULT (WINAPI *create_fn)(GUID *, IDirectDraw **, IUnknown *);
    IDirectDraw *dd = NULL;
    IDirectDrawSurface *surface = NULL;
    DDSURFACEDESC desc = {sizeof(desc)}, lock = {sizeof(lock)};
    DDBLTFX effect = {sizeof(effect)};
    HMODULE dll = LoadLibraryA("ddraw.dll");
    create_fn create = dll ? (create_fn)GetProcAddress(dll,"DirectDrawCreate") : NULL;
    HRESULT hr = create ? create(NULL, &dd, NULL) : E_NOINTERFACE;
    BOOL interfaces = FALSE, pixels = FALSE;
    if (FAILED(hr) || !dd) goto done;
    interfaces = query_older_interfaces(dd);
    hr = IDirectDraw_SetCooperativeLevel(dd, window, DDSCL_NORMAL);
    if (FAILED(hr)) goto done;
    desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
    desc.dwWidth = 4; desc.dwHeight = 3;
    desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
    desc.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
    desc.ddpfPixelFormat.dwFlags = DDPF_RGB;
    desc.ddpfPixelFormat.dwRGBBitCount = 32;
    desc.ddpfPixelFormat.dwRBitMask = 0xff0000; desc.ddpfPixelFormat.dwGBitMask = 0xff00;
    desc.ddpfPixelFormat.dwBBitMask = 0xff;
    hr = IDirectDraw_CreateSurface(dd, &desc, &surface, NULL);
    if (FAILED(hr)) goto done;
    effect.dwFillColor = 0x25476b;
    hr = IDirectDrawSurface_Blt(surface, NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &effect);
    if (FAILED(hr)) goto done;
    hr = IDirectDrawSurface_Lock(surface, NULL, &lock, DDLOCK_WAIT | DDLOCK_READONLY, NULL);
    if (FAILED(hr)) goto done;
    pixels = lock.lpSurface && lock.lPitch >= 16;
    for (unsigned y = 0; pixels && y < 3; ++y) {
        const DWORD *row = (const DWORD *)((BYTE *)lock.lpSurface + y * lock.lPitch);
        for (unsigned x = 0; x < 4; ++x) if ((row[x] & 0xffffff) != effect.dwFillColor) pixels = FALSE;
    }
    hr = IDirectDrawSurface_Unlock(surface, NULL);
    pixels = pixels && SUCCEEDED(hr);
done:
    if (surface) IDirectDrawSurface_Release(surface);
    if (dd) IDirectDraw_Release(dd);
    if (dll) FreeLibrary(dll);
    report(pixels ? "DirectDraw color fill and 12-pixel readback PASS\r\n" : "DirectDraw fill/readback FAIL ");
    if (!pixels) { report_hex((DWORD)hr); report("\r\n"); }
    return interfaces && pixels;
}

BOOL test_d3d8(HWND window);

void WINAPI early_entry(void)
{
    initialize_test();
    HWND window = CreateWindowExA(0,"STATIC","WINDX legacy graphics test",WS_POPUP,0,0,4,3,
            NULL,NULL,GetModuleHandleA(NULL),NULL);
    BOOL draw = window && test_ddraw(window);
    BOOL d3d8 = window && test_d3d8(window);
    if (window) DestroyWindow(window);
    ExitProcess(draw && d3d8 ? 0 : 1);
}
