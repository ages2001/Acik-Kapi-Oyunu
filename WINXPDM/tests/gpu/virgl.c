/* Submit a triangle to a virtio GPU through Linux DRM, GBM and EGL.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

static GLuint shader(GLenum type,const char *source)
{
    GLuint object=glCreateShader(type);
    GLint ok=0;
    glShaderSource(object,1,&source,NULL);
    glCompileShader(object);
    glGetShaderiv(object,GL_COMPILE_STATUS,&ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(object,sizeof(log),NULL,log);
        fprintf(stderr,"Shader compilation failed: %s\n",log);
        glDeleteShader(object);
        return 0;
    }
    return object;
}

int main(void)
{
    const EGLint config_attrs[]={EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,EGL_SURFACE_TYPE,0,EGL_NONE};
    const EGLint ctx_attrs[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};
    const GLfloat triangle[]={-1,-1,1,-1,-1,1};
    struct gbm_device *gbm=NULL;
    EGLDisplay display=EGL_NO_DISPLAY;
    EGLContext context=EGL_NO_CONTEXT;
    EGLConfig config;
    EGLint count;
    GLuint vs,fs,program,tex,fb;
    GLint status;
    GLubyte inside[4],outside[4];
    const char *renderer;
    int fd=open("/dev/dri/renderD128",O_RDWR),rc=1;
    if (fd<0) { perror("render node"); return 1; }
    gbm=gbm_create_device(fd);
    if (!gbm) goto done;
    display=eglGetPlatformDisplay(EGL_PLATFORM_GBM_KHR,gbm,NULL);
    if (display==EGL_NO_DISPLAY || !eglInitialize(display,NULL,NULL) ||
        !eglChooseConfig(display,config_attrs,&config,1,&count) || !count ||
        !eglBindAPI(EGL_OPENGL_ES_API)) goto done;
    context=eglCreateContext(display,config,EGL_NO_CONTEXT,ctx_attrs);
    if (context==EGL_NO_CONTEXT || !eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,context)) goto done;
    renderer=(const char*)glGetString(GL_RENDERER);
    printf("WXP GL_RENDERER %s\n",renderer?renderer:"unavailable");
    if (!renderer || !strstr(renderer,"virgl")) goto done;
    vs=shader(GL_VERTEX_SHADER,"attribute vec2 p; void main(){gl_Position=vec4(p,0.0,1.0);}");
    fs=shader(GL_FRAGMENT_SHADER,"precision mediump float; void main(){gl_FragColor=vec4(1.,0.,0.,1.);}");
    if (!vs || !fs) goto done;
    program=glCreateProgram();
    glAttachShader(program,vs); glAttachShader(program,fs);
    glBindAttribLocation(program,0,"p");
    glLinkProgram(program);
    glGetProgramiv(program,GL_LINK_STATUS,&status);
    if (!status) goto done;
    glGenTextures(1,&tex);
    glBindTexture(GL_TEXTURE_2D,tex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,32,32,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    glGenFramebuffers(1,&fb);
    glBindFramebuffer(GL_FRAMEBUFFER,fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tex,0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE) goto done;
    glViewport(0,0,32,32);
    glClearColor(0,0,1,1);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(program);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,triangle);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES,0,3);
    glReadPixels(4,4,1,1,GL_RGBA,GL_UNSIGNED_BYTE,inside);
    glReadPixels(27,27,1,1,GL_RGBA,GL_UNSIGNED_BYTE,outside);
    printf("WXP GL_PIXELS %u,%u,%u,%u %u,%u,%u,%u\n",inside[0],inside[1],inside[2],inside[3],
           outside[0],outside[1],outside[2],outside[3]);
    if (glGetError()==GL_NO_ERROR && !memcmp(inside,(GLubyte[]){255,0,0,255},4) &&
        !memcmp(outside,(GLubyte[]){0,0,255,255},4)) {
        puts("WXP VIRGL_RENDER PASS");
        rc=0;
    }
done:
    if (rc) fprintf(stderr,"VirGL drawing failed (EGL 0x%x)\n",eglGetError());
    if (display!=EGL_NO_DISPLAY) {
        eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
        if (context!=EGL_NO_CONTEXT) eglDestroyContext(display,context);
        eglTerminate(display);
    }
    if (gbm) gbm_device_destroy(gbm);
    close(fd);
    return rc;
}
