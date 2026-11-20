#define WIN32_LEAN_AND_MEAN
#include <windows.h>

struct input {
    const BYTE *data;
    SIZE_T size, pos;
};

struct output {
    BYTE *data;
    SIZE_T size, pos;
};

static WORD word_at(const BYTE *p)
{
    return (WORD)(p[0] | (p[1] << 8));
}

static HRSRC resource_for_user(HINSTANCE module, LPCWSTR name, WORD type)
{
    LPCWSTR kind = MAKEINTRESOURCEW(type);
    HRSRC result = FindResourceExW(module, kind, name, GetUserDefaultLangID());
    if (!result) result = FindResourceExW(module, kind, name, GetSystemDefaultLangID());
    if (!result) result = FindResourceExW(module, kind, name, MAKELANGID(LANG_NEUTRAL,SUBLANG_NEUTRAL));
    if (!result) result = FindResourceW(module, name, kind);
    return result;
}

int WINAPI wx_load_string(HINSTANCE module, UINT id, WCHAR *text, int size)
{
    if (!text || size < 0) return 0;
    if (!module) module = GetModuleHandleW(NULL);
    HRSRC r = resource_for_user(module, MAKEINTRESOURCEW((id >> 4) + 1), 6);
    HGLOBAL loaded = r ? LoadResource(module, r) : NULL;
    const BYTE *data = loaded ? LockResource(loaded) : NULL;
    SIZE_T bytes = data ? SizeofResource(module,r) : 0;
    for (unsigned i = 0; bytes >= 2; ++i) {
        unsigned length = word_at(data);
        data += 2; bytes -= 2;
        if (length > bytes / 2) break;
        if (i == (id & 15)) {
            if (size == 0) { *(const WCHAR **)text = (const WCHAR *)data; return length; }
            if (length >= (unsigned)size) length = size - 1;
            CopyMemory(text, data, length * sizeof(*text));
            text[length] = 0;
            return length;
        }
        data += 2 * length; bytes -= 2 * length;
    }
    if (size) *text = 0;
    return 0;
}

static const BYTE *take(struct input *in, SIZE_T n)
{
    if (n > in->size - in->pos) return NULL;
    const BYTE *p = in->data + in->pos;
    in->pos += n;
    return p;
}

static BOOL put(struct output *out, const void *data, SIZE_T n)
{
    if (!data || n > out->size - out->pos) return FALSE;
    CopyMemory(out->data + out->pos, data, n);
    out->pos += n;
    return TRUE;
}

/* A menu, class or title is a Unicode string, zero, or 0xffff and an ordinal. */
static BOOL field(struct input *in, struct output *out)
{
    SIZE_T start = in->pos;
    const BYTE *p = take(in, 2);
    if (!p) return FALSE;
    if (word_at(p) == 0xffff) {
        if (!take(in, 2)) return FALSE;
    } else {
        while (word_at(p)) {
            p = take(in, 2);
            if (!p) return FALSE;
        }
    }
    return put(out, in->data + start, in->pos - start);
}

static BOOL dialog_font(struct input *in, struct output *out)
{
    static const WCHAR shell[] = L"MS Shell Dlg", legacy[] = L"MS Sans Serif";
    if (sizeof(shell) <= in->size - in->pos) {
        SIZE_T i = 0;
        while (i < sizeof(shell) / sizeof(*shell) && word_at(in->data + in->pos + 2 * i) == shell[i]) ++i;
        if (2 * i == sizeof(shell)) {
            take(in, sizeof(shell));
            return put(out, legacy, sizeof(legacy));
        }
    }
    return field(in, out);
}

static BOOL convert(const BYTE *data, SIZE_T bytes, BYTE *storage, SIZE_T capacity, BOOL extended)
{
    struct input in = {data, bytes, 0};
    struct output out = {storage, capacity, 0};
    const BYTE *h = take(&in, extended ? 26 : 18);
    if (!h) return FALSE;
    if (extended) {
        if (!put(&out, h + 12, 4) || !put(&out, h + 8, 4) || !put(&out, h + 16, 10)) return FALSE;
    } else if (!put(&out, h, 18)) return FALSE;
    unsigned items = word_at(h + (extended ? 16 : 8));
    if (!field(&in, &out) || !field(&in, &out) || !field(&in, &out)) return FALSE;
    if (word_at(h + (extended ? 12 : 0)) & DS_SETFONT) {
        const BYTE *font = take(&in, extended ? 6 : 2);
        if (!put(&out, font, 2) || !dialog_font(&in, &out)) return FALSE;
    }
    while (items--) {
        static const DWORD zero = 0;
        if (!take(&in, (0 - in.pos) & 3) || !put(&out, &zero, (0 - out.pos) & 3)) return FALSE;
        const BYTE *item = take(&in, extended ? 24 : 18);
        if (!item) return FALSE;
        if (extended) {
            if (!put(&out, item + 8, 4) || !put(&out, item + 4, 4) ||
                    !put(&out, item + 12, 8) || !put(&out, item + 20, 2)) return FALSE;
        } else if (!put(&out, item, 18)) return FALSE;
        if (!field(&in, &out) || !field(&in, &out)) return FALSE;
        const BYTE *length = take(&in, 2);
        if (!length) return FALSE;
        WORD extra = word_at(length);
        /* Extended templates count data bytes. Standard templates count the
         * size word as well for a nonempty creation-data array. */
        if ((extended && extra > 0xfffd) || (!extended && extra == 1)) return FALSE;
        WORD total = extended && extra ? extra + 2 : extra;
        if (!extended && extra) extra -= 2;
        if (!put(&out, &total, 2) || !put(&out, take(&in, extra), extra)) return FALSE;
    }
    return TRUE;
}

static const DLGTEMPLATE *template_data(HINSTANCE module, LPCWSTR name, void **owned)
{
    *owned = NULL;
    HRSRC r = resource_for_user(module, name, 5);
    if (!r) return NULL;
    DWORD size = SizeofResource(module, r);
    HGLOBAL loaded = LoadResource(module, r);
    const BYTE *data = loaded ? LockResource(loaded) : NULL;
    if (!data || size < 4) return NULL;
    BOOL extended = word_at(data) == 1 && word_at(data + 2) == 0xffff;
    if (size > (SIZE_T)-1 - 8) return NULL;
    /* Allow the older font's name and its DWORD alignment in standard dialogs. */
    SIZE_T capacity = size + 8;
    BYTE *copy = HeapAlloc(GetProcessHeap(), 0, capacity);
    if (!copy) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return NULL; }
    if (!convert(data, size, copy, capacity, extended)) {
        HeapFree(GetProcessHeap(), 0, copy);
        SetLastError(ERROR_INVALID_DATA);
        return NULL;
    }
    *owned = copy;
    return (const DLGTEMPLATE *)copy;
}

HWND WINAPI wx_create_dialog(HINSTANCE module, LPCWSTR name, HWND parent, DLGPROC proc, LPARAM arg)
{
    if (!module) module = GetModuleHandleW(NULL);
    void *owned;
    const DLGTEMPLATE *dialog = template_data(module, name, &owned);
    if (!dialog) return NULL;
    HWND result = CreateDialogIndirectParamW(module, dialog, parent, proc, arg);
    if (owned) {
        DWORD error = GetLastError();
        HeapFree(GetProcessHeap(), 0, owned);
        SetLastError(error);
    }
    return result;
}

INT_PTR WINAPI wx_dialog_box(HINSTANCE module, LPCWSTR name, HWND parent, DLGPROC proc, LPARAM arg)
{
    if (!module) module = GetModuleHandleW(NULL);
    void *owned;
    const DLGTEMPLATE *dialog = template_data(module, name, &owned);
    if (!dialog) return -1;
    INT_PTR result = DialogBoxIndirectParamW(module, dialog, parent, proc, arg);
    if (owned) {
        DWORD error = GetLastError();
        HeapFree(GetProcessHeap(), 0, owned);
        SetLastError(error);
    }
    return result;
}
