#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#define INITGUID
#include <windows.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include "test_output.h"

typedef HRESULT (WINAPI *serialize_fn)(const D3D12_ROOT_SIGNATURE_DESC *,D3D_ROOT_SIGNATURE_VERSION,ID3DBlob **,ID3DBlob **);
typedef HRESULT (WINAPI *deserialize_fn)(const void *,SIZE_T,REFIID,void **);
typedef HRESULT (WINAPI *device_fn)(IUnknown *,D3D_FEATURE_LEVEL,REFIID,void **);

static BOOL signatures(HMODULE dll)
{
    serialize_fn serialize = (serialize_fn)GetProcAddress(dll,"D3D12SerializeRootSignature");
    deserialize_fn deserialize = (deserialize_fn)GetProcAddress(dll,"D3D12CreateRootSignatureDeserializer");
    D3D12_ROOT_PARAMETER param = {0};
    D3D12_ROOT_SIGNATURE_DESC root = {0};
    ID3DBlob *blob = NULL, *errors = NULL;
    ID3D12RootSignatureDeserializer *reader = NULL;
    BOOL ok = FALSE;
    HRESULT hr = E_NOINTERFACE;
    if (!serialize || !deserialize) goto done;
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    param.Constants.ShaderRegister = 3;
    param.Constants.RegisterSpace = 1;
    param.Constants.Num32BitValues = 4;
    param.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    root.NumParameters = 1; root.pParameters = &param;
    hr = serialize(&root, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors);
    if (FAILED(hr) || !blob) goto done;
    hr = deserialize(ID3D10Blob_GetBufferPointer(blob),ID3D10Blob_GetBufferSize(blob),
            &IID_ID3D12RootSignatureDeserializer,(void **)&reader);
    if (FAILED(hr) || !reader) goto done;
    const D3D12_ROOT_SIGNATURE_DESC *out = ID3D12RootSignatureDeserializer_GetRootSignatureDesc(reader);
    if (!out || out->NumParameters != 1 || !out->pParameters) goto done;
    const D3D12_ROOT_PARAMETER *p = out->pParameters;
    ok = p->ParameterType == param.ParameterType && p->Constants.ShaderRegister == 3 &&
            p->Constants.RegisterSpace == 1 && p->Constants.Num32BitValues == 4 &&
            p->ShaderVisibility == param.ShaderVisibility;
done:
    if (reader) ID3D12RootSignatureDeserializer_Release(reader);
    if (errors) ID3D10Blob_Release(errors);
    if (blob) ID3D10Blob_Release(blob);
    if (ok) report("D3D12 root signature serialization and deserialization PASS\r\n");
    else { report("D3D12 root signature FAIL, HRESULT "); report_hex((DWORD)hr); report("\r\n"); }
    return ok;
}

static HRESULT buffer(ID3D12Device *dev, D3D12_HEAP_TYPE heap, D3D12_RESOURCE_STATES state, ID3D12Resource **res)
{
    D3D12_HEAP_PROPERTIES props = {0};
    D3D12_RESOURCE_DESC desc = {0};
    props.Type = heap; props.CreationNodeMask = props.VisibleNodeMask = 1;
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = 128; desc.Height = 1; desc.DepthOrArraySize = desc.MipLevels = 1;
    desc.SampleDesc.Count = 1; desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if (state == D3D12_RESOURCE_STATE_UNORDERED_ACCESS) desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    return ID3D12Device_CreateCommittedResource(dev, &props, D3D12_HEAP_FLAG_NONE, &desc, state,
            NULL, &IID_ID3D12Resource, (void **)res);
}

static BOOL compute_work(HMODULE dll, ID3D12Device *dev, ID3D12CommandQueue *queue,
        ID3D12CommandAllocator *allocator, ID3D12GraphicsCommandList *list, ID3D12Fence *fence,
        HANDLE event, ID3D12Resource *readback)
{
    typedef HRESULT (WINAPI *compile_fn)(const void *,SIZE_T,LPCSTR,const D3D_SHADER_MACRO *,ID3DInclude *,
            LPCSTR,LPCSTR,UINT,UINT,ID3DBlob **,ID3DBlob **);
    static const char kernel[] =
            "RWByteAddressBuffer result : register(u0);"
            "[numthreads(4,1,1)] void main(uint3 tid : SV_DispatchThreadID) {"
            "uint i=tid.x; result.Store(i*4, i*i+37);}";
    D3D12_ROOT_PARAMETER param = {0};
    D3D12_ROOT_SIGNATURE_DESC layout = {0};
    D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {0};
    D3D12_RESOURCE_BARRIER barrier = {0};
    D3D12_RANGE all = {0, 128}, none = {0, 0};
    ID3D12RootSignature *root = NULL;
    ID3D12PipelineState *pso = NULL;
    ID3D12Resource *storage = NULL;
    ID3DBlob *serialized = NULL, *shader = NULL;
    ID3D12CommandList *commands[] = {(ID3D12CommandList *)list};
    HMODULE compiler = LoadLibraryA("d3dcompiler_43.dll");
    compile_fn compile = compiler ? (compile_fn)GetProcAddress(compiler,"D3DCompile") : NULL;
    serialize_fn serialize = (serialize_fn)GetProcAddress(dll,"D3D12SerializeRootSignature");
    HRESULT hr = E_NOINTERFACE;
    BOOL ok = FALSE;
    DWORD *values;
    const char *stage = "HLSL compiler";
    if (!compile || !serialize) goto done;
    hr = compile(kernel,sizeof(kernel)-1,NULL,NULL,NULL,"main","cs_5_0",0,0,&shader,NULL);
    if (FAILED(hr)) goto done;
    stage = "UAV root signature";
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    layout.NumParameters = 1; layout.pParameters = &param;
    hr = serialize(&layout,D3D_ROOT_SIGNATURE_VERSION_1,&serialized,NULL);
    if (FAILED(hr)) goto done;
    hr = ID3D12Device_CreateRootSignature(dev,0,ID3D10Blob_GetBufferPointer(serialized),
            ID3D10Blob_GetBufferSize(serialized),&IID_ID3D12RootSignature,(void **)&root);
    if (FAILED(hr)) goto done;
    stage = "compute pipeline";
    desc.pRootSignature = root;
    desc.CS.pShaderBytecode = ID3D10Blob_GetBufferPointer(shader);
    desc.CS.BytecodeLength = ID3D10Blob_GetBufferSize(shader);
    hr = ID3D12Device_CreateComputePipelineState(dev,&desc,&IID_ID3D12PipelineState,(void **)&pso);
    if (FAILED(hr)) goto done;
    stage = "UAV buffer";
    hr = buffer(dev,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,&storage);
    if (FAILED(hr)) goto done;
    stage = "command submission";
    if (FAILED(hr = ID3D12CommandAllocator_Reset(allocator)) ||
            FAILED(hr = ID3D12GraphicsCommandList_Reset(list,allocator,pso))) goto done;
    ID3D12GraphicsCommandList_SetComputeRootSignature(list,root);
    ID3D12GraphicsCommandList_SetComputeRootUnorderedAccessView(list,0,ID3D12Resource_GetGPUVirtualAddress(storage));
    ID3D12GraphicsCommandList_Dispatch(list,8,1,1);
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = storage;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    ID3D12GraphicsCommandList_ResourceBarrier(list,1,&barrier);
    ID3D12GraphicsCommandList_CopyBufferRegion(list,readback,0,storage,0,128);
    if (FAILED(hr = ID3D12GraphicsCommandList_Close(list))) goto done;
    ID3D12CommandQueue_ExecuteCommandLists(queue,1,commands);
    if (FAILED(hr = ID3D12CommandQueue_Signal(queue,fence,2)) ||
            FAILED(hr = ID3D12Fence_SetEventOnCompletion(fence,2,event)) ||
            WaitForSingleObject(event,30000) != WAIT_OBJECT_0) goto done;
    stage = "32-value comparison";
    if (FAILED(hr = ID3D12Resource_Map(readback,0,&all,(void **)&values))) goto done;
    ok = TRUE;
    for (UINT i = 0; i < 32; ++i) if (values[i] != i*i+37) ok = FALSE;
    ID3D12Resource_Unmap(readback,0,&none);
done:
    if (storage) ID3D12Resource_Release(storage);
    if (pso) ID3D12PipelineState_Release(pso);
    if (root) ID3D12RootSignature_Release(root);
    if (serialized) ID3D10Blob_Release(serialized);
    if (shader) ID3D10Blob_Release(shader);
    if (compiler) FreeLibrary(compiler);
    if (ok) report("D3D12 cs_5_0 dispatch, UAV transition and 32 results PASS\r\n");
    else { report("D3D12 compute FAIL at "); report(stage); report(", HRESULT "); report_hex((DWORD)hr); report("\r\n"); }
    return ok;
}

static BOOL copy_test(HMODULE dll)
{
    device_fn create = (device_fn)GetProcAddress(dll,"D3D12CreateDevice");
    ID3D12Device *dev = NULL;
    ID3D12CommandQueue *queue = NULL;
    ID3D12CommandAllocator *allocator = NULL;
    ID3D12GraphicsCommandList *list = NULL;
    ID3D12Resource *src = NULL, *dst = NULL;
    ID3D12Fence *fence = NULL;
    D3D12_COMMAND_QUEUE_DESC qdesc = {0};
    D3D12_RANGE none = {0, 0}, all = {0, 128};
    HANDLE event = NULL;
    DWORD *values;
    const char *stage = "device creation";
    HRESULT hr = create ? create(NULL,D3D_FEATURE_LEVEL_11_0,&IID_ID3D12Device,(void **)&dev) : E_NOINTERFACE;
    BOOL ok = FALSE, compute_ok = FALSE;
    if (FAILED(hr) || !dev) goto done;
    D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_12_1,D3D_FEATURE_LEVEL_12_0,D3D_FEATURE_LEVEL_11_1,D3D_FEATURE_LEVEL_11_0};
    D3D12_FEATURE_DATA_FEATURE_LEVELS caps = {sizeof(levels)/sizeof(levels[0]), levels, 0};
    if (SUCCEEDED(ID3D12Device_CheckFeatureSupport(dev,D3D12_FEATURE_FEATURE_LEVELS,&caps,sizeof(caps)))) {
        report("D3D12 maximum queried feature level "); report_hex(caps.MaxSupportedFeatureLevel); report("\r\n");
    }
    stage = "queue, allocator and command list";
    hr = ID3D12Device_CreateCommandQueue(dev,&qdesc,&IID_ID3D12CommandQueue,(void **)&queue);
    if (FAILED(hr)) goto done;
    hr = ID3D12Device_CreateCommandAllocator(dev,D3D12_COMMAND_LIST_TYPE_DIRECT,&IID_ID3D12CommandAllocator,(void **)&allocator);
    if (FAILED(hr)) goto done;
    hr = ID3D12Device_CreateCommandList(dev,0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator,NULL,
            &IID_ID3D12GraphicsCommandList,(void **)&list);
    if (FAILED(hr)) goto done;
    stage = "upload and readback resources";
    if (FAILED(hr = buffer(dev,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ,&src)) ||
            FAILED(hr = buffer(dev,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST,&dst))) goto done;
    hr = ID3D12Resource_Map(src,0,&none,(void **)&values);
    if (FAILED(hr)) goto done;
    for (UINT i = 0; i < 32; ++i) values[i] = i * 271 + 7;
    ID3D12Resource_Unmap(src,0,&all);
    stage = "copy submission and fence";
    ID3D12GraphicsCommandList_CopyBufferRegion(list,dst,0,src,0,128);
    if (FAILED(hr = ID3D12GraphicsCommandList_Close(list))) goto done;
    if (FAILED(hr = ID3D12Device_CreateFence(dev,0,D3D12_FENCE_FLAG_NONE,&IID_ID3D12Fence,(void **)&fence))) goto done;
    event = CreateEventA(NULL,FALSE,FALSE,NULL);
    if (!event) goto done;
    ID3D12CommandList *commands[] = {(ID3D12CommandList *)list};
    ID3D12CommandQueue_ExecuteCommandLists(queue,1,commands);
    if (FAILED(hr = ID3D12CommandQueue_Signal(queue,fence,1)) ||
            FAILED(hr = ID3D12Fence_SetEventOnCompletion(fence,1,event)) ||
            WaitForSingleObject(event,30000) != WAIT_OBJECT_0) goto done;
    stage = "32-word readback";
    hr = ID3D12Resource_Map(dst,0,&all,(void **)&values);
    if (FAILED(hr)) goto done;
    ok = TRUE;
    for (UINT i = 0; i < 32; ++i) if (values[i] != i * 271 + 7) ok = FALSE;
    ID3D12Resource_Unmap(dst,0,&none);
    if (ok) compute_ok = compute_work(dll,dev,queue,allocator,list,fence,event,dst);
done:
    if (event) CloseHandle(event);
    if (fence) ID3D12Fence_Release(fence);
    if (dst) ID3D12Resource_Release(dst);
    if (src) ID3D12Resource_Release(src);
    if (list) ID3D12GraphicsCommandList_Release(list);
    if (allocator) ID3D12CommandAllocator_Release(allocator);
    if (queue) ID3D12CommandQueue_Release(queue);
    if (dev) ID3D12Device_Release(dev);
    if (ok) report("D3D12 command queue, fence and 32-word buffer copy/readback PASS\r\n");
    else { report("D3D12 FAIL at "); report(stage); report(", HRESULT "); report_hex((DWORD)hr); report("\r\n"); }
    return ok && compute_ok;
}

void WINAPI d3d12_entry(void)
{
    initialize_test();
    char path[MAX_PATH];
    DWORD i = GetModuleFileNameA(NULL,path,sizeof(path));
    if (!i || i >= sizeof(path)) ExitProcess(1);
    while (i && path[i-1] != '\\' && path[i-1] != '/') --i;
    if (i + sizeof("d3d12.dll") > sizeof(path)) ExitProcess(1);
    lstrcpyA(path+i,"d3d12.dll");
    HMODULE dll = LoadLibraryA(path);
    if (!dll) { report("Cannot load app-local d3d12.dll, error "); report_hex(GetLastError()); report("\r\n"); ExitProcess(1); }
    report("D3D12 module "); report(path); report("\r\n");
    BOOL root = signatures(dll);
    BOOL queue = copy_test(dll);
    FreeLibrary(dll);
    ExitProcess(root && queue ? 0 : 1);
}
