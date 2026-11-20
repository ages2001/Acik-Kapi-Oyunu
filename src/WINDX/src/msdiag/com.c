#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <objbase.h>

static DWORD apartment = TLS_OUT_OF_INDEXES;
static CRITICAL_SECTION lock;
static HMODULE system_ole;
static BOOL checked_ole;
struct server_entry { HMODULE module; struct server_entry *next; };
static struct server_entry *servers;

FARPROC wx_ole_proc(const char *name)
{
    EnterCriticalSection(&lock);
    if (!checked_ole) {
        char path[MAX_PATH];
        UINT len = GetSystemDirectoryA(path, MAX_PATH);
        checked_ole = TRUE;
        if (len && len < MAX_PATH - 10) {
            lstrcatA(path, "\\ole32.dll");
            if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) system_ole = LoadLibraryA(path);
        }
    }
    HMODULE module = system_ole;
    LeaveCriticalSection(&lock);
    return module ? GetProcAddress(module, name) : NULL;
}

BOOL WINAPI wx_com_entry(HINSTANCE instance, DWORD reason, void *reserved)
{
    if (reason == DLL_PROCESS_ATTACH) {
        apartment = TlsAlloc();
        if (apartment != TLS_OUT_OF_INDEXES) InitializeCriticalSection(&lock);
        return apartment != TLS_OUT_OF_INDEXES;
    }
    if (reason == DLL_PROCESS_DETACH && apartment != TLS_OUT_OF_INDEXES) {
        TlsFree(apartment);
        DeleteCriticalSection(&lock);
    }
    return TRUE;
}

HRESULT WINAPI wx_co_init(void *reserved)
{
    HRESULT (WINAPI *native)(void *) = (void *)wx_ole_proc("CoInitialize");
    if (native) return native(reserved);
    ULONG_PTR count = (ULONG_PTR)TlsGetValue(apartment);
    if (reserved) return E_INVALIDARG;
    if (!TlsSetValue(apartment, (void *)(count + 1))) return E_OUTOFMEMORY;
    return count ? S_FALSE : S_OK;
}

void WINAPI wx_co_uninit(void)
{
    void (WINAPI *native)(void) = (void *)wx_ole_proc("CoUninitialize");
    if (native) { native(); return; }
    ULONG_PTR count = (ULONG_PTR)TlsGetValue(apartment);
    if (count) TlsSetValue(apartment, (void *)(count - 1));
}

HRESULT WINAPI wx_co_create(REFCLSID clsid, IUnknown *outer, DWORD context, REFIID iid, void **result)
{
    static const IID class_factory = {1,0,0,{0xc0,0,0,0,0,0,0,0x46}};
    static const CLSID dxdiag_provider = {0xa65b8071,0x3bfe,0x4213,{0x9a,0x5b,0x49,0x1d,0xa4,0x46,0x1c,0xa7}};
    typedef HRESULT (WINAPI *get_factory_fn)(REFCLSID, REFIID, void **);
    IClassFactory *factory = NULL;
    char key[86], path[MAX_PATH];
    LONG size = sizeof(path);
    HMODULE module;
    get_factory_fn get;
    HRESULT hr;
    HRESULT (WINAPI *native)(REFCLSID,IUnknown *,DWORD,REFIID,void **) = (void *)wx_ole_proc("CoCreateInstance");
    if (native) return native(clsid,outer,context,iid,result);
    if (!result) return E_POINTER;
    *result = NULL;
    if (!clsid || !iid) return E_INVALIDARG;
    if (!TlsGetValue(apartment)) return CO_E_NOTINITIALIZED;
    if (!(context & CLSCTX_INPROC_SERVER)) return REGDB_E_CLASSNOTREG;
    wsprintfA(key,"CLSID\\{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}\\InprocServer32",
            clsid->Data1, clsid->Data2, clsid->Data3, clsid->Data4[0], clsid->Data4[1], clsid->Data4[2],
            clsid->Data4[3], clsid->Data4[4], clsid->Data4[5], clsid->Data4[6], clsid->Data4[7]);
    BOOL private_server = FALSE;
    if (IsEqualCLSID(clsid, &dxdiag_provider)) {
        DWORD len = GetModuleFileNameA(NULL, path, MAX_PATH);
        if (len && len < MAX_PATH) {
            char *file = path + len;
            while (file > path && file[-1] != '\\') --file;
            if ((SIZE_T)(file - path) + sizeof("dxdiagn.dll") <= sizeof(path)) {
                lstrcpyA(file, "dxdiagn.dll");
                private_server = GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
            }
        }
    }
    if (!private_server) {
        if (RegQueryValueA(HKEY_CLASSES_ROOT, key, path, &size) != ERROR_SUCCESS || size <= 0 || size > MAX_PATH)
            return REGDB_E_CLASSNOTREG;
        path[MAX_PATH-1] = 0;
    }
    module = LoadLibraryA(path);
    if (!module) return HRESULT_FROM_WIN32(GetLastError());
    get = (get_factory_fn)GetProcAddress(module, "DllGetClassObject");
    if (!get) { FreeLibrary(module); return CO_E_ERRORINDLL; }
    /* Hold exactly one reference to each server for the process lifetime.
     * Living COM objects continue to execute its code after the caller returns. */
    EnterCriticalSection(&lock);
    struct server_entry *entry;
    for (entry = servers; entry && entry->module != module; entry = entry->next) {}
    if (entry) {
        LeaveCriticalSection(&lock);
        FreeLibrary(module);
    } else {
        entry = HeapAlloc(GetProcessHeap(), 0, sizeof(*entry));
        if (!entry) { LeaveCriticalSection(&lock); FreeLibrary(module); return E_OUTOFMEMORY; }
        entry->module = module;
        entry->next = servers;
        servers = entry;
        LeaveCriticalSection(&lock);
    }
    hr = get(clsid, &class_factory, (void **)&factory);
    if (SUCCEEDED(hr)) {
        hr = IClassFactory_CreateInstance(factory, outer, iid, result);
        IClassFactory_Release(factory);
    }
    return hr;
}

HRESULT WINAPI wx_ole_init(void *reserved)
{
    HRESULT (WINAPI *native)(void *) = (void *)wx_ole_proc("OleInitialize");
    return native ? native(reserved) : wx_co_init(reserved);
}

void WINAPI wx_ole_uninit(void)
{
    void (WINAPI *native)(void) = (void *)wx_ole_proc("OleUninitialize");
    if (native) native(); else wx_co_uninit();
}

static const IID iid_malloc = {2,0,0,{0xc0,0,0,0,0,0,0,0x46}};
static const IID iid_unknown = {0,0,0,{0xc0,0,0,0,0,0,0,0x46}};

static HRESULT WINAPI malloc_query(IMalloc *self, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualIID(iid, &iid_malloc) && !IsEqualIID(iid, &iid_unknown)) return E_NOINTERFACE;
    *out = self;
    return S_OK;
}
static ULONG WINAPI malloc_ref(IMalloc *self) { (void)self; return 1; }
static void *WINAPI malloc_alloc(IMalloc *self, SIZE_T size)
{ (void)self; return HeapAlloc(GetProcessHeap(), 0, size); }
static void WINAPI malloc_free(IMalloc *self, void *p)
{ (void)self; if (p) HeapFree(GetProcessHeap(), 0, p); }
static void *WINAPI malloc_resize(IMalloc *self, void *p, SIZE_T size)
{
    if (!p) return malloc_alloc(self, size);
    if (!size) { malloc_free(self, p); return NULL; }
    return HeapReAlloc(GetProcessHeap(), 0, p, size);
}
static SIZE_T WINAPI malloc_size(IMalloc *self, void *p)
{ (void)self; return p ? HeapSize(GetProcessHeap(), 0, p) : (SIZE_T)-1; }
static int WINAPI malloc_owned(IMalloc *self, void *p) { (void)self; (void)p; return -1; }
static void WINAPI malloc_minimize(IMalloc *self)
{
    SIZE_T (WINAPI *compact)(HANDLE,DWORD) = (void *)GetProcAddress(GetModuleHandleA("kernel32.dll"), "HeapCompact");
    (void)self;
    if (compact) compact(GetProcessHeap(), 0);
}

static IMallocVtbl allocator_methods = {malloc_query, malloc_ref, malloc_ref, malloc_alloc,
    malloc_resize, malloc_free, malloc_size, malloc_owned, malloc_minimize};
static IMalloc allocator = {&allocator_methods};

HRESULT WINAPI wx_get_malloc(DWORD context, IMalloc **out)
{
    HRESULT (WINAPI *native)(DWORD,IMalloc **) = (void *)wx_ole_proc("CoGetMalloc");
    if (native) return native(context, out);
    if (!out) return E_POINTER;
    *out = NULL;
    if (context != MEMCTX_TASK) return E_INVALIDARG;
    *out = &allocator;
    return S_OK;
}
void WINAPI wx_task_free(void *p)
{
    void (WINAPI *native)(void *) = (void *)wx_ole_proc("CoTaskMemFree");
    if (native) native(p); else malloc_free(&allocator, p);
}

HRESULT WINAPI wx_com_security(PSECURITY_DESCRIPTOR sd, LONG count, SOLE_AUTHENTICATION_SERVICE *svc,
        void *reserved, DWORD level, DWORD imp, void *auth, DWORD flags, void *reserved2)
{
    HRESULT (WINAPI *native)(PSECURITY_DESCRIPTOR,LONG,SOLE_AUTHENTICATION_SERVICE *,void *,DWORD,DWORD,
                             void *,DWORD,void *) = (void *)wx_ole_proc("CoInitializeSecurity");
    if (native) return native(sd, count, svc, reserved, level, imp, auth, flags, reserved2);
    /* The bridge activates in-process servers only. Default process settings
     * never grant an out-of-process or remote activation. */
    if (!sd && count == -1 && !svc && !reserved && !auth && !reserved2 && flags == EOAC_NONE) return S_OK;
    return E_NOTIMPL;
}

int WINAPI wx_guid_string(REFGUID id, WCHAR *text, int size)
{
    if (!id || !text || size < 39) return 0;
    return wsprintfW(text,L"{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",id->Data1,
           id->Data2,id->Data3,id->Data4[0],id->Data4[1],id->Data4[2],id->Data4[3],
           id->Data4[4],id->Data4[5],id->Data4[6],id->Data4[7]) + 1;
}

static BOOL hex_field(const WCHAR **s, int digits, DWORD *out)
{
    DWORD value=0;
    for (int i=0;i<digits;++i) {
        WCHAR c=*(*s)++;
        if (c >= L'0' && c <= L'9') value=value*16+c-L'0';
        else if ((c|32) >= L'a' && (c|32) <= L'f') value=value*16+(c|32)-L'a'+10;
        else return FALSE;
    }
    *out=value;
    return TRUE;
}

HRESULT WINAPI wx_clsid_string(const WCHAR *s, CLSID *id)
{
    HRESULT (WINAPI *native)(const WCHAR *,CLSID *) = (void *)wx_ole_proc("CLSIDFromString");
    if (native) return native(s, id);
    if (!id) return E_INVALIDARG;
    ZeroMemory(id,sizeof(*id));
    if (!s) return S_OK;
    if (*s++ != L'{') return CO_E_CLASSSTRING;
    GUID parsed; DWORD v;
    if (!hex_field(&s,8,&parsed.Data1) || *s++ != L'-' || !hex_field(&s,4,&v)) return CO_E_CLASSSTRING;
    parsed.Data2=(WORD)v;
    if (*s++ != L'-' || !hex_field(&s,4,&v) || *s++ != L'-') return CO_E_CLASSSTRING;
    parsed.Data3=(WORD)v;
    for(unsigned i=0; i<8; ++i) {
        if (i==2 && *s++ != L'-') return CO_E_CLASSSTRING;
        if (!hex_field(&s,2,&v)) return CO_E_CLASSSTRING;
        parsed.Data4[i]=(BYTE)v;
    }
    if (*s++ != L'}' || *s) return CO_E_CLASSSTRING;
    *id=parsed;
    return S_OK;
}
