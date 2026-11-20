#define WIN32_LEAN_AND_MEAN
#define INITGUID
#include <windows.h>
#include <ddraw.h>
#include "test_output.h"

static bool checked(const char *stage, HRESULT hr)
{
    if (SUCCEEDED(hr)) return true;
    report(stage); report(" FAIL "); report_hex(static_cast<DWORD>(hr)); report("\r\n");
    return false;
}

template<class I> struct Reference {
    I *object;
    Reference() : object(nullptr) {}
    ~Reference() { if (object) object->Release(); }
    Reference(const Reference &) = delete;
    I *operator->() const { return object; }
    I **out() { return &object; }
    void **com_out() { return reinterpret_cast<void **>(&object); }
};

template<class Desc>
static void offscreen(Desc &desc)
{
    desc = {};
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
    desc.dwWidth = 4; desc.dwHeight = 3;
}

static HRESULT unlock(IDirectDrawSurface *s, void *p, RECT *) { return s->Unlock(p); }
static HRESULT unlock(IDirectDrawSurface2 *s, void *p, RECT *) { return s->Unlock(p); }
static HRESULT unlock(IDirectDrawSurface3 *s, void *p, RECT *) { return s->Unlock(p); }
static HRESULT unlock(IDirectDrawSurface4 *s, void *, RECT *r) { return s->Unlock(r); }
static HRESULT unlock(IDirectDrawSurface7 *s, void *, RECT *r) { return s->Unlock(r); }

template<class S, class Desc>
static bool test_surfaces(S *src, S *dst, S *primary, HWND window)
{
    DDBLTFX effect = {sizeof(effect)};
    Desc data = {sizeof(data)};
    RECT middle = {0,1,4,2}, edit = {2,2,3,3};
    effect.dwFillColor = 0xffffff;
    if (!checked("fill", src->Blt(nullptr, nullptr, nullptr, DDBLT_COLORFILL | DDBLT_WAIT, &effect))) return false;
    effect.dwFillColor = 0xff0000;
    if (!checked("row fill", src->Blt(&middle, nullptr, nullptr, DDBLT_COLORFILL, &effect)) ||
            !checked("copy", dst->BltFast(0, 0, src, nullptr, DDBLTFAST_WAIT))) return false;
    if (!checked("subrectangle lock", dst->Lock(&edit, &data, DDLOCK_WAIT, nullptr))) return false;
    if (!data.lpSurface || data.lPitch < 16) return false;
    *static_cast<DWORD *>(data.lpSurface) = 0x123456;
    if (!checked("subrectangle unlock", unlock(dst, data.lpSurface, &edit))) return false;
    data.dwSize = sizeof(data);
    if (!checked("read lock", dst->Lock(nullptr, &data, DDLOCK_WAIT | DDLOCK_READONLY, nullptr))) return false;
    bool pixels = data.lpSurface && data.dwWidth == 4 && data.dwHeight == 3;
    for (unsigned y=0; pixels && y<3; ++y) {
        const DWORD *row = reinterpret_cast<const DWORD *>(static_cast<const BYTE *>(data.lpSurface) + data.lPitch * y);
        for (unsigned x=0; x<4; ++x)
            pixels = pixels && row[x] == (y==1 ? 0xff0000u : y==2 && x==2 ? 0x123456u : 0xffffffu);
    }
    if (!checked("read unlock", unlock(dst, data.lpSurface, nullptr)) || !pixels) return false;
    HDC device = nullptr;
    if (!checked("GetDC",dst->GetDC(&device)) || !device) return false;
    COLORREF before = GetPixel(device,1,1);
    COLORREF drawn = SetPixel(device,1,1,RGB(0,0,255));
    if (!checked("ReleaseDC",dst->ReleaseDC(device)) || before != RGB(255,0,0) || drawn != RGB(0,0,255)) return false;
    if (!checked("GDI read lock", dst->Lock(nullptr, &data, DDLOCK_WAIT | DDLOCK_READONLY, nullptr))) return false;
    auto row1 = static_cast<const BYTE *>(data.lpSurface) + data.lPitch;
    auto row2 = row1 + data.lPitch;
    pixels = (reinterpret_cast<const DWORD *>(row1)[1] & 0xffffffu) == 0x0000ff &&
            (reinterpret_cast<const DWORD *>(row2)[2] & 0xffffffu) == 0x123456;
    if (!checked("GDI read unlock",unlock(dst,data.lpSurface,nullptr)) || !pixels) return false;
    POINT origin = {8,8};
    if (!ClientToScreen(window,&origin)) return false;
    RECT region = {origin.x,origin.y,origin.x+32,origin.y+24};
    if (!checked("primary stretch", primary->Blt(&region, src, nullptr, DDBLT_WAIT, nullptr))) return false;
    HDC screen=GetDC(window);
    if (!screen) return false;
    COLORREF a=GetPixel(screen,12,11), b=GetPixel(screen,12,19), c=GetPixel(screen,12,27);
    ReleaseDC(window,screen);
    return a==RGB(255,255,255) && b==RGB(255,0,0) && c==RGB(255,255,255);
}

template<class Draw, class S, class Desc>
static bool palette_test(Draw *draw)
{
    Reference<S> indexed, rgb;
    Reference<IDirectDrawPalette> palette, attached;
    PALETTEENTRY entries[256]={};
    entries[1].peRed=255; entries[2].peGreen=255; entries[3].peBlue=255;
    if (!checked("CreatePalette",draw->CreatePalette(DDPCAPS_8BIT|DDPCAPS_ALLOW256,entries,palette.out(),nullptr))) return false;
    Desc desc; offscreen(desc);
    desc.dwWidth=5;
    if (!checked("RGB destination",draw->CreateSurface(&desc,rgb.out(),nullptr))) return false;
    desc.dwFlags|=DDSD_PIXELFORMAT;
    desc.ddpfPixelFormat.dwSize=sizeof(desc.ddpfPixelFormat);
    desc.ddpfPixelFormat.dwFlags=DDPF_RGB|DDPF_PALETTEINDEXED8;
    desc.ddpfPixelFormat.dwRGBBitCount=8;
    if (!checked("indexed surface",draw->CreateSurface(&desc,indexed.out(),nullptr)) ||
            !checked("SetPalette",indexed->SetPalette(palette.object)) ||
            !checked("GetPalette",indexed->GetPalette(attached.out())) || attached.object!=palette.object) return false;
    Desc lock={sizeof(lock)};
    if (!checked("index lock",indexed->Lock(nullptr,&lock,DDLOCK_WAIT,nullptr))) return false;
    if (lock.ddpfPixelFormat.dwRGBBitCount!=8 || lock.lPitch<5) return false;
    for (unsigned y=0;y<3;++y) {
        BYTE *row=static_cast<BYTE *>(lock.lpSurface)+y*lock.lPitch;
        for (unsigned x=0;x<5;++x) row[x]=static_cast<BYTE>(x%3+1);
    }
    if (!checked("index unlock",unlock(indexed.object,lock.lpSurface,nullptr))) return false;
    DDCOLORKEY key={2,2}, read_key={};
    DDBLTFX effect={sizeof(effect)};effect.dwFillColor=0x123456;
    if (!checked("source key", indexed->SetColorKey(DDCKEY_SRCBLT,&key)) ||
            !checked("read source key",indexed->GetColorKey(DDCKEY_SRCBLT,&read_key)) ||
            read_key.dwColorSpaceLowValue!=2 || read_key.dwColorSpaceHighValue!=2 ||
            !checked("background",rgb->Blt(nullptr,nullptr,nullptr,DDBLT_COLORFILL,&effect)) ||
            !checked("index conversion with key",rgb->BltFast(0,0,indexed.object,nullptr,DDBLTFAST_SRCCOLORKEY)))
        return false;
    if (!checked("converted RGB",rgb->Lock(nullptr,&lock,DDLOCK_READONLY,nullptr))) return false;
    bool pass=true;
    for (unsigned y=0;y<3;++y) {
        const DWORD *row=reinterpret_cast<const DWORD *>(static_cast<const BYTE *>(lock.lpSurface)+y*lock.lPitch);
        for (unsigned x=0;x<5;++x) {
            const DWORD expected = x%3==0 ? 0xff0000u : x%3==1 ? 0x123456u : 0x0000ffu;
            if ((row[x]&0xffffffu)!=expected) pass=false;
        }
    }
    if (!checked("converted unlock",unlock(rgb.object,lock.lpSurface,nullptr)) || !pass) return false;
    PALETTEENTRY yellow={255,255,0,0}, read={};
    if (!checked("update palette",palette->SetEntries(0,1,1,&yellow)) ||
            !checked("read palette",palette->GetEntries(0,1,1,&read)) ||
            read.peRed!=255 || read.peGreen!=255 || read.peBlue ||
            !checked("update copy",rgb->Blt(nullptr,indexed.object,nullptr,DDBLT_WAIT,nullptr)) ||
            !checked("changed pixel",rgb->Lock(nullptr,&lock,DDLOCK_READONLY,nullptr))) return false;
    pass=(*static_cast<DWORD *>(lock.lpSurface)&0xffffffu)==0xffff00u;
    if (!checked("changed unlock",unlock(rgb.object,lock.lpSurface,nullptr)) || !pass) return false;
    effect.dwFillColor=0;
    if (!checked("erase indices",indexed->Blt(nullptr,nullptr,nullptr,DDBLT_COLORFILL,&effect)) ||
            !checked("RGB to palette",indexed->Blt(nullptr,rgb.object,nullptr,DDBLT_WAIT,nullptr)) ||
            !checked("palette readback",indexed->Lock(nullptr,&lock,DDLOCK_READONLY,nullptr))) return false;
    for (unsigned y=0;y<3;++y) {
        const BYTE *row=static_cast<const BYTE *>(lock.lpSurface)+y*lock.lPitch;
        for (unsigned x=0;x<5;++x) if (row[x]!=x%3+1) pass=false;
    }
    return checked("palette readback unlock",unlock(indexed.object,lock.lpSurface,nullptr)) && pass;
}

template<typename S,typename Desc>
static HRESULT WINAPI list_surface(S *surface, Desc *desc, void *user)
{
    auto *count=static_cast<unsigned *>(user);
    if (desc->dwFlags&DDSD_CAPS) ++*count;
    surface->Release();
    return DDENUMRET_OK;
}

static bool extra_surface(IDirectDraw *,IDirectDrawSurface *) { return true; }
static bool extra_surface(IDirectDraw2 *,IDirectDrawSurface *) { return true; }
template<typename Draw,typename S>
static bool extra_surface(Draw *draw,S *surface)
{
    static const GUID tag={0x5f9b80d7,0x408d,0x42db,{0xa9,0x86,0xad,0xa1,0xc6,0x22,0xeb,0x2e}};
    DWORD value=0x12345678, stored=0, bytes=sizeof(stored), first, next;
    if (!checked("uniqueness",surface->GetUniquenessValue(&first)) ||
            !checked("private data",surface->SetPrivateData(tag,&value,sizeof(value),DDSPD_VOLATILE)) ||
            !checked("read private data",surface->GetPrivateData(tag,&stored,&bytes)) || bytes!=sizeof(stored) || stored!=value ||
            !checked("advance uniqueness",surface->ChangeUniquenessValue()) ||
            !checked("read uniqueness",surface->GetUniquenessValue(&next)) || next==first ||
            surface->GetPrivateData(tag,&stored,&bytes)!=DDERR_EXPIRED ||
            !checked("free private data",surface->FreePrivateData(tag))) return false;
    HDC dc=nullptr;
    if (!checked("acquire DC mapping",surface->GetDC(&dc))) return false;
    Reference<S> owner;
    bool mapped=checked("surface from DC",draw->GetSurfaceFromDC(dc,owner.out())) && owner.object==surface;
    return checked("release mapped DC",surface->ReleaseDC(dc)) && mapped;
}

template<typename Draw,typename S,typename Desc>
static bool clip_test(Draw *draw,S *surface,HWND window)
{
    Reference<IDirectDrawClipper> clipper;
    if (!checked("create clipper",draw->CreateClipper(0,clipper.out(),nullptr))) return false;
    struct ClipData { RGNDATAHEADER header; RECT rects[2]; };
    ClipData input={{sizeof(RGNDATAHEADER),RDH_RECTANGLES,2,2*sizeof(RECT),{0,0,4,3}},
                    {{0,0,2,1},{2,2,4,3}}};
    if (!checked("set clip rectangles",clipper->SetClipList(reinterpret_cast<RGNDATA *>(&input),0))) return false;
    DWORD bytes=0;
    if (!checked("clip list size",clipper->GetClipList(nullptr,nullptr,&bytes)) || bytes<sizeof(input)) return false;
    BOOL changed=FALSE;
    if (!checked("clip changed",clipper->IsClipListChanged(&changed)) || !changed) return false;
    DDBLTFX fx={sizeof(fx)};
    if (!checked("clear clipped target",surface->Blt(nullptr,nullptr,nullptr,DDBLT_COLORFILL,&fx)) ||
            !checked("attach clipper",surface->SetClipper(clipper.object))) return false;
    fx.dwFillColor=0xffffff;
    if (!checked("clipped fill",surface->Blt(nullptr,nullptr,nullptr,DDBLT_COLORFILL,&fx))) return false;
    Desc lock={sizeof(lock)};
    if (!checked("clipped readback",surface->Lock(nullptr,&lock,DDLOCK_READONLY,nullptr))) return false;
    bool valid=true;
    for (unsigned y=0;y<3;++y) {
        const DWORD *row=reinterpret_cast<const DWORD *>(static_cast<const BYTE *>(lock.lpSurface)+y*lock.lPitch);
        for (unsigned x=0;x<4;++x)
            if ((row[x]&0xffffffu)!=((y==0&&x<2)||(y==2&&x>=2)?0xffffffu:0)) valid=false;
    }
    if (!checked("clipped unlock",unlock(surface,lock.lpSurface,nullptr)) || !valid ||
            !checked("detach clipper",surface->SetClipper(nullptr)) ||
            !checked("window clip",clipper->SetHWnd(0,window))) return false;
    HWND reported=nullptr;
    bytes=0;
    return checked("clip window",clipper->GetHWnd(&reported)) && reported==window &&
            checked("window clip list size",clipper->GetClipList(nullptr,nullptr,&bytes)) && bytes>=sizeof(RGNDATAHEADER);
}

template<class Draw, class S, class Desc>
static bool generation(Draw *d, HWND window)
{
    if (!checked("cooperative level", d->SetCooperativeLevel(window,DDSCL_NORMAL))) return false;
    Desc mode = {sizeof(mode)};
    if (!checked("display mode",d->GetDisplayMode(&mode)) || mode.dwWidth < 160 || mode.dwHeight < 80) return false;
    Desc desc; offscreen(desc);
    Reference<S> src, dst, primary;
    if (!checked("source",d->CreateSurface(&desc,src.out(),nullptr)) ||
            !checked("dest",d->CreateSurface(&desc,dst.out(),nullptr))) return false;
    desc={};
    desc.dwSize=sizeof(desc); desc.dwFlags=DDSD_CAPS; desc.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE;
    if (!checked("primary",d->CreateSurface(&desc,primary.out(),nullptr))) return false;
    unsigned count=0;
    using Callback = HRESULT (WINAPI *)(S *,Desc *,void *);
    Callback callback=list_surface<S,Desc>;
    if (!checked("enumerate live surfaces",d->EnumSurfaces(DDENUMSURFACES_DOESEXIST|DDENUMSURFACES_ALL,
            nullptr,&count,callback)) || count!=3) return false;
    return test_surfaces<S,Desc>(src.object,dst.object,primary.object,window) &&
            palette_test<Draw,S,Desc>(d) && extra_surface(d,src.object) && clip_test<Draw,S,Desc>(d,src.object,window);
}

template<class S>
static bool earlier_surface(IDirectDraw2 *d, REFIID iid, HWND window)
{
    Reference<IDirectDrawSurface> a,b,p;
    Reference<S> src,dst,primary;
    DDSURFACEDESC desc; offscreen(desc);
    if (!checked("source",d->CreateSurface(&desc,a.out(),nullptr)) ||
            !checked("dest",d->CreateSurface(&desc,b.out(),nullptr))) return false;
    desc={};desc.dwSize=sizeof(desc);desc.dwFlags=DDSD_CAPS;desc.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE;
    if (!checked("primary",d->CreateSurface(&desc,p.out(),nullptr)) ||
            !checked("source interface",a->QueryInterface(iid,src.com_out())) ||
            !checked("dest interface",b->QueryInterface(iid,dst.com_out())) ||
            !checked("primary interface",p->QueryInterface(iid,primary.com_out()))) return false;
    Reference<IDirectDraw2> owning;
    if (!checked("GetDDInterface",src->GetDDInterface(owning.com_out())) || owning.object!=d) return false;
    return test_surfaces<S,DDSURFACEDESC>(src.object,dst.object,primary.object,window);
}

static BOOL CALLBACK adapter(GUID *id, char *desc, char *name, void *ctx, HMONITOR monitor)
{
    (void)monitor;
    BOOL *seen=static_cast<BOOL *>(ctx);
    if (!id && desc && *desc && name && *name) *seen=TRUE;
    return DDENUMRET_OK;
}

static bool run(HMODULE dll, HWND window)
{
    auto create = reinterpret_cast<decltype(&DirectDrawCreate)>(GetProcAddress(dll,"DirectDrawCreate"));
    auto create_ex = reinterpret_cast<decltype(&DirectDrawCreateEx)>(GetProcAddress(dll,"DirectDrawCreateEx"));
    auto enumerate = reinterpret_cast<decltype(&DirectDrawEnumerateExA)>(GetProcAddress(dll,"DirectDrawEnumerateExA"));
    BOOL seen=FALSE;
    if (!create || !create_ex || !enumerate || FAILED(enumerate(adapter,&seen,0)) || !seen) return false;
    report("Default display adapter enumeration PASS\r\n");
    Reference<IDirectDraw> one;
    Reference<IDirectDraw2> two;
    Reference<IDirectDraw4> four;
    Reference<IDirectDraw7> seven;
    if (!checked("DirectDrawCreate",create(nullptr,one.out(),nullptr)) ||
            !checked("DirectDraw2",one->QueryInterface(IID_IDirectDraw2,two.com_out())) ||
            !checked("DirectDraw4",one->QueryInterface(IID_IDirectDraw4,four.com_out())) ||
            !checked("DirectDrawCreateEx",create_ex(nullptr,seven.com_out(),IID_IDirectDraw7,nullptr))) return false;
    DDDEVICEIDENTIFIER2 id;
    if (!checked("GetDeviceIdentifier",seven->GetDeviceIdentifier(&id,0)) || !id.szDriver[0] || !id.szDescription[0]) return false;
    report("DirectDraw7 adapter "); report(id.szDescription); report("\r\n");
    if (!generation<IDirectDraw,IDirectDrawSurface,DDSURFACEDESC>(one.object,window)) return false;
    report("DirectDraw1 surface1 fill, copy, subrectangle, GDI DC and primary pixels PASS\r\n");
    if (!generation<IDirectDraw2,IDirectDrawSurface,DDSURFACEDESC>(two.object,window) ||
            !earlier_surface<IDirectDrawSurface2>(two.object,IID_IDirectDrawSurface2,window) ||
            !earlier_surface<IDirectDrawSurface3>(two.object,IID_IDirectDrawSurface3,window)) return false;
    report("DirectDraw2 surface1/2/3 fill, copy, subrectangle, GDI DC and primary pixels PASS\r\n");
    if (!generation<IDirectDraw4,IDirectDrawSurface4,DDSURFACEDESC2>(four.object,window)) return false;
    report("DirectDraw4 surface4 fill, copy, subrectangle, GDI DC and primary pixels PASS\r\n");
    if (!generation<IDirectDraw7,IDirectDrawSurface7,DDSURFACEDESC2>(seven.object,window)) return false;
    report("DirectDraw7 surface7 fill, copy, subrectangle, GDI DC and primary pixels PASS\r\n");
    report("Two disjoint clip rectangles, fill/readback and native window clip list on all four interfaces PASS\r\n");
    report("Live surface enumeration, DirectDraw4/7 private data, uniqueness and surface lookup by DC PASS\r\n");
    report("8-bit palette lookup/update, color key and RGB-to-index round trip on all four device interfaces PASS\r\n");
    return true;
}

extern "C" void WINAPI legacy_draw_entry(void)
{
    initialize_test();
    char path[MAX_PATH];
    HMODULE dll=LoadLibraryA(".\\ddraw.dll");
    if (dll && GetModuleFileNameA(dll,path,sizeof(path))) { report("DirectDraw module ");report(path);report("\r\n"); }
    HWND window=CreateWindowExA(WS_EX_TOPMOST,"STATIC","WINDX DirectDraw interface test",WS_VISIBLE|WS_POPUP,
            60,60,160,80,nullptr,nullptr,GetModuleHandleA(nullptr),nullptr);
    if (window) UpdateWindow(window);
    bool ok=window && dll && run(dll,window);
    if (window) DestroyWindow(window);
    if (dll) FreeLibrary(dll);
    report(ok?"PASS\r\n":"FAIL\r\n");
    ExitProcess(ok?0:1);
}
