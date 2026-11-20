/* Typed ABI adapters for the successive DirectDraw interface layouts.
 * All views of a surface share its pixel store, lock state and COM identity. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>
#include <new>
#include "versions.h"

struct Family2 {
    using Draw = IDirectDraw2;
    using ReturnedSurface = IDirectDrawSurface;
    using Surface = IDirectDrawSurface2;
    using Desc = DDSURFACEDESC;
    using Caps = DDSCAPS;
    using EnumSurfaces = LPDDENUMSURFACESCALLBACK;
    using EnumModes = LPDDENUMMODESCALLBACK;
    using Identifier = DDDEVICEIDENTIFIER;
    using UnlockArg = void *;
    static REFIID draw_id() { return IID_IDirectDraw2; }
    static REFIID surface_id() { return IID_IDirectDrawSurface; }
};
struct Family3 : Family2 { using Surface = IDirectDrawSurface3; };
struct Family4 {
    using Draw = IDirectDraw4;
    using ReturnedSurface = IDirectDrawSurface4;
    using Surface = IDirectDrawSurface4;
    using Desc = DDSURFACEDESC2;
    using Caps = DDSCAPS2;
    using EnumSurfaces = LPDDENUMSURFACESCALLBACK2;
    using EnumModes = LPDDENUMMODESCALLBACK2;
    using Identifier = DDDEVICEIDENTIFIER;
    using UnlockArg = RECT *;
    static REFIID draw_id() { return IID_IDirectDraw4; }
    static REFIID surface_id() { return IID_IDirectDrawSurface4; }
};
struct Family7 : Family4 {
    using Draw = IDirectDraw7;
    using ReturnedSurface = IDirectDrawSurface7;
    using Surface = IDirectDrawSurface7;
    using EnumSurfaces = LPDDENUMSURFACESCALLBACK7;
    using Identifier = DDDEVICEIDENTIFIER2;
    static REFIID draw_id() { return IID_IDirectDraw7; }
    static REFIID surface_id() { return IID_IDirectDrawSurface7; }
};

template<typename To, typename From>
static void copy_description(To &to, const From &from)
{
    to = {};
    to.dwSize = sizeof(to);
    to.dwFlags = from.dwFlags;
    to.dwWidth = from.dwWidth;
    to.dwHeight = from.dwHeight;
    to.lPitch = from.lPitch;
    to.dwRefreshRate = from.dwRefreshRate;
    to.dwBackBufferCount = from.dwBackBufferCount;
    to.lpSurface = from.lpSurface;
    to.ddpfPixelFormat = from.ddpfPixelFormat;
    to.ddsCaps.dwCaps = from.ddsCaps.dwCaps;
}
static bool extra_caps(const DDSURFACEDESC &) { return false; }
static bool extra_caps(const DDSURFACEDESC2 &desc)
{
    return desc.ddsCaps.dwCaps2 || desc.ddsCaps.dwCaps3 || desc.ddsCaps.dwCaps4;
}

template<typename S>
static HRESULT return_surface(HRESULT hr, IDirectDrawSurface *surface, REFIID iid, S **out)
{
    if (FAILED(hr)) return hr;
    hr = surface->QueryInterface(iid, reinterpret_cast<void **>(out));
    surface->Release();
    return hr;
}

template<typename Family>
class DrawView final : public Family::Draw {
    using Surface = typename Family::ReturnedSurface;
    using Desc = typename Family::Desc;
    using Caps = typename Family::Caps;
    using EnumModes = typename Family::EnumModes;
    using EnumSurfacesCallback = typename Family::EnumSurfaces;
    using Identifier = typename Family::Identifier;
    IDirectDraw *draw;
    struct ModeContext { EnumModes callback; void *user; };
    struct SurfaceContext { EnumSurfacesCallback callback; void *user; HRESULT hr; };
    static HRESULT CALLBACK surface_callback(IDirectDrawSurface *original,DDSURFACEDESC *old,void *opaque)
    {
        auto *state=static_cast<SurfaceContext *>(opaque);
        Surface *surface=nullptr;
        state->hr=return_surface(DD_OK,original,Family::surface_id(),&surface);
        if (FAILED(state->hr)) return DDENUMRET_CANCEL;
        Desc desc;
        copy_description(desc,*old);
        return state->callback(surface,&desc,state->user);
    }
    static HRESULT CALLBACK mode_callback(DDSURFACEDESC *old, void *ctx)
    {
        auto *state = static_cast<ModeContext *>(ctx);
        Desc desc;
        copy_description(desc, *old);
        return state->callback(&desc, state->user);
    }
public:
    explicit DrawView(IDirectDraw *base) : draw(base) {}
    HRESULT WINAPI QueryInterface(REFIID iid, void **out) override { return draw->QueryInterface(iid, out); }
    ULONG WINAPI AddRef() override { return draw->AddRef(); }
    ULONG WINAPI Release() override { return draw->Release(); }
    HRESULT WINAPI Compact() override { return draw->Compact(); }
    HRESULT WINAPI CreateClipper(DWORD flags, IDirectDrawClipper **out, IUnknown *outer) override
    { return draw->CreateClipper(flags, out, outer); }
    HRESULT WINAPI CreatePalette(DWORD flags, PALETTEENTRY *entries, IDirectDrawPalette **out, IUnknown *outer) override
    { return draw->CreatePalette(flags, entries, out, outer); }
    HRESULT WINAPI CreateSurface(Desc *desc, Surface **out, IUnknown *outer) override
    {
        if (!out) return DDERR_INVALIDPARAMS;
        *out = nullptr;
        if (!desc || desc->dwSize != sizeof(*desc)) return DDERR_INVALIDPARAMS;
        if ((desc->dwFlags & DDSD_CAPS) && extra_caps(*desc)) return DDERR_INVALIDCAPS;
        DDSURFACEDESC old;
        copy_description(old, *desc);
        IDirectDrawSurface *surface = nullptr;
        HRESULT hr = draw->CreateSurface(&old, &surface, outer);
        if (SUCCEEDED(hr)) wx_set_surface_origin(surface, &Family::draw_id());
        return return_surface(hr, surface, Family::surface_id(), out);
    }
    HRESULT WINAPI DuplicateSurface(Surface *source, Surface **out) override
    {
        if (!source || !out) return DDERR_INVALIDPARAMS;
        *out = nullptr;
        IDirectDrawSurface *old = nullptr, *duplicate = nullptr;
        HRESULT hr = source->QueryInterface(IID_IDirectDrawSurface, reinterpret_cast<void **>(&old));
        if (FAILED(hr)) return hr;
        hr = draw->DuplicateSurface(old, &duplicate);
        old->Release();
        return return_surface(hr, duplicate, Family::surface_id(), out);
    }
    HRESULT WINAPI EnumDisplayModes(DWORD flags, Desc *filter, void *user, EnumModes callback) override
    {
        if (!callback || (filter && filter->dwSize != sizeof(*filter))) return DDERR_INVALIDPARAMS;
        DDSURFACEDESC old;
        if (filter) copy_description(old, *filter);
        ModeContext ctx = {callback, user};
        return draw->EnumDisplayModes(flags, filter ? &old : nullptr, &ctx, mode_callback);
    }
    HRESULT WINAPI EnumSurfaces(DWORD flags, Desc *filter, void *user, EnumSurfacesCallback callback) override
    {
        if (!callback || (filter && filter->dwSize!=sizeof(*filter))) return DDERR_INVALIDPARAMS;
        DDSURFACEDESC old;
        if (filter) copy_description(old,*filter);
        SurfaceContext state={callback,user,DD_OK};
        HRESULT hr=draw->EnumSurfaces(flags,filter?&old:nullptr,&state,surface_callback);
        return FAILED(hr)?hr:state.hr;
    }
    HRESULT WINAPI FlipToGDISurface() override { return draw->FlipToGDISurface(); }
    HRESULT WINAPI GetCaps(DDCAPS *hardware, DDCAPS *software) override { return draw->GetCaps(hardware, software); }
    HRESULT WINAPI GetDisplayMode(Desc *desc) override
    {
        if (!desc || desc->dwSize != sizeof(*desc)) return DDERR_INVALIDPARAMS;
        DDSURFACEDESC old = {sizeof(old)};
        HRESULT hr = draw->GetDisplayMode(&old);
        if (SUCCEEDED(hr)) copy_description(*desc, old);
        return hr;
    }
    HRESULT WINAPI GetFourCCCodes(DWORD *count, DWORD *codes) override { return draw->GetFourCCCodes(count, codes); }
    HRESULT WINAPI GetGDISurface(Surface **out) override
    {
        if (!out) return DDERR_INVALIDPARAMS;
        *out = nullptr;
        IDirectDrawSurface *surface = nullptr;
        HRESULT hr = draw->GetGDISurface(&surface);
        return return_surface(hr, surface, Family::surface_id(), out);
    }
    HRESULT WINAPI GetMonitorFrequency(DWORD *hz) override { return draw->GetMonitorFrequency(hz); }
    HRESULT WINAPI GetScanLine(DWORD *line) override { return draw->GetScanLine(line); }
    HRESULT WINAPI GetVerticalBlankStatus(BOOL *state) override { return draw->GetVerticalBlankStatus(state); }
    HRESULT WINAPI Initialize(GUID *id) override { return draw->Initialize(id); }
    HRESULT WINAPI RestoreDisplayMode() override { return draw->RestoreDisplayMode(); }
    HRESULT WINAPI SetCooperativeLevel(HWND window, DWORD flags) override { return draw->SetCooperativeLevel(window, flags); }
    HRESULT WINAPI SetDisplayMode(DWORD w, DWORD h, DWORD depth, DWORD refresh, DWORD flags) override
    { return flags || refresh ? DDERR_NOEXCLUSIVEMODE : draw->SetDisplayMode(w, h, depth); }
    HRESULT WINAPI WaitForVerticalBlank(DWORD flags, HANDLE event) override { return draw->WaitForVerticalBlank(flags, event); }
    HRESULT WINAPI GetAvailableVidMem(Caps *caps, DWORD *total, DWORD *free) override
    {
        if (!caps || (!total && !free)) return DDERR_INVALIDPARAMS;
        MEMORYSTATUS memory = {sizeof(memory)};
        if (caps->dwCaps & DDSCAPS_SYSTEMMEMORY) GlobalMemoryStatus(&memory);
        if (total) *total = memory.dwTotalPhys > MAXDWORD ? MAXDWORD : static_cast<DWORD>(memory.dwTotalPhys);
        if (free) *free = memory.dwAvailPhys > MAXDWORD ? MAXDWORD : static_cast<DWORD>(memory.dwAvailPhys);
        return DD_OK;
    }
    HRESULT WINAPI GetSurfaceFromDC(HDC dc, Surface **out)
    {
        if (!out) return DDERR_INVALIDPARAMS;
        *out = nullptr;
        IDirectDrawSurface *surface = nullptr;
        HRESULT hr = wx_find_surface_dc(draw, dc, &surface);
        return return_surface(hr, surface, Family::surface_id(), out);
    }
    HRESULT WINAPI RestoreAllSurfaces() { return DD_OK; }
    HRESULT WINAPI TestCooperativeLevel() { return wx_draw_cooperative_state(draw); }
    HRESULT WINAPI GetDeviceIdentifier(Identifier *id, DWORD flags)
    {
        if (!id || (flags & ~DDGDI_GETHOSTIDENTIFIER)) return DDERR_INVALIDPARAMS;
        *id = {};
        lstrcpyA(id->szDriver, "ddraw.dll");
        lstrcpyA(id->szDescription, "WINDX GDI software display");
        id->liDriverVersion.HighPart = 0x00010000;
        /* A software-device ID, stable across machines and distinct from PCI IDs. */
        id->guidDeviceIdentifier = {0x95c65985, 0x88d0, 0x49de, {0xa5,0x2e,0x07,0x76,0xb6,0xa3,0x6d,0x90}};
        return DD_OK;
    }
    HRESULT WINAPI StartModeTest(SIZE *, DWORD, DWORD) { return DDERR_NOEXCLUSIVEMODE; }
    HRESULT WINAPI EvaluateMode(DWORD, DWORD *) { return DDERR_NOEXCLUSIVEMODE; }
};

static HRESULT unlock_surface(IDirectDrawSurface *surface, void *address) { return surface->Unlock(address); }
static HRESULT unlock_surface(IDirectDrawSurface *surface, RECT *rect) { return wx_surface_unlock_rect(surface, rect); }

template<typename Family>
class SurfaceView final : public Family::Surface {
    using Surface = typename Family::Surface;
    using Desc = typename Family::Desc;
    using Caps = typename Family::Caps;
    using Enums = typename Family::EnumSurfaces;
    using UnlockArg = typename Family::UnlockArg;
    IDirectDrawSurface *base;
public:
    explicit SurfaceView(IDirectDrawSurface *surface) : base(surface) {}
    HRESULT WINAPI QueryInterface(REFIID iid, void **out) override { return base->QueryInterface(iid, out); }
    ULONG WINAPI AddRef() override { return base->AddRef(); }
    ULONG WINAPI Release() override { return base->Release(); }
    HRESULT WINAPI AddAttachedSurface(Surface *) override { return DDERR_UNSUPPORTED; }
    HRESULT WINAPI AddOverlayDirtyRect(RECT *r) override { return base->AddOverlayDirtyRect(r); }
    HRESULT WINAPI Blt(RECT *dst, Surface *source, RECT *src, DWORD flags, DDBLTFX *fx) override
    {
        IDirectDrawSurface *original = nullptr;
        if (source && FAILED(source->QueryInterface(IID_IDirectDrawSurface, reinterpret_cast<void **>(&original))))
            return DDERR_INVALIDOBJECT;
        HRESULT hr = base->Blt(dst, original, src, flags, fx);
        if (original) original->Release();
        return hr;
    }
    HRESULT WINAPI BltBatch(DDBLTBATCH *list, DWORD count, DWORD flags) override { return base->BltBatch(list, count, flags); }
    HRESULT WINAPI BltFast(DWORD x, DWORD y, Surface *source, RECT *src, DWORD flags) override
    {
        IDirectDrawSurface *original = nullptr;
        if (!source || FAILED(source->QueryInterface(IID_IDirectDrawSurface, reinterpret_cast<void **>(&original))))
            return DDERR_INVALIDOBJECT;
        HRESULT hr = base->BltFast(x, y, original, src, flags);
        original->Release();
        return hr;
    }
    HRESULT WINAPI DeleteAttachedSurface(DWORD, Surface *) override { return DDERR_SURFACENOTATTACHED; }
    HRESULT WINAPI EnumAttachedSurfaces(void *, Enums callback) override { return callback ? DD_OK : DDERR_INVALIDPARAMS; }
    HRESULT WINAPI EnumOverlayZOrders(DWORD, void *, Enums) override { return DDERR_NOOVERLAYHW; }
    HRESULT WINAPI Flip(Surface *, DWORD) override { return DDERR_NOTFLIPPABLE; }
    HRESULT WINAPI GetAttachedSurface(Caps *, Surface **out) override
    { if (!out) return DDERR_INVALIDPARAMS; *out = nullptr; return DDERR_NOTFOUND; }
    HRESULT WINAPI GetBltStatus(DWORD flags) override { return base->GetBltStatus(flags); }
    HRESULT WINAPI GetCaps(Caps *caps) override
    {
        if (!caps) return DDERR_INVALIDPARAMS;
        DDSCAPS old;
        HRESULT hr = base->GetCaps(&old);
        if (SUCCEEDED(hr)) { *caps = {}; caps->dwCaps = old.dwCaps; }
        return hr;
    }
    HRESULT WINAPI GetClipper(IDirectDrawClipper **out) override { return base->GetClipper(out); }
    HRESULT WINAPI GetColorKey(DWORD flags, DDCOLORKEY *key) override { return base->GetColorKey(flags, key); }
    HRESULT WINAPI GetDC(HDC *dc) override { return base->GetDC(dc); }
    HRESULT WINAPI GetFlipStatus(DWORD flags) override { return base->GetFlipStatus(flags); }
    HRESULT WINAPI GetOverlayPosition(LONG *x, LONG *y) override { return base->GetOverlayPosition(x, y); }
    HRESULT WINAPI GetPalette(IDirectDrawPalette **out) override { return base->GetPalette(out); }
    HRESULT WINAPI GetPixelFormat(DDPIXELFORMAT *fmt) override { return base->GetPixelFormat(fmt); }
    HRESULT WINAPI GetSurfaceDesc(Desc *desc) override
    {
        if (!desc || desc->dwSize != sizeof(*desc)) return DDERR_INVALIDPARAMS;
        DDSURFACEDESC old = {sizeof(old)};
        HRESULT hr = base->GetSurfaceDesc(&old);
        if (SUCCEEDED(hr)) copy_description(*desc, old);
        return hr;
    }
    HRESULT WINAPI Initialize(IDirectDraw *, Desc *) override { return DDERR_ALREADYINITIALIZED; }
    HRESULT WINAPI IsLost() override { return base->IsLost(); }
    HRESULT WINAPI Lock(RECT *r, Desc *desc, DWORD flags, HANDLE event) override
    {
        if (!desc || desc->dwSize != sizeof(*desc)) return DDERR_INVALIDPARAMS;
        DDSURFACEDESC old = {sizeof(old)};
        HRESULT hr = base->Lock(r, &old, flags, event);
        if (SUCCEEDED(hr)) copy_description(*desc, old);
        return hr;
    }
    HRESULT WINAPI ReleaseDC(HDC dc) override { return base->ReleaseDC(dc); }
    HRESULT WINAPI Restore() override { return base->Restore(); }
    HRESULT WINAPI SetClipper(IDirectDrawClipper *clipper) override { return base->SetClipper(clipper); }
    HRESULT WINAPI SetColorKey(DWORD flags, DDCOLORKEY *key) override { return base->SetColorKey(flags, key); }
    HRESULT WINAPI SetOverlayPosition(LONG x, LONG y) override { return base->SetOverlayPosition(x, y); }
    HRESULT WINAPI SetPalette(IDirectDrawPalette *p) override { return base->SetPalette(p); }
    HRESULT WINAPI Unlock(UnlockArg arg) override { return unlock_surface(base, arg); }
    HRESULT WINAPI UpdateOverlay(RECT *, Surface *, RECT *, DWORD, DDOVERLAYFX *) override { return DDERR_NOOVERLAYHW; }
    HRESULT WINAPI UpdateOverlayDisplay(DWORD flags) override { return base->UpdateOverlayDisplay(flags); }
    HRESULT WINAPI UpdateOverlayZOrder(DWORD, Surface *) override { return DDERR_NOOVERLAYHW; }
    HRESULT WINAPI GetDDInterface(void **out) override { return wx_surface_owner(base, out); }
    HRESULT WINAPI PageLock(DWORD flags) override { return wx_surface_page_lock(base, flags, FALSE); }
    HRESULT WINAPI PageUnlock(DWORD flags) override { return wx_surface_page_lock(base, flags, TRUE); }
    HRESULT WINAPI SetSurfaceDesc(Desc *, DWORD) { return DDERR_UNSUPPORTED; }
    HRESULT WINAPI SetPrivateData(REFGUID tag, void *data, DWORD bytes, DWORD flags) { return wx_private_set(base,&tag,data,bytes,flags); }
    HRESULT WINAPI GetPrivateData(REFGUID tag, void *data, DWORD *bytes) { return wx_private_get(base,&tag,data,bytes); }
    HRESULT WINAPI FreePrivateData(REFGUID tag) { return wx_private_free(base,&tag); }
    HRESULT WINAPI GetUniquenessValue(DWORD *value) { return wx_surface_uniqueness(base, value, FALSE); }
    HRESULT WINAPI ChangeUniquenessValue() { return wx_surface_uniqueness(base, nullptr, TRUE); }
    HRESULT WINAPI SetPriority(DWORD) { return DDERR_INVALIDOBJECT; }
    HRESULT WINAPI GetPriority(DWORD *) { return DDERR_INVALIDOBJECT; }
    HRESULT WINAPI SetLOD(DWORD) { return DDERR_INVALIDOBJECT; }
    HRESULT WINAPI GetLOD(DWORD *) { return DDERR_INVALIDOBJECT; }
};

struct DrawViews {
    DrawView<Family2> two;
    DrawView<Family4> four;
    DrawView<Family7> seven;
    explicit DrawViews(IDirectDraw *d) : two(d), four(d), seven(d) {}
};
struct SurfaceViews {
    SurfaceView<Family2> two;
    SurfaceView<Family3> three;
    SurfaceView<Family4> four;
    SurfaceView<Family7> seven;
    explicit SurfaceViews(IDirectDrawSurface *s) : two(s), three(s), four(s), seven(s) {}
};

extern "C" void *wx_create_draw_views(IDirectDraw *draw)
{
    void *memory = HeapAlloc(GetProcessHeap(), 0, sizeof(DrawViews));
    return memory ? new(memory) DrawViews(draw) : nullptr;
}
extern "C" void *wx_create_surface_views(IDirectDrawSurface *surface)
{
    void *memory = HeapAlloc(GetProcessHeap(), 0, sizeof(SurfaceViews));
    return memory ? new(memory) SurfaceViews(surface) : nullptr;
}
extern "C" void *wx_draw_view(void *views, const IID *iid)
{
    if (!iid) return nullptr;
    auto *v = static_cast<DrawViews *>(views);
    if (wx_same_guid(iid, &IID_IDirectDraw2)) return &v->two;
    if (wx_same_guid(iid, &IID_IDirectDraw4)) return &v->four;
    if (wx_same_guid(iid, &IID_IDirectDraw7)) return &v->seven;
    return nullptr;
}
extern "C" void *wx_surface_view(void *views, const IID *iid)
{
    if (!iid) return nullptr;
    auto *v = static_cast<SurfaceViews *>(views);
    if (wx_same_guid(iid, &IID_IDirectDrawSurface2)) return &v->two;
    if (wx_same_guid(iid, &IID_IDirectDrawSurface3)) return &v->three;
    if (wx_same_guid(iid, &IID_IDirectDrawSurface4)) return &v->four;
    if (wx_same_guid(iid, &IID_IDirectDrawSurface7)) return &v->seven;
    return nullptr;
}
