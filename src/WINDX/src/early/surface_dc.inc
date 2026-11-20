/* GDI drawing into the surface. NT 3.1 supports device-dependent bitmaps but
 * cannot select a 32-bit bitmap into a 4-bit display DC. Preserve untouched
 * RGB pixels using a snapshot of the device's initial color conversion. */
static void dc_bitmap_info(BITMAPINFO *info, const Surface *s)
{
    clear_bytes(info, sizeof(*info));
    info->bmiHeader.biSize = sizeof(info->bmiHeader);
    info->bmiHeader.biWidth = s->width;
    info->bmiHeader.biHeight = -(LONG)s->height;
    info->bmiHeader.biPlanes = 1;
    info->bmiHeader.biBitCount = 32;
    info->bmiHeader.biCompression = BI_RGB;
}

static HRESULT WINAPI get_dc(IDirectDrawSurface *iface, HDC *result)
{
    Surface *s = (Surface *)iface;
    if (!result) return DDERR_INVALIDPARAMS;
    *result = NULL;
    EnterCriticalSection(&s->owner->guard);
    HRESULT hr = DDERR_CANTCREATEDC;
    HDC display = NULL, dc = NULL;
    HBITMAP bitmap = NULL, previous;
    DWORD *bits = NULL, *snapshot = NULL, *indexed_rgb = NULL;
    const DWORD *rgb = s->pixels;
    DWORD count = s->width * s->height, bytes = count * sizeof(DWORD);
    BITMAPINFO info;
    HBITMAP (WINAPI *create_dib)(HDC, const BITMAPINFO *, UINT, void **, HANDLE, DWORD);
    if (s->lock_address || s->dc) { hr = s->dc ? DDERR_DCALREADYCREATED : DDERR_SURFACEBUSY; goto done; }
    if (s->indexed) {
        if (!s->palette) { hr=DDERR_NOPALETTEATTACHED; goto done; }
        indexed_rgb=HeapAlloc(GetProcessHeap(),0,bytes);
        if (!indexed_rgb) { hr=DDERR_OUTOFMEMORY; goto done; }
        for (DWORD y=0;y<s->height;++y) for (DWORD x=0;x<s->width;++x)
            indexed_rgb[y*s->width+x]=decode_rgb(s,read_pixel(s,x,y));
        rgb=indexed_rgb;
    }
    display = GetDC(NULL);
    if (!display || !(dc = CreateCompatibleDC(display))) goto done;
    dc_bitmap_info(&info,s);
    create_dib = (void *)GetProcAddress(GetModuleHandleA("gdi32.dll"), "CreateDIBSection");
    if (create_dib) bitmap = create_dib(display,&info,DIB_RGB_COLORS,(void **)&bits,NULL,0);
    if (bitmap) {
        for (DWORD i=0;i<count;++i) bits[i] = rgb[i];
    } else {
        bitmap = CreateCompatibleBitmap(display,s->width,s->height);
        if (!bitmap || SetDIBits(display,bitmap,0,s->height,rgb,&info,DIB_RGB_COLORS) != (int)s->height) goto done;
        snapshot = HeapAlloc(GetProcessHeap(),0,bytes);
        if (!snapshot) { hr = DDERR_OUTOFMEMORY; goto done; }
        if (GetDIBits(display,bitmap,0,s->height,snapshot,&info,DIB_RGB_COLORS) != (int)s->height) goto done;
    }
    previous = SelectObject(dc,bitmap);
    if (!previous || previous == (HBITMAP)HGDI_ERROR) goto done;
    s->dc = dc; s->dc_bitmap = bitmap; s->dc_previous = previous;
    s->dc_bits = bits; s->dc_snapshot = snapshot;
    *result = dc;
    hr = DD_OK;
done:
    if (display) ReleaseDC(NULL,display);
    if (indexed_rgb) HeapFree(GetProcessHeap(),0,indexed_rgb);
    if (FAILED(hr)) {
        if (snapshot) HeapFree(GetProcessHeap(),0,snapshot);
        if (bitmap) DeleteObject(bitmap);
        if (dc) DeleteDC(dc);
    }
    LeaveCriticalSection(&s->owner->guard);
    return hr;
}

static HRESULT WINAPI release_dc(IDirectDrawSurface *iface, HDC dc)
{
    Surface *s = (Surface *)iface;
    EnterCriticalSection(&s->owner->guard);
    HRESULT hr = DD_OK;
    if (!dc || dc != s->dc) { hr = DDERR_NODC; goto done; }
    DWORD count = s->width * s->height;
    SelectObject(dc,s->dc_previous);
    if (s->dc_bits) {
        /* Read after completing drawing queued against the DIB section. */
        GdiFlush();
        for (DWORD i=0;i<count;++i) {
            DWORD rgb=s->dc_bits[i]&0xffffffu, x=i%s->width, y=i/s->width;
            if (decode_rgb(s,read_pixel(s,x,y))!=rgb) write_pixel(s,x,y,encode_rgb(s,rgb));
        }
    } else {
        DWORD *converted = HeapAlloc(GetProcessHeap(),0,count * sizeof(DWORD));
        BITMAPINFO info;
        dc_bitmap_info(&info,s);
        if (!converted) hr = DDERR_OUTOFMEMORY;
        else if (GetDIBits(dc,s->dc_bitmap,0,s->height,converted,&info,DIB_RGB_COLORS) != (int)s->height)
            hr = DDERR_NOGDI;
        else for (DWORD i=0;i<count;++i)
            if ((converted[i] & 0xffffffu) != (s->dc_snapshot[i] & 0xffffffu))
                write_pixel(s,i%s->width,i/s->width,encode_rgb(s,converted[i] & 0xffffffu));
        if (converted) HeapFree(GetProcessHeap(),0,converted);
        HeapFree(GetProcessHeap(),0,s->dc_snapshot);
    }
    DeleteObject(s->dc_bitmap);
    DeleteDC(dc);
    s->dc = NULL; s->dc_bitmap = NULL; s->dc_bits = NULL; s->dc_snapshot = NULL;
    if (SUCCEEDED(hr)) {
        if (!++s->uniqueness) s->uniqueness = 1;
        hr = present(s);
    }
done:
    LeaveCriticalSection(&s->owner->guard);
    return hr;
}
