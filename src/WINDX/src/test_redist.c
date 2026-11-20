#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <d3dcompiler.h>
#include <d3d11shader.h>
#include <d3dx9math.h>
#include "test_output.h"

typedef D3DXMATRIX *(WINAPI *translation_fn)(D3DXMATRIX *, FLOAT, FLOAT, FLOAT);
typedef D3DXVECTOR3 *(WINAPI *transform_fn)(D3DXVECTOR3 *, const D3DXVECTOR3 *, const D3DXMATRIX *);
typedef HRESULT (WINAPI *assemble_fn)(const void *, SIZE_T, LPCSTR, const D3D_SHADER_MACRO *,
        ID3DInclude *, UINT, ID3DBlob **, ID3DBlob **);
typedef HRESULT (WINAPI *compile_fn)(const void *, SIZE_T, LPCSTR, const D3D_SHADER_MACRO *,
        ID3DInclude *, LPCSTR, LPCSTR, UINT, UINT, ID3DBlob **, ID3DBlob **);
typedef HRESULT (WINAPI *reflect_fn)(const void *, SIZE_T, REFIID, void **);

/* d3dx11core.idl defines this three-method interface without IUnknown. */
struct data_loader;
struct data_loader_methods {
    HRESULT (WINAPI *load)(struct data_loader *);
    HRESULT (WINAPI *decompress)(struct data_loader *, void **, SIZE_T *);
    HRESULT (WINAPI *destroy)(struct data_loader *);
};
struct data_loader { const struct data_loader_methods *methods; };
typedef HRESULT (WINAPI *memory_loader_fn)(const void *, SIZE_T, struct data_loader **);

static BOOL test_memory_loader(const char *name, const char *entry)
{
    static const BYTE source[] = {0, 255, 1, 0, 37};
    HMODULE module = LoadLibraryA(name);
    struct data_loader *loader = NULL;
    SIZE_T i, size = 0;
    void *data = NULL;
    if (!module) return FALSE;
    memory_loader_fn create = (memory_loader_fn)GetProcAddress(module, entry);
    BOOL pass = create && SUCCEEDED(create(source, sizeof(source), &loader)) && loader &&
            SUCCEEDED(loader->methods->load(loader)) &&
            SUCCEEDED(loader->methods->decompress(loader, &data, &size)) && data && size == sizeof(source);
    for (i = 0; pass && i < size; ++i)
        pass = ((const BYTE *)data)[i] == source[i];
    if (loader) loader->methods->destroy(loader);
    FreeLibrary(module);
    return pass;
}

static BOOL test_math(UINT version)
{
    char name[] = "d3dx9_00.dll";
    name[6] = '0' + version / 10;
    name[7] = '0' + version % 10;
    HMODULE module = LoadLibraryA(name);
    D3DXMATRIX matrix;
    D3DXVECTOR3 point = {1, 2, 3}, result;
    if (!module) return FALSE;
    translation_fn translate = (translation_fn)GetProcAddress(module, "D3DXMatrixTranslation");
    transform_fn transform = (transform_fn)GetProcAddress(module, "D3DXVec3TransformCoord");
    typedef BOOL (WINAPI *check_version_fn)(UINT, UINT);
    check_version_fn check = (check_version_fn)GetProcAddress(module, "D3DXCheckVersion");
    BOOL pass = check && check(D3D_SDK_VERSION, version) && !check(D3D_SDK_VERSION, version + 1) &&
            translate && transform && translate(&matrix, 4, -5, 2) == &matrix &&
            transform(&result, &point, &matrix) == &result &&
            result.x == 5 && result.y == -3 && result.z == 5;
    FreeLibrary(module);
    return pass;
}

static BOOL test_assembler(HMODULE compiler)
{
    const char source[] = "ps_2_0\ndef c0, 1, 0, 0, 1\nmov r0, c0\nmov oC0, r0\n";
    assemble_fn assemble = (assemble_fn)GetProcAddress(compiler, "D3DAssemble");
    ID3DBlob *code = NULL;
    BOOL pass = assemble && SUCCEEDED(assemble(source, sizeof(source) - 1, NULL,
            NULL, NULL, 0, &code, NULL)) && code && ID3D10Blob_GetBufferSize(code) >= 8 &&
            *(const DWORD *)ID3D10Blob_GetBufferPointer(code) == 0xffff0200u;
    if (code) ID3D10Blob_Release(code);
    return pass;
}

static BOOL test_reflection(HMODULE compiler)
{
    /* The June 2010 reflection interface has its own IID. */
    static const GUID iid = {0x0a233719, 0x3960, 0x4578, {0x9d,0x7c,0x20,0x3b,0x8b,0x1d,0x9c,0xc1}};
    const char source[] = "float4 main(float4 p : SV_Position) : SV_Target { return p; }";
    compile_fn compile = (compile_fn)GetProcAddress(compiler, "D3DCompile");
    reflect_fn reflect = (reflect_fn)GetProcAddress(compiler, "D3DReflect");
    ID3DBlob *code = NULL;
    ID3D11ShaderReflection *reflection = NULL;
    D3D11_SHADER_DESC desc;
    BOOL pass = compile && reflect && SUCCEEDED(compile(source, sizeof(source) - 1, NULL,
            NULL, NULL, "main", "ps_5_0", 0, 0, &code, NULL)) && code &&
            SUCCEEDED(reflect(ID3D10Blob_GetBufferPointer(code), ID3D10Blob_GetBufferSize(code),
                    &iid, (void **)&reflection)) && reflection &&
            SUCCEEDED(reflection->lpVtbl->GetDesc(reflection, &desc)) &&
            desc.InputParameters == 1 && desc.OutputParameters == 1;
    if (reflection) reflection->lpVtbl->Release(reflection);
    if (code) ID3D10Blob_Release(code);
    return pass;
}

void WINAPI redist_entry(void)
{
    initialize_test();
    HMODULE compiler = LoadLibraryA("d3dcompiler_43.dll");
    BOOL math = TRUE;
    for (UINT version = 24; version <= 43; ++version) {
        if (!test_math(version)) {
            report("D3DX9 version/math FAIL, SDK version 0x");
            report_hex(version);
            report("\r\n");
            math = FALSE;
        }
    }
    if (math) report("D3DX9_24 through D3DX9_43 version/math PASS\r\n");
    BOOL assembly = compiler && test_assembler(compiler);
    report(assembly ? "D3DAssemble ps_2_0 PASS\r\n" : "D3DAssemble ps_2_0 FAIL\r\n");
    BOOL reflection = compiler && test_reflection(compiler);
    report(reflection ? "D3DReflect signatures PASS\r\n" : "D3DReflect signatures FAIL\r\n");
    BOOL memory10 = TRUE;
    for (UINT version = 33; version <= 43; ++version) {
        char name[] = "d3dx10_00.dll";
        name[7] = '0' + version / 10;
        name[8] = '0' + version % 10;
        if (!test_memory_loader(name, "D3DX10CreateAsyncMemoryLoader")) {
            report(name); report(" memory loader FAIL\r\n"); memory10 = FALSE;
        }
    }
    if (memory10) report("D3DX10_33 through D3DX10_43 memory loaders PASS\r\n");
    BOOL memory42 = test_memory_loader("d3dx11_42.dll", "D3DX11CreateAsyncMemoryLoader");
    BOOL memory43 = test_memory_loader("d3dx11_43.dll", "D3DX11CreateAsyncMemoryLoader");
    BOOL memory = memory42 && memory43;
    report(memory ? "D3DX11_42 and D3DX11_43 memory loaders PASS\r\n" : "D3DX11_42 and D3DX11_43 memory loaders FAIL\r\n");
    if (compiler) FreeLibrary(compiler);
    ExitProcess(math && assembly && reflection && memory10 && memory ? 0 : 1);
}
