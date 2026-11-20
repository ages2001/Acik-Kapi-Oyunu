#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <oleauto.h>

/* BSTRs carry a byte length immediately before their UTF-16 payload. Keep the
 * allocation protocol identical on both sides of the in-process COM boundary. */
static BSTR alloc_string(const WCHAR *text, UINT bytes)
{
    if (bytes > (SIZE_T)-1 - sizeof(DWORD) - sizeof(WCHAR)) return NULL;
    DWORD *base = HeapAlloc(GetProcessHeap(), 0, sizeof(*base) + bytes + sizeof(WCHAR));
    if (!base) return NULL;
    *base = bytes;
    BYTE *value = (BYTE *)(base + 1);
    if (bytes) CopyMemory(value, text, bytes);
    value[bytes] = value[bytes+1] = 0;
    return (BSTR)value;
}

BSTR WINAPI wx_alloc_string(const WCHAR *text)
{
    return text ? alloc_string(text, lstrlenW(text)*sizeof(WCHAR)) : NULL;
}

void WINAPI wx_free_string(BSTR value)
{
    if (value) HeapFree(GetProcessHeap(), 0, (DWORD *)value - 1);
}

void WINAPI wx_variant_init(VARIANTARG *v)
{
    if (v) ZeroMemory(v, sizeof(*v));
}

HRESULT WINAPI wx_variant_clear(VARIANTARG *v)
{
    if (!v) return E_INVALIDARG;
    if (!(V_VT(v) & VT_BYREF)) {
        switch (V_VT(v)) {
        case VT_BSTR: wx_free_string(V_BSTR(v)); break;
        case VT_UNKNOWN: if (V_UNKNOWN(v)) IUnknown_Release(V_UNKNOWN(v)); break;
        case VT_DISPATCH: if (V_DISPATCH(v)) IDispatch_Release(V_DISPATCH(v)); break;
        case VT_EMPTY: case VT_NULL: case VT_I1: case VT_UI1: case VT_I2: case VT_UI2:
        case VT_BOOL: case VT_ERROR: case VT_I4: case VT_UI4: case VT_INT: case VT_UINT:
        case VT_I8: case VT_UI8: case VT_R4: case VT_R8: case VT_DATE: case VT_CY: case VT_DECIMAL: break;
        default: return DISP_E_BADVARTYPE;
        }
    }
    wx_variant_init(v);
    return S_OK;
}

HRESULT WINAPI wx_variant_copy(VARIANTARG *out, const VARIANTARG *source)
{
    if (!source || !out) return E_INVALIDARG;
    if (out == source) return S_OK;
    VARIANTARG copy = *source;
    if (!(V_VT(source) & VT_BYREF)) {
        if (V_VT(source) & (VT_ARRAY | VT_VECTOR) || V_VT(source) == VT_RECORD) return DISP_E_BADVARTYPE;
        if (V_VT(source) == VT_BSTR && V_BSTR(source)) {
            V_BSTR(&copy) = alloc_string(V_BSTR(source), ((DWORD *)V_BSTR(source))[-1]);
            if (!V_BSTR(&copy)) return E_OUTOFMEMORY;
        }
        if (V_VT(source) == VT_UNKNOWN && V_UNKNOWN(source)) IUnknown_AddRef(V_UNKNOWN(source));
        if (V_VT(source) == VT_DISPATCH && V_DISPATCH(source)) IDispatch_AddRef(V_DISPATCH(source));
    }
    HRESULT hr = wx_variant_clear(out);
    if (FAILED(hr)) { wx_variant_clear(&copy); return hr; }
    *out = copy;
    return S_OK;
}

HRESULT WINAPI wx_variant_change(VARIANTARG *out, const VARIANTARG *source, USHORT flags, VARTYPE type)
{
    (void)flags;
    if (!out || !source) return E_INVALIDARG;
    if (type == V_VT(source)) return wx_variant_copy(out, source);
    if (type != VT_BSTR) return DISP_E_TYPEMISMATCH;
    ULONGLONG number;
    BOOL negative = FALSE;
    switch (V_VT(source)) {
    case VT_I8: negative=V_I8(source)<0; number=(ULONGLONG)V_I8(source); break;
    case VT_UI8: number=V_UI8(source); break;
    case VT_I4: case VT_INT: negative=V_I4(source)<0; number=(ULONGLONG)(LONGLONG)V_I4(source); break;
    case VT_UI4: case VT_UINT: number=V_UI4(source); break;
    case VT_I2: negative=V_I2(source)<0; number=(ULONGLONG)(LONGLONG)V_I2(source); break;
    case VT_UI2: number=V_UI2(source); break;
    default: return DISP_E_TYPEMISMATCH;
    }
    if (negative) number=0-number;
    WCHAR buf[24], *next=buf+23;
    *next=0;
    do { *--next=L'0'+(WCHAR)(number%10); number/=10; } while (number);
    if (negative) *--next=L'-';
    BSTR string=wx_alloc_string(next);
    if (!string) return E_OUTOFMEMORY;
    HRESULT hr=wx_variant_clear(out);
    if (FAILED(hr)) { wx_free_string(string); return hr; }
    V_VT(out)=VT_BSTR; V_BSTR(out)=string;
    return S_OK;
}

HRESULT WINAPI wx_array_access(SAFEARRAY *array, void **data)
{
    if (!array || !data || array->cLocks == (ULONG)-1) return E_INVALIDARG;
    InterlockedIncrement((LONG *)&array->cLocks);
    *data = array->pvData;
    return S_OK;
}

HRESULT WINAPI wx_array_release(SAFEARRAY *array)
{
    if (!array || !array->cLocks) return E_UNEXPECTED;
    InterlockedDecrement((LONG *)&array->cLocks);
    return S_OK;
}
