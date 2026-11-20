typedef struct Clipper {
    IDirectDrawClipper iface;
    LONG refs;
    CRITICAL_SECTION guard;
    HWND window;
    HRGN region, previous;
    BOOL changed;
} Clipper;

static ULONG WINAPI clipper_addref(IDirectDrawClipper *iface)
{ return InterlockedIncrement(&((Clipper *)iface)->refs); }

static ULONG WINAPI clipper_release(IDirectDrawClipper *iface)
{
    Clipper *c=(Clipper *)iface;
    ULONG count=InterlockedDecrement(&c->refs);
    if (!count) {
        if (c->region) DeleteObject(c->region);
        if (c->previous) DeleteObject(c->previous);
        DeleteCriticalSection(&c->guard);
        HeapFree(GetProcessHeap(),0,c);
    }
    return count;
}
static HRESULT WINAPI clipper_query(IDirectDrawClipper *iface,REFIID iid,void **out)
{
    if (!out) return E_POINTER;
    *out=NULL;
    if (!wx_same_guid(iid,&IID_IUnknown) && !wx_same_guid(iid,&IID_IDirectDrawClipper)) return E_NOINTERFACE;
    *out=iface; clipper_addref(iface);
    return DD_OK;
}
/* Make an independent snapshot. System clipping regions on the 9x display
 * drivers use device coordinates. NT's SYSRGN is already in screen space. */
static HRESULT clipper_region(Clipper *c, HRGN *out)
{
    *out=NULL;
    if (!c->window && !c->region) return DDERR_NOCLIPLIST;
    HRGN region=CreateRectRgn(0,0,0,0);
    if (!region) return DDERR_OUTOFMEMORY;
    BOOL ok;
    if (c->window) {
        HDC dc=GetDC(c->window);
        if (!dc) { DeleteObject(region); return DDERR_INVALIDOBJECT; }
        int (WINAPI *random_rgn)(HDC,HRGN,int)=(void *)GetProcAddress(GetModuleHandleA("gdi32.dll"),"GetRandomRgn");
        ok=random_rgn && random_rgn(dc,region,SYSRGN)==1;
        if (ok && GetVersion() & 0x80000000u) {
            POINT org;
            ok=GetDCOrgEx(dc,&org) && OffsetRgn(region,org.x,org.y)!=ERROR;
        }
        if (!ok) {
            RECT r; POINT org={0,0};
            ok=GetClientRect(c->window,&r) && ClientToScreen(c->window,&org) &&
                    SetRectRgn(region,org.x,org.y,org.x+r.right,org.y+r.bottom);
        }
        ReleaseDC(c->window,dc);
    } else ok=CombineRgn(region,c->region,NULL,RGN_COPY)!=ERROR;
    if (!ok) { DeleteObject(region); return DDERR_INVALIDCLIPLIST; }
    *out=region;
    return DD_OK;
}
static HRESULT WINAPI clipper_list(IDirectDrawClipper *iface,RECT *rect,RGNDATA *out,DWORD *size)
{
    if (!size) return DDERR_INVALIDPARAMS;
    Clipper *c=(Clipper *)iface;
    HRGN region;
    EnterCriticalSection(&c->guard);
    HRESULT hr=clipper_region(c,&region);
    LeaveCriticalSection(&c->guard);
    if (FAILED(hr)) return hr;
    if (rect) {
        HRGN limits=CreateRectRgnIndirect(rect);
        if (!limits || CombineRgn(region,region,limits,RGN_AND)==ERROR) hr=DDERR_INVALIDCLIPLIST;
        if (limits) DeleteObject(limits);
    }
    if (SUCCEEDED(hr)) {
        DWORD required=GetRegionData(region,0,NULL);
        if (!required) hr=DDERR_INVALIDCLIPLIST;
        else if (out && *size<required) hr=DDERR_REGIONTOOSMALL;
        else if (out && !GetRegionData(region,required,out)) hr=DDERR_INVALIDCLIPLIST;
        *size=required;
    }
    DeleteObject(region);
    return hr;
}
static HRESULT WINAPI clipper_window(IDirectDrawClipper *iface,HWND *out)
{
    if (!out) return DDERR_INVALIDPARAMS;
    Clipper *c=(Clipper *)iface;
    EnterCriticalSection(&c->guard);
    *out=c->window;
    LeaveCriticalSection(&c->guard);
    return DD_OK;
}
static HRESULT WINAPI clipper_initialize(IDirectDrawClipper *iface,IDirectDraw *d,DWORD flags)
{ (void)iface; (void)d; (void)flags; return DDERR_ALREADYINITIALIZED; }
static HRESULT WINAPI clipper_changed(IDirectDrawClipper *iface,BOOL *out)
{
    if (!out) return DDERR_INVALIDPARAMS;
    Clipper *c=(Clipper *)iface;
    EnterCriticalSection(&c->guard);
    *out=c->changed;
    if (c->window) {
        HRGN current;
        if (SUCCEEDED(clipper_region(c,&current))) {
            if (!c->previous || !EqualRgn(current,c->previous)) *out=TRUE;
            if (c->previous) DeleteObject(c->previous);
            c->previous=current;
        }
    }
    c->changed=FALSE;
    LeaveCriticalSection(&c->guard);
    return DD_OK;
}
static HRESULT WINAPI clipper_set_list(IDirectDrawClipper *iface,RGNDATA *data,DWORD flags)
{
    if (flags) return DDERR_INVALIDPARAMS;
    Clipper *c=(Clipper *)iface;
    HRGN region=NULL;
    if (data) {
        if (data->rdh.dwSize!=sizeof(RGNDATAHEADER) || data->rdh.iType!=RDH_RECTANGLES ||
            data->rdh.nCount>(MAXDWORD-sizeof(RGNDATAHEADER))/sizeof(RECT)) return DDERR_INVALIDCLIPLIST;
        DWORD bytes=data->rdh.nCount*sizeof(RECT);
        if (data->rdh.nRgnSize && data->rdh.nRgnSize<bytes) return DDERR_INVALIDCLIPLIST;
        region=data->rdh.nCount ? ExtCreateRegion(NULL,sizeof(RGNDATAHEADER)+bytes,data) : CreateRectRgn(0,0,0,0);
        if (!region) return DDERR_INVALIDCLIPLIST;
    }
    EnterCriticalSection(&c->guard);
    HRESULT hr=DD_OK;
    if (c->window) {
        if (region) DeleteObject(region);
        hr=DDERR_CLIPPERISUSINGHWND;
    } else {
        if (c->region) DeleteObject(c->region);
        c->region=region; c->changed=TRUE;
    }
    LeaveCriticalSection(&c->guard);
    return hr;
}
static HRESULT WINAPI clipper_set_window(IDirectDrawClipper *iface,DWORD flags,HWND window)
{
    if (flags || (window && !IsWindow(window))) return DDERR_INVALIDPARAMS;
    Clipper *c=(Clipper *)iface;
    EnterCriticalSection(&c->guard);
    if (c->window!=window) c->changed=TRUE;
    c->window=window;
    LeaveCriticalSection(&c->guard);
    return DD_OK;
}
static IDirectDrawClipperVtbl clipper_methods={clipper_query,clipper_addref,clipper_release,clipper_list,
    clipper_window,clipper_initialize,clipper_changed,clipper_set_list,clipper_set_window};
HRESULT WINAPI DirectDrawCreateClipper(DWORD flags,IDirectDrawClipper **out,IUnknown *outer)
{
    if (!out) return DDERR_INVALIDPARAMS;
    *out=NULL;
    if (outer) return CLASS_E_NOAGGREGATION;
    if (flags) return DDERR_INVALIDPARAMS;
    Clipper *c=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*c));
    if (!c) return DDERR_OUTOFMEMORY;
    InitializeCriticalSection(&c->guard);
    c->iface.lpVtbl=&clipper_methods; c->refs=1;
    *out=&c->iface;
    return DD_OK;
}
static HRESULT WINAPI clipper(IDirectDraw *draw,DWORD flags,IDirectDrawClipper **out,IUnknown *outer)
{ (void)draw; return DirectDrawCreateClipper(flags,out,outer); }
static HRESULT WINAPI get_clipper(IDirectDrawSurface *iface,IDirectDrawClipper **out)
{
    if (!out) return DDERR_INVALIDPARAMS;
    Surface *s=(Surface *)iface;
    EnterCriticalSection(&s->owner->guard);
    *out=s->clipper;
    if (*out) IDirectDrawClipper_AddRef(*out);
    LeaveCriticalSection(&s->owner->guard);
    return *out ? DD_OK : DDERR_NOCLIPPERATTACHED;
}
static HRESULT WINAPI set_clipper(IDirectDrawSurface *iface,IDirectDrawClipper *clip)
{
    Surface *s=(Surface *)iface;
    if (clip) IDirectDrawClipper_AddRef(clip);
    EnterCriticalSection(&s->owner->guard);
    IDirectDrawClipper *old=s->clipper;
    s->clipper=clip;
    LeaveCriticalSection(&s->owner->guard);
    if (old) IDirectDrawClipper_Release(old);
    return DD_OK;
}
static HRESULT clipping_data(IDirectDrawClipper *clip,RECT *dest,RGNDATA **out)
{
    DWORD size=0;
    HRESULT hr=IDirectDrawClipper_GetClipList(clip,dest,NULL,&size);
    if (FAILED(hr)) return hr;
    if (size<sizeof(RGNDATAHEADER)) return DDERR_INVALIDCLIPLIST;
    RGNDATA *data=HeapAlloc(GetProcessHeap(),0,size);
    if (!data) return DDERR_OUTOFMEMORY;
    DWORD available=size;
    hr=IDirectDrawClipper_GetClipList(clip,dest,data,&available);
    if (SUCCEEDED(hr) && (available>size || available<sizeof(RGNDATAHEADER) || data->rdh.dwSize!=sizeof(RGNDATAHEADER) ||
            data->rdh.iType!=RDH_RECTANGLES || data->rdh.nCount>(available-sizeof(RGNDATAHEADER))/sizeof(RECT)))
        hr=DDERR_INVALIDCLIPLIST;
    if (FAILED(hr)) { HeapFree(GetProcessHeap(),0,data); return hr; }
    *out=data;
    return DD_OK;
}
static BOOL inside_clip(const RGNDATA *clip,LONG x,LONG y)
{
    const RECT *rect=(const RECT *)clip->Buffer;
    for (DWORD i=0;i<clip->rdh.nCount;++i)
        if (x>=rect[i].left && x<rect[i].right && y>=rect[i].top && y<rect[i].bottom) return TRUE;
    return FALSE;
}
