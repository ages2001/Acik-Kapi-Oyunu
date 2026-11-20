#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <d3dcompiler.h>

typedef HRESULT (WINAPI *compile_fn)(LPCVOID, SIZE_T, LPCSTR,
        const D3D_SHADER_MACRO *, ID3DInclude *, LPCSTR, LPCSTR, UINT, UINT,
        ID3DBlob **, ID3DBlob **);
typedef HRESULT (WINAPI *disassemble_fn)(LPCVOID, SIZE_T, UINT, LPCSTR, ID3DBlob **);
#include "test_output.h"

static HMODULE load_compiler(void)
{
    char path[MAX_PATH];
    DWORD length = GetModuleFileNameA(NULL, path, sizeof(path));
    HMODULE module;

    if (!length || length >= sizeof(path))
        return NULL;
    while (length && path[length - 1] != '\\')
        --length;
    if (length + sizeof("d3dcompiler_43.dll") > sizeof(path))
        return NULL;
    lstrcpyA(path + length, "d3dcompiler_43.dll");
    module = LoadLibraryA(path);
    if (!module) {
        /* NT 3.1 FAT volumes expose only 8.3 names. */
        lstrcpyA(path + length, "wxcomp43.dll");
        module = LoadLibraryA(path);
    }
    if (module) {
        report("Loaded ");
        report(path);
        report("\r\n");
    }
    return module;
}

static BOOL contains_text(ID3DBlob *blob, const char *token)
{
    const char *bytes = ID3D10Blob_GetBufferPointer(blob);
    SIZE_T size = ID3D10Blob_GetBufferSize(blob);
    SIZE_T length = lstrlenA(token);
    SIZE_T i, j;
    for (i = 0; length <= size - i; ++i) {
        for (j = 0; j < length && bytes[i + j] == token[j]; ++j) {}
        if (j == length)
            return TRUE;
    }
    return FALSE;
}

static BOOL test_profile(compile_fn compile, disassemble_fn disassemble,
        const char *profile, const char *source, DWORD signature)
{
    ID3DBlob *code = NULL, *messages = NULL, *assembly = NULL;
    BOOL pass = FALSE;
    HRESULT hr;
    report("Compiling ");
    report(profile);
    report("\r\n");
    hr = compile(source, lstrlenA(source), "test.hlsl", NULL, NULL,
            "main", profile, 0, 0, &code, &messages);
    if (SUCCEEDED(hr) && code && ID3D10Blob_GetBufferSize(code) >= sizeof(DWORD)
            && *(const DWORD *)ID3D10Blob_GetBufferPointer(code) == signature) {
        hr = disassemble(ID3D10Blob_GetBufferPointer(code), ID3D10Blob_GetBufferSize(code),
                0, NULL, &assembly);
        pass = SUCCEEDED(hr) && assembly && contains_text(assembly, profile);
    }
    if (!pass && messages) {
        DWORD written;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), ID3D10Blob_GetBufferPointer(messages),
                (DWORD)ID3D10Blob_GetBufferSize(messages), &written, NULL);
    }
    if (assembly) ID3D10Blob_Release(assembly);
    if (messages) ID3D10Blob_Release(messages);
    if (code) ID3D10Blob_Release(code);
    report(profile);
    report(pass ? " compile and disassemble PASS\r\n" : " compile or disassemble FAIL\r\n");
    return pass;
}

static BOOL test_invalid_shader(compile_fn compile)
{
    const char source[] = "float4 main(:";
    ID3DBlob *code = NULL, *errors = NULL;
    HRESULT hr = compile(source, sizeof(source) - 1, "invalid.hlsl", NULL, NULL,
            "main", "ps_5_0", 0, 0, &code, &errors);
    BOOL pass = FAILED(hr) && !code && errors && ID3D10Blob_GetBufferSize(errors) > 1;
    if (code) ID3D10Blob_Release(code);
    if (errors) ID3D10Blob_Release(errors);
    report(pass ? "Invalid HLSL diagnostics PASS\r\n" : "Invalid HLSL diagnostics FAIL\r\n");
    return pass;
}

void WINAPI test_entry(void)
{
    HMODULE compiler_module;
    compile_fn compile;
    disassemble_fn disassemble;
    BOOL pass;
    initialize_test();
    report("WINDX shader compiler test\r\n");
    compiler_module = load_compiler();
    if (!compiler_module) {
        report("FAIL loading the application-local compiler DLL\r\n");
        ExitProcess(1);
    }
    compile = (compile_fn)GetProcAddress(compiler_module, "D3DCompile");
    disassemble = (disassemble_fn)GetProcAddress(compiler_module, "D3DDisassemble");
    if (!compile || !disassemble) {
        report("FAIL resolving compiler entry points\r\n");
        ExitProcess(1);
    }
    pass = test_profile(compile, disassemble, "ps_3_0",
            "float4 main(float4 c : COLOR0) : COLOR0 { return c * 0.5; }", 0xffff0300u);
    if (!test_profile(compile, disassemble, "ps_5_0",
            "float4 main(float4 p : SV_Position) : SV_Target { return p * 0.25; }", 0x43425844u))
        pass = FALSE;
    if (!test_invalid_shader(compile))
        pass = FALSE;
    FreeLibrary(compiler_module);
    ExitProcess(pass ? 0 : 1);
}
