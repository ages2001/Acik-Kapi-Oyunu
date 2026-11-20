#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

BOOL WINAPI wx_shell_w(SHELLEXECUTEINFOW *s)
{
    if (!s || s->cbSize < sizeof(*s)) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    if (s->fMask & (SEE_MASK_IDLIST | SEE_MASK_CLASSNAME | SEE_MASK_CLASSKEY)) {
        SetLastError(ERROR_NOT_SUPPORTED);
        return FALSE;
    }
    s->hProcess = NULL;
    s->hInstApp = ShellExecuteW(s->hwnd,s->lpVerb,s->lpFile,s->lpParameters,s->lpDirectory,s->nShow);
    return (UINT_PTR)s->hInstApp > 32;
}

UINT WINAPI wx_get_dib_colors(HDC dc, UINT first, UINT count, RGBQUAD *colors)
{
    typedef UINT (WINAPI *get_fn)(HDC,UINT,UINT,RGBQUAD *);
    get_fn get = (get_fn)GetProcAddress(GetModuleHandleA("gdi32.dll"),"GetDIBColorTable");
    if (get) return get(dc,first,count,colors);
    SetLastError(ERROR_NOT_SUPPORTED);
    return 0;
}

BOOL WINAPI wx_charset(DWORD *src, CHARSETINFO *out, DWORD flags)
{
    static const struct { BYTE charset; UINT codepage; BYTE bit; } table[] = {
        {0,1252,0},{238,1250,1},{204,1251,2},{161,1253,3},{162,1254,4},{177,1255,5},{178,1256,6},
        {186,1257,7},{163,1258,8},{222,874,16},{128,932,17},{134,936,18},{129,949,19},
        {136,950,20},{130,1361,21},{2,42,31}
    };
    if (!out || (flags==TCI_SRCFONTSIG && !src)) return FALSE;
    for (unsigned i=0; i<sizeof(table)/sizeof(table[0]); ++i) {
        DWORD bit = 1u << table[i].bit;
        if ((flags==TCI_SRCCHARSET && (UINT_PTR)src==table[i].charset) ||
                (flags==TCI_SRCCODEPAGE && (UINT_PTR)src==table[i].codepage) ||
                (flags==TCI_SRCFONTSIG && (*src & bit))) {
            ZeroMemory(out,sizeof(*out));
            out->ciCharset=table[i].charset;
            out->ciACP=table[i].codepage;
            out->fs.fsCsb[0]=bit;
            return TRUE;
        }
    }
    return FALSE;
}

struct font_query {
    const LOGFONTA *filter;
    FONTENUMPROCA proc;
    LPARAM arg;
};

static int CALLBACK enumerate_font(const LOGFONTA *font, const TEXTMETRICA *metric, DWORD type, LPARAM param)
{
    const ENUMLOGFONTA *legacy = (const ENUMLOGFONTA *)font;
    struct font_query *query = (struct font_query *)param;
    ENUMLOGFONTEXA extended = {0};
    NEWTEXTMETRICEXA new_metric = {0};
    if (query->filter->lfCharSet != DEFAULT_CHARSET && font->lfCharSet != query->filter->lfCharSet) return 1;
    extended.elfLogFont = *font;
    CopyMemory(extended.elfFullName, legacy->elfFullName, sizeof(extended.elfFullName));
    CopyMemory(extended.elfStyle, legacy->elfStyle, sizeof(extended.elfStyle));
    if (type & TRUETYPE_FONTTYPE) CopyMemory(&new_metric.ntmTm, metric, sizeof(NEWTEXTMETRICA));
    else CopyMemory(&new_metric.ntmTm, metric, sizeof(*metric));
    return query->proc(&extended.elfLogFont, (const TEXTMETRICA *)&new_metric, type, query->arg);
}

int WINAPI wx_enum_fonts(HDC dc, LOGFONTA *filter, FONTENUMPROCA proc, LPARAM arg, DWORD flags)
{
    struct font_query query = {filter, proc, arg};
    if (!filter || !proc || flags) { SetLastError(ERROR_INVALID_PARAMETER); return 0; }
    return EnumFontFamiliesA(dc, filter->lfFaceName[0] ? filter->lfFaceName : NULL, enumerate_font, (LPARAM)&query);
}
