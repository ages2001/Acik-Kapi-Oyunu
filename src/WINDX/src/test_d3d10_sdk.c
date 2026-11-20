#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <d3d10.h>
#include <d3d10shader.h>
#include <d3d10effect.h>
#include "test_output.h"

typedef HRESULT (WINAPI *preprocess_fn)(const void *,SIZE_T,const char *,const D3D10_SHADER_MACRO *,
        ID3D10Include *,ID3D10Blob **,ID3D10Blob **);
typedef HRESULT (WINAPI *compile_fn)(const char *,SIZE_T,const char *,const D3D10_SHADER_MACRO *,
        ID3D10Include *,const char *,const char *,UINT,ID3D10Blob **,ID3D10Blob **);
typedef HRESULT (WINAPI *reflect_fn)(const void *,SIZE_T,ID3D10ShaderReflection **);
typedef HRESULT (WINAPI *disassemble_fn)(const void *,SIZE_T,BOOL,const char *,ID3D10Blob **);
typedef HRESULT (WINAPI *signature_fn)(const void *,SIZE_T,ID3D10Blob **);
typedef HRESULT (WINAPI *compile_fx_fn)(void *,SIZE_T,const char *,const D3D10_SHADER_MACRO *,
        ID3D10Include *,UINT,UINT,ID3D10Blob **,ID3D10Blob **);
typedef HRESULT (WINAPI *create_fx_fn)(void *,SIZE_T,UINT,ID3D10Device *,ID3D10EffectPool *,ID3D10Effect **);

static BOOL contains(ID3D10Blob *blob, const char *text)
{
    const char *data = ID3D10Blob_GetBufferPointer(blob);
    SIZE_T size = ID3D10Blob_GetBufferSize(blob), len = lstrlenA(text);
    for (SIZE_T i = 0; len <= size && i <= size - len; ++i) {
        SIZE_T j = 0;
        while (j < len && data[i + j] == text[j]) ++j;
        if (j == len) return TRUE;
    }
    return FALSE;
}

static BOOL reflect_constant(ID3D10ShaderReflection *reflection, UINT minor)
{
    D3D10_SHADER_DESC shader;
    D3D10_SHADER_BUFFER_DESC buffer;
    D3D10_SHADER_VARIABLE_DESC variable;
    D3D10_SHADER_TYPE_DESC type;
    D3D10_SIGNATURE_PARAMETER_DESC input, output;
    D3D10_SHADER_INPUT_BIND_DESC binding;
    ID3D10ShaderReflectionConstantBuffer *constants;
    ID3D10ShaderReflectionVariable *tint;
    ID3D10ShaderReflectionType *value;
    if (FAILED(reflection->lpVtbl->GetDesc(reflection, &shader)) || shader.Version != (0x40 | minor) ||
            shader.ConstantBuffers != 1 || shader.BoundResources != 1 ||
            shader.InputParameters != 1 || shader.OutputParameters != 1) return FALSE;
    if (FAILED(reflection->lpVtbl->GetInputParameterDesc(reflection, 0, &input)) ||
            FAILED(reflection->lpVtbl->GetOutputParameterDesc(reflection, 0, &output)) ||
            lstrcmpiA(input.SemanticName, "SV_Position") || lstrcmpiA(output.SemanticName, "SV_Target")) return FALSE;
    if (FAILED(reflection->lpVtbl->GetResourceBindingDesc(reflection, 0, &binding)) ||
            lstrcmpA(binding.Name, "Colors") || binding.BindPoint != 1) return FALSE;
    constants = reflection->lpVtbl->GetConstantBufferByName(reflection, "Colors");
    if (!constants || FAILED(constants->lpVtbl->GetDesc(constants, &buffer)) ||
            buffer.Variables != 1 || buffer.Size != 16) return FALSE;
    tint = constants->lpVtbl->GetVariableByName(constants, "tint");
    if (!tint || FAILED(tint->lpVtbl->GetDesc(tint, &variable)) || variable.StartOffset || variable.Size != 16) return FALSE;
    value = tint->lpVtbl->GetType(tint);
    return value && SUCCEEDED(value->lpVtbl->GetDesc(value, &type)) && type.Rows == 1 && type.Columns == 4;
}

static BOOL shader_tools(HMODULE module, UINT minor)
{
    static const char hlsl[] = "cbuffer Colors : register(b1) {float4 tint;};\n"
            "float4 main(float4 p : SV_Position) : SV_Target {return tint * WINDX_GAIN + p;}";
    static const D3D10_SHADER_MACRO macros[] = {{"WINDX_GAIN", "2.0f"}, {NULL, NULL}};
    static const char *const signatures[] = {"D3D10GetInputSignatureBlob","D3D10GetOutputSignatureBlob",
                                             "D3D10GetInputAndOutputSignatureBlob"};
    const char *profile = minor ? "ps_4_1" : "ps_4_0";
    preprocess_fn preprocess = (void *)GetProcAddress(module, "D3D10PreprocessShader");
    compile_fn compile = (void *)GetProcAddress(module, "D3D10CompileShader");
    reflect_fn reflect = (void *)GetProcAddress(module, "D3D10ReflectShader");
    disassemble_fn disassemble = (void *)GetProcAddress(module, "D3D10DisassembleShader");
    ID3D10Blob *source = NULL, *shader = NULL, *assembly = NULL, *signature = NULL;
    ID3D10ShaderReflection *reflection = NULL;
    const void *data;
    SIZE_T size;
    HRESULT hr;
    const char *stage = "entry points";
    BOOL pass = FALSE;
    if (!preprocess) { stage = "D3D10PreprocessShader export"; goto done; }
    if (!compile) { stage = "D3D10CompileShader export"; goto done; }
    if (!reflect) { stage = "D3D10ReflectShader export"; goto done; }
    if (!disassemble) { stage = "D3D10DisassembleShader export"; goto done; }
    stage = "preprocessing";
    if (FAILED(preprocess(hlsl, sizeof(hlsl)-1, NULL, macros, NULL, &source, NULL)) ||
            !source || !contains(source, "2.0f") || contains(source, "WINDX_GAIN")) goto done;
    stage = "shader compilation";
    if (FAILED(compile(hlsl, sizeof(hlsl)-1, NULL, macros, NULL, "main", profile, 0, &shader, NULL)) || !shader) goto done;
    data = ID3D10Blob_GetBufferPointer(shader);
    size = ID3D10Blob_GetBufferSize(shader);
    stage = "disassembly";
    if (FAILED(disassemble(data, size, FALSE, NULL, &assembly)) || !assembly || !contains(assembly, profile)) goto done;
    stage = "reflection";
    hr = reflect(data, size, &reflection);
    if (FAILED(hr) || !reflection) {
        report("D3D10ReflectShader returned "); report_hex((DWORD)hr); report("\r\n");
        goto done;
    }
    stage = "reflection descriptors";
    if (!reflect_constant(reflection, minor)) goto done;
    for (unsigned i = 0; i < sizeof(signatures)/sizeof(signatures[0]); ++i) {
        signature_fn get = (void *)GetProcAddress(module, signatures[i]);
        stage = signatures[i];
        if (!get || FAILED(get(data, size, &signature)) || !signature || !contains(signature, "DXBC") ||
                (i != 1 && !contains(signature, "ISGN")) || (i != 0 && !contains(signature, "OSGN"))) goto done;
        ID3D10Blob_Release(signature);
        signature = NULL;
    }
    pass = TRUE;
done:
    if (!pass) { report("SDK failure at "); report(stage); report("\r\n"); }
    if (signature) ID3D10Blob_Release(signature);
    if (reflection) reflection->lpVtbl->Release(reflection);
    if (assembly) ID3D10Blob_Release(assembly);
    if (shader) ID3D10Blob_Release(shader);
    if (source) ID3D10Blob_Release(source);
    return pass;
}

static BOOL effect_tools(HMODULE module, ID3D10Device *device)
{
    static char source[] = "cbuffer Colors {float4 tint; float gain;}; "
                           "Texture2D image; technique10 Render { pass First {} }";
    compile_fx_fn compile = (void *)GetProcAddress(module, "D3D10CompileEffectFromMemory");
    create_fx_fn create = (void *)GetProcAddress(module, "D3D10CreateEffectFromMemory");
    ID3D10Blob *code = NULL, *diagnostics = NULL;
    ID3D10Effect *effect = NULL;
    ID3D10EffectTechnique *technique;
    ID3D10EffectPass *pass;
    D3D10_EFFECT_DESC desc;
    D3D10_TECHNIQUE_DESC td;
    D3D10_PASS_DESC pd;
    BOOL result = FALSE;
    HRESULT hr = E_NOINTERFACE;
    const char *stage = "entry points";
    if (!compile || !create) goto done;
    stage = "FX4 compilation";
    hr = compile(source, sizeof(source)-1, NULL, NULL, NULL, 0, 0, &code, &diagnostics);
    if (FAILED(hr) || !code) goto done;
    stage = "FX4 device object";
    hr = create(ID3D10Blob_GetBufferPointer(code), ID3D10Blob_GetBufferSize(code), 0, device, NULL, &effect);
    if (FAILED(hr) || !effect) goto done;
    stage = "effect description";
    if (FAILED(hr = effect->lpVtbl->GetDesc(effect, &desc)) || desc.Techniques != 1 ||
            desc.IsChildEffect || desc.ConstantBuffers != 1 || desc.GlobalVariables != 3 ||
            desc.SharedConstantBuffers || desc.SharedGlobalVariables) goto done;
    stage = "technique description";
    technique = effect->lpVtbl->GetTechniqueByName(effect, "Render");
    if (!technique || FAILED(hr = technique->lpVtbl->GetDesc(technique, &td)) || td.Passes != 1) goto done;
    stage = "pass description";
    pass = technique->lpVtbl->GetPassByName(technique, "First");
    result = pass && SUCCEEDED(hr = pass->lpVtbl->GetDesc(pass, &pd)) && pd.Name && !lstrcmpA(pd.Name, "First");
done:
    if (!result) {
        report("SDK failure at "); report(stage); report(", HRESULT "); report_hex((DWORD)hr); report("\r\n");
        if (diagnostics) {
            DWORD written;
            WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), ID3D10Blob_GetBufferPointer(diagnostics),
                    (DWORD)ID3D10Blob_GetBufferSize(diagnostics), &written, NULL);
        }
    }
    if (effect) effect->lpVtbl->Release(effect);
    if (diagnostics) ID3D10Blob_Release(diagnostics);
    if (code) ID3D10Blob_Release(code);
    return result;
}

BOOL test_d3d10_sdk(HMODULE module, ID3D10Device *device, BOOL version1)
{
    BOOL shaders = shader_tools(module, version1 ? 1 : 0);
    BOOL effects = effect_tools(module, device);
    report(shaders ? "SDK preprocessing, compilation, disassembly, reflection and signatures PASS\r\n"
                   : "SDK shader tools FAIL\r\n");
    report(effects ? "FX4 compilation, effect creation and named technique/pass PASS\r\n"
                   : "FX4 effect test FAIL\r\n");
    return shaders && effects;
}
