/* Windowed DirectDraw software surfaces for Win32 systems with classic GDI. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <ddraw.h>
#include <stddef.h>
#include "early/versions.h"

typedef struct Draw Draw;
typedef struct Surface Surface;
typedef struct Palette Palette;
typedef struct PrivateData PrivateData;
struct Draw {
    IDirectDraw iface;
    LONG refs;
    CRITICAL_SECTION guard;
    HWND window;
    Surface *primary;
    Surface *surfaces;
    void *views;
};
struct Surface {
    IDirectDrawSurface iface;
    LONG refs;
    Draw *owner;
    DWORD width, height, caps, pitch;
    BOOL indexed;
    Palette *palette;
    IDirectDrawClipper *clipper;
    DDCOLORKEY source_key, dest_key;
    BOOL key_source, key_dest;
    DWORD *pixels;
    void *lock_address;
    BOOL read_only;
    RECT lock_rect;
    void *views;
    const IID *origin;
    Surface *next;
    DWORD page_locks, uniqueness;
    PrivateData *private_data;
    HDC dc;
    HBITMAP dc_bitmap, dc_previous;
    DWORD *dc_bits, *dc_snapshot;
};

struct Palette {
    IDirectDrawPalette iface;
    Draw *owner;
    LONG refs;
    DWORD flags;
    PALETTEENTRY entries[256];
};
static void release_surface_data(Surface *s);
static IDirectDrawPaletteVtbl palette_vtbl;
static ULONG WINAPI palette_release(IDirectDrawPalette *iface);
static IDirectDrawVtbl draw_vtbl;
static IDirectDrawSurfaceVtbl surface_vtbl;

static void clear_bytes(void *data, DWORD size)
{
    BYTE *bytes = data;
    while (size--) *bytes++ = 0;
}
BOOL wx_same_guid(REFGUID a, REFGUID b)
{
    unsigned int i;
    if (!a || !b) return FALSE;
    for (i = 0; i < sizeof(GUID); ++i)
        if (((const BYTE *)a)[i] != ((const BYTE *)b)[i]) return FALSE;
    return TRUE;
}

static ULONG WINAPI draw_addref(IDirectDraw *iface) { return InterlockedIncrement(&((Draw *)iface)->refs); }
static ULONG WINAPI draw_release(IDirectDraw *iface)
{
    Draw *d = (Draw *)iface;
    ULONG count = InterlockedDecrement(&d->refs);
    if (!count) {
        HeapFree(GetProcessHeap(), 0, d->views);
        DeleteCriticalSection(&d->guard);
        HeapFree(GetProcessHeap(), 0, d);
    }
    return count;
}
static HRESULT WINAPI draw_query(IDirectDraw *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (wx_same_guid(iid, &IID_IUnknown) || wx_same_guid(iid, &IID_IDirectDraw)) *out = iface;
    else *out = wx_draw_view(((Draw *)iface)->views, iid);
    if (!*out) return E_NOINTERFACE;
    draw_addref(iface);
    return DD_OK;
}
static ULONG WINAPI surface_addref(IDirectDrawSurface *iface)
{
    return InterlockedIncrement(&((Surface *)iface)->refs);
}
static ULONG WINAPI surface_release(IDirectDrawSurface *iface)
{
    Surface *s = (Surface *)iface;
    Draw *d = s->owner;
    ULONG count;
    EnterCriticalSection(&d->guard);
    count = InterlockedDecrement(&s->refs);
    if (!count) {
        if (d->primary == s) d->primary = NULL;
        Surface **link = &d->surfaces;
        while (*link != s) link = &(*link)->next;
        *link = s->next;
    }
    LeaveCriticalSection(&d->guard);
    if (!count) {
        if (s->dc) {
            SelectObject(s->dc, s->dc_previous);
            DeleteObject(s->dc_bitmap);
            DeleteDC(s->dc);
            if (s->dc_snapshot) HeapFree(GetProcessHeap(), 0, s->dc_snapshot);
        }
        release_surface_data(s);
        if (s->clipper) IDirectDrawClipper_Release(s->clipper);
        if (s->palette) palette_release(&s->palette->iface);
        HeapFree(GetProcessHeap(), 0, s->views);
        VirtualFree(s->pixels, 0, MEM_RELEASE);
        HeapFree(GetProcessHeap(), 0, s);
        draw_release(&d->iface);
    }
    return count;
}
static HRESULT WINAPI surface_query(IDirectDrawSurface *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (wx_same_guid(iid, &IID_IUnknown) || wx_same_guid(iid, &IID_IDirectDrawSurface)) *out = iface;
    else *out = wx_surface_view(((Surface *)iface)->views, iid);
    if (!*out) return E_NOINTERFACE;
    surface_addref(iface);
    return DD_OK;
}
static void pixel_format(DDPIXELFORMAT *fmt, BOOL indexed)
{
    clear_bytes(fmt, sizeof(*fmt));
    fmt->dwSize = sizeof(*fmt);
    if (indexed) {
        fmt->dwFlags = DDPF_RGB | DDPF_PALETTEINDEXED8;
        fmt->dwRGBBitCount = 8;
        return;
    }
    fmt->dwFlags = DDPF_RGB;
    fmt->dwRGBBitCount = 32;
    fmt->dwRBitMask = 0xff0000;
    fmt->dwGBitMask = 0x00ff00;
    fmt->dwBBitMask = 0x0000ff;
}
static BOOL supported_format(const DDPIXELFORMAT *fmt)
{
    if (fmt->dwSize == sizeof(*fmt) && fmt->dwRGBBitCount == 8 &&
            fmt->dwFlags == (DDPF_RGB | DDPF_PALETTEINDEXED8)) return TRUE;
    return fmt->dwSize == sizeof(*fmt) && fmt->dwFlags == DDPF_RGB && fmt->dwRGBBitCount == 32 &&
            fmt->dwRBitMask == 0xff0000 && fmt->dwGBitMask == 0xff00 && fmt->dwBBitMask == 0xff;
}
static HRESULT describe(DDSURFACEDESC *desc, DWORD width, DWORD height, DWORD caps, BOOL indexed)
{
    if (!desc || desc->dwSize != sizeof(*desc)) return DDERR_INVALIDPARAMS;
    clear_bytes(desc, sizeof(*desc));
    desc->dwSize = sizeof(*desc);
    desc->dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PITCH | DDSD_PIXELFORMAT;
    if (caps) desc->dwFlags |= DDSD_CAPS;
    desc->dwWidth = width;
    desc->dwHeight = height;
    desc->lPitch = indexed ? (width + 3) & ~3u : width * sizeof(DWORD);
    desc->ddsCaps.dwCaps = caps;
    pixel_format(&desc->ddpfPixelFormat, indexed);
    return DD_OK;
}
static HRESULT WINAPI draw_mode(IDirectDraw *iface, DDSURFACEDESC *desc)
{
    return describe(desc, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), 0, FALSE);
}
static HRESULT WINAPI draw_coop(IDirectDraw *iface, HWND window, DWORD flags)
{
    Draw *d = (Draw *)iface;
    if (!(flags & DDSCL_NORMAL) || (flags & ~(DDSCL_NORMAL | DDSCL_MULTITHREADED |
            DDSCL_FPUPRESERVE | DDSCL_NOWINDOWCHANGES))) return DDERR_UNSUPPORTED;
    if (!window) window = GetDesktopWindow();
    if (!IsWindow(window)) return DDERR_INVALIDPARAMS;
    EnterCriticalSection(&d->guard);
    d->window = window;
    LeaveCriticalSection(&d->guard);
    return DD_OK;
}
static HRESULT WINAPI draw_create_surface(IDirectDraw *iface, DDSURFACEDESC *desc,
        IDirectDrawSurface **out, IUnknown *outer)
{
    Draw *d = (Draw *)iface;
    Surface *s;
    DWORD caps, width, height;
    BOOL primary, indexed;
    DWORD pitch;
    HRESULT hr = DD_OK;
    if (!out) return DDERR_INVALIDPARAMS;
    *out = NULL;
    if (outer) return CLASS_E_NOAGGREGATION;
    if (!desc || desc->dwSize != sizeof(*desc) || !(desc->dwFlags & DDSD_CAPS)) return DDERR_INVALIDPARAMS;
    if (desc->dwFlags & ~(DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT)) return DDERR_UNSUPPORTED;
    caps = desc->ddsCaps.dwCaps;
    primary = (caps & DDSCAPS_PRIMARYSURFACE) != 0;
    if (caps & ~(DDSCAPS_PRIMARYSURFACE | DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY)) return DDERR_INVALIDCAPS;
    if (primary && (caps & DDSCAPS_OFFSCREENPLAIN)) return DDERR_INVALIDCAPS;
    if ((desc->dwFlags & DDSD_PIXELFORMAT) && !supported_format(&desc->ddpfPixelFormat)) return DDERR_INVALIDPIXELFORMAT;
    if (primary && (desc->dwFlags & (DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT))) return DDERR_INVALIDPARAMS;
    if (!primary && (desc->dwFlags & (DDSD_WIDTH | DDSD_HEIGHT)) != (DDSD_WIDTH | DDSD_HEIGHT)) return DDERR_INVALIDPARAMS;
    width = primary ? (DWORD)GetSystemMetrics(SM_CXSCREEN) : desc->dwWidth;
    height = primary ? (DWORD)GetSystemMetrics(SM_CYSCREEN) : desc->dwHeight;
    if (!width || !height || width > 32767 || height > 32767 || height > 0x7fffffffu / 4 / width)
        return DDERR_INVALIDPARAMS;
    indexed = (desc->dwFlags & DDSD_PIXELFORMAT) && (desc->ddpfPixelFormat.dwFlags & DDPF_PALETTEINDEXED8);
    pitch = indexed ? (width + 3) & ~3u : width * 4;
    EnterCriticalSection(&d->guard);
    if (!d->window) hr = DDERR_NOCOOPERATIVELEVELSET;
    else if (primary && d->primary) hr = DDERR_PRIMARYSURFACEALREADYEXISTS;
    else {
        s = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*s));
        if (!s || !(s->views = wx_create_surface_views(&s->iface)) ||
                !(s->pixels = VirtualAlloc(NULL, pitch * height, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE))) {
            if (s) {
                if (s->views) HeapFree(GetProcessHeap(), 0, s->views);
                HeapFree(GetProcessHeap(), 0, s);
            }
            hr = DDERR_OUTOFMEMORY;
        } else {
            s->iface.lpVtbl = &surface_vtbl;
            s->refs = 1;
            s->indexed = indexed;
            s->pitch = pitch;
            s->width = width;
            s->height = height;
            s->caps = (primary ? DDSCAPS_PRIMARYSURFACE | DDSCAPS_VISIBLE : DDSCAPS_OFFSCREENPLAIN) | DDSCAPS_SYSTEMMEMORY;
            s->owner = d;
            s->origin = &IID_IDirectDraw;
            s->uniqueness = 1;
            s->next = d->surfaces;
            d->surfaces = s;
            draw_addref(iface);
            if (primary) d->primary = s;
            *out = &s->iface;
        }
    }
    LeaveCriticalSection(&d->guard);
    return hr;
}
static HRESULT WINAPI draw_primary(IDirectDraw *iface, IDirectDrawSurface **out)
{
    Draw *d = (Draw *)iface;
    if (!out) return DDERR_INVALIDPARAMS;
    EnterCriticalSection(&d->guard);
    *out = d->primary ? &d->primary->iface : NULL;
    if (*out) surface_addref(*out);
    LeaveCriticalSection(&d->guard);
    return *out ? DD_OK : DDERR_NOTFOUND;
}
static HRESULT present(Surface *s)
{
    HWND window = s->owner->window;
    POINT origin = {0, 0};
    BITMAPINFO bitmap;
    int result;
    HDC dc;
    if (!(s->caps & DDSCAPS_PRIMARYSURFACE)) return DD_OK;
    if (!ClientToScreen(window, &origin) || !(dc = GetDC(window))) return DDERR_NOGDI;
    clear_bytes(&bitmap, sizeof(bitmap));
    bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap.bmiHeader.biWidth = s->width;
    bitmap.bmiHeader.biHeight = -(LONG)s->height;
    bitmap.bmiHeader.biPlanes = 1;
    bitmap.bmiHeader.biBitCount = 32;
    bitmap.bmiHeader.biCompression = BI_RGB;
    result = StretchDIBits(dc, -origin.x, -origin.y, s->width, s->height, 0, 0, s->width, s->height,
            s->pixels, &bitmap, DIB_RGB_COLORS, SRCCOPY);
    ReleaseDC(window, dc);
    return result == (int)GDI_ERROR ? DDERR_NOGDI : DD_OK;
}
static BOOL surface_rect(const Surface *s, const RECT *input, RECT *rect)
{
    if (input) *rect = *input;
    else { rect->left = rect->top = 0; rect->right = s->width; rect->bottom = s->height; }
    return rect->left >= 0 && rect->top >= 0 && rect->right > rect->left && rect->bottom > rect->top &&
            (DWORD)rect->right <= s->width && (DWORD)rect->bottom <= s->height;
}
#include "early/palette.inc"
#include "early/clipper.inc"

static HRESULT WINAPI surface_blt(IDirectDrawSurface *iface, RECT *dest_rect, IDirectDrawSurface *source,
        RECT *source_rect, DWORD flags, DDBLTFX *fx)
{
    Surface *dst = (Surface *)iface, *src = (Surface *)source;
    BOOL fill = (flags & DDBLT_COLORFILL) != 0;
    RECT dr, sr = {0};
    DWORD *snapshot = NULL;
    RGNDATA *clips = NULL;
    BOOL use_src_key = flags & (DDBLT_KEYSRC | DDBLT_KEYSRCOVERRIDE);
    BOOL use_dst_key = flags & (DDBLT_KEYDEST | DDBLT_KEYDESTOVERRIDE);
    DDCOLORKEY sk = {0}, dk = {0};
    LONG x, y, sw = 0, sh = 0, dw, dh;
    HRESULT hr;
    if (flags & ~(DDBLT_COLORFILL | DDBLT_WAIT | DDBLT_DONOTWAIT | DDBLT_KEYSRC |
            DDBLT_KEYSRCOVERRIDE | DDBLT_KEYDEST | DDBLT_KEYDESTOVERRIDE)) return DDERR_UNSUPPORTED;
    if ((flags & (DDBLT_KEYSRCOVERRIDE | DDBLT_KEYDESTOVERRIDE)) && (!fx || fx->dwSize != sizeof(*fx)))
        return DDERR_INVALIDPARAMS;
    if ((fill && use_src_key) || ((flags & DDBLT_KEYSRC) && (flags & DDBLT_KEYSRCOVERRIDE)) ||
            ((flags & DDBLT_KEYDEST) && (flags & DDBLT_KEYDESTOVERRIDE))) return DDERR_INVALIDPARAMS;
    if (!surface_rect(dst, dest_rect, &dr)) return DDERR_INVALIDRECT;
    if (fill) {
        if (!fx || fx->dwSize != sizeof(*fx) || src || source_rect) return DDERR_INVALIDPARAMS;
    } else {
        if (!src || source->lpVtbl != &surface_vtbl || src->owner != dst->owner) return DDERR_INVALIDOBJECT;
        if (!surface_rect(src, source_rect, &sr)) return DDERR_INVALIDRECT;
        sw = sr.right - sr.left;
        sh = sr.bottom - sr.top;
    }
    dw = dr.right - dr.left;
    dh = dr.bottom - dr.top;
    EnterCriticalSection(&dst->owner->guard);
    if (dst->dc || dst->lock_address || (src && (src->dc || src->lock_address))) { hr = DDERR_SURFACEBUSY; goto done; }
    if (dst->clipper) {
        hr=clipping_data(dst->clipper,&dr,&clips);
        if (FAILED(hr)) goto done;
    }
    if ((flags & DDBLT_KEYSRC) && !src->key_source) { hr=DDERR_NOCOLORKEY; goto done; }
    if ((flags & DDBLT_KEYDEST) && !dst->key_dest) { hr=DDERR_NOCOLORKEY; goto done; }
    if (use_src_key) sk = flags & DDBLT_KEYSRCOVERRIDE ? fx->ddckSrcColorkey : src->source_key;
    if (use_dst_key) dk = flags & DDBLT_KEYDESTOVERRIDE ? fx->ddckDestColorkey : dst->dest_key;
    if (src && src->indexed != dst->indexed && ((src->indexed && !src->palette) || (dst->indexed && !dst->palette))) {
        hr=DDERR_NOPALETTEATTACHED; goto done;
    }
    if (src == dst && sr.left < dr.right && dr.left < sr.right && sr.top < dr.bottom && dr.top < sr.bottom) {
        snapshot = HeapAlloc(GetProcessHeap(), 0, sw * sh * sizeof(DWORD));
        if (!snapshot) { hr = DDERR_OUTOFMEMORY; goto done; }
        for (y = 0; y < sh; ++y)
            for (x = 0; x < sw; ++x) snapshot[y * sw + x] = read_pixel(src,sr.left+x,sr.top+y);
    }
    for (y = 0; y < dh; ++y) {
        for (x = 0; x < dw; ++x) {
            DWORD color;
            LONG dx=dr.left+x, dy=dr.top+y;
            if (clips && !inside_clip(clips,dx,dy)) continue;
            if (use_dst_key) {
                DWORD old=read_pixel(dst,dx,dy);
                if (old<dk.dwColorSpaceLowValue || old>dk.dwColorSpaceHighValue) continue;
            }
            if (fill) color=fx->dwFillColor;
            else {
                LONG sx=x*sw/dw, sy=y*sh/dh;
                color = snapshot ? snapshot[sy*sw+sx] : read_pixel(src,sr.left+sx,sr.top+sy);
                if (use_src_key && color>=sk.dwColorSpaceLowValue && color<=sk.dwColorSpaceHighValue) continue;
                if (src->indexed != dst->indexed) color=encode_rgb(dst,decode_rgb(src,color));
            }
            write_pixel(dst,dx,dy,color);
        }
    }
    if (!++dst->uniqueness) dst->uniqueness = 1;
    hr = present(dst);
done:
    if (clips) HeapFree(GetProcessHeap(),0,clips);
    if (snapshot) HeapFree(GetProcessHeap(), 0, snapshot);
    LeaveCriticalSection(&dst->owner->guard);
    return hr;
}
static HRESULT WINAPI surface_fast(IDirectDrawSurface *iface, DWORD x, DWORD y, IDirectDrawSurface *source,
        RECT *source_rect, DWORD flags)
{
    RECT sr, dr;
    if (flags & ~(DDBLTFAST_WAIT | DDBLTFAST_DONOTWAIT | DDBLTFAST_SRCCOLORKEY | DDBLTFAST_DESTCOLORKEY)) return DDERR_UNSUPPORTED;
    if (!source || source->lpVtbl != &surface_vtbl) return DDERR_INVALIDOBJECT;
    if (((Surface *)iface)->clipper) return DDERR_BLTFASTCANTCLIP;
    if (x > 32767 || y > 32767 || !surface_rect((Surface *)source, source_rect, &sr)) return DDERR_INVALIDRECT;
    dr.left = x; dr.top = y; dr.right = x + sr.right - sr.left; dr.bottom = y + sr.bottom - sr.top;
    DWORD blt_flags = 0;
    if (flags & DDBLTFAST_SRCCOLORKEY) blt_flags |= DDBLT_KEYSRC;
    if (flags & DDBLTFAST_DESTCOLORKEY) blt_flags |= DDBLT_KEYDEST;
    return surface_blt(iface, &dr, source, &sr, blt_flags, NULL);
}
static HRESULT WINAPI surface_desc(IDirectDrawSurface *iface, DDSURFACEDESC *desc)
{
    Surface *s = (Surface *)iface;
    return describe(desc, s->width, s->height, s->caps, s->indexed);
}
static HRESULT WINAPI surface_lock(IDirectDrawSurface *iface, RECT *rect, DDSURFACEDESC *desc, DWORD flags, HANDLE event)
{
    Surface *s = (Surface *)iface;
    RECT r;
    HRESULT hr;
    if (event || (flags & ~(DDLOCK_WAIT | DDLOCK_DONOTWAIT | DDLOCK_READONLY | DDLOCK_WRITEONLY |
            DDLOCK_NOSYSLOCK | DDLOCK_DISCARDCONTENTS))) return DDERR_UNSUPPORTED;
    if (!surface_rect(s, rect, &r)) return DDERR_INVALIDRECT;
    EnterCriticalSection(&s->owner->guard);
    if (s->dc || s->lock_address) hr = DDERR_SURFACEBUSY;
    else if (SUCCEEDED(hr = surface_desc(iface, desc))) {
        s->lock_address = (BYTE *)s->pixels + (DWORD)r.top * s->pitch + (DWORD)r.left * (s->indexed ? 1 : 4);
        s->lock_rect = r;
        s->read_only = (flags & DDLOCK_READONLY) != 0;
        desc->dwFlags |= DDSD_LPSURFACE;
        desc->lpSurface = s->lock_address;
        desc->dwWidth = r.right - r.left;
        desc->dwHeight = r.bottom - r.top;
    }
    LeaveCriticalSection(&s->owner->guard);
    return hr;
}
static HRESULT WINAPI surface_unlock(IDirectDrawSurface *iface, void *address)
{
    Surface *s = (Surface *)iface;
    HRESULT hr = DD_OK;
    EnterCriticalSection(&s->owner->guard);
    if (!s->lock_address) hr = DDERR_NOTLOCKED;
    else if (address && s->lock_address != address) hr = DDERR_INVALIDPARAMS;
    else {
        s->lock_address = NULL;
        if (!s->read_only) {
            if (!++s->uniqueness) s->uniqueness = 1;
            hr = present(s);
        }
    }
    LeaveCriticalSection(&s->owner->guard);
    return hr;
}
static HRESULT WINAPI surface_format(IDirectDrawSurface *iface, DDPIXELFORMAT *fmt)
{
    if (!fmt || fmt->dwSize != sizeof(*fmt)) return DDERR_INVALIDPARAMS;
    pixel_format(fmt, ((Surface *)iface)->indexed);
    return DD_OK;
}
static HRESULT WINAPI surface_caps(IDirectDrawSurface *iface, DDSCAPS *caps)
{
    if (!caps) return DDERR_INVALIDPARAMS;
    caps->dwCaps = ((Surface *)iface)->caps;
    return DD_OK;
}
static HRESULT WINAPI draw_fourcc(IDirectDraw *iface, DWORD *count, DWORD *codes)
{
    if (!count) return DDERR_INVALIDPARAMS;
    *count = 0;
    return DD_OK;
}
static BOOL write_caps(DDCAPS *caps, BOOL emulated)
{
    DWORD size = caps->dwSize;
    if (size < offsetof(DDCAPS, ddsOldCaps) + sizeof(DDSCAPS) || size > sizeof(DDCAPS)) return FALSE;
    clear_bytes(caps, size);
    caps->dwSize = size;
    if (emulated) {
        caps->dwCaps = DDCAPS_BLT | DDCAPS_BLTCOLORFILL | DDCAPS_BLTSTRETCH | DDCAPS_GDI | DDCAPS_PALETTE | DDCAPS_COLORKEY;
        caps->dwCKeyCaps = DDCKEYCAPS_SRCBLT | DDCKEYCAPS_SRCBLTCLRSPACE |
                DDCKEYCAPS_DESTBLT | DDCKEYCAPS_DESTBLTCLRSPACE;
        caps->dwPalCaps = DDPCAPS_8BIT | DDPCAPS_ALLOW256;
        caps->ddsOldCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
        if (size >= offsetof(DDCAPS, ddsCaps) + sizeof(DDSCAPS2)) caps->ddsCaps.dwCaps = caps->ddsOldCaps.dwCaps;
    }
    return TRUE;
}
static HRESULT WINAPI draw_caps(IDirectDraw *iface, DDCAPS *hardware, DDCAPS *software)
{
    if ((!hardware && !software) || (hardware && !write_caps(hardware, FALSE)) ||
            (software && !write_caps(software, TRUE))) return DDERR_INVALIDPARAMS;
    return DD_OK;
}
static HRESULT WINAPI draw_modes(IDirectDraw *iface, DWORD flags, DDSURFACEDESC *filter, void *ctx, LPDDENUMMODESCALLBACK callback)
{
    DDSURFACEDESC mode;
    if (!callback || (filter && filter->dwSize != sizeof(*filter))) return DDERR_INVALIDPARAMS;
    if (flags || (filter && (filter->dwFlags & ~(DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT)))) return DDERR_UNSUPPORTED;
    mode.dwSize = sizeof(mode);
    draw_mode(iface, &mode);
    if (!filter || ((!(filter->dwFlags & DDSD_WIDTH) || filter->dwWidth == mode.dwWidth) &&
            (!(filter->dwFlags & DDSD_HEIGHT) || filter->dwHeight == mode.dwHeight) &&
            (!(filter->dwFlags & DDSD_PIXELFORMAT) || supported_format(&filter->ddpfPixelFormat)))) callback(&mode, ctx);
    return DD_OK;
}
static HRESULT WINAPI surface_blt_status(IDirectDrawSurface *iface, DWORD flags)
{
    Surface *s = (Surface *)iface;
    if (flags != DDGBS_CANBLT && flags != DDGBS_ISBLTDONE) return DDERR_INVALIDPARAMS;
    EnterCriticalSection(&s->owner->guard);
    HRESULT hr = s->lock_address || s->dc ? DDERR_SURFACEBUSY : DD_OK;
    LeaveCriticalSection(&s->owner->guard);
    return hr;
}
/* Keep the original COM method signatures and return the specific capability error. */
#define DRAW_METHOD(name, result, ...) static HRESULT WINAPI name(IDirectDraw *self, ##__VA_ARGS__) { return result; }
#define SURF_METHOD(name, result, ...) static HRESULT WINAPI name(IDirectDrawSurface *self, ##__VA_ARGS__) { return result; }
DRAW_METHOD(compact, DD_OK)
DRAW_METHOD(duplicate, DDERR_UNSUPPORTED, IDirectDrawSurface *src, IDirectDrawSurface **p)
DRAW_METHOD(flip_gdi, DDERR_UNSUPPORTED)
static HRESULT WINAPI frequency(IDirectDraw *iface, DWORD *value)
{
    if (!value) return DDERR_INVALIDPARAMS;
    HDC screen=GetDC(NULL);
    if (!screen) return DDERR_NOGDI;
    int hz=GetDeviceCaps(screen,VREFRESH);
    ReleaseDC(NULL,screen);
    if (hz<=1) return DDERR_UNSUPPORTED;
    *value=(DWORD)hz;
    return DD_OK;
}
DRAW_METHOD(scanline, DDERR_UNSUPPORTED, DWORD *p)
DRAW_METHOD(vblank, DDERR_NOVSYNCHW, BOOL *p)
DRAW_METHOD(draw_init, DDERR_ALREADYINITIALIZED, GUID *g)
DRAW_METHOD(restore_mode, DD_OK)
DRAW_METHOD(set_mode, DDERR_NOEXCLUSIVEMODE, DWORD w, DWORD h, DWORD b)
DRAW_METHOD(wait_vblank, DDERR_NOVSYNCHW, DWORD f, HANDLE e)
SURF_METHOD(attach, DDERR_UNSUPPORTED, IDirectDrawSurface *s)
SURF_METHOD(dirty, DDERR_NOTAOVERLAYSURFACE, RECT *r)
SURF_METHOD(batch, DDERR_UNSUPPORTED, DDBLTBATCH *b, DWORD n, DWORD f)
SURF_METHOD(detach, DDERR_SURFACENOTATTACHED, DWORD f, IDirectDrawSurface *s)
SURF_METHOD(enum_attached, DD_OK, void *c, LPDDENUMSURFACESCALLBACK cb)
SURF_METHOD(enum_overlay, DDERR_NOOVERLAYHW, DWORD f, void *c, LPDDENUMSURFACESCALLBACK cb)
SURF_METHOD(flip, DDERR_NOTFLIPPABLE, IDirectDrawSurface *s, DWORD f)
SURF_METHOD(attached, DDERR_NOTFOUND, DDSCAPS *c, IDirectDrawSurface **s)

SURF_METHOD(flip_status, DDERR_NOTFLIPPABLE, DWORD f)
SURF_METHOD(get_pos, DDERR_NOTAOVERLAYSURFACE, LONG *x, LONG *y)
SURF_METHOD(surface_init, DDERR_ALREADYINITIALIZED, IDirectDraw *d, DDSURFACEDESC *s)
SURF_METHOD(available, DD_OK)

SURF_METHOD(set_pos, DDERR_NOTAOVERLAYSURFACE, LONG x, LONG y)
SURF_METHOD(overlay, DDERR_NOOVERLAYHW, RECT *r, IDirectDrawSurface *s, RECT *d, DWORD f, DDOVERLAYFX *fx)
SURF_METHOD(overlay_display, DDERR_NOOVERLAYHW, DWORD f)
SURF_METHOD(overlay_order, DDERR_NOOVERLAYHW, DWORD f, IDirectDrawSurface *s)

#include "early/surface_dc.inc"
#include "early/private_data.inc"

static HRESULT WINAPI enum_surfaces(IDirectDraw *iface, DWORD flags, DDSURFACEDESC *filter,
        void *context, LPDDENUMSURFACESCALLBACK callback)
{
    Draw *d=(Draw *)iface;
    DWORD match=flags & (DDENUMSURFACES_ALL | DDENUMSURFACES_MATCH | DDENUMSURFACES_NOMATCH);
    DWORD supported=DDSD_WIDTH | DDSD_HEIGHT | DDSD_CAPS | DDSD_PIXELFORMAT;
    if (!callback || (match!=DDENUMSURFACES_ALL && match!=DDENUMSURFACES_MATCH && match!=DDENUMSURFACES_NOMATCH))
        return DDERR_INVALIDPARAMS;
    if (flags != (match | DDENUMSURFACES_DOESEXIST)) return DDERR_UNSUPPORTED;
    if (match!=DDENUMSURFACES_ALL && (!filter || filter->dwSize!=sizeof(*filter))) return DDERR_INVALIDPARAMS;
    if (filter && (filter->dwFlags & ~supported)) return DDERR_UNSUPPORTED;
    EnterCriticalSection(&d->guard);
    SIZE_T count=0, selected=0;
    for (Surface *s=d->surfaces; s; s=s->next) ++count;
    if (!count) { LeaveCriticalSection(&d->guard); return DD_OK; }
    Surface **list=HeapAlloc(GetProcessHeap(),0,count*sizeof(*list));
    if (!list) { LeaveCriticalSection(&d->guard); return DDERR_OUTOFMEMORY; }
    for (Surface *s=d->surfaces; s; s=s->next) {
        if (match!=DDENUMSURFACES_ALL) {
            DWORD fields=filter->dwFlags;
            BOOL included=(!(fields&DDSD_WIDTH) || filter->dwWidth==s->width) &&
                    (!(fields&DDSD_HEIGHT) || filter->dwHeight==s->height) &&
                    (!(fields&DDSD_CAPS) || (filter->ddsCaps.dwCaps & s->caps)==filter->ddsCaps.dwCaps) &&
                    (!(fields&DDSD_PIXELFORMAT) || (supported_format(&filter->ddpfPixelFormat) &&
                            filter->ddpfPixelFormat.dwRGBBitCount==(s->indexed?8u:32u)));
            if (included!=(match==DDENUMSURFACES_MATCH)) continue;
        }
        surface_addref(&s->iface);
        list[selected++]=s;
    }
    LeaveCriticalSection(&d->guard);
    /* DirectDraw transfers a reference for every surface passed to the
     * callback. Keep unvisited entries alive even when a callback releases or
     * creates other surfaces. */
    SIZE_T i=0;
    while (i<selected) {
        DDSURFACEDESC desc={sizeof(desc)};
        IDirectDrawSurface *surface=&list[i++]->iface;
        surface_desc(surface,&desc);
        if (callback(surface,&desc,context)==DDENUMRET_CANCEL) break;
    }
    while (i<selected) surface_release(&list[i++]->iface);
    HeapFree(GetProcessHeap(),0,list);
    return DD_OK;
}

static IDirectDrawVtbl draw_vtbl = {
    draw_query, draw_addref, draw_release, compact, clipper, palette, draw_create_surface, duplicate,
    draw_modes, enum_surfaces, flip_gdi, draw_caps, draw_mode, draw_fourcc, draw_primary, frequency,
    scanline, vblank, draw_init, restore_mode, draw_coop, set_mode, wait_vblank
};
static IDirectDrawSurfaceVtbl surface_vtbl = {
    surface_query, surface_addref, surface_release, attach, dirty, surface_blt, batch, surface_fast,
    detach, enum_attached, enum_overlay, flip, attached, surface_blt_status, surface_caps, get_clipper,
    get_key, get_dc, flip_status, get_pos, get_palette, surface_format, surface_desc, surface_init,
    available, surface_lock, release_dc, available, set_clipper, set_key, set_pos, set_palette,
    surface_unlock, overlay, overlay_display, overlay_order
};
HRESULT WINAPI DirectDrawCreate(GUID *driver, IDirectDraw **out, IUnknown *outer)
{
    Draw *d;
    if (!out) return DDERR_INVALIDPARAMS;
    *out = NULL;
    if (outer) return CLASS_E_NOAGGREGATION;
    if (driver == (GUID *)(ULONG_PTR)DDCREATE_HARDWAREONLY) return DDERR_NODIRECTDRAWHW;
    if (driver && driver != (GUID *)(ULONG_PTR)DDCREATE_EMULATIONONLY && !wx_same_guid(driver, &GUID_NULL))
        return DDERR_INVALIDDIRECTDRAWGUID;
    d = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*d));
    if (!d) return DDERR_OUTOFMEMORY;
    d->views = wx_create_draw_views(&d->iface);
    if (!d->views) { HeapFree(GetProcessHeap(), 0, d); return DDERR_OUTOFMEMORY; }
    d->iface.lpVtbl = &draw_vtbl;
    d->refs = 1;
    InitializeCriticalSection(&d->guard);
    *out = &d->iface;
    return DD_OK;
}

void wx_set_surface_origin(IDirectDrawSurface *iface, const IID *origin)
{
    ((Surface *)iface)->origin = origin;
}

HRESULT wx_surface_owner(IDirectDrawSurface *iface, void **out)
{
    Surface *s = (Surface *)iface;
    return draw_query(&s->owner->iface, s->origin, out);
}

HRESULT wx_surface_unlock_rect(IDirectDrawSurface *iface, const RECT *rect)
{
    Surface *s = (Surface *)iface;
    HRESULT hr;
    EnterCriticalSection(&s->owner->guard);
    if (!s->lock_address) hr = DDERR_NOTLOCKED;
    else if (rect && (rect->left != s->lock_rect.left || rect->top != s->lock_rect.top ||
            rect->right != s->lock_rect.right || rect->bottom != s->lock_rect.bottom)) hr = DDERR_INVALIDRECT;
    else hr = surface_unlock(iface, NULL);
    LeaveCriticalSection(&s->owner->guard);
    return hr;
}

HRESULT wx_surface_page_lock(IDirectDrawSurface *iface, DWORD flags, BOOL unlock)
{
    Surface *s = (Surface *)iface;
    if (flags) return DDERR_INVALIDPARAMS;
    EnterCriticalSection(&s->owner->guard);
    HRESULT hr = DD_OK;
    if (unlock && !s->page_locks) hr = DDERR_NOTPAGELOCKED;
    else if (unlock && s->page_locks == 1 && !VirtualUnlock(s->pixels, s->pitch * s->height)) hr = DDERR_CANTPAGEUNLOCK;
    else if (unlock) --s->page_locks;
    else if (!s->page_locks && !VirtualLock(s->pixels, s->pitch * s->height)) hr = DDERR_CANTPAGELOCK;
    else ++s->page_locks;
    LeaveCriticalSection(&s->owner->guard);
    return hr;
}

HRESULT wx_surface_uniqueness(IDirectDrawSurface *iface, DWORD *value, BOOL advance)
{
    Surface *s = (Surface *)iface;
    if (!advance && !value) return DDERR_INVALIDPARAMS;
    EnterCriticalSection(&s->owner->guard);
    if (advance && !++s->uniqueness) s->uniqueness = 1;
    if (value) *value = s->uniqueness;
    LeaveCriticalSection(&s->owner->guard);
    return DD_OK;
}

HRESULT wx_draw_cooperative_state(IDirectDraw *iface)
{
    Draw *draw = (Draw *)iface;
    return draw->window && IsWindow(draw->window) ? DD_OK : DDERR_NOCOOPERATIVELEVELSET;
}

HRESULT wx_find_surface_dc(IDirectDraw *iface, HDC dc, IDirectDrawSurface **out)
{
    Draw *draw = (Draw *)iface;
    if (!out || !dc) return DDERR_INVALIDPARAMS;
    *out = NULL;
    EnterCriticalSection(&draw->guard);
    for (Surface *s = draw->surfaces; s; s = s->next) {
        if (s->dc == dc) {
            *out = &s->iface;
            surface_addref(*out);
            break;
        }
    }
    LeaveCriticalSection(&draw->guard);
    return *out ? DD_OK : DDERR_NOTFOUND;
}

HRESULT WINAPI DirectDrawCreateEx(GUID *driver, void **out, REFIID iid, IUnknown *outer)
{
    if (!out) return DDERR_INVALIDPARAMS;
    *out = NULL;
    if (!wx_same_guid(iid, &IID_IDirectDraw7)) return DDERR_INVALIDPARAMS;
    IDirectDraw *draw;
    HRESULT hr = DirectDrawCreate(driver, &draw, outer);
    if (SUCCEEDED(hr)) {
        hr = draw_query(draw, iid, out);
        draw_release(draw);
    }
    return hr;
}

HRESULT WINAPI DirectDrawEnumerateA(LPDDENUMCALLBACKA cb, void *context)
{
    char desc[] = "WINDX GDI software display", driver[] = "display";
    if (!cb) return DDERR_INVALIDPARAMS;
    cb(NULL, desc, driver, context);
    return DD_OK;
}
HRESULT WINAPI DirectDrawEnumerateW(LPDDENUMCALLBACKW cb, void *context)
{
    WCHAR desc[] = L"WINDX GDI software display", driver[] = L"display";
    if (!cb) return DDERR_INVALIDPARAMS;
    cb(NULL, desc, driver, context);
    return DD_OK;
}
static HMONITOR primary_monitor(void)
{
    HMONITOR (WINAPI *from)(HWND,DWORD) = (void *)GetProcAddress(GetModuleHandleA("user32.dll"), "MonitorFromWindow");
    return from ? from(NULL, MONITOR_DEFAULTTOPRIMARY) : NULL;
}
HRESULT WINAPI DirectDrawEnumerateExA(LPDDENUMCALLBACKEXA cb, void *context, DWORD flags)
{
    char desc[] = "WINDX GDI software display", driver[] = "display";
    if (!cb || flags & ~(DDENUM_ATTACHEDSECONDARYDEVICES | DDENUM_DETACHEDSECONDARYDEVICES | DDENUM_NONDISPLAYDEVICES))
        return DDERR_INVALIDPARAMS;
    cb(NULL, desc, driver, context, primary_monitor());
    return DD_OK;
}
HRESULT WINAPI DirectDrawEnumerateExW(LPDDENUMCALLBACKEXW cb, void *context, DWORD flags)
{
    WCHAR desc[] = L"WINDX GDI software display", driver[] = L"display";
    if (!cb || flags & ~(DDENUM_ATTACHEDSECONDARYDEVICES | DDENUM_DETACHEDSECONDARYDEVICES | DDENUM_NONDISPLAYDEVICES))
        return DDERR_INVALIDPARAMS;
    cb(NULL, desc, driver, context, primary_monitor());
    return DD_OK;
}
