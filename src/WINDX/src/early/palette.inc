static ULONG WINAPI palette_addref(IDirectDrawPalette *iface)
{
    return InterlockedIncrement(&((Palette *)iface)->refs);
}
static ULONG WINAPI palette_release(IDirectDrawPalette *iface)
{
    Palette *p = (Palette *)iface;
    ULONG refs = InterlockedDecrement(&p->refs);
    if (!refs) {
        Draw *owner = p->owner;
        HeapFree(GetProcessHeap(),0,p);
        draw_release(&owner->iface);
    }
    return refs;
}
static HRESULT WINAPI palette_query(IDirectDrawPalette *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!wx_same_guid(iid,&IID_IUnknown) && !wx_same_guid(iid,&IID_IDirectDrawPalette)) return E_NOINTERFACE;
    palette_addref(iface);
    *out = iface;
    return DD_OK;
}
static HRESULT WINAPI palette_caps(IDirectDrawPalette *iface, DWORD *caps)
{
    if (!caps) return DDERR_INVALIDPARAMS;
    *caps = ((Palette *)iface)->flags;
    return DD_OK;
}
static HRESULT WINAPI palette_entries(IDirectDrawPalette *iface, DWORD flags, DWORD first, DWORD count, PALETTEENTRY *entries)
{
    Palette *p = (Palette *)iface;
    if (flags || !entries || first >= 256 || count > 256-first) return DDERR_INVALIDPARAMS;
    EnterCriticalSection(&p->owner->guard);
    for (DWORD i=0; i<count; ++i) entries[i] = p->entries[first+i];
    LeaveCriticalSection(&p->owner->guard);
    return DD_OK;
}
static HRESULT WINAPI palette_init(IDirectDrawPalette *iface, IDirectDraw *draw, DWORD flags, PALETTEENTRY *entries)
{ (void)iface; (void)draw; (void)flags; (void)entries; return DDERR_ALREADYINITIALIZED; }
static HRESULT WINAPI palette_set_entries(IDirectDrawPalette *iface, DWORD flags, DWORD first, DWORD count, PALETTEENTRY *entries)
{
    Palette *p = (Palette *)iface;
    if (flags || !entries || first >= 256 || count > 256-first) return DDERR_INVALIDPARAMS;
    EnterCriticalSection(&p->owner->guard);
    for (DWORD i=0; i<count; ++i) p->entries[first+i] = entries[i];
    if (!(p->flags & DDPCAPS_ALLOW256)) {
        p->entries[0]=(PALETTEENTRY){0,0,0,0};
        p->entries[255]=(PALETTEENTRY){255,255,255,0};
    }
    LeaveCriticalSection(&p->owner->guard);
    return DD_OK;
}
static IDirectDrawPaletteVtbl palette_vtbl = {
    palette_query,palette_addref,palette_release,palette_caps,palette_entries,palette_init,palette_set_entries
};
static HRESULT WINAPI palette(IDirectDraw *iface, DWORD flags, PALETTEENTRY *entries, IDirectDrawPalette **out, IUnknown *outer)
{
    if (!out) return DDERR_INVALIDPARAMS;
    *out = NULL;
    if (outer) return CLASS_E_NOAGGREGATION;
    if (!entries) return DDERR_INVALIDPARAMS;
    if (!(flags & DDPCAPS_8BIT) || flags & ~(DDPCAPS_8BIT | DDPCAPS_ALLOW256 | DDPCAPS_INITIALIZE))
        return DDERR_UNSUPPORTED;
    Palette *p = HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*p));
    if (!p) return DDERR_OUTOFMEMORY;
    p->iface.lpVtbl=&palette_vtbl;
    p->refs=1; p->flags=flags & ~DDPCAPS_INITIALIZE; p->owner=(Draw *)iface;
    draw_addref(iface);
    palette_set_entries(&p->iface,0,0,256,entries);
    *out=&p->iface;
    return DD_OK;
}
static HRESULT WINAPI set_palette(IDirectDrawSurface *iface, IDirectDrawPalette *palette)
{
    Surface *s=(Surface *)iface;
    Palette *p=(Palette *)palette;
    if (!s->indexed) return DDERR_NOTPALETTIZED;
    if (p && (palette->lpVtbl != &palette_vtbl || p->owner != s->owner)) return DDERR_INVALIDOBJECT;
    EnterCriticalSection(&s->owner->guard);
    if (s->dc) { LeaveCriticalSection(&s->owner->guard); return DDERR_SURFACEBUSY; }
    if (p) palette_addref(palette);
    if (s->palette) palette_release(&s->palette->iface);
    s->palette=p;
    LeaveCriticalSection(&s->owner->guard);
    return DD_OK;
}
static HRESULT WINAPI get_palette(IDirectDrawSurface *iface, IDirectDrawPalette **out)
{
    Surface *s=(Surface *)iface;
    if (!out) return DDERR_INVALIDPARAMS;
    EnterCriticalSection(&s->owner->guard);
    *out=s->palette ? &s->palette->iface : NULL;
    if (*out) palette_addref(*out);
    LeaveCriticalSection(&s->owner->guard);
    return *out ? DD_OK : DDERR_NOPALETTEATTACHED;
}

static DWORD read_pixel(const Surface *s, DWORD x, DWORD y)
{
    if (s->indexed) return ((const BYTE *)s->pixels)[y*s->pitch+x];
    return s->pixels[y*s->width+x];
}
static void write_pixel(Surface *s, DWORD x, DWORD y, DWORD value)
{
    if (s->indexed) ((BYTE *)s->pixels)[y*s->pitch+x]=(BYTE)value;
    else s->pixels[y*s->width+x]=value;
}
static DWORD decode_rgb(const Surface *s, DWORD value)
{
    if (!s->indexed) return value;
    const PALETTEENTRY *c=&s->palette->entries[(BYTE)value];
    return ((DWORD)c->peRed<<16) | ((DWORD)c->peGreen<<8) | c->peBlue;
}
static DWORD encode_rgb(const Surface *s, DWORD value)
{
    if (!s->indexed) return value;
    int r=(value>>16)&255, g=(value>>8)&255, b=value&255;
    DWORD best=0, distance=MAXDWORD;
    for (DWORD i=0;i<256;++i) {
        const PALETTEENTRY *c=&s->palette->entries[i];
        int dr=r-c->peRed, dg=g-c->peGreen, db=b-c->peBlue;
        DWORD next=dr*dr+dg*dg+db*db;
        if (next<distance) { distance=next; best=i; if (!next) break; }
    }
    return best;
}
static HRESULT WINAPI set_key(IDirectDrawSurface *iface, DWORD flags, DDCOLORKEY *key)
{
    Surface *s=(Surface *)iface;
    DWORD kind=flags & ~DDCKEY_COLORSPACE;
    BOOL src=kind==DDCKEY_SRCBLT;
    if (!src && kind!=DDCKEY_DESTBLT) return DDERR_UNSUPPORTED;
    if (key && (key->dwColorSpaceLowValue>key->dwColorSpaceHighValue ||
            (!(flags&DDCKEY_COLORSPACE) && key->dwColorSpaceLowValue!=key->dwColorSpaceHighValue)))
        return DDERR_INVALIDPARAMS;
    EnterCriticalSection(&s->owner->guard);
    if (src) { s->key_source=key!=NULL; if (key) s->source_key=*key; }
    else { s->key_dest=key!=NULL; if (key) s->dest_key=*key; }
    LeaveCriticalSection(&s->owner->guard);
    return DD_OK;
}
static HRESULT WINAPI get_key(IDirectDrawSurface *iface, DWORD flags, DDCOLORKEY *key)
{
    Surface *s=(Surface *)iface;
    if (!key || (flags!=DDCKEY_SRCBLT && flags!=DDCKEY_DESTBLT)) return DDERR_INVALIDPARAMS;
    EnterCriticalSection(&s->owner->guard);
    BOOL exists=flags==DDCKEY_SRCBLT ? s->key_source : s->key_dest;
    if (exists) *key=flags==DDCKEY_SRCBLT ? s->source_key : s->dest_key;
    LeaveCriticalSection(&s->owner->guard);
    return exists ? DD_OK : DDERR_NOCOLORKEY;
}
