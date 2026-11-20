#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

static HANDLE system_brushes[32], system_pens[32], checker;
static HANDLE remember(HANDLE volatile *target, HANDLE value)
{
    HANDLE other = InterlockedCompareExchangePointer(target, value, NULL);
    if (other && value) DeleteObject(value);
    return other ? other : value;
}
COLORREF WINAPI wx_sys_color(int index)
{
    if ((unsigned)index > COLOR_BTNHIGHLIGHT && LOBYTE(LOWORD(GetVersion())) < 4) {
        switch (index) {
        case COLOR_3DDKSHADOW: index = COLOR_WINDOWFRAME; break;
        case COLOR_3DLIGHT: case COLOR_MENUBAR: index = COLOR_BTNFACE; break;
        case COLOR_INFOTEXT: index = COLOR_WINDOWTEXT; break;
        case COLOR_INFOBK: index = COLOR_WINDOW; break;
        case COLOR_HOTLIGHT: case COLOR_MENUHILIGHT: index = COLOR_HIGHLIGHT; break;
        case COLOR_GRADIENTACTIVECAPTION: index = COLOR_ACTIVECAPTION; break;
        case COLOR_GRADIENTINACTIVECAPTION: index = COLOR_INACTIVECAPTION; break;
        default: return 0;
        }
    }
    return GetSysColor(index);
}
#define sys_color wx_sys_color

int WINAPI wx_system_metric(int metric)
{
    if (metric >= 0 && (metric <= SM_CMOUSEBUTTONS || LOBYTE(LOWORD(GetVersion())) >= 4))
        return GetSystemMetrics(metric);
    /* NT 3.1's metric table ends at SM_CMOUSEBUTTONS (43). Its exported
     * entry point does not reject larger indices. Derive newer metrics from
     * the actual native border, icon, frame and display sizes. */
    switch (metric) {
    case SM_CXEDGE: return 2 * GetSystemMetrics(SM_CXBORDER);
    case SM_CYEDGE: return 2 * GetSystemMetrics(SM_CYBORDER);
    case SM_CXSMICON: return GetSystemMetrics(SM_CXICON) / 2;
    case SM_CYSMICON: return GetSystemMetrics(SM_CYICON) / 2;
    case SM_CXMINSPACING: case SM_CXMINIMIZED: return GetSystemMetrics(SM_CXMIN);
    case SM_CYMINSPACING: case SM_CYMINIMIZED: return GetSystemMetrics(SM_CYMIN);
    case SM_CYSMCAPTION: return GetSystemMetrics(SM_CYCAPTION);
    case SM_CXSMSIZE: case SM_CXMENUSIZE: return GetSystemMetrics(SM_CXSIZE);
    case SM_CYSMSIZE: case SM_CYMENUSIZE: return GetSystemMetrics(SM_CYSIZE);
    case SM_CXMAXTRACK: case SM_CXMAXIMIZED: return GetSystemMetrics(SM_CXSCREEN) + 2 * GetSystemMetrics(SM_CXFRAME);
    case SM_CYMAXTRACK: case SM_CYMAXIMIZED: return GetSystemMetrics(SM_CYSCREEN) + 2 * GetSystemMetrics(SM_CYFRAME);
    case SM_CXDRAG: return GetSystemMetrics(SM_CXDOUBLECLK);
    case SM_CYDRAG: return GetSystemMetrics(SM_CYDOUBLECLK);
    case SM_CXMENUCHECK: return LOWORD(GetMenuCheckMarkDimensions());
    case SM_CYMENUCHECK: return HIWORD(GetMenuCheckMarkDimensions());
    case SM_CXVIRTUALSCREEN: return GetSystemMetrics(SM_CXSCREEN);
    case SM_CYVIRTUALSCREEN: return GetSystemMetrics(SM_CYSCREEN);
    case SM_CMONITORS: case SM_SAMEDISPLAYFORMAT: return 1;
    default: return 0;
    }
}
HBRUSH WINAPI wx_sys_brush(int index)
{
    if ((unsigned)index >= 32) return NULL;
    if (system_brushes[index]) return system_brushes[index];
    return remember(&system_brushes[index], CreateSolidBrush(sys_color(index)));
}
static HPEN wx_sys_pen(int index)
{
    if ((unsigned)index >= 32) return NULL;
    if (system_pens[index]) return system_pens[index];
    return remember(&system_pens[index], CreatePen(PS_SOLID, 1, sys_color(index)));
}
static HBRUSH wx_checker(void)
{
    static const WORD rows[] = {0x55,0xaa,0x55,0xaa,0x55,0xaa,0x55,0xaa};
    if (checker) return checker;
    HBITMAP bitmap = CreateBitmap(8,8,1,1,rows);
    HBRUSH brush = bitmap ? CreatePatternBrush(bitmap) : NULL;
    if (bitmap) DeleteObject(bitmap);
    return remember(&checker, brush);
}

/* These two routines are built directly from the bundled Wine 2.0.5 sources. */
#define GetSysColorBrush wx_sys_brush
#define SYSCOLOR_GetPen wx_sys_pen
#define SYSCOLOR_Get55AABrush wx_checker
#define GetLayout(dc) 0u
#define GetTextExtentPoint32W GetTextExtentPointW
#define DrawEdge wx_draw_edge
#define DrawFrameControl wx_draw_frame
#define TRACE(...) ((void)0)
#define WARN(...) ((void)0)
#include "wine_draw.inc"

static HBITMAP scale_bitmap(HBITMAP source, int width, int height, BOOL mono)
{
    BITMAP info;
    if (!GetObjectW(source, sizeof(info), &info)) return NULL;
    if (!width) width = info.bmWidth;
    if (!height) height = info.bmHeight;
    HDC desktop = GetDC(NULL), from = CreateCompatibleDC(desktop), to = CreateCompatibleDC(desktop);
    HBITMAP dst = mono ? CreateBitmap(width,height,1,1,NULL) : CreateCompatibleBitmap(desktop,width,height);
    HBITMAP oldfrom = NULL, oldto = NULL;
    BOOL ok = FALSE;
    if (from && to && dst) {
        oldfrom = SelectObject(from, source); oldto = SelectObject(to, dst);
        if (oldfrom && oldto) ok = StretchBlt(to,0,0,width,height,from,0,0,info.bmWidth,info.bmHeight,SRCCOPY);
        if (oldfrom) SelectObject(from,oldfrom);
        if (oldto) SelectObject(to,oldto);
    }
    if (from) DeleteDC(from);
    if (to) DeleteDC(to);
    if (desktop) ReleaseDC(NULL,desktop);
    if (!ok && dst) { DeleteObject(dst); dst = NULL; }
    return dst;
}

HANDLE WINAPI wx_copy_image(HANDLE source, UINT kind, int width, int height, UINT flags)
{
    HANDLE result = NULL;
    if (!source) return NULL;
    if (kind == IMAGE_BITMAP) {
        BITMAP bm;
        if (GetObjectW(source,sizeof(bm),&bm)) result = scale_bitmap(source,width,height,bm.bmPlanes*bm.bmBitsPixel==1);
    } else if (kind == IMAGE_ICON || kind == IMAGE_CURSOR) {
        ICONINFO icon;
        if (!width && !height) result = CopyIcon(source);
        else if (GetIconInfo(source,&icon)) {
            ICONINFO copy = icon;
            copy.hbmMask = scale_bitmap(icon.hbmMask,width,icon.hbmColor ? height : 2*height,TRUE);
            copy.hbmColor = icon.hbmColor ? scale_bitmap(icon.hbmColor,width,height,FALSE) : NULL;
            if (copy.hbmMask && (!icon.hbmColor || copy.hbmColor)) result = CreateIconIndirect(&copy);
            if (copy.hbmMask) DeleteObject(copy.hbmMask);
            if (copy.hbmColor) DeleteObject(copy.hbmColor);
            DeleteObject(icon.hbmMask);
            if (icon.hbmColor) DeleteObject(icon.hbmColor);
        }
    }
    if (result && (flags & LR_COPYDELETEORG)) {
        if (kind==IMAGE_BITMAP) DeleteObject(source);
        else if (kind==IMAGE_CURSOR) DestroyCursor(source);
        else DestroyIcon(source);
    }
    return result;
}

HANDLE WINAPI wx_load_image_w(HINSTANCE module, LPCWSTR name, UINT kind, int width, int height, UINT flags)
{
    HANDLE image;
    if (flags & LR_LOADFROMFILE) { SetLastError(ERROR_NOT_SUPPORTED); return NULL; }
    if (kind == IMAGE_BITMAP) image = LoadBitmapW(module,name);
    else if (kind == IMAGE_CURSOR) image = LoadCursorW(module,name);
    else if (kind == IMAGE_ICON) image = LoadIconW(module,name);
    else return NULL;
    if (!image || (!width && !height)) return image;
    return wx_copy_image(image,kind,width,height,kind==IMAGE_BITMAP ? LR_COPYDELETEORG : 0);
}

BOOL WINAPI wx_draw_icon(HDC dc, int x, int y, HICON image, int w, int h, UINT frame, HBRUSH background, UINT flags)
{
    ICONINFO info;
    BITMAP mask;
    if (!GetIconInfo(image,&info)) return FALSE;
    BOOL ok = GetObjectW(info.hbmMask,sizeof(mask),&mask) != 0;
    int source_h = ok ? (info.hbmColor ? mask.bmHeight : mask.bmHeight/2) : 0;
    if (!w) w = flags & DI_DEFAULTSIZE ? GetSystemMetrics(SM_CXICON) : mask.bmWidth;
    if (!h) h = flags & DI_DEFAULTSIZE ? GetSystemMetrics(SM_CYICON) : source_h;
    HDC work = CreateCompatibleDC(dc);
    if (!work) ok = FALSE;
    if (ok && background) { RECT r={x,y,x+w,y+h}; FillRect(dc,&r,background); }
    COLORREF old_text = SetTextColor(dc,RGB(0,0,0)), old_back = SetBkColor(dc,RGB(255,255,255));
    HBITMAP old = work ? SelectObject(work,info.hbmMask) : NULL;
    if (ok && (flags & DI_MASK)) ok = StretchBlt(dc,x,y,w,h,work,0,0,mask.bmWidth,source_h,
            (flags & DI_IMAGE) ? SRCAND : SRCCOPY);
    if (ok && (flags & DI_IMAGE)) {
        if (info.hbmColor) SelectObject(work,info.hbmColor);
        ok = StretchBlt(dc,x,y,w,h,work,0,info.hbmColor ? 0 : source_h,mask.bmWidth,source_h,
                (flags & DI_MASK) ? SRCINVERT : SRCCOPY);
    }
    SetTextColor(dc,old_text); SetBkColor(dc,old_back);
    if (old) SelectObject(work,old);
    if (work) DeleteDC(work);
    DeleteObject(info.hbmMask);
    if (info.hbmColor) DeleteObject(info.hbmColor);
    return ok;
}

int WINAPI wx_draw_text(HDC dc, WCHAR *text, int count, RECT *rect, UINT flags, DRAWTEXTPARAMS *params)
{
    if (!rect || (params && params->cbSize != sizeof(*params))) return 0;
    RECT area = *rect;
    if (params) { area.left += params->iLeftMargin; area.right -= params->iRightMargin; }
    int height = DrawTextW(dc,text,count,&area,flags);
    if (flags & DT_CALCRECT) {
        *rect = area;
        if (params) { rect->left -= params->iLeftMargin; rect->right += params->iRightMargin; }
    }
    if (params) params->uiLengthDrawn = height ? (count < 0 ? lstrlenW(text) : count) : 0;
    return height;
}

static const char *page_key(int bar)
{
    return bar == SB_HORZ ? "WindxHScrollPage" : bar == SB_VERT ? "WindxVScrollPage" : "WindxCtlScrollPage";
}
BOOL WINAPI wx_scroll_get(HWND wnd, int bar, SCROLLINFO *s)
{
    if (!s || s->cbSize < FIELD_OFFSET(SCROLLINFO,nTrackPos)) return FALSE;
    if ((s->fMask & SIF_RANGE) && !GetScrollRange(wnd,bar,&s->nMin,&s->nMax)) return FALSE;
    if (s->fMask & SIF_PAGE) s->nPage = (UINT)(UINT_PTR)GetPropA(wnd,page_key(bar));
    if (s->fMask & SIF_POS) s->nPos = GetScrollPos(wnd,bar);
    if ((s->fMask & SIF_TRACKPOS) && s->cbSize >= sizeof(*s)) s->nTrackPos = GetScrollPos(wnd,bar);
    return TRUE;
}
int WINAPI wx_scroll_set(HWND wnd, int bar, const SCROLLINFO *s, BOOL repaint)
{
    if (!s || s->cbSize < FIELD_OFFSET(SCROLLINFO,nTrackPos)) return 0;
    if (s->fMask & SIF_PAGE) SetPropA(wnd,page_key(bar),(HANDLE)(UINT_PTR)s->nPage);
    if (s->fMask & SIF_RANGE) SetScrollRange(wnd,bar,s->nMin,s->nMax,repaint);
    if (s->fMask & SIF_POS) SetScrollPos(wnd,bar,s->nPos,repaint);
    return GetScrollPos(wnd,bar);
}

BOOL WINAPI wx_mode_w(LPCWSTR display, DWORD index, DEVMODEW *mode)
{
    if (!mode || (index != 0 && index != ENUM_CURRENT_SETTINGS && index != ENUM_REGISTRY_SETTINGS)) return FALSE;
    HDC dc=GetDC(NULL);
    if (!dc) return FALSE;
    mode->dmPelsWidth=GetDeviceCaps(dc,HORZRES);
    mode->dmPelsHeight=GetDeviceCaps(dc,VERTRES);
    mode->dmBitsPerPel=GetDeviceCaps(dc,BITSPIXEL)*GetDeviceCaps(dc,PLANES);
    mode->dmDisplayFrequency=GetDeviceCaps(dc,VREFRESH);
    mode->dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL;
    if (mode->dmDisplayFrequency>1) mode->dmFields|=DM_DISPLAYFREQUENCY;
    ReleaseDC(NULL,dc);
    return TRUE;
}

HKL WINAPI wx_keyboard_layout(DWORD thread)
{
    char name[KL_NAMELENGTH];
    DWORD layout = 0;
    if (thread && thread != GetCurrentThreadId()) { SetLastError(ERROR_NOT_SUPPORTED); return NULL; }
    if (!GetKeyboardLayoutNameA(name)) return NULL;
    for (unsigned i = 0; i < 8 && name[i]; ++i) {
        char c = name[i];
        unsigned digit = (c >= '0' && c <= '9') ? c - '0' : (c | 32) - 'a' + 10;
        if (digit >= 16) return NULL;
        layout = (layout << 4) | digit;
    }
    if (!HIWORD(layout)) layout = MAKELONG(LOWORD(layout), LOWORD(layout));
    return (HKL)(ULONG_PTR)layout;
}

int WINAPI wx_keyboard_list(int size, HKL *layouts)
{
    HKL current = wx_keyboard_layout(0);
    if (!current || size < 0 || (size && !layouts)) return 0;
    if (size) layouts[0] = current;
    return 1;
}

LPARAM WINAPI wx_set_message_extra(LPARAM value)
{
    LPARAM previous = GetMessageExtraInfo();
    if (previous != value) SetLastError(ERROR_NOT_SUPPORTED);
    return previous;
}
