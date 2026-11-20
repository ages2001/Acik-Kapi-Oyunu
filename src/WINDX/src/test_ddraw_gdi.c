#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <ddraw.h>
#include "test_output.h"

static BOOL checked(const char *stage, HRESULT hr)
{
    if (SUCCEEDED(hr)) return TRUE;
    report(stage); report(" FAIL "); report_hex((DWORD)hr); report("\r\n");
    return FALSE;
}

static BOOL test(IDirectDraw *draw, HWND window)
{
    DDSURFACEDESC desc = {sizeof(desc)}, lock = {sizeof(lock)};
    DDBLTFX effect = {sizeof(effect)};
    RECT red_row = {0, 1, 4, 2}, clip = {1, 1, 4, 3}, dest;
    RECT invalid = {3, 1, 5, 2};
    POINT origin = {8, 8};
    IDirectDrawSurface *primary = NULL, *src = NULL, *copy = NULL;
    IDirectDraw *identity = NULL;
    HDC dc;
    COLORREF sample[3];
    BOOL ok = FALSE, pixels = TRUE;
    unsigned x, y;

    if (!checked("QueryInterface", IDirectDraw_QueryInterface(draw, &IID_IDirectDraw, (void **)&identity))) goto done;
    if (identity != draw) goto done;
    if (!checked("SetCooperativeLevel", IDirectDraw_SetCooperativeLevel(draw, window, DDSCL_NORMAL))) goto done;
    desc.dwFlags = DDSD_CAPS;
    desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    if (!checked("Create primary", IDirectDraw_CreateSurface(draw, &desc, &primary, NULL))) goto done;
    desc.dwFlags |= DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
    desc.dwWidth = 4; desc.dwHeight = 3;
    desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
    desc.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT); desc.ddpfPixelFormat.dwFlags = DDPF_RGB;
    desc.ddpfPixelFormat.dwRGBBitCount = 32; desc.ddpfPixelFormat.dwRBitMask = 0xff0000;
    desc.ddpfPixelFormat.dwGBitMask = 0xff00; desc.ddpfPixelFormat.dwBBitMask = 0xff;
    if (!checked("Create source", IDirectDraw_CreateSurface(draw, &desc, &src, NULL)) ||
        !checked("Create destination", IDirectDraw_CreateSurface(draw, &desc, &copy, NULL))) goto done;
    effect.dwFillColor = 0xffffff;
    if (!checked("White fill", IDirectDrawSurface_Blt(src, NULL, NULL, NULL, DDBLT_COLORFILL, &effect))) goto done;
    effect.dwFillColor = 0xff0000;
    if (!checked("Red row", IDirectDrawSurface_Blt(src, &red_row, NULL, NULL, DDBLT_COLORFILL, &effect))) goto done;
    if (!checked("BltFast", IDirectDrawSurface_BltFast(copy, 0, 0, src, NULL, DDBLTFAST_WAIT))) goto done;
    if (!checked("Subrectangle lock", IDirectDrawSurface_Lock(copy, &clip, &lock, DDLOCK_WAIT, NULL))) goto done;
    if (!lock.lpSurface || lock.lPitch < 16) goto done;
    ((DWORD *)((BYTE *)lock.lpSurface + lock.lPitch))[0] = 0x123456;
    if (!checked("Unlock write", IDirectDrawSurface_Unlock(copy, lock.lpSurface))) goto done;
    if (!checked("Read lock", IDirectDrawSurface_Lock(copy, NULL, &lock, DDLOCK_READONLY, NULL))) goto done;
    for (y = 0; y < 3; ++y) for (x = 0; x < 4; ++x) {
        const DWORD *row = (const DWORD *)((const BYTE *)lock.lpSurface + y * lock.lPitch);
        DWORD wanted = y == 1 ? 0xff0000 : (x == 1 && y == 2 ? 0x123456 : 0xffffff);
        if (row[x] != wanted) pixels = FALSE;
    }
    if (!checked("Unlock read", IDirectDrawSurface_Unlock(copy, NULL)) || !pixels) goto done;
    if (IDirectDrawSurface_Blt(copy, &invalid, src, NULL, 0, NULL) != DDERR_INVALIDRECT) goto done;
    report("DirectDraw RGB32 fill, copy, subrectangle lock and 12-pixel readback PASS\r\n");

    if (!ClientToScreen(window, &origin)) goto done;
    dest.left = origin.x; dest.top = origin.y; dest.right = origin.x + 32; dest.bottom = origin.y + 24;
    if (!checked("Primary blit", IDirectDrawSurface_Blt(primary, &dest, src, NULL, DDBLT_WAIT, NULL))) goto done;
    dc = GetDC(window);
    if (!dc) goto done;
    sample[0] = GetPixel(dc, 12, 11); sample[1] = GetPixel(dc, 12, 19); sample[2] = GetPixel(dc, 12, 27);
    ReleaseDC(window, dc);
    if (sample[0] != RGB(255,255,255) || sample[1] != RGB(255,0,0) || sample[2] != RGB(255,255,255)) {
        report("Window readback FAIL "); report_hex(sample[0]); report(" "); report_hex(sample[1]);
        report(" "); report_hex(sample[2]); report("\r\n"); goto done;
    }
    report("DirectDraw primary stretch blit, window white/red/white readback PASS\r\n");
    ok = TRUE;
done:
    if (copy) IDirectDrawSurface_Release(copy);
    if (src) IDirectDrawSurface_Release(src);
    if (primary) IDirectDrawSurface_Release(primary);
    if (identity) IDirectDraw_Release(identity);
    return ok;
}

void WINAPI directdraw_entry(void)
{
    typedef HRESULT (WINAPI *create_fn)(GUID *, IDirectDraw **, IUnknown *);
    char path[MAX_PATH];
    HMODULE dll = NULL;
    HWND window = NULL;
    IDirectDraw *draw = NULL;
    create_fn create;
    DWORD n = GetModuleFileNameA(NULL, path, sizeof(path));
    BOOL ok = FALSE;
    initialize_test();
    if (!n || n >= sizeof(path)) goto done;
    while (n && path[n-1] != '\\' && path[n-1] != '/') --n;
    if (n + sizeof("ddraw.dll") > sizeof(path)) goto done;
    lstrcpyA(path + n, "ddraw.dll");
    dll = LoadLibraryA(path);
    create = dll ? (create_fn)GetProcAddress(dll, "DirectDrawCreate") : NULL;
    if (!create) { report("DirectDrawCreate load FAIL "); report_hex(GetLastError()); report("\r\n"); goto done; }
    GetModuleFileNameA(dll, path, sizeof(path)); report("DirectDraw module "); report(path); report("\r\n");
    if (!checked("DirectDrawCreate", create(NULL, &draw, NULL))) goto done;
    window = CreateWindowExA(WS_EX_TOPMOST, "STATIC", "WINDX DirectDraw display test",
            WS_POPUP | WS_BORDER | WS_VISIBLE, 60, 60, 160, 80, NULL, NULL, GetModuleHandleA(NULL), NULL);
    if (window) { UpdateWindow(window); ok = test(draw, window); }
done:
    if (draw) IDirectDraw_Release(draw);
    if (window) DestroyWindow(window);
    if (dll) FreeLibrary(dll);
    report(ok ? "PASS\r\n" : "FAIL\r\n");
    ExitProcess(ok ? 0 : 1);
}
