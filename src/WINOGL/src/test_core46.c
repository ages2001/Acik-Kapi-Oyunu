#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/wglext.h>
#include <stdint.h>
#include "test_output.h"
#include "test_spirv.h"

typedef int (WINAPI *choose_fn)(HDC, const PIXELFORMATDESCRIPTOR *);
typedef BOOL (WINAPI *set_fn)(HDC, int, const PIXELFORMATDESCRIPTOR *);
typedef HGLRC (WINAPI *create_fn)(HDC);
typedef BOOL (WINAPI *current_fn)(HDC, HGLRC);
typedef BOOL (WINAPI *delete_fn)(HGLRC);
typedef PROC (WINAPI *resolve_fn)(LPCSTR);
typedef const GLubyte *(APIENTRY *string_fn)(GLenum);
typedef void (APIENTRY *integer_fn)(GLenum, GLint *);

static PROC get_proc(resolve_fn resolve, const char *name)
{
    PROC p = resolve(name);
    return (ULONG_PTR)p <= 3 || p == (PROC)(INT_PTR)-1 ? NULL : p;
}

static void report_missing_extensions(resolve_fn resolve, integer_fn get_integer)
{
    static const char *const required[] = {
        "GL_ARB_gl_spirv", "GL_ARB_spirv_extensions", "GL_ARB_indirect_parameters",
        "GL_ARB_pipeline_statistics_query", "GL_ARB_polygon_offset_clamp",
        "GL_ARB_shader_atomic_counter_ops", "GL_ARB_shader_draw_parameters",
        "GL_ARB_shader_group_vote", "GL_ARB_texture_filter_anisotropic",
        "GL_ARB_transform_feedback_overflow_query"
    };
    PFNGLGETSTRINGIPROC indexed_string = (PFNGLGETSTRINGIPROC)get_proc(resolve, "glGetStringi");
    GLint count = 0;
    if (!indexed_string) return;
    get_integer(GL_NUM_EXTENSIONS, &count);
    for (unsigned int i = 0; i < sizeof(required)/sizeof(required[0]); ++i) {
        GLint j;
        for (j = 0; j < count; ++j) {
            const char *name = (const char *)indexed_string(GL_EXTENSIONS, j);
            if (name && !lstrcmpA(name, required[i])) break;
        }
        if (j == count) { report("Missing extension "); report(required[i]); report("\r\n"); }
    }
}

static BOOL test_compute(resolve_fn resolve, int minor, BOOL use_spirv)
{
    static const char code[] =
        "layout(local_size_x=4) in;"
        "layout(std430,binding=0) buffer Out {uint value[];};"
        "void main(){uint i=gl_GlobalInvocationID.x;value[i]=i*i+7u;}";
    const GLchar *source[] = {minor == 6 ? "#version 460 core\n" : "#version 450 core\n", code};
    const GLuint expected_addend = use_spirv ? 17u : 7u;
    GLuint values[16] = {0}, shader = 0, program = 0, buffer = 0;
    GLint status = GL_FALSE;
    BOOL pass = FALSE;
#define LOAD(type, name) type name = (type)get_proc(resolve, #name); if (!name) return FALSE
    LOAD(PFNGLCREATESHADERPROC, glCreateShader);
    LOAD(PFNGLSHADERSOURCEPROC, glShaderSource);
    LOAD(PFNGLCOMPILESHADERPROC, glCompileShader);
    LOAD(PFNGLGETSHADERIVPROC, glGetShaderiv);
    LOAD(PFNGLCREATEPROGRAMPROC, glCreateProgram);
    LOAD(PFNGLATTACHSHADERPROC, glAttachShader);
    LOAD(PFNGLLINKPROGRAMPROC, glLinkProgram);
    LOAD(PFNGLGETPROGRAMIVPROC, glGetProgramiv);
    LOAD(PFNGLUSEPROGRAMPROC, glUseProgram);
    LOAD(PFNGLGENBUFFERSPROC, glGenBuffers);
    LOAD(PFNGLBINDBUFFERPROC, glBindBuffer);
    LOAD(PFNGLBUFFERDATAPROC, glBufferData);
    LOAD(PFNGLBINDBUFFERBASEPROC, glBindBufferBase);
    LOAD(PFNGLDISPATCHCOMPUTEPROC, glDispatchCompute);
    LOAD(PFNGLMEMORYBARRIERPROC, glMemoryBarrier);
    LOAD(PFNGLGETBUFFERSUBDATAPROC, glGetBufferSubData);
    LOAD(PFNGLDELETEBUFFERSPROC, glDeleteBuffers);
    LOAD(PFNGLDELETEPROGRAMPROC, glDeleteProgram);
    LOAD(PFNGLDELETESHADERPROC, glDeleteShader);
#undef LOAD
    shader = glCreateShader(GL_COMPUTE_SHADER);
    if (use_spirv) {
        PFNGLSHADERBINARYPROC binary = (PFNGLSHADERBINARYPROC)get_proc(resolve, "glShaderBinary");
        PFNGLSPECIALIZESHADERPROC specialize = (PFNGLSPECIALIZESHADERPROC)get_proc(resolve, "glSpecializeShader");
        const GLuint constant_id = 0;
        if (!binary || !specialize) goto done;
        binary(1, &shader, GL_SHADER_BINARY_FORMAT_SPIR_V, wogl_spirv, sizeof(wogl_spirv));
        specialize(shader, "main", 1, &constant_id, &expected_addend);
    } else {
        glShaderSource(shader, 2, source, NULL);
        glCompileShader(shader);
    }
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) goto done;
    program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (!status) goto done;
    glUseProgram(program);
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(values), values, GL_DYNAMIC_READ);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffer);
    glDispatchCompute(4, 1, 1);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(values), values);
    pass = TRUE;
    for (GLuint i = 0; i < 16; ++i)
        if (values[i] != i * i + expected_addend) pass = FALSE;
done:
    if (buffer) glDeleteBuffers(1, &buffer);
    if (program) glDeleteProgram(program);
    if (shader) glDeleteShader(shader);
    return pass;
}

void WINAPI wgl_test_entry(void)
{
    int attributes[] = {WGL_CONTEXT_MAJOR_VERSION_ARB,4,WGL_CONTEXT_MINOR_VERSION_ARB,6,
            WGL_CONTEXT_PROFILE_MASK_ARB,WGL_CONTEXT_CORE_PROFILE_BIT_ARB,0};
    HMODULE gl;
    HGLRC bootstrap = NULL, core = NULL;
    HDC dc = NULL;
    HWND window = NULL;
    WNDCLASSA wc = {0};
    PIXELFORMATDESCRIPTOR pfd = {0};
    int result = 1;
    const char *command = GetCommandLineA();
    if (*command == '"') {
        ++command;
        while (*command && *command != '"') ++command;
        if (*command) ++command;
    } else {
        while (*command && *command != ' ' && *command != '\t') ++command;
    }
    while (*command == ' ' || *command == '\t') ++command;
    if (*command) {
        static const char option[] = "--gl45";
        const char *expected = option;
        while (*expected && *command == *expected) { ++command; ++expected; }
        while (*command == ' ' || *command == '\t') ++command;
        if (*expected || *command) { report("Usage: wogltest.exe [--gl45]\r\n"); ExitProcess(1); }
        attributes[3] = 5;
    }
    const int requested_minor = attributes[3];
    const char *target = requested_minor == 6 ? "OpenGL 4.6" : "OpenGL 4.5";
    initialize_test();
    gl = LoadLibraryA("opengl32.dll");
    if (!gl) { report("OpenGL library unavailable\r\n"); ExitProcess(1); }
    choose_fn choose = (choose_fn)GetProcAddress(gl, "wglChoosePixelFormat");
    set_fn set = (set_fn)GetProcAddress(gl, "wglSetPixelFormat");
    if (!choose || !set) {
        HMODULE gdi = LoadLibraryA("gdi32.dll");
        if (gdi) {
            choose = (choose_fn)GetProcAddress(gdi, "ChoosePixelFormat");
            set = (set_fn)GetProcAddress(gdi, "SetPixelFormat");
        }
    }
    create_fn create = (create_fn)GetProcAddress(gl, "wglCreateContext");
    current_fn current = (current_fn)GetProcAddress(gl, "wglMakeCurrent");
    delete_fn destroy = (delete_fn)GetProcAddress(gl, "wglDeleteContext");
    resolve_fn resolve = (resolve_fn)GetProcAddress(gl, "wglGetProcAddress");
    string_fn get_string = (string_fn)GetProcAddress(gl, "glGetString");
    integer_fn get_integer = (integer_fn)GetProcAddress(gl, "glGetIntegerv");
    if (!choose || !set || !create || !current || !destroy || !resolve || !get_string || !get_integer) goto done;
    wc.style = CS_OWNDC; wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "WINOGLCoreTest";
    if (!RegisterClassA(&wc)) goto done;
    window = CreateWindowA(wc.lpszClassName, "", WS_POPUP, 0, 0, 4, 4, NULL, NULL, wc.hInstance, NULL);
    if (!window || !(dc = GetDC(window))) goto done;
    pfd.nSize = sizeof(pfd); pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA; pfd.cColorBits = 32;
    int format = choose(dc, &pfd);
    if (!format || !set(dc, format, &pfd) || !(bootstrap = create(dc)) || !current(dc, bootstrap)) goto done;
    const GLubyte *version = get_string(GL_VERSION), *renderer = get_string(GL_RENDERER);
    if (!version || !renderer) goto done;
    report("Bootstrap GL "); report((const char *)version); report("\r\nRenderer ");
    report((const char *)renderer); report("\r\n");
    PFNWGLCREATECONTEXTATTRIBSARBPROC create_core =
            (PFNWGLCREATECONTEXTATTRIBSARBPROC)get_proc(resolve, "wglCreateContextAttribsARB");
    if (!create_core || !(core = create_core(dc, NULL, attributes))) {
        report(target); report(" core context unavailable\r\n"); result = 2; goto done;
    }
    if (!current(dc, core)) goto done;
    GLint major = 0, minor = 0, profile = 0;
    get_integer(GL_MAJOR_VERSION, &major); get_integer(GL_MINOR_VERSION, &minor);
    get_integer(GL_CONTEXT_PROFILE_MASK, &profile);
    if (major < 4 || (major == 4 && minor < requested_minor) || !(profile & GL_CONTEXT_CORE_PROFILE_BIT)) goto done;
    version = get_string(GL_VERSION);
    const GLubyte *language = get_string(GL_SHADING_LANGUAGE_VERSION);
    if (!version || !language) goto done;
    report("Core GL "); report((const char *)version);
    report("\r\nGLSL "); report((const char *)language); report("\r\n");
    if (requested_minor == 5) report_missing_extensions(resolve, get_integer);
    if (!test_compute(resolve, requested_minor, FALSE)) goto done;
    report(target); report(" GLSL compute/readback PASS\r\n");
    if (requested_minor == 6) {
        if (!test_compute(resolve, requested_minor, TRUE)) goto done;
        report("OpenGL 4.6 SPIR-V specialization/readback PASS\r\n");
    }
    result = 0;
done:
    if (bootstrap) current(NULL, NULL);
    if (core) destroy(core);
    if (bootstrap) destroy(bootstrap);
    if (dc) ReleaseDC(window, dc);
    if (window) DestroyWindow(window);
    if (result == 1) report("OpenGL validation failed\r\n");
    FreeLibrary(gl);
    ExitProcess(result);
}
