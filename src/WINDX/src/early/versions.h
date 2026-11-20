#ifndef WINDX_DDRAW_VERSIONS_H
#define WINDX_DDRAW_VERSIONS_H

#ifdef __cplusplus
extern "C" {
#endif
/* The C implementation owns one family of COM interface views per object. */
BOOL wx_same_guid(const GUID *a, const GUID *b);
void *wx_create_draw_views(IDirectDraw *base);
void *wx_create_surface_views(IDirectDrawSurface *base);
void *wx_draw_view(void *views, const IID *iid);
void *wx_surface_view(void *views, const IID *iid);
void wx_set_surface_origin(IDirectDrawSurface *surface, const IID *origin);
HRESULT wx_surface_owner(IDirectDrawSurface *surface, void **out);
HRESULT wx_surface_unlock_rect(IDirectDrawSurface *surface, const RECT *rect);
HRESULT wx_surface_page_lock(IDirectDrawSurface *surface, DWORD flags, BOOL unlock);
HRESULT wx_private_set(IDirectDrawSurface *surface, const GUID *tag, void *data, DWORD bytes, DWORD flags);
HRESULT wx_private_get(IDirectDrawSurface *surface, const GUID *tag, void *data, DWORD *bytes);
HRESULT wx_private_free(IDirectDrawSurface *surface, const GUID *tag);
HRESULT wx_surface_uniqueness(IDirectDrawSurface *surface, DWORD *value, BOOL advance);
HRESULT wx_draw_cooperative_state(IDirectDraw *draw);
HRESULT wx_find_surface_dc(IDirectDraw *draw, HDC dc, IDirectDrawSurface **out);
#ifdef __cplusplus
}
#endif
#endif
