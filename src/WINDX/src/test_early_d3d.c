#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>
#include "test_output.h"

enum { SIDE = 32, BACKGROUND = 0x173159, FOREGROUND = 0x2ca9df };

static D3DTLVERTEX vertices[] = {
    {.sx = 3, .sy = 3, .sz = 0.5f, .rhw = 1, .color = 0xff000000u | FOREGROUND},
    {.sx = 27, .sy = 3, .sz = 0.5f, .rhw = 1, .color = 0xff000000u | FOREGROUND},
    {.sx = 3, .sy = 27, .sz = 0.5f, .rhw = 1, .color = 0xff000000u | FOREGROUND}
};

static HRESULT checked(const char *operation, HRESULT result)
{
    if (FAILED(result)) {
        report(operation); report(" failed, HRESULT "); report_hex((DWORD)result); report("\r\n");
    }
    return result;
}

#define TRY(call) do { hr = checked(#call, (call)); if (FAILED(hr)) goto done; } while (0)

static HRESULT check_pixels(IDirectDrawSurface *surface, BOOL drawn)
{
    DDSURFACEDESC lock = {sizeof(lock)};
    HRESULT hr = IDirectDrawSurface_Lock(surface, NULL, &lock, DDLOCK_WAIT | DDLOCK_READONLY, NULL);
    if (FAILED(hr)) return hr;
    BOOL ok = lock.lpSurface && lock.dwWidth == SIDE && lock.dwHeight == SIDE &&
            lock.lPitch >= SIDE * (LONG)sizeof(DWORD) && lock.ddpfPixelFormat.dwRGBBitCount == 32;
    for (unsigned y = 0; ok && y < SIDE; ++y) {
        const DWORD *row = (const DWORD *)((BYTE *)lock.lpSurface + y * lock.lPitch);
        for (unsigned x = 0; x < SIDE; ++x) {
            DWORD wanted = BACKGROUND;
            if (drawn) {
                /* Keep samples away from the three edges. Generations differ in
                 * their subpixel precision and half-pixel rasterization rules. */
                if (x >= 5 && y >= 5 && x + y <= 26) wanted = FOREGROUND;
                else if (x > 1 && y > 1 && x + y < 32) continue;
            }
            if ((row[x] & 0xffffff) != wanted) {
                report("Pixel mismatch x="); report_hex(x); report(" y="); report_hex(y);
                report(" expected="); report_hex(wanted); report(" actual="); report_hex(row[x]); report("\r\n");
                ok = FALSE;
                break;
            }
        }
    }
    hr = IDirectDrawSurface_Unlock(surface, NULL);
    return FAILED(hr) ? hr : ok ? S_OK : E_FAIL;
}

static HRESULT clear_target(IDirectDrawSurface *surface)
{
    DDBLTFX fill = {sizeof(fill)};
    fill.dwFillColor = BACKGROUND;
    HRESULT hr = IDirectDrawSurface_Blt(surface, NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fill);
    return FAILED(hr) ? hr : check_pixels(surface, FALSE);
}

struct command_stream {
    D3DTLVERTEX vertices[3];
    D3DINSTRUCTION process;
    D3DPROCESSVERTICES transform;
    D3DINSTRUCTION state;
    D3DSTATE states[2];
    D3DINSTRUCTION draw;
    D3DTRIANGLE triangle;
    D3DINSTRUCTION end;
};

static HRESULT execute_triangle(IDirect3DDevice *device, IDirect3DViewport *viewport)
{
    D3DEXECUTEBUFFERDESC desc = {sizeof(desc)};
    D3DEXECUTEDATA data = {sizeof(data)};
    IDirect3DExecuteBuffer *buffer = NULL;
    struct command_stream *stream;
    HRESULT hr;
    desc.dwFlags = D3DDEB_BUFSIZE | D3DDEB_CAPS;
    desc.dwBufferSize = sizeof(*stream);
    desc.dwCaps = D3DDEBCAPS_SYSTEMMEMORY;
    TRY(IDirect3DDevice_CreateExecuteBuffer(device, &desc, &buffer, NULL));
    TRY(IDirect3DExecuteBuffer_Lock(buffer, &desc));
    stream = desc.lpData;
    *stream = (struct command_stream){0};
    for (unsigned i = 0; i < 3; ++i) stream->vertices[i] = vertices[i];
    stream->process = (D3DINSTRUCTION){D3DOP_PROCESSVERTICES, sizeof(stream->transform), 1};
    stream->transform.dwFlags = D3DPROCESSVERTICES_COPY;
    stream->transform.dwCount = 3;
    stream->state = (D3DINSTRUCTION){D3DOP_STATERENDER, sizeof(D3DSTATE), 2};
    stream->states[0].drstRenderStateType = D3DRENDERSTATE_CULLMODE;
    stream->states[0].dwArg[0] = D3DCULL_NONE;
    stream->states[1].drstRenderStateType = D3DRENDERSTATE_ZENABLE;
    stream->states[1].dwArg[0] = D3DZB_FALSE;
    stream->draw = (D3DINSTRUCTION){D3DOP_TRIANGLE, sizeof(D3DTRIANGLE), 1};
    stream->triangle.v1 = 0; stream->triangle.v2 = 1; stream->triangle.v3 = 2;
    stream->triangle.wFlags = D3DTRIFLAG_START;
    stream->end = (D3DINSTRUCTION){D3DOP_EXIT, 0, 0};
    TRY(IDirect3DExecuteBuffer_Unlock(buffer));
    data.dwVertexCount = 3;
    data.dwInstructionOffset = FIELD_OFFSET(struct command_stream, process);
    data.dwInstructionLength = sizeof(*stream) - data.dwInstructionOffset;
    TRY(IDirect3DExecuteBuffer_SetExecuteData(buffer, &data));
    TRY(IDirect3DDevice_BeginScene(device));
    hr = checked("IDirect3DDevice::Execute", IDirect3DDevice_Execute(device, buffer, viewport, D3DEXECUTE_CLIPPED));
    HRESULT end = checked("IDirect3DDevice::EndScene", IDirect3DDevice_EndScene(device));
    if (SUCCEEDED(hr)) hr = end;
done:
    if (buffer) IDirect3DExecuteBuffer_Release(buffer);
    return hr;
}

static HRESULT draw_d3d1(IDirectDraw *ddraw, IDirectDrawSurface *surface)
{
    IDirect3D *api = NULL;
    IDirect3DDevice *device = NULL;
    IDirect3DViewport *viewport = NULL;
    D3DVIEWPORT settings = {sizeof(settings), 0, 0, SIDE, SIDE, SIDE / 2, SIDE / 2, 1, 1, 0, 1};
    HRESULT hr;
    TRY(IDirectDraw_QueryInterface(ddraw, &IID_IDirect3D, (void **)&api));
    TRY(IDirectDrawSurface_QueryInterface(surface, &IID_IDirect3DHALDevice, (void **)&device));
    TRY(IDirect3D_CreateViewport(api, &viewport, NULL));
    TRY(IDirect3DDevice_AddViewport(device, viewport));
    TRY(IDirect3DViewport_SetViewport(viewport, &settings));
    TRY(clear_target(surface));
    TRY(execute_triangle(device, viewport));
    TRY(check_pixels(surface, TRUE));
done:
    if (viewport && device) IDirect3DDevice_DeleteViewport(device, viewport);
    if (viewport) IDirect3DViewport_Release(viewport);
    if (device) IDirect3DDevice_Release(device);
    if (api) IDirect3D_Release(api);
    return hr;
}

static HRESULT draw_d3d2(IDirectDraw *ddraw, IDirectDrawSurface *surface)
{
    IDirect3D2 *api = NULL;
    IDirect3DDevice2 *device = NULL;
    IDirect3DViewport2 *viewport = NULL;
    D3DVIEWPORT2 settings = {sizeof(settings), 0, 0, SIDE, SIDE, -1, 1, 2, 2, 0, 1};
    HRESULT hr;
    TRY(IDirectDraw_QueryInterface(ddraw, &IID_IDirect3D2, (void **)&api));
    TRY(IDirect3D2_CreateDevice(api, &IID_IDirect3DHALDevice, surface, &device));
    TRY(IDirect3D2_CreateViewport(api, &viewport, NULL));
    TRY(IDirect3DDevice2_AddViewport(device, viewport));
    TRY(IDirect3DViewport2_SetViewport2(viewport, &settings));
    TRY(IDirect3DDevice2_SetCurrentViewport(device, viewport));
    TRY(IDirect3DDevice2_SetRenderState(device, D3DRENDERSTATE_CULLMODE, D3DCULL_NONE));
    TRY(IDirect3DDevice2_SetRenderState(device, D3DRENDERSTATE_ZENABLE, D3DZB_FALSE));
    TRY(clear_target(surface));
    TRY(IDirect3DDevice2_BeginScene(device));
    hr = checked("IDirect3DDevice2::DrawPrimitive", IDirect3DDevice2_DrawPrimitive(device,
            D3DPT_TRIANGLELIST, D3DVT_TLVERTEX, vertices, 3, 0));
    HRESULT end = checked("IDirect3DDevice2::EndScene", IDirect3DDevice2_EndScene(device));
    if (SUCCEEDED(hr)) hr = end;
    if (SUCCEEDED(hr)) hr = checked("Direct3D 2 pixels", check_pixels(surface, TRUE));
done:
    if (viewport && device) IDirect3DDevice2_DeleteViewport(device, viewport);
    if (viewport) IDirect3DViewport2_Release(viewport);
    if (device) IDirect3DDevice2_Release(device);
    if (api) IDirect3D2_Release(api);
    return hr;
}

static HRESULT draw_d3d3(IDirectDraw *ddraw, IDirectDrawSurface *surface)
{
    IDirect3D3 *api = NULL;
    IDirect3DDevice3 *device = NULL;
    IDirect3DViewport3 *viewport = NULL;
    IDirectDrawSurface4 *target = NULL;
    D3DVIEWPORT2 settings = {sizeof(settings), 0, 0, SIDE, SIDE, -1, 1, 2, 2, 0, 1};
    HRESULT hr;
    TRY(IDirectDraw_QueryInterface(ddraw, &IID_IDirect3D3, (void **)&api));
    TRY(IDirectDrawSurface_QueryInterface(surface, &IID_IDirectDrawSurface4, (void **)&target));
    TRY(IDirect3D3_CreateDevice(api, &IID_IDirect3DHALDevice, target, &device, NULL));
    TRY(IDirect3D3_CreateViewport(api, &viewport, NULL));
    TRY(IDirect3DDevice3_AddViewport(device, viewport));
    TRY(IDirect3DViewport3_SetViewport2(viewport, &settings));
    TRY(IDirect3DDevice3_SetCurrentViewport(device, viewport));
    TRY(IDirect3DDevice3_SetRenderState(device, D3DRENDERSTATE_CULLMODE, D3DCULL_NONE));
    TRY(IDirect3DDevice3_SetRenderState(device, D3DRENDERSTATE_ZENABLE, D3DZB_FALSE));
    TRY(clear_target(surface));
    TRY(IDirect3DDevice3_BeginScene(device));
    hr = checked("IDirect3DDevice3::DrawPrimitive", IDirect3DDevice3_DrawPrimitive(device,
            D3DPT_TRIANGLELIST, D3DFVF_TLVERTEX, vertices, 3, 0));
    HRESULT end = checked("IDirect3DDevice3::EndScene", IDirect3DDevice3_EndScene(device));
    if (SUCCEEDED(hr)) hr = end;
    if (SUCCEEDED(hr)) hr = checked("Direct3D 3 pixels", check_pixels(surface, TRUE));
done:
    if (viewport && device) IDirect3DDevice3_DeleteViewport(device, viewport);
    if (viewport) IDirect3DViewport3_Release(viewport);
    if (device) IDirect3DDevice3_Release(device);
    if (target) IDirectDrawSurface4_Release(target);
    if (api) IDirect3D3_Release(api);
    return hr;
}

static HRESULT draw_d3d7(IDirectDraw *ddraw, IDirectDrawSurface *surface)
{
    IDirect3D7 *api = NULL;
    IDirect3DDevice7 *device = NULL;
    IDirectDrawSurface7 *target = NULL;
    D3DVIEWPORT7 settings = {0, 0, SIDE, SIDE, 0, 1};
    HRESULT hr;
    TRY(IDirectDraw_QueryInterface(ddraw, &IID_IDirect3D7, (void **)&api));
    TRY(IDirectDrawSurface_QueryInterface(surface, &IID_IDirectDrawSurface7, (void **)&target));
    TRY(IDirect3D7_CreateDevice(api, &IID_IDirect3DHALDevice, target, &device));
    TRY(IDirect3DDevice7_SetViewport(device, &settings));
    TRY(IDirect3DDevice7_SetRenderState(device, D3DRENDERSTATE_CULLMODE, D3DCULL_NONE));
    TRY(IDirect3DDevice7_SetRenderState(device, D3DRENDERSTATE_ZENABLE, D3DZB_FALSE));
    TRY(clear_target(surface));
    TRY(IDirect3DDevice7_BeginScene(device));
    hr = checked("IDirect3DDevice7::DrawPrimitive", IDirect3DDevice7_DrawPrimitive(device,
            D3DPT_TRIANGLELIST, D3DFVF_TLVERTEX, vertices, 3, 0));
    HRESULT end = checked("IDirect3DDevice7::EndScene", IDirect3DDevice7_EndScene(device));
    if (SUCCEEDED(hr)) hr = end;
    if (SUCCEEDED(hr)) hr = checked("Direct3D 7 pixels", check_pixels(surface, TRUE));
done:
    if (device) IDirect3DDevice7_Release(device);
    if (target) IDirectDrawSurface7_Release(target);
    if (api) IDirect3D7_Release(api);
    return hr;
}

typedef HRESULT (*draw_fn)(IDirectDraw *, IDirectDrawSurface *);
typedef HRESULT (WINAPI *create_fn)(GUID *, IDirectDraw **, IUnknown *);

static HRESULT test_device(HWND window, create_fn create, draw_fn draw)
{
    IDirectDraw *ddraw = NULL;
    IDirectDrawSurface *surface = NULL;
    DDSURFACEDESC desc = {sizeof(desc)};
    HRESULT hr;
    TRY(create(NULL, &ddraw, NULL));
    TRY(IDirectDraw_SetCooperativeLevel(ddraw, window, DDSCL_NORMAL));
    desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
    desc.dwWidth = SIDE; desc.dwHeight = SIDE;
    desc.ddsCaps.dwCaps = DDSCAPS_3DDEVICE | DDSCAPS_OFFSCREENPLAIN;
    desc.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
    desc.ddpfPixelFormat.dwFlags = DDPF_RGB;
    desc.ddpfPixelFormat.dwRGBBitCount = 32;
    desc.ddpfPixelFormat.dwRBitMask = 0xff0000; desc.ddpfPixelFormat.dwGBitMask = 0xff00;
    desc.ddpfPixelFormat.dwBBitMask = 0xff;
    TRY(IDirectDraw_CreateSurface(ddraw, &desc, &surface, NULL));
    hr = draw(ddraw, surface);
done:
    if (surface) IDirectDrawSurface_Release(surface);
    if (ddraw) IDirectDraw_Release(ddraw);
    return hr;
}

void WINAPI old3d_entry(void)
{
    static const struct {char version; const char *name; draw_fn draw;} tests[] = {
        {'1', "Direct3D 1 execute buffer", draw_d3d1}, {'2', "Direct3D 2 DrawPrimitive", draw_d3d2},
        {'3', "Direct3D 3 FVF DrawPrimitive", draw_d3d3}, {'7', "Direct3D 7 DrawPrimitive", draw_d3d7}
    };
    initialize_test();
    const char *arg = GetCommandLineA();
    if (*arg == '"') { ++arg; while (*arg && *arg != '"') ++arg; if (*arg) ++arg; }
    else while (*arg && *arg != ' ' && *arg != '\t') ++arg;
    while (*arg == ' ' || *arg == '\t') ++arg;
    char selected = *arg;
    if (selected) {
        ++arg;
        while (*arg == ' ' || *arg == '\t') ++arg;
        if (*arg) selected = '?';
    }
    HWND window = CreateWindowExA(0, "STATIC", "WINDX early Direct3D test", WS_POPUP,
            0, 0, SIDE, SIDE, NULL, NULL, GetModuleHandleA(NULL), NULL);
    char path[MAX_PATH];
    DWORD path_size = GetModuleFileNameA(NULL, path, sizeof(path));
    if (!path_size || path_size >= sizeof(path)) ExitProcess(1);
    char *name = path;
    for (char *p = path; *p; ++p) if (*p == '\\' || *p == '/') name = p + 1;
    if ((SIZE_T)(name-path) + sizeof("ddraw.dll") > sizeof(path)) ExitProcess(1);
    lstrcpyA(name, "ddraw.dll");
    HMODULE module = LoadLibraryA(path);
    if (!module) {
        DWORD error = GetLastError();
        report("Could not load "); report(path); report(" error "); report_hex(error); report("\r\n");
    }
    if (module && GetModuleFileNameA(module, path, sizeof(path))) {
        report("ddraw.dll loaded from "); report(path); report("\r\n");
    }
    create_fn create = module ? (create_fn)GetProcAddress(module, "DirectDrawCreate") : NULL;
    if (!window || !create) { report("Cannot open DirectDraw or create the test window\r\n"); ExitProcess(1); }
    BOOL ok = TRUE, ran = FALSE;
    for (unsigned i = 0; i < sizeof(tests) / sizeof(*tests); ++i) {
        if (selected && selected != tests[i].version) continue;
        ran = TRUE;
        report(tests[i].name); report("\r\n");
        HRESULT hr = test_device(window, create, tests[i].draw);
        report(tests[i].name);
        report(SUCCEEDED(hr) ? " triangle and surface readback PASS\r\n" : " FAIL\r\n");
        if (FAILED(hr)) ok = FALSE;
    }
    FreeLibrary(module);
    DestroyWindow(window);
    if (!ran) report("Usage: wxold3d.exe [1|2|3|7]\r\n");
    ExitProcess(ok && ran ? 0 : 1);
}
