#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#include "test_output.h"

typedef int (WINAPI *choose_fn)(HDC, const PIXELFORMATDESCRIPTOR *);
typedef BOOL (WINAPI *set_format_fn)(HDC, int, const PIXELFORMATDESCRIPTOR *);
typedef HGLRC (WINAPI *create_fn)(HDC);
typedef BOOL (WINAPI *current_fn)(HDC, HGLRC);
typedef BOOL (WINAPI *delete_fn)(HGLRC);
typedef const GLubyte *(APIENTRY *string_fn)(GLenum);
typedef void (APIENTRY *color_fn)(GLclampf, GLclampf, GLclampf, GLclampf);
typedef void (APIENTRY *clear_fn)(GLbitfield);
typedef void (APIENTRY *read_fn)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, GLvoid *);
typedef GLenum (APIENTRY *error_fn)(void);

void WINAPI wgl_test_entry(void)
{
    char path[MAX_PATH];
    HINSTANCE instance = GetModuleHandleA(NULL);
    HMODULE gl;
    WNDCLASSA wc = {0};
    HWND window;
    HDC dc;
    HGLRC context = NULL;
    PIXELFORMATDESCRIPTOR pfd = {0};
    BYTE pixels[4 * 3 * 4] = {0};
    UINT i, length = GetModuleFileNameA(instance, path, sizeof(path));
    int format;
    BOOL pass = FALSE;

    initialize_test();
    if (!length || length >= sizeof(path)) ExitProcess(1);
    while (length && path[length - 1] != '\\') --length;
    if (length + sizeof("opengl32.dll") > sizeof(path)) ExitProcess(1);
    lstrcpyA(path + length, "opengl32.dll");
    gl = LoadLibraryA(path);
    if (!gl) {
        report("Cannot load app-local OpenGL, error ");
        report_hex(GetLastError());
        report("\r\n");
        ExitProcess(1);
    }
    report("OpenGL module ");
    if (GetModuleFileNameA(gl, path, sizeof(path))) report(path);
    report("\r\n");
    choose_fn choose = (choose_fn)GetProcAddress(gl, "wglChoosePixelFormat");
    set_format_fn set_format = (set_format_fn)GetProcAddress(gl, "wglSetPixelFormat");
    create_fn create = (create_fn)GetProcAddress(gl, "wglCreateContext");
    current_fn make_current = (current_fn)GetProcAddress(gl, "wglMakeCurrent");
    delete_fn destroy = (delete_fn)GetProcAddress(gl, "wglDeleteContext");
    string_fn get_string = (string_fn)GetProcAddress(gl, "glGetString");
    color_fn set_color = (color_fn)GetProcAddress(gl, "glClearColor");
    clear_fn clear = (clear_fn)GetProcAddress(gl, "glClear");
    read_fn read_pixels = (read_fn)GetProcAddress(gl, "glReadPixels");
    error_fn gl_error = (error_fn)GetProcAddress(gl, "glGetError");
    if (!choose || !set_format || !create || !make_current || !destroy ||
            !get_string || !set_color || !clear || !read_pixels || !gl_error) {
        report("Missing WGL or GL entry point\r\n");
        ExitProcess(1);
    }
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = instance;
    wc.lpszClassName = "WindxWglTest";
    if (!RegisterClassA(&wc)) ExitProcess(1);
    window = CreateWindowExA(0, wc.lpszClassName, "WINDX WGL test", WS_POPUP,
            0, 0, 4, 3, NULL, NULL, instance, NULL);
    if (!window) ExitProcess(1);
    dc = GetDC(window);
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cAlphaBits = 8;
    format = choose(dc, &pfd);
    if (!format || !set_format(dc, format, &pfd) || !(context = create(dc)) ||
            !make_current(dc, context)) {
        report("Cannot create or bind WGL context, error ");
        report_hex(GetLastError());
        report("\r\n");
        goto done;
    }
    const GLubyte *renderer = get_string(GL_RENDERER), *version = get_string(GL_VERSION);
    if (!renderer || !version) goto done;
    report("GL renderer "); report((const char *)renderer); report("\r\nGL version ");
    report((const char *)version); report("\r\n");
    set_color(0, 1, 0, 1);
    clear(GL_COLOR_BUFFER_BIT);
    read_pixels(0, 0, 4, 3, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    pass = gl_error() == GL_NO_ERROR;
    for (i = 0; i < sizeof(pixels); i += 4)
        if (pixels[i] || pixels[i + 1] != 255 || pixels[i + 2] || pixels[i + 3] != 255)
            pass = FALSE;
done:
    if (context) {
        make_current(NULL, NULL);
        destroy(context);
    }
    ReleaseDC(window, dc);
    DestroyWindow(window);
    FreeLibrary(gl);
    report(pass ? "WGL clear/readback PASS\r\n" : "WGL clear/readback FAIL\r\n");
    ExitProcess(pass ? 0 : 1);
}
