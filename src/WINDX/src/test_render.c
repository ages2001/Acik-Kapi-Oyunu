#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <d3d9.h>
#include <d3d10_1.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include "test_output.h"

typedef IDirect3D9 *(WINAPI *create9_fn)(UINT);
typedef HRESULT (WINAPI *create11_fn)(IDXGIAdapter *, D3D_DRIVER_TYPE,
        HMODULE, UINT, const D3D_FEATURE_LEVEL *, UINT, UINT, ID3D11Device **,
        D3D_FEATURE_LEVEL *, ID3D11DeviceContext **);

/* A non-square target exercises both horizontal addressing and row pitch. */
enum { WIDTH = 4, HEIGHT = 3 };

static BOOL pixels_match(const void *base, SIZE_T pitch, DWORD color)
{
    UINT row, col;
    if (!base || pitch < WIDTH * sizeof(color))
        return FALSE;
    for (row = 0; row < HEIGHT; ++row) {
        const DWORD *pixels = (const DWORD *)((const BYTE *)base + row * pitch);
        for (col = 0; col < WIDTH; ++col)
            if (pixels[col] != color) return FALSE;
    }
    return TRUE;
}

typedef HRESULT (WINAPI *compile_fn)(const void *, SIZE_T, LPCSTR, const D3D_SHADER_MACRO *,
        ID3DInclude *, LPCSTR, LPCSTR, UINT, UINT, ID3DBlob **, ID3DBlob **);

static HMODULE load_shader_compiler(void)
{
    HMODULE compiler = LoadLibraryA("d3dcompiler_43.dll");
    if (!compiler) compiler = LoadLibraryA("wxcomp43.dll");
    return compiler;
}

static HRESULT compile_shader(HMODULE compiler, const char *source, const char *profile, ID3DBlob **code)
{
    compile_fn compile = compiler ? (compile_fn)GetProcAddress(compiler, "D3DCompile") : NULL;
    if (!compile) return E_NOINTERFACE;
    return compile(source, lstrlenA(source), NULL, NULL, NULL, "main", profile, 0, 0, code, NULL);
}

static HMODULE load_runtime(const char *name)
{
    char path[MAX_PATH];
    HMODULE module = LoadLibraryA(name);
    report(name);
    if (module && GetModuleFileNameA(module, path, sizeof(path))) {
        report(" loaded from ");
        report(path);
    } else {
        report(" load error ");
        report_hex(GetLastError());
    }
    report("\r\n");
    return module;
}

static BOOL test_surface_dc(IDirect3DDevice9 *device)
{
    IDirect3DSurface9 *surface = NULL;
    D3DLOCKED_RECT mapped;
    HDC dc;
    BOOL pass = FALSE;
    HRESULT hr = IDirect3DDevice9_CreateOffscreenPlainSurface(device, WIDTH, HEIGHT,
            D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &surface, NULL);
    if (FAILED(hr)) return FALSE;
    if (FAILED(IDirect3DSurface9_LockRect(surface, &mapped, NULL, 0))) goto done;
    ((DWORD *)((BYTE *)mapped.pBits + mapped.Pitch))[1] = 0xff123456;
    IDirect3DSurface9_UnlockRect(surface);
    if (FAILED(IDirect3DSurface9_GetDC(surface, &dc))) goto done;
    pass = GetPixel(dc, 1, 1) == RGB(0x12, 0x34, 0x56) &&
            SetPixel(dc, 2, 1, RGB(0x10, 0x20, 0x30)) == RGB(0x10, 0x20, 0x30);
    hr = IDirect3DSurface9_ReleaseDC(surface, dc);
    if (!pass || FAILED(hr) || FAILED(IDirect3DSurface9_LockRect(surface, &mapped, NULL, D3DLOCK_READONLY))) {
        pass = FALSE;
        goto done;
    }
    pass = (((DWORD *)((BYTE *)mapped.pBits + mapped.Pitch))[2] & 0x00ffffff) == 0x00102030;
    IDirect3DSurface9_UnlockRect(surface);
done:
    IDirect3DSurface9_Release(surface);
    return pass;
}

static BOOL read_pixels9(IDirect3DDevice9 *device, IDirect3DSurface9 *rt,
        IDirect3DSurface9 *readback, DWORD expected)
{
    D3DLOCKED_RECT mapped;
    if (FAILED(IDirect3DDevice9_GetRenderTargetData(device, rt, readback)) ||
            FAILED(IDirect3DSurface9_LockRect(readback, &mapped, NULL, D3DLOCK_READONLY))) return FALSE;
    BOOL pass = mapped.Pitch > 0 && pixels_match(mapped.pBits, mapped.Pitch, expected);
    IDirect3DSurface9_UnlockRect(readback);
    return pass;
}

static BOOL draw_triangle9(IDirect3DDevice9 *device)
{
    static const float vertices[][4] = {{-2,-2,0,1},{2*WIDTH+2,-2,0,1},{-2,2*HEIGHT+2,0,1}};
    static const char source[] = "float4 main() : COLOR {return float4(1,0,1,1);}";
    HMODULE compiler = load_shader_compiler();
    ID3DBlob *code = NULL;
    IDirect3DPixelShader9 *shader = NULL;
    BOOL pass = FALSE;
    HRESULT hr = compile_shader(compiler, source, "ps_2_0", &code);
    if (FAILED(hr) || FAILED(IDirect3DDevice9_CreatePixelShader(device,
            ID3D10Blob_GetBufferPointer(code), &shader))) goto done;
    if (FAILED(IDirect3DDevice9_SetPixelShader(device, shader)) ||
            FAILED(IDirect3DDevice9_SetFVF(device, D3DFVF_XYZRHW)) ||
            FAILED(IDirect3DDevice9_SetRenderState(device, D3DRS_CULLMODE, D3DCULL_NONE)) ||
            FAILED(IDirect3DDevice9_SetRenderState(device, D3DRS_ZENABLE, FALSE)) ||
            FAILED(IDirect3DDevice9_BeginScene(device))) goto done;
    hr = IDirect3DDevice9_DrawPrimitiveUP(device, D3DPT_TRIANGLELIST, 1, vertices, sizeof(vertices[0]));
    pass = SUCCEEDED(IDirect3DDevice9_EndScene(device)) && SUCCEEDED(hr);
done:
    if (shader) IDirect3DPixelShader9_Release(shader);
    if (code) ID3D10Blob_Release(code);
    if (compiler) FreeLibrary(compiler);
    return pass;
}

static BOOL test_d3d9(HWND window)
{
    D3DPRESENT_PARAMETERS presentation = {0};
    D3DADAPTER_IDENTIFIER9 adapter;
    IDirect3D9 *api = NULL;
    IDirect3DDevice9 *device = NULL;
    IDirect3DSurface9 *rt = NULL, *readback = NULL;
    HMODULE module = load_runtime("d3d9.dll");
    create9_fn create;
    HRESULT hr = E_FAIL;
    BOOL pass = FALSE, dc_pass = FALSE, draw_pass = FALSE;
    if (!module)
        return FALSE;
    create = (create9_fn)GetProcAddress(module, "Direct3DCreate9");
    if (!create || !(api = create(D3D_SDK_VERSION)))
        goto done;
    if (SUCCEEDED(IDirect3D9_GetAdapterIdentifier(api, 0, 0, &adapter))) {
        report("Adapter: ");
        report(adapter.Description);
        report("\r\n");
    }
    presentation.Windowed = TRUE;
    presentation.BackBufferWidth = WIDTH;
    presentation.BackBufferHeight = HEIGHT;
    presentation.SwapEffect = D3DSWAPEFFECT_DISCARD;
    presentation.hDeviceWindow = window;
    hr = IDirect3D9_CreateDevice(api, 0, D3DDEVTYPE_HAL, window,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING, &presentation, &device);
    if (FAILED(hr)) goto done;
    hr = IDirect3DDevice9_CreateRenderTarget(device, WIDTH, HEIGHT, D3DFMT_A8R8G8B8,
            D3DMULTISAMPLE_NONE, 0, FALSE, &rt, NULL);
    if (FAILED(hr)) goto done;
    hr = IDirect3DDevice9_SetRenderTarget(device, 0, rt);
    if (FAILED(hr)) goto done;
    hr = IDirect3DDevice9_Clear(device, 0, NULL, D3DCLEAR_TARGET, 0xff00ff00, 1.0f, 0);
    if (FAILED(hr)) goto done;
    hr = IDirect3DDevice9_CreateOffscreenPlainSurface(device, WIDTH, HEIGHT,
            D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &readback, NULL);
    if (FAILED(hr)) goto done;
    pass = read_pixels9(device, rt, readback, 0xff00ff00u);
    draw_pass = draw_triangle9(device) && read_pixels9(device, rt, readback, 0xffff00ffu);
done:
    if (device) dc_pass = test_surface_dc(device);
    if (readback) IDirect3DSurface9_Release(readback);
    if (rt) IDirect3DSurface9_Release(rt);
    if (device) IDirect3DDevice9_Release(device);
    if (api) IDirect3D9_Release(api);
    FreeLibrary(module);
    report(pass ? "D3D9 clear/readback PASS\r\n" : "D3D9 clear/readback FAIL, HRESULT ");
    if (!pass) { report_hex((DWORD)hr); report("\r\n"); }
    report(dc_pass ? "D3D9 surface GetDC/ReleaseDC PASS\r\n" : "D3D9 surface GetDC/ReleaseDC FAIL\r\n");
    report(draw_pass ? "D3D9 pixel shader draw/readback PASS\r\n" : "D3D9 pixel shader draw/readback FAIL\r\n");
    return pass && dc_pass && draw_pass;
}

static BOOL read_pixels11(ID3D11DeviceContext *context, ID3D11Texture2D *rt,
        ID3D11Texture2D *staging, DWORD expected)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    ID3D11DeviceContext_CopyResource(context, (ID3D11Resource *)staging, (ID3D11Resource *)rt);
    if (FAILED(ID3D11DeviceContext_Map(context, (ID3D11Resource *)staging, 0, D3D11_MAP_READ, 0, &mapped)))
        return FALSE;
    BOOL pass = pixels_match(mapped.pData, mapped.RowPitch, expected);
    ID3D11DeviceContext_Unmap(context, (ID3D11Resource *)staging, 0);
    return pass;
}

static const char vertex_source[] =
        "float4 main(uint id : SV_VertexID) : SV_Position {"
        " if (id == 0) return float4(-1,1,0,1);"
        " if (id == 1) return float4(3,1,0,1);"
        " return float4(-1,-3,0,1); }";
static const char pixel_source[] = "float4 main() : SV_Target {return float4(1,0,1,1);}";

static BOOL read_pixels10(ID3D10Device *device, ID3D10Texture2D *target,
        ID3D10Texture2D *staging, DWORD expected)
{
    D3D10_MAPPED_TEXTURE2D mapped;
    ID3D10Device_CopyResource(device, (ID3D10Resource *)staging, (ID3D10Resource *)target);
    if (FAILED(ID3D10Texture2D_Map(staging, 0, D3D10_MAP_READ, 0, &mapped))) return FALSE;
    BOOL match = pixels_match(mapped.pData, mapped.RowPitch, expected);
    ID3D10Texture2D_Unmap(staging, 0);
    return match;
}

BOOL test_d3d10_sdk(HMODULE module, ID3D10Device *device, BOOL version1);

static BOOL test_d3d10(BOOL version1)
{
    typedef HRESULT (WINAPI *create10_fn)(IDXGIAdapter *, D3D10_DRIVER_TYPE, HMODULE, UINT, UINT, ID3D10Device **);
    typedef HRESULT (WINAPI *create101_fn)(IDXGIAdapter *, D3D10_DRIVER_TYPE, HMODULE, UINT,
            D3D10_FEATURE_LEVEL1, UINT, ID3D10Device1 **);
    const FLOAT green[4] = {0, 1, 0, 1};
    D3D10_VIEWPORT viewport = {0, 0, WIDTH, HEIGHT, 0, 1};
    D3D10_TEXTURE2D_DESC desc = {0};
    ID3D10Device *device = NULL;
    ID3D10Device1 *device1 = NULL;
    ID3D10Texture2D *target = NULL, *staging = NULL;
    ID3D10RenderTargetView *view = NULL;
    ID3D10VertexShader *vs = NULL;
    ID3D10PixelShader *ps = NULL;
    ID3DBlob *vs_code = NULL, *ps_code = NULL;
    HMODULE module = load_runtime(version1 ? "d3d10_1.dll" : "d3d10.dll");
    HMODULE compiler = load_shader_compiler();
    HRESULT hr = E_NOINTERFACE;
    BOOL pass = FALSE;
    const char *stage = "device creation";
    if (!module) goto done;
    if (version1) {
        create101_fn create = (create101_fn)GetProcAddress(module, "D3D10CreateDevice1");
        if (!create) goto done;
        hr = create(NULL, D3D10_DRIVER_TYPE_HARDWARE, NULL, 0, D3D10_FEATURE_LEVEL_10_1,
                D3D10_1_SDK_VERSION, &device1);
        if (FAILED(hr) || !device1) goto done;
        device = (ID3D10Device *)device1;
        stage = "feature level 10_1";
        if (ID3D10Device1_GetFeatureLevel(device1) != D3D10_FEATURE_LEVEL_10_1) goto done;
    } else {
        create10_fn create = (create10_fn)GetProcAddress(module, "D3D10CreateDevice");
        if (!create) goto done;
        hr = create(NULL, D3D10_DRIVER_TYPE_HARDWARE, NULL, 0, D3D10_SDK_VERSION, &device);
        if (FAILED(hr) || !device) goto done;
    }
    stage = "render target creation";
    desc.Width = WIDTH; desc.Height = HEIGHT;
    desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BindFlags = D3D10_BIND_RENDER_TARGET;
    hr = ID3D10Device_CreateTexture2D(device, &desc, NULL, &target);
    if (FAILED(hr)) goto done;
    hr = ID3D10Device_CreateRenderTargetView(device, (ID3D10Resource *)target, NULL, &view);
    if (FAILED(hr)) goto done;
    desc.BindFlags = 0;
    desc.Usage = D3D10_USAGE_STAGING;
    desc.CPUAccessFlags = D3D10_CPU_ACCESS_READ;
    hr = ID3D10Device_CreateTexture2D(device, &desc, NULL, &staging);
    if (FAILED(hr)) goto done;
    stage = "clear and readback";
    ID3D10Device_ClearRenderTargetView(device, view, green);
    if (!read_pixels10(device, target, staging, 0xff00ff00)) goto done;
    stage = "HLSL vertex and pixel shaders";
    hr = compile_shader(compiler, vertex_source, version1 ? "vs_4_1" : "vs_4_0", &vs_code);
    if (FAILED(hr)) goto done;
    hr = compile_shader(compiler, pixel_source, version1 ? "ps_4_1" : "ps_4_0", &ps_code);
    if (FAILED(hr)) goto done;
    hr = ID3D10Device_CreateVertexShader(device, ID3D10Blob_GetBufferPointer(vs_code),
            ID3D10Blob_GetBufferSize(vs_code), &vs);
    if (FAILED(hr)) goto done;
    hr = ID3D10Device_CreatePixelShader(device, ID3D10Blob_GetBufferPointer(ps_code),
            ID3D10Blob_GetBufferSize(ps_code), &ps);
    if (FAILED(hr)) goto done;
    stage = "triangle and readback";
    ID3D10Device_OMSetRenderTargets(device, 1, &view, NULL);
    ID3D10Device_RSSetViewports(device, 1, &viewport);
    ID3D10Device_IASetPrimitiveTopology(device, D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D10Device_VSSetShader(device, vs);
    ID3D10Device_PSSetShader(device, ps);
    ID3D10Device_Draw(device, 3, 0);
    pass = read_pixels10(device, target, staging, 0xffff00ff);
done:
    report(version1 ? "Direct3D 10.1 SM4.1" : "Direct3D 10 SM4.0");
    if (pass) report(" clear, shader draw and 12-pixel readback PASS\r\n");
    else { report(" FAIL at "); report(stage); report(", HRESULT "); report_hex((DWORD)hr); report("\r\n"); }
    if (device) {
        BOOL sdk = test_d3d10_sdk(module, device, version1);
        pass = pass && sdk;
        ID3D10Device_ClearState(device);
    }
    if (vs) ID3D10VertexShader_Release(vs);
    if (ps) ID3D10PixelShader_Release(ps);
    if (ps_code) ID3D10Blob_Release(ps_code);
    if (vs_code) ID3D10Blob_Release(vs_code);
    if (view) ID3D10RenderTargetView_Release(view);
    if (staging) ID3D10Texture2D_Release(staging);
    if (target) ID3D10Texture2D_Release(target);
    if (device) ID3D10Device_Release(device);
    if (compiler) FreeLibrary(compiler);
    if (module) FreeLibrary(module);
    return pass;
}

static BOOL draw_triangle11(ID3D11Device *device, ID3D11DeviceContext *context, ID3D11RenderTargetView *view)
{
    D3D11_VIEWPORT viewport = {0, 0, WIDTH, HEIGHT, 0, 1};
    ID3D11VertexShader *vs = NULL;
    ID3D11PixelShader *ps = NULL;
    ID3DBlob *vs_code = NULL, *ps_code = NULL;
    HMODULE compiler = load_shader_compiler();
    BOOL pass = FALSE;
    if (FAILED(compile_shader(compiler, vertex_source, "vs_4_0", &vs_code)) ||
            FAILED(compile_shader(compiler, pixel_source, "ps_4_0", &ps_code))) goto done;
    if (FAILED(ID3D11Device_CreateVertexShader(device, ID3D10Blob_GetBufferPointer(vs_code),
            ID3D10Blob_GetBufferSize(vs_code), NULL, &vs)) ||
            FAILED(ID3D11Device_CreatePixelShader(device, ID3D10Blob_GetBufferPointer(ps_code),
            ID3D10Blob_GetBufferSize(ps_code), NULL, &ps))) goto done;
    ID3D11DeviceContext_OMSetRenderTargets(context, 1, &view, NULL);
    ID3D11DeviceContext_RSSetViewports(context, 1, &viewport);
    ID3D11DeviceContext_IASetPrimitiveTopology(context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_VSSetShader(context, vs, NULL, 0);
    ID3D11DeviceContext_PSSetShader(context, ps, NULL, 0);
    ID3D11DeviceContext_Draw(context, 3, 0);
    pass = TRUE;
done:
    if (ps) ID3D11PixelShader_Release(ps);
    if (vs) ID3D11VertexShader_Release(vs);
    if (ps_code) ID3D10Blob_Release(ps_code);
    if (vs_code) ID3D10Blob_Release(vs_code);
    if (compiler) FreeLibrary(compiler);
    return pass;
}

static BOOL compute_shader11(ID3D11Device *device, ID3D11DeviceContext *context)
{
    static const char source[] =
        "RWBuffer<uint> output_values : register(u0);"
        "[numthreads(4,1,1)] void main(uint3 tid : SV_DispatchThreadID) {"
        "uint i = tid.x; output_values[i] = i*i+17;}";
    enum { count = 32 };
    HMODULE compiler = load_shader_compiler();
    ID3DBlob *bytecode = NULL;
    ID3D11ComputeShader *shader = NULL;
    ID3D11Buffer *output = NULL, *readback = NULL;
    ID3D11UnorderedAccessView *view = NULL, *unbound = NULL;
    D3D11_BUFFER_DESC buffer = {0};
    D3D11_UNORDERED_ACCESS_VIEW_DESC desc = {0};
    D3D11_MAPPED_SUBRESOURCE mapped;
    const char *stage = "HLSL compilation";
    BOOL passed = FALSE;
    HRESULT hr = compile_shader(compiler, source, "cs_5_0", &bytecode);
    if (FAILED(hr)) goto done;
    stage = "compute shader creation";
    hr = ID3D11Device_CreateComputeShader(device, ID3D10Blob_GetBufferPointer(bytecode),
            ID3D10Blob_GetBufferSize(bytecode), NULL, &shader);
    if (FAILED(hr)) goto done;
    buffer.ByteWidth = count * sizeof(UINT);
    buffer.Usage = D3D11_USAGE_DEFAULT;
    buffer.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    stage = "output buffer creation";
    hr = ID3D11Device_CreateBuffer(device, &buffer, NULL, &output);
    if (FAILED(hr)) goto done;
    desc.Format = DXGI_FORMAT_R32_UINT;
    desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    desc.Buffer.NumElements = count;
    stage = "unordered access view creation";
    hr = ID3D11Device_CreateUnorderedAccessView(device, (ID3D11Resource *)output, &desc, &view);
    if (FAILED(hr)) goto done;
    buffer.Usage = D3D11_USAGE_STAGING;
    buffer.BindFlags = 0;
    buffer.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stage = "readback buffer creation";
    hr = ID3D11Device_CreateBuffer(device, &buffer, NULL, &readback);
    if (FAILED(hr)) goto done;
    ID3D11DeviceContext_CSSetShader(context, shader, NULL, 0);
    ID3D11DeviceContext_CSSetUnorderedAccessViews(context, 0, 1, &view, NULL);
    ID3D11DeviceContext_Dispatch(context, count / 4, 1, 1);
    ID3D11DeviceContext_CSSetUnorderedAccessViews(context, 0, 1, &unbound, NULL);
    ID3D11DeviceContext_CSSetShader(context, NULL, NULL, 0);
    ID3D11DeviceContext_CopyResource(context, (ID3D11Resource *)readback, (ID3D11Resource *)output);
    stage = "buffer mapping";
    hr = ID3D11DeviceContext_Map(context, (ID3D11Resource *)readback, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) goto done;
    stage = "32-value comparison";
    passed = TRUE;
    for (UINT i = 0; i < count; ++i)
        if (((const UINT *)mapped.pData)[i] != i*i+17) passed = FALSE;
    ID3D11DeviceContext_Unmap(context, (ID3D11Resource *)readback, 0);
done:
    if (!passed) { report("Failed at "); report(stage); report(", HRESULT "); report_hex((DWORD)hr); report("\r\n"); }
    if (readback) ID3D11Buffer_Release(readback);
    if (view) ID3D11UnorderedAccessView_Release(view);
    if (output) ID3D11Buffer_Release(output);
    if (shader) ID3D11ComputeShader_Release(shader);
    if (bytecode) ID3D10Blob_Release(bytecode);
    if (compiler) FreeLibrary(compiler);
    return passed;
}

static BOOL test_d3d11(BOOL compute_only)
{
    static const float green[] = {0.0f, 1.0f, 0.0f, 1.0f};
    D3D11_TEXTURE2D_DESC desc = {0};
    D3D_FEATURE_LEVEL level;
    ID3D11Device *device = NULL;
    ID3D11DeviceContext *context = NULL;
    ID3D11Texture2D *rt = NULL, *staging = NULL;
    ID3D11RenderTargetView *view = NULL;
    HMODULE module = load_runtime("d3d11.dll");
    create11_fn create;
    HRESULT hr = E_FAIL;
    BOOL pass = FALSE, draw_pass = FALSE;
    if (!module)
        return FALSE;
    create = (create11_fn)GetProcAddress(module, "D3D11CreateDevice");
    if (!create) goto done;
    hr = create(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION,
            &device, &level, &context);
    if (FAILED(hr)) goto done;
    report("D3D11 feature level ");
    report_hex(level);
    report("\r\n");
    if (compute_only) {
        if (level >= D3D_FEATURE_LEVEL_11_0) pass = compute_shader11(device, context);
        else report("Shader model 5 compute requires feature level 11_0\r\n");
        goto done;
    }
    desc.Width = WIDTH;
    desc.Height = HEIGHT;
    desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    hr = ID3D11Device_CreateTexture2D(device, &desc, NULL, &rt);
    if (FAILED(hr)) goto done;
    hr = ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource *)rt, NULL, &view);
    if (FAILED(hr)) goto done;
    ID3D11DeviceContext_ClearRenderTargetView(context, view, green);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    hr = ID3D11Device_CreateTexture2D(device, &desc, NULL, &staging);
    if (FAILED(hr)) goto done;
    pass = read_pixels11(context, rt, staging, 0xff00ff00u);
    draw_pass = draw_triangle11(device, context, view) && read_pixels11(context, rt, staging, 0xffff00ffu);
done:
    if (staging) ID3D11Texture2D_Release(staging);
    if (view) ID3D11RenderTargetView_Release(view);
    if (rt) ID3D11Texture2D_Release(rt);
    if (context) ID3D11DeviceContext_Release(context);
    if (device) ID3D11Device_Release(device);
    FreeLibrary(module);
    if (compute_only) {
        report(pass ? "D3D11 cs_5_0 dispatch and 32 results PASS\r\n" : "D3D11 cs_5_0 dispatch FAIL\r\n");
        return pass;
    }
    report(pass ? "D3D11 clear/readback PASS\r\n" : "D3D11 clear/readback FAIL, HRESULT ");
    if (!pass) { report_hex((DWORD)hr); report("\r\n"); }
    report(draw_pass ? "D3D11 shader draw/readback PASS\r\n" : "D3D11 shader draw/readback FAIL\r\n");
    return pass && draw_pass;
}

void WINAPI render_entry(void)
{
    HWND window;
    BOOL pass9 = TRUE, pass11 = TRUE;
    const char *arg = GetCommandLineA();
    if (*arg == '"') {
        ++arg;
        while (*arg && *arg != '"') ++arg;
        if (*arg) ++arg;
    } else {
        while (*arg && *arg != ' ' && *arg != '\t') ++arg;
    }
    while (*arg == ' ' || *arg == '\t') ++arg;
    const char *end = arg;
    while (*end && *end != ' ' && *end != '\t') ++end;
    BOOL only10 = end - arg == 2 && arg[0] == '1' && arg[1] == '0';
    BOOL only101 = end - arg == 4 && arg[0] == '1' && arg[1] == '0' && arg[2] == '.' && arg[3] == '1';
    BOOL only9 = end - arg == 1 && arg[0] == '9';
    BOOL only11 = end - arg == 2 && arg[0] == '1' && arg[1] == '1';
    BOOL compute_only = end - arg == 2 && arg[0] == 'c' && arg[1] == 's';
    while (*end == ' ' || *end == '\t') ++end;
    if (*end || (*arg && !only9 && !only10 && !only101 && !only11 && !compute_only)) {
        report("Usage: wxrender.exe [9|10|10.1|11|cs]\r\n");
        ExitProcess(2);
    }
    initialize_test();
    if (only10 || only101) ExitProcess(test_d3d10(only101) ? 0 : 1);
    window = CreateWindowExA(0, "STATIC", "WINDX render test", WS_POPUP,
            0, 0, WIDTH, HEIGHT, NULL, NULL, GetModuleHandleA(NULL), NULL);
    if (!window) {
        report("FAIL creating a test window\r\n");
        ExitProcess(1);
    }
    if (!only11 && !compute_only) pass9 = test_d3d9(window);
    if (!only9) pass11 = test_d3d11(compute_only);
    DestroyWindow(window);
    ExitProcess(pass9 && pass11 ? 0 : 1);
}
