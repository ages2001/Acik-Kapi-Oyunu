#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <d3d8.h>
#include "test_output.h"

enum { WIDTH = 4, HEIGHT = 3 };

static const DWORD clear_color = 0xff692550;

static const DWORD vertex_declaration[] = {
    D3DVSD_STREAM(0),
    D3DVSD_REG(D3DVSDE_POSITION, D3DVSDT_FLOAT4),
    D3DVSD_REG(D3DVSDE_TEXCOORD0, D3DVSDT_FLOAT2),
    D3DVSD_END()
};

/* The encoded operands are the documented shader-model 1 bytecode accepted by
 * CreateVertexShader and CreatePixelShader. The comments give the assembly. */
static const DWORD vertex_11[] = {
    D3DVS_VERSION(1,1),
    D3DSIO_MUL, 0xc00f0000, 0x90e40000, 0xa0e40000,  /* mul oPos, v0, c0 */
    D3DSIO_MOV, 0xe0030000, 0x90e40007,              /* mov oT0.xy, v7 */
    D3DSIO_MOV, 0xe0070001, 0xa0e40001,              /* mov oT1.xyz, c1 */
    D3DSIO_MOV, 0xe0070002, 0xa0e40002,              /* mov oT2.xyz, c2 */
    D3DVS_END()
};
static const DWORD pixel_11[] = {
    D3DPS_VERSION(1,1),
    D3DSIO_MOV, 0x800f0000, 0xa0e40000,              /* mov r0, c0 */
    D3DPS_END()
};
static const DWORD pixel_12[] = {
    D3DPS_VERSION(1,2),
    D3DSIO_TEX, 0xb00f0000,                          /* tex t0 */
    D3DSIO_TEXREG2RGB, 0xb00f0001, 0xb0e40000,       /* texreg2rgb t1, t0 */
    D3DSIO_MOV, 0x800f0000, 0xb0e40001,              /* mov r0, t1 */
    D3DPS_END()
};
static const DWORD pixel_13[] = {
    D3DPS_VERSION(1,3),
    D3DSIO_TEX, 0xb00f0000,                          /* tex t0 */
    D3DSIO_TEXM3x2PAD, 0xb00f0001, 0xb0e40000,       /* texm3x2pad t1, t0 */
    D3DSIO_TEXM3x2DEPTH, 0xb00f0002, 0xb0e40000,     /* texm3x2depth t2, t0 */
    D3DSIO_MOV, 0x800f0000, 0xa0e40000,              /* mov r0, c0 */
    D3DPS_END()
};
static const DWORD texture_14[] = {
    D3DPS_VERSION(1,4),
    D3DSIO_TEX, 0x800f0000, 0xb0e40000,              /* texld r0, t0 */
    D3DPS_END()
};
static const DWORD pixel_14[] = {
    D3DPS_VERSION(1,4),
    D3DSIO_TEX, 0x800f0000, 0xb0e40000,              /* texld r0, t0 */
    D3DSIO_PHASE,
    D3DSIO_TEX, 0x800f0001, 0x80e40000,              /* texld r1, r0 */
    D3DSIO_MUL, 0x800f0000, 0x80e40001, 0xa0e40000,  /* mul r0, r1, c0 */
    D3DPS_END()
};

static BOOL succeeded(const char *operation, HRESULT hr)
{
    if (SUCCEEDED(hr)) return TRUE;
    report(operation);
    report(" failed ");
    report_hex((DWORD)hr);
    report("\r\n");
    return FALSE;
}

static BOOL read_target(IDirect3DSurface8 *target, DWORD expected)
{
    D3DLOCKED_RECT lock;
    if (!succeeded("LockRect(target)", IDirect3DSurface8_LockRect(target, &lock, NULL, D3DLOCK_READONLY)))
        return FALSE;
    BOOL ok = lock.pBits && lock.Pitch >= WIDTH * (int)sizeof(DWORD);
    for (unsigned y = 0; ok && y < HEIGHT; ++y) {
        const DWORD *row = (const DWORD *)((const BYTE *)lock.pBits + y * lock.Pitch);
        for (unsigned x = 0; x < WIDTH; ++x) {
            if (row[x] != expected) {
                report("Pixel ("); report_hex(x); report(","); report_hex(y); report(") expected "); report_hex(expected); report(" got ");
                report_hex(row[x]); report("\r\n");
                ok = FALSE;
                break;
            }
        }
    }
    return succeeded("UnlockRect(target)", IDirect3DSurface8_UnlockRect(target)) && ok;
}

static BOOL fill_texture(IDirect3DDevice8 *device, unsigned width, unsigned height,
        const DWORD *pixels, IDirect3DTexture8 **texture)
{
    D3DLOCKED_RECT lock;
    if (!succeeded("CreateTexture", IDirect3DDevice8_CreateTexture(device, width, height, 1, 0,
            D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, texture))) return FALSE;
    if (!succeeded("LockRect(texture)", IDirect3DTexture8_LockRect(*texture, 0, &lock, NULL, 0))) return FALSE;
    BOOL ok = lock.pBits && lock.Pitch >= (int)(width * sizeof(*pixels));
    for (unsigned y = 0; ok && y < height; ++y) {
        DWORD *row = (DWORD *)((BYTE *)lock.pBits + y * lock.Pitch);
        for (unsigned x = 0; x < width; ++x) row[x] = pixels[y * width + x];
    }
    return succeeded("UnlockRect(texture)", IDirect3DTexture8_UnlockRect(*texture, 0)) && ok;
}

static BOOL draw(IDirect3DDevice8 *device, IDirect3DSurface8 *target, const DWORD *bytecode,
        const float *constant, float initial_depth, DWORD expected)
{
    static const float vertices[][6] = {
        {-2,  2, 0.5f, 1, 0.25f, 0.25f},
        {16,  2, 0.5f, 1, 0.25f, 0.25f},
        {-2,-16, 0.5f, 1, 0.25f, 0.25f}
    };
    DWORD shader = 0;
    DWORD clear_flags = initial_depth >= 0 ? D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER : D3DCLEAR_TARGET;
    BOOL ok = FALSE;
    if (!succeeded("CreatePixelShader", IDirect3DDevice8_CreatePixelShader(device, bytecode, &shader))) goto done;
    if (!succeeded("SetPixelShader", IDirect3DDevice8_SetPixelShader(device, shader))) goto done;
    if (!succeeded("SetPixelShaderConstant", IDirect3DDevice8_SetPixelShaderConstant(device, 0, constant, 1))) goto done;
    if (!succeeded("Clear", IDirect3DDevice8_Clear(device, 0, NULL, clear_flags, clear_color, initial_depth, 0)) ||
            !read_target(target, clear_color)) goto done;
    if (!succeeded("BeginScene", IDirect3DDevice8_BeginScene(device))) goto done;
    HRESULT drawn = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLELIST, 1, vertices, sizeof(vertices[0]));
    HRESULT ended = IDirect3DDevice8_EndScene(device);
    ok = succeeded("DrawPrimitiveUP", drawn) && succeeded("EndScene", ended) && read_target(target, expected);
done:
    if (shader) {
        IDirect3DDevice8_SetPixelShader(device, 0);
        IDirect3DDevice8_DeletePixelShader(device, shader);
    }
    return ok;
}

static BOOL test_depth_shader(IDirect3DDevice8 *device, IDirect3DSurface8 *target, const float *color)
{
    IDirect3DSurface8 *depth = NULL;
    BOOL ok = FALSE;
    if (!succeeded("CreateDepthStencilSurface", IDirect3DDevice8_CreateDepthStencilSurface(device,
            WIDTH, HEIGHT, D3DFMT_D16, D3DMULTISAMPLE_NONE, &depth))) return FALSE;
    if (!succeeded("SetRenderTarget(depth)", IDirect3DDevice8_SetRenderTarget(device, target, depth)) ||
            !succeeded("Enable depth", IDirect3DDevice8_SetRenderState(device, D3DRS_ZENABLE, D3DZB_TRUE)) ||
            !succeeded("Depth comparison", IDirect3DDevice8_SetRenderState(device, D3DRS_ZFUNC, D3DCMP_LESSEQUAL)))
        goto done;
    /* The matrix rows select G and R. G/R is 64/191, between 0.25 and
     * 0.4. An implementation using the incoming vertex depth (0.5) fails. */
    ok = draw(device, target, pixel_13, color, 0.4f, 0xff00ffff) &&
            draw(device, target, pixel_13, color, 0.25f, clear_color);
done:
    IDirect3DDevice8_SetRenderState(device, D3DRS_ZENABLE, D3DZB_FALSE);
    IDirect3DDevice8_SetRenderTarget(device, target, NULL);
    IDirect3DSurface8_Release(depth);
    return ok;
}

static BOOL test_shaders(IDirect3DDevice8 *device, IDirect3DSurface8 *target)
{
    static const float vertex_constants[][4] = {{1,1,1,1}, {0,1,0,0}, {1,0,0,0}};
    static const float teal[] = {0,1,1,1};
    static const DWORD coordinates[] = {0xffbf4000};
    static const DWORD colors[] = {0xffac1753, 0xff25479b, 0xffed1d16, 0xff7519a5};
    IDirect3DTexture8 *lookup = NULL, *palette = NULL;
    DWORD vertex = 0;
    BOOL sm11 = FALSE, sm12 = FALSE, sm13 = FALSE, sm14 = FALSE;
    if (!succeeded("CreateVertexShader", IDirect3DDevice8_CreateVertexShader(device, vertex_declaration,
            vertex_11, &vertex, 0))) goto done;
    if (!succeeded("SetVertexShader", IDirect3DDevice8_SetVertexShader(device, vertex)) ||
            !succeeded("SetVertexShaderConstant", IDirect3DDevice8_SetVertexShaderConstant(device, 0, vertex_constants, 3)) ||
            !succeeded("SetRenderState(CULLMODE)", IDirect3DDevice8_SetRenderState(device, D3DRS_CULLMODE, D3DCULL_NONE)))
        goto done;

    sm11 = draw(device, target, pixel_11, teal, -1, 0xff00ffff);
    report(sm11 ? "Direct3D 8 VS1.1/PS1.1 triangle and 12-pixel readback PASS\r\n" :
            "Direct3D 8 VS1.1/PS1.1 draw FAIL\r\n");

    if (!fill_texture(device, 1, 1, coordinates, &lookup) || !fill_texture(device, 2, 2, colors, &palette))
        goto done;
    for (unsigned stage = 0; stage < 2; ++stage) {
        if (!succeeded("SetTexture", IDirect3DDevice8_SetTexture(device, stage,
                (IDirect3DBaseTexture8 *)(stage ? palette : lookup))) ||
                !succeeded("SetTextureStageState(MINFILTER)", IDirect3DDevice8_SetTextureStageState(device,
                    stage, D3DTSS_MINFILTER, D3DTEXF_POINT)) ||
                !succeeded("SetTextureStageState(MAGFILTER)", IDirect3DDevice8_SetTextureStageState(device,
                    stage, D3DTSS_MAGFILTER, D3DTEXF_POINT))) goto done;
    }
    sm12 = draw(device, target, pixel_12, teal, -1, colors[1]);
    report(sm12 ? "Direct3D 8.1 PS1.2 texreg2rgb lookup and 12-pixel readback PASS\r\n" :
            "Direct3D 8.1 PS1.2 dependent lookup FAIL\r\n");
    sm13 = test_depth_shader(device, target, teal);
    report(sm13 ? "Direct3D 8.1 PS1.3 texm3x2depth acceptance and rejection PASS\r\n" :
            "Direct3D 8.1 PS1.3 programmed depth FAIL\r\n");
    /* The first texture selects the upper-right texel in a four-color second
     * texture. The pixel constant removes its red channel after the lookup. */
    BOOL sampled = draw(device, target, texture_14, teal, -1, coordinates[0]);
    report(sampled ? "PS1.4 texture coordinate sample PASS\r\n" : "PS1.4 texture coordinate sample FAIL\r\n");
    sm14 = draw(device, target, pixel_14, teal, -1, 0xff00479b) && sampled;
    BOOL independent = succeeded("SetTexture(palette)", IDirect3DDevice8_SetTexture(device, 0,
            (IDirect3DBaseTexture8 *)palette)) && draw(device, target, texture_14, teal, -1, colors[0]);
    report(independent ? "2D palette sampling PASS\r\n" : "2D palette sampling FAIL\r\n");
    sm14 = sm14 && independent;
    report(sm14 ? "Direct3D 8.1 PS1.4 phase, dependent texture lookup and 12-pixel readback PASS\r\n" :
            "Direct3D 8.1 PS1.4 dependent lookup FAIL\r\n");
done:
    IDirect3DDevice8_SetTexture(device, 0, NULL);
    IDirect3DDevice8_SetTexture(device, 1, NULL);
    if (palette) IDirect3DTexture8_Release(palette);
    if (lookup) IDirect3DTexture8_Release(lookup);
    if (vertex) {
        IDirect3DDevice8_SetVertexShader(device, D3DFVF_XYZ);
        IDirect3DDevice8_DeleteVertexShader(device, vertex);
    }
    return sm11 && sm12 && sm13 && sm14;
}

void WINAPI d3d81_entry(void)
{
    typedef IDirect3D8 *(WINAPI *create_fn)(UINT);
    char path[MAX_PATH];
    HMODULE runtime = NULL;
    HWND window = NULL;
    IDirect3D8 *api = NULL;
    IDirect3DDevice8 *device = NULL;
    IDirect3DSurface8 *target = NULL;
    D3DPRESENT_PARAMETERS params = {0};
    D3DDISPLAYMODE mode;
    D3DCAPS8 caps;
    BOOL ok = FALSE;
    initialize_test();
    DWORD len = GetModuleFileNameA(NULL, path, MAX_PATH);
    if (!len || len >= MAX_PATH) goto done;
    while (len && path[len - 1] != '\\') --len;
    if (!len || len + sizeof("d3d8.dll") > MAX_PATH) goto done;
    lstrcpyA(path + len, "d3d8.dll");
    runtime = LoadLibraryA(path);
    if (!runtime) { report("Could not load "); report(path); report("\r\n"); goto done; }
    report("d3d8.dll loaded from "); report(path); report("\r\n");
    create_fn create = (create_fn)GetProcAddress(runtime, "Direct3DCreate8");
    if (!create || !(api = create(D3D_SDK_VERSION))) goto done;
    window = CreateWindowExA(0, "STATIC", "WINDX Direct3D 8.1 test", WS_POPUP, 0, 0, WIDTH, HEIGHT,
            NULL, NULL, GetModuleHandleA(NULL), NULL);
    if (!window || !succeeded("GetAdapterDisplayMode", IDirect3D8_GetAdapterDisplayMode(api, 0, &mode))) goto done;
    params.Windowed = TRUE;
    params.hDeviceWindow = window;
    params.SwapEffect = D3DSWAPEFFECT_DISCARD;
    params.BackBufferWidth = WIDTH;
    params.BackBufferHeight = HEIGHT;
    params.BackBufferFormat = mode.Format;
    if (!succeeded("CreateDevice", IDirect3D8_CreateDevice(api, 0, D3DDEVTYPE_HAL, window,
            D3DCREATE_HARDWARE_VERTEXPROCESSING, &params, &device))) goto done;
    if (!succeeded("GetDeviceCaps", IDirect3DDevice8_GetDeviceCaps(device, &caps))) goto done;
    report("Device vertex shader version "); report_hex(caps.VertexShaderVersion);
    report(", pixel shader version "); report_hex(caps.PixelShaderVersion); report("\r\n");
    if (!succeeded("CreateRenderTarget", IDirect3DDevice8_CreateRenderTarget(device, WIDTH, HEIGHT,
            D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, TRUE, &target)) ||
            !succeeded("SetRenderTarget", IDirect3DDevice8_SetRenderTarget(device, target, NULL))) goto done;
    ok = test_shaders(device, target);
done:
    if (target) IDirect3DSurface8_Release(target);
    if (device) IDirect3DDevice8_Release(device);
    if (api) IDirect3D8_Release(api);
    if (window) DestroyWindow(window);
    if (runtime) FreeLibrary(runtime);
    ExitProcess(ok ? 0 : 1);
}
