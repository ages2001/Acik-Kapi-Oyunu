#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* NT 3.5 has one display and no public ANSI mode enumeration entry point. */
BOOL WINAPI wx_display_mode(const char *device, DWORD index, DEVMODEA *mode)
{
    typedef BOOL (WINAPI *enumerate_fn)(const char *,DWORD,DEVMODEA *);
    enumerate_fn native = (enumerate_fn)GetProcAddress(GetModuleHandleA("user32.dll"), "EnumDisplaySettingsA");
    HDC dc;
    if (native) return native(device, index, mode);
    if (!mode || (index != ENUM_CURRENT_SETTINGS && index != ENUM_REGISTRY_SETTINGS && index != 0)) return FALSE;
    dc = GetDC(NULL);
    if (!dc) return FALSE;
    mode->dmPelsWidth = GetDeviceCaps(dc, HORZRES);
    mode->dmPelsHeight = GetDeviceCaps(dc, VERTRES);
    mode->dmBitsPerPel = GetDeviceCaps(dc, PLANES) * GetDeviceCaps(dc, BITSPIXEL);
    mode->dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
    mode->dmDisplayFrequency = GetDeviceCaps(dc, VREFRESH);
    if (mode->dmDisplayFrequency > 1) mode->dmFields |= DM_DISPLAYFREQUENCY;
    ReleaseDC(NULL, dc);
    return TRUE;
}

HANDLE WINAPI wx_load_image(HINSTANCE instance, const char *name, UINT type, int width, int height, UINT flags)
{
    HANDLE (WINAPI *fn)(HINSTANCE,const char *,UINT,int,int,UINT) =
            (void *)GetProcAddress(GetModuleHandleA("user32.dll"), "LoadImageA");
    if (fn) return fn(instance, name, type, width, height, flags);
    if (!(flags & ~(LR_DEFAULTSIZE | LR_SHARED)) && !width && !height) {
        if (type == IMAGE_BITMAP) return LoadBitmapA(instance, name);
        if (type == IMAGE_CURSOR) return LoadCursorA(instance, name);
        if (type == IMAGE_ICON) return LoadIconA(instance, name);
    }
    SetLastError(ERROR_NOT_SUPPORTED);
    return NULL;
}
