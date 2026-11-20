#ifndef WINDX_INVENTORY_DEVICE_H
#define WINDX_INVENTORY_DEVICE_H
struct wx_interface {
    struct {
        HRESULT (WINAPI *query)(struct wx_interface *, const GUID *, void **);
        ULONG (WINAPI *addref)(struct wx_interface *);
        ULONG (WINAPI *release)(struct wx_interface *);
    } *methods;
};

static HMODULE wx_local_runtime(const char *directory, const char *name)
{
    char path[MAX_PATH], actual[MAX_PATH];
    if (!wx_component_path(path, directory, name)) return NULL;
    HMODULE module = LoadLibraryExA(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!module) return NULL;
    /* A different DLL with the same basename may already be loaded by a host. */
    DWORD size = GetModuleFileNameA(module, actual, MAX_PATH);
    if (!size || size >= MAX_PATH || lstrcmpiA(actual, path)) {
        FreeLibrary(module);
        return NULL;
    }
    return module;
}

static BOOL wx_measure_renderer(const char *directory, char *label, DWORD capacity)
{
    HMODULE gl = wx_local_runtime(directory, "opengl32.dll");
    if (!gl) return FALSE;
    int (WINAPI *choose)(HDC,const PIXELFORMATDESCRIPTOR *) = (void *)GetProcAddress(gl, "wglChoosePixelFormat");
    BOOL (WINAPI *set_format)(HDC,int,const PIXELFORMATDESCRIPTOR *) = (void *)GetProcAddress(gl, "wglSetPixelFormat");
    HGLRC (WINAPI *create)(HDC) = (void *)GetProcAddress(gl, "wglCreateContext");
    BOOL (WINAPI *bind)(HDC,HGLRC) = (void *)GetProcAddress(gl, "wglMakeCurrent");
    BOOL (WINAPI *destroy)(HGLRC) = (void *)GetProcAddress(gl, "wglDeleteContext");
    const unsigned char *(WINAPI *get_string)(UINT) = (void *)GetProcAddress(gl, "glGetString");
    WNDCLASSA cls = {0};
    cls.style = CS_OWNDC;
    cls.lpfnWndProc = DefWindowProcA;
    cls.hInstance = GetModuleHandleA(NULL);
    cls.lpszClassName = "WindxRendererInventory";
    HWND window = NULL;
    HDC dc = NULL;
    HGLRC context = NULL;
    BOOL measured = FALSE, bound = FALSE;
    ATOM atom = 0;
    if (!choose || !set_format || !create || !bind || !destroy || !get_string) goto done;
    if (!(atom = RegisterClassA(&cls))) goto done;
    window = CreateWindowExA(0, cls.lpszClassName, "", WS_POPUP, 0, 0, 4, 4, NULL, NULL, cls.hInstance, NULL);
    if (!window || !(dc = GetDC(window))) goto done;
    PIXELFORMATDESCRIPTOR format = {0};
    format.nSize = sizeof(format);
    format.nVersion = 1;
    format.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    format.iPixelType = PFD_TYPE_RGBA;
    format.cColorBits = 24;
    format.cAlphaBits = 8;
    int id = choose(dc, &format);
    if (!id || !set_format(dc, id, &format) || !(context = create(dc)) || !(bound = bind(dc, context))) goto done;
    const char *renderer = (const char *)get_string(0x1f01); /* GL_RENDERER */
    if (renderer && *renderer) { wx_inventory_copy(label, renderer, capacity); measured = TRUE; }
done:
    if (bound) bind(NULL, NULL);
    if (context) destroy(context);
    if (dc) ReleaseDC(window, dc);
    if (window) DestroyWindow(window);
    if (atom) UnregisterClassA(cls.lpszClassName, cls.hInstance);
    FreeLibrary(gl);
    return measured;
}

static const char *wx_feature_label(UINT level)
{
    switch (level) {
    case 0xc200: return "12.2";
    case 0xc100: return "12.1";
    case 0xc000: return "12.0";
    case 0xb100: return "11.1";
    case 0xb000: return "11.0";
    case 0xa100: return "10.1";
    case 0xa000: return "10.0";
    case 0x9300: return "9.3";
    case 0x9200: return "9.2";
    case 0x9100: return "9.1";
    default: return NULL;
    }
}

static BOOL wx_probe_device12(const char *directory, char *label)
{
    static const GUID device_id = {0x189819f1,0x1db6,0x4b57,{0xbe,0x54,0x18,0x21,0x33,0x9b,0x85,0xf7}};
    static const UINT levels[] = {0xc200,0xc100,0xc000,0xb100,0xb000};
    HMODULE library = wx_local_runtime(directory, "d3d12.dll");
    if (!library) return FALSE;
    HRESULT (WINAPI *create)(void *,UINT,const GUID *,void **) = (void *)GetProcAddress(library, "D3D12CreateDevice");
    BOOL result = FALSE;
    for (UINT i = 0; create && i < sizeof(levels) / sizeof(levels[0]); ++i) {
        struct wx_interface *device = NULL;
        if (SUCCEEDED(create(NULL, levels[i], &device_id, (void **)&device)) && device) {
            device->methods->release(device);
            wsprintfA(label, "WINDX Direct3D 12 (feature level %s)", wx_feature_label(levels[i]));
            result = TRUE;
            break;
        }
    }
    FreeLibrary(library);
    return result;
}

static BOOL wx_probe_device11(const char *directory, char *label)
{
    static const UINT levels[] = {0xb000,0xa100,0xa000,0x9300,0x9200,0x9100};
    HMODULE library = wx_local_runtime(directory, "d3d11.dll");
    if (!library) return FALSE;
    HRESULT (WINAPI *create)(void *,UINT,HMODULE,UINT,const UINT *,UINT,UINT,struct wx_interface **,
            UINT *,void *) = (void *)GetProcAddress(library, "D3D11CreateDevice");
    struct wx_interface *device = NULL;
    UINT selected = 0;
    BOOL result = create && SUCCEEDED(create(NULL, 1, NULL, 0, levels, sizeof(levels) / sizeof(levels[0]),
            7, &device, &selected, NULL)) && device && wx_feature_label(selected);
    if (result) wsprintfA(label, "WINDX Direct3D 11 (feature level %s)", wx_feature_label(selected));
    if (device) device->methods->release(device);
    FreeLibrary(library);
    return result;
}

static BOOL wx_probe_device10(const char *directory, BOOL version_1, char *label)
{
    HMODULE library = wx_local_runtime(directory, version_1 ? "d3d10_1.dll" : "d3d10.dll");
    if (!library) return FALSE;
    struct wx_interface *device = NULL;
    HRESULT hr = E_NOINTERFACE;
    if (version_1) {
        HRESULT (WINAPI *create)(void *,UINT,HMODULE,UINT,UINT,UINT,struct wx_interface **) =
                (void *)GetProcAddress(library, "D3D10CreateDevice1");
        if (create) hr = create(NULL, 0, NULL, 0, 0xa100, 0x20, &device);
    } else {
        HRESULT (WINAPI *create)(void *,UINT,HMODULE,UINT,UINT,struct wx_interface **) =
                (void *)GetProcAddress(library, "D3D10CreateDevice");
        if (create) hr = create(NULL, 0, NULL, 0, 29, &device);
    }
    BOOL result = SUCCEEDED(hr) && device;
    if (result) lstrcpyA(label, version_1 ? "WINDX Direct3D 10.1 device" : "WINDX Direct3D 10 device");
    if (device) device->methods->release(device);
    FreeLibrary(library);
    return result;
}


BOOL wx_probe_legacy8(HMODULE library);
BOOL wx_probe_legacy9(HMODULE library);

static BOOL wx_probe_ddraw(const char *directory, char *label)
{
    static const struct { GUID id; unsigned generation; } versions[] = {
        {{0x15e65ec0,0x3b9c,0x11d2,{0xb9,0x2f,0x00,0x60,0x97,0x97,0xea,0x5b}}, 7},
        {{0x9c59509a,0x39bd,0x11d1,{0x8c,0x4a,0x00,0xc0,0x4f,0xd9,0x30,0xc5}}, 4},
        {{0xb3a6f3e0,0x2b43,0x11cf,{0xa2,0xde,0x00,0xaa,0x00,0xb9,0x33,0x56}}, 2}
    };
    HMODULE library = wx_local_runtime(directory, "ddraw.dll");
    if (!library) return FALSE;
    HRESULT (WINAPI *create)(GUID *,struct wx_interface **,void *) =
            (void *)GetProcAddress(library, "DirectDrawCreate");
    struct wx_interface *draw = NULL;
    BOOL ok = create && SUCCEEDED(create(NULL, &draw, NULL)) && draw;
    if (ok) {
        unsigned generation = 1;
        for (unsigned i = 0; i < sizeof(versions) / sizeof(versions[0]); ++i) {
            struct wx_interface *newer = NULL;
            if (SUCCEEDED(draw->methods->query(draw, &versions[i].id, (void **)&newer)) && newer) {
                generation = versions[i].generation;
                newer->methods->release(newer);
                break;
            }
        }
        wsprintfA(label, "WINDX DirectDraw %u interface", generation);
        draw->methods->release(draw);
    }
    FreeLibrary(library);
    return ok;
}

static BOOL wx_probe_legacy(const char *directory, char *label)
{
    HMODULE library = wx_local_runtime(directory, "d3d9.dll");
    BOOL ok = library && wx_probe_legacy9(library);
    if (library) FreeLibrary(library);
    if (ok) { lstrcpyA(label, "WINDX Direct3D 9 device"); return TRUE; }
    library = wx_local_runtime(directory, "d3d8.dll");
    ok = library && wx_probe_legacy8(library);
    if (library) FreeLibrary(library);
    if (ok) { lstrcpyA(label, "WINDX Direct3D 8 device"); return TRUE; }
    return wx_probe_ddraw(directory, label);
}
#endif
