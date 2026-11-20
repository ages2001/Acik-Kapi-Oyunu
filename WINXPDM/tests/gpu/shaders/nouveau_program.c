/* Compile two graphics stages for the selected NVIDIA chipset with Mesa Nouveau.
 * The no-op DRM shim accepts command submissions without executing the draw.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/glcorearb.h>

/* Keep the GL dispatch table tied to the same Mesa build as the EGL context. */
#define FUNCTIONS(X) \
    X(glGetString) X(glGetError) X(glCreateShader) X(glShaderSource) \
    X(glCompileShader) X(glGetShaderiv) X(glGetShaderInfoLog) X(glCreateProgram) \
    X(glAttachShader) X(glLinkProgram) X(glGetProgramiv) X(glGetProgramInfoLog) \
    X(glGenVertexArrays) X(glBindVertexArray) X(glGenFramebuffers) X(glBindFramebuffer) \
    X(glGenTextures) X(glBindTexture) X(glTexStorage2D) X(glFramebufferTexture2D) \
    X(glCheckFramebufferStatus) X(glViewport) X(glUseProgram) X(glDrawArrays)

#define DECLARE(fn) __typeof__(&fn) fn;
static struct { FUNCTIONS(DECLARE) } gl;
#undef DECLARE

static int load_gl(void)
{
#define LOAD(fn) \
    gl.fn = (__typeof__(gl.fn))eglGetProcAddress(#fn); \
    if (!gl.fn) { fprintf(stderr, "Missing GL entry point %s\n", #fn); return 0; }
    FUNCTIONS(LOAD)
#undef LOAD
    return 1;
}

static GLuint compile(GLenum type, const char *path)
{
    char source[8192], log[1024];
    const char *input = source;
    GLint compiled = 0;
    FILE *file = fopen(path, "rb");
    if (!file) {
        perror(path);
        return 0;
    }
    size_t count = fread(source, 1, sizeof(source) - 1, file);
    int complete = feof(file) && !ferror(file);
    fclose(file);
    if (!complete)
        return 0;
    source[count] = 0;
    GLuint shader = gl.glCreateShader(type);
    gl.glShaderSource(shader, 1, &input, NULL);
    gl.glCompileShader(shader);
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        gl.glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        fprintf(stderr, "%s: %s\n", path, log);
        return 0;
    }
    return shader;
}

int main(int argc, char **argv)
{
    const char *chip = getenv("NOUVEAU_CHIPSET");
    const EGLint config_attrs[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_SURFACE_TYPE, 0, EGL_NONE};
    const EGLint context_attrs[] = {EGL_CONTEXT_MAJOR_VERSION, 4, EGL_CONTEXT_MINOR_VERSION, 3,
                                    EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
                                    EGL_NONE};
    EGLConfig config;
    EGLint count = 0;
    if (argc != 3 || !chip)
        return 2;
    setvbuf(stdout, NULL, _IONBF, 0);
    int fd = open("/dev/dri/renderD128", O_RDWR);
    if (fd < 0) {
        perror("Open DRM shim");
        return 1;
    }
    struct gbm_device *gbm = gbm_create_device(fd);
    if (!gbm) {
        perror("gbm_create_device");
        return 1;
    }
    EGLDisplay display = eglGetDisplay((EGLNativeDisplayType)gbm);
    if (!eglInitialize(display, NULL, NULL) || !eglChooseConfig(display, config_attrs, &config, 1, &count)
        || !count || !eglBindAPI(EGL_OPENGL_API)) {
        fprintf(stderr, "EGL initialization failed: 0x%x\n", eglGetError());
        return 1;
    }
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attrs);
    if (context == EGL_NO_CONTEXT || !eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context)
        || !load_gl()) {
        fprintf(stderr, "OpenGL 4.3 context failed: 0x%x\n", eglGetError());
        return 1;
    }
    const char *renderer = (const char *)gl.glGetString(GL_RENDERER);
    char expected[32];
    snprintf(expected, sizeof(expected), "NV%s", chip);
    printf("Nouveau chipset=%s renderer=%s\n", chip, renderer ? renderer : "(missing)");
    if (!renderer || strcasecmp(expected, renderer))
        return 1;

    GLuint vs = compile(GL_VERTEX_SHADER, argv[1]);
    GLuint fs = compile(GL_FRAGMENT_SHADER, argv[2]);
    if (!vs || !fs)
        return 1;
    GLuint program = gl.glCreateProgram();
    gl.glAttachShader(program, vs);
    gl.glAttachShader(program, fs);
    gl.glLinkProgram(program);
    GLint linked = 0;
    gl.glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[1024];
        gl.glGetProgramInfoLog(program, sizeof(log), NULL, log);
        fprintf(stderr, "%s\n", log);
        return 1;
    }

    GLuint vao, framebuffer, target, source;
    gl.glGenVertexArrays(1, &vao);
    gl.glBindVertexArray(vao);
    gl.glGenFramebuffers(1, &framebuffer);
    gl.glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    gl.glGenTextures(1, &target);
    gl.glBindTexture(GL_TEXTURE_2D, target);
    gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 16, 16);
    gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target, 0);
    if (gl.glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        return 1;
    gl.glGenTextures(1, &source);
    gl.glBindTexture(GL_TEXTURE_2D, source);
    gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 2, 2);
    gl.glViewport(0, 0, 16, 16);
    gl.glUseProgram(program);
    gl.glDrawArrays(GL_TRIANGLES, 0, 3);
    GLenum error = gl.glGetError();
    printf("Program linked, validation draw GL error=0x%x\n", error);
    return error != GL_NO_ERROR;
}
