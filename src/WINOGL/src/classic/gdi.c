#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static FARPROC graphics(const char *module, const char *name)
{
    HMODULE dll = GetModuleHandleA(module);
    return dll ? GetProcAddress(dll, name) : NULL;
}
static FARPROC pixel_function(const char *gdi_name, const char *wgl_name)
{
    FARPROC fn = graphics("gdi32.dll", gdi_name);
    return fn ? fn : graphics("opengl32.dll", wgl_name);
}
int WINAPI wx_choose(HDC dc, const PIXELFORMATDESCRIPTOR *format)
{
    int (WINAPI *fn)(HDC,const PIXELFORMATDESCRIPTOR *) = (void *)pixel_function("ChoosePixelFormat","wglChoosePixelFormat");
    return fn ? fn(dc,format) : 0;
}
int WINAPI wx_describe(HDC dc, int index, UINT bytes, PIXELFORMATDESCRIPTOR *format)
{
    int (WINAPI *fn)(HDC,int,UINT,PIXELFORMATDESCRIPTOR *) = (void *)pixel_function("DescribePixelFormat","wglDescribePixelFormat");
    return fn ? fn(dc,index,bytes,format) : 0;
}
BOOL WINAPI wx_set_pixel_format(HDC dc, int index, const PIXELFORMATDESCRIPTOR *format)
{
    BOOL (WINAPI *fn)(HDC,int,const PIXELFORMATDESCRIPTOR *) = (void *)pixel_function("SetPixelFormat","wglSetPixelFormat");
    return fn && fn(dc,index,format);
}
int WINAPI wx_get_pixel_format(HDC dc)
{
    int (WINAPI *fn)(HDC) = (void *)pixel_function("GetPixelFormat","wglGetPixelFormat");
    return fn ? fn(dc) : 0;
}
HBITMAP WINAPI wx_dib_section(HDC dc, const BITMAPINFO *info, UINT usage, void **pixels, HANDLE section, DWORD offset)
{
    HBITMAP (WINAPI *fn)(HDC,const BITMAPINFO *,UINT,void **,HANDLE,DWORD) = (void *)graphics("gdi32.dll","CreateDIBSection");
    if (fn) return fn(dc,info,usage,pixels,section,offset);
    if (pixels) *pixels = NULL;
    SetLastError(ERROR_NOT_SUPPORTED);
    return NULL;
}
UINT WINAPI wx_dib_palette(HDC dc, UINT start, UINT count, const RGBQUAD *colors)
{
    UINT (WINAPI *fn)(HDC,UINT,UINT,const RGBQUAD *) = (void *)graphics("gdi32.dll","SetDIBColorTable");
    if (fn) return fn(dc,start,count,colors);
    SetLastError(ERROR_NOT_SUPPORTED);
    return 0;
}
BOOL WINAPI wx_get_gamma(HDC dc, void *ramp)
{
    BOOL (WINAPI *fn)(HDC,void *) = (void *)graphics("gdi32.dll", "GetDeviceGammaRamp");
    if (fn) return fn(dc,ramp);
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}
BOOL WINAPI wx_set_gamma(HDC dc, void *ramp)
{
    BOOL (WINAPI *fn)(HDC,void *) = (void *)graphics("gdi32.dll", "SetDeviceGammaRamp");
    if (fn) return fn(dc,ramp);
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}
