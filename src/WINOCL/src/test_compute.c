/* The same kernel and validation run with Windows and POSIX ICD loaders. */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#endif
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#include <CL/cl.h>
#include <CL/cl_ext.h>
#include <CL/cl_function_types.h>

static const char *stage = "platform discovery";

#ifdef _WIN32
typedef HMODULE library_t;
static void *symbol(library_t lib, const char *name) { return (void *)GetProcAddress(lib, name); }
static void output(const char *s) { DWORD n; WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), s, lstrlenA(s), &n, NULL); }
static void *allocate(size_t n) { return HeapAlloc(GetProcessHeap(), 0, n); }
static void discard(void *p) { if (p) HeapFree(GetProcessHeap(), 0, p); }
#else
typedef void *library_t;
#define symbol dlsym
#define allocate malloc
#define discard free
static void output(const char *s) { fputs(s, stdout); }
#endif

/* Use the core or extension spelling supported by the actual compute device. */
#define API_LIST(X) X(clGetPlatformIDs) X(clGetDeviceIDs) X(clGetDeviceInfo) X(clGetExtensionFunctionAddressForPlatform) \
    X(clCreateContext) X(clCreateCommandQueue) X(clCreateBuffer) X(clCreateProgramWithSource) \
    X(clBuildProgram) X(clGetProgramBuildInfo) X(clCreateKernel) X(clSetKernelArg) \
    X(clEnqueueNDRangeKernel) X(clEnqueueReadBuffer) X(clReleaseKernel) X(clReleaseProgram) \
    X(clReleaseMemObject) X(clReleaseCommandQueue) X(clReleaseContext) X(clGetKernelSuggestedLocalWorkSize)
struct api {
#define FIELD(name) name##_t *name;
    API_LIST(FIELD)
#undef FIELD
};

static int has_extension(const char *extensions, const char *name)
{
    while (*extensions) {
        const char *word = extensions, *needle = name;
        while (*word && *word != ' ' && *word == *needle) { ++word; ++needle; }
        if (!*needle && (!*word || *word == ' ')) return 1;
        while (*extensions && *extensions != ' ') ++extensions;
        while (*extensions == ' ') ++extensions;
    }
    return 0;
}

static int compute(const struct api *a, cl_platform_id platform, cl_device_id device)
{
    static const char kernel_source[] =
        "__kernel void combine(__global const int *x,__global const int *y,__global int *z){"
        "size_t i=get_global_id(0); z[i]=3*x[i]-2*y[i]+7; if(i==0) z[get_global_size(0)]=(int)get_local_size(0);}";
    const char *source = kernel_source;
    enum { count = 32 };
    cl_int x[count], y[count], z[count + 1];
    size_t global = count, suggested = 0;
    cl_context_properties properties[] = {CL_CONTEXT_PLATFORM, (cl_context_properties)platform, 0};
    cl_context context = NULL;
    cl_command_queue queue = NULL;
    cl_program program = NULL;
    cl_kernel kernel = NULL;
    cl_mem buffers[3] = {NULL, NULL, NULL};
    cl_version version = 0;
    clGetKernelSuggestedLocalWorkSize_t *query = NULL;
    size_t extension_bytes = 0;
    char *extensions = NULL;
    const char *query_name = NULL;
    char name[256] = {0}, info[256] = {0};
    cl_int err;
    int passed = 0;
    const char *failed_stage;
    stage = "context creation";
    a->clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(name), name, NULL);
    a->clGetDeviceInfo(device, CL_DEVICE_VERSION, sizeof(info), info, NULL);
    output("Device "); output(name); output("\r\nVersion "); output(info); output("\r\n");
    for (int i = 0; i < count; ++i) { x[i] = i - 11; y[i] = i*i - 5; z[i] = 0; }
    context = a->clCreateContext(properties, 1, &device, NULL, NULL, &err);
    if (!context || err != CL_SUCCESS) goto done;
    stage = "command queue creation";
    queue = a->clCreateCommandQueue(context, device, 0, &err);
    if (!queue || err != CL_SUCCESS) goto done;
    stage = "program creation";
    program = a->clCreateProgramWithSource(context, 1, &source, NULL, &err);
    if (!program || err != CL_SUCCESS) goto done;
    stage = "program build";
    if (a->clBuildProgram(program, 1, &device, NULL, NULL, NULL) != CL_SUCCESS) {
        size_t size = 0;
        if (a->clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &size) == CL_SUCCESS &&
                size && size < (size_t)-1) {
            char *log = allocate(size + 1);
            if (log) {
                log[size] = 0;
                if (a->clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, size, log, NULL) == CL_SUCCESS)
                    output(log);
                discard(log);
            }
        }
        goto done;
    }
    stage = "kernel creation";
    kernel = a->clCreateKernel(program, "combine", &err);
    if (!kernel || err != CL_SUCCESS) goto done;
    stage = "work size capability discovery";
    if (a->clGetDeviceInfo(device, CL_DEVICE_NUMERIC_VERSION, sizeof(version), &version, NULL) == CL_SUCCESS &&
            version >= CL_MAKE_VERSION(3, 1, 0)) {
        query = a->clGetKernelSuggestedLocalWorkSize;
        query_name = "OpenCL 3.1";
    } else {
        if (a->clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, 0, NULL, &extension_bytes) != CL_SUCCESS ||
                !extension_bytes) goto done;
        extensions = allocate(extension_bytes);
        if (!extensions || a->clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, extension_bytes, extensions, NULL) != CL_SUCCESS)
            goto done;
        extensions[extension_bytes - 1] = 0;
        if (has_extension(extensions, "cl_khr_suggested_local_work_size")) {
            query = (clGetKernelSuggestedLocalWorkSize_t *)a->clGetExtensionFunctionAddressForPlatform(
                    platform, "clGetKernelSuggestedLocalWorkSizeKHR");
            if (!query) goto done;
            query_name = "cl_khr_suggested_local_work_size";
        }
    }
    if (query && query(queue, kernel, 1, NULL, &global, &suggested) != CL_INVALID_KERNEL_ARGS) goto done;
    stage = "buffer allocation";
    buffers[0] = a->clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(x), x, &err);
    if (!buffers[0]) goto done;
    buffers[1] = a->clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(y), y, &err);
    if (!buffers[1]) goto done;
    buffers[2] = a->clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(z), NULL, &err);
    if (!buffers[2]) goto done;
    stage = "kernel argument binding";
    for (cl_uint i = 0; i < 3; ++i)
        if (a->clSetKernelArg(kernel, i, sizeof(buffers[i]), &buffers[i]) != CL_SUCCESS) goto done;
    stage = "local work size query";
    if (query) {
        size_t overflow = (size_t)-1, zero = 0;
        if (query(queue, kernel, 1, &overflow, &global, &suggested) != CL_INVALID_GLOBAL_OFFSET ||
                query(queue, kernel, 1, NULL, &zero, &suggested) != CL_INVALID_GLOBAL_WORK_SIZE ||
                query(queue, kernel, 1, NULL, &global, NULL) != CL_INVALID_VALUE ||
                query(queue, kernel, 1, NULL, &global, &suggested) != CL_SUCCESS || !suggested) goto done;
    }
    stage = "kernel dispatch or readback";
    if (a->clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global, NULL, 0, NULL, NULL) != CL_SUCCESS ||
            a->clEnqueueReadBuffer(queue, buffers[2], CL_TRUE, 0, sizeof(z), z, 0, NULL, NULL) != CL_SUCCESS) goto done;
    stage = "result comparison";
    for (int i = 0; i < count; ++i)
        if (z[i] != 3*x[i] - 2*y[i] + 7) goto done;
    if (query) {
        stage = "suggested versus executed local work size";
        if (z[count] <= 0 || (size_t)z[count] != suggested) goto done;
        output(query_name); output(" work size query and executed group size PASS\r\n");
    }
    passed = 1;
done:
    discard(extensions);
    failed_stage = stage;
    if (!passed) { output("OpenCL test failed at "); output(failed_stage); output("\r\n"); }
    stage = "kernel release";
    if (kernel) a->clReleaseKernel(kernel);
    stage = "program release";
    if (program) a->clReleaseProgram(program);
    stage = "buffer release";
    for (int i = 0; i < 3; ++i) if (buffers[i]) a->clReleaseMemObject(buffers[i]);
    stage = "queue release";
    if (queue) a->clReleaseCommandQueue(queue);
    stage = "context release";
    if (context) a->clReleaseContext(context);
    if (passed) output("OpenCL kernel and 32 results PASS\r\n");
    return passed;
}

static int run(library_t lib)
{
    struct api a;
    cl_uint platforms = 0;
    cl_int err;
    int tested = 0, failed = 0;
    if (!lib) { output("OpenCL library unavailable\r\n"); return 1; }
#define LOAD(name) a.name = (name##_t *)symbol(lib, #name); if (!a.name) { output("Missing " #name "\r\n"); return 1; }
    API_LIST(LOAD)
#undef LOAD
    output("OpenCL 3.1 loader entry points found\r\n");
    err = a.clGetPlatformIDs(0, NULL, &platforms);
    if (err == CL_PLATFORM_NOT_FOUND_KHR || (err == CL_SUCCESS && !platforms)) {
        output("No installed compute platform\r\n"); return 2;
    }
    if (err != CL_SUCCESS || platforms > (size_t)-1 / sizeof(cl_platform_id)) return 1;
    cl_platform_id *ids = allocate(platforms * sizeof(*ids));
    if (!ids || a.clGetPlatformIDs(platforms, ids, NULL) != CL_SUCCESS) { discard(ids); return 1; }
    for (cl_uint i = 0; i < platforms; ++i) {
        cl_uint devices = 0;
        err = a.clGetDeviceIDs(ids[i], CL_DEVICE_TYPE_ALL, 0, NULL, &devices);
        if (err == CL_DEVICE_NOT_FOUND) continue;
        if (err != CL_SUCCESS || !devices || devices > (size_t)-1 / sizeof(cl_device_id)) { failed = 1; continue; }
        cl_device_id *list = allocate(devices * sizeof(*list));
        if (!list || a.clGetDeviceIDs(ids[i], CL_DEVICE_TYPE_ALL, devices, list, NULL) != CL_SUCCESS) failed = 1;
        else for (cl_uint j = 0; j < devices; ++j) { ++tested; if (!compute(&a, ids[i], list[j])) failed = 1; }
        discard(list);
    }
    discard(ids);
    if (!tested && !failed) output("No compute devices on the installed platforms\r\n");
    return failed ? 1 : tested ? 0 : 2;
}

#ifdef _WIN32
static void output_hex(DWORD value)
{
    char text[9];
    for (unsigned int i = 0; i < 8; ++i)
        text[i] = "0123456789abcdef"[(value >> (28 - 4*i)) & 15];
    text[8] = 0;
    output(text);
}

static LONG WINAPI report_fault(EXCEPTION_POINTERS *fault)
{
    MEMORY_BASIC_INFORMATION memory;
    char path[MAX_PATH];
    void *address = fault->ExceptionRecord->ExceptionAddress;
    output("Windows exception ");
    output_hex(fault->ExceptionRecord->ExceptionCode);
    output(" at "); output(stage);
    if (VirtualQuery(address, &memory, sizeof(memory)) && memory.AllocationBase && memory.Type == MEM_IMAGE &&
            GetModuleFileNameA((HMODULE)memory.AllocationBase, path, sizeof(path))) {
        output(" in "); output(path); output(" at module offset ");
        output_hex((DWORD)((ULONG_PTR)address - (ULONG_PTR)memory.AllocationBase));
    }
    output("\r\n");
    return EXCEPTION_EXECUTE_HANDLER;
}

void WINAPI test_entry(void)
{
    SetUnhandledExceptionFilter(report_fault);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, path, sizeof(path));
    if (!n || n >= sizeof(path)) ExitProcess(1);
    while (n && path[n-1] != '\\') --n;
    if (n + sizeof("OpenCL.dll") > sizeof(path)) ExitProcess(1);
    lstrcpyA(path + n, "OpenCL.dll");
    HMODULE lib = LoadLibraryA(path);
    int result = run(lib);
    stage = "driver unload";
    if (lib) FreeLibrary(lib);
    ExitProcess(result);
}
#else
int main(void)
{
    library_t lib = dlopen("libOpenCL.so.1", RTLD_NOW | RTLD_LOCAL);
    int result = run(lib);
    if (lib) dlclose(lib);
    return result;
}
#endif
