#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

#include <bcm_host.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <png.h>

typedef struct _AppCtx {

    EGLDisplay eglDisplay;
    EGLConfig eglConfig;
    EGLContext eglContext;
    EGLSurface eglSurfaceWindow;

    EGLint surfaceWidth;
    EGLint surfaceHeight;

    GLuint program;

    GLuint imageWidth;
    GLuint imageHeight;
    unsigned char *imageData;

    GLuint vertexShader;
    GLuint fragShader;
    GLuint position;
    GLuint texcoord;
    GLuint texture;

} AppCtx;

const char *vshader = R"(
       attribute vec4 position;
       attribute vec2 texcoord;
       varying vec2 texcoordVarying;
       void main() {
           gl_Position = position;
           texcoordVarying = texcoord;
       }
   )";

const char *fshader = R"(
       precision mediump float;
       varying vec2 texcoordVarying;
       uniform sampler2D texture;
       void main() {
           gl_FragColor = texture2D(texture, texcoordVarying);
       }
   )";

const float vertices[] = {
    -1.0f, 1.0f, 0.0f, -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, -1.0f, 0.0f
};
const float texcoords[] = {
    0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f
};

void setupGL(AppCtx *ctx) {

    ctx->vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(ctx->vertexShader, 1, &vshader, nullptr);
    glCompileShader(ctx->vertexShader);

    ctx->fragShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(ctx->fragShader, 1, &fshader, nullptr);
    glCompileShader(ctx->fragShader);

    ctx->program = glCreateProgram();
    glAttachShader(ctx->program, ctx->vertexShader);
    glAttachShader(ctx->program, ctx->fragShader);
    glLinkProgram(ctx->program);

    ctx->position = glGetAttribLocation(ctx->program, "position");
    glEnableVertexAttribArray(ctx->position);
    ctx->texcoord = glGetAttribLocation(ctx->program, "texcoord");
    glEnableVertexAttribArray(ctx->texcoord);

    glGenTextures(1, &ctx->texture);
    glBindTexture(GL_TEXTURE_2D, ctx->texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
}

void termGL(AppCtx *ctx) {

    glDeleteTextures(1, &ctx->texture);

    glDetachShader(ctx->program, ctx->vertexShader);
    glDetachShader(ctx->program, ctx->fragShader);

    glDeleteProgram(ctx->program);

    glDeleteShader(ctx->vertexShader);
    glDeleteShader(ctx->fragShader);
}

int loadPng(AppCtx *ctx, char *filename) {

    FILE *fp = fopen(filename, "rb");
    if (fp == nullptr) {
        printf("fopen failed\n");
        return (-1);
    }

    png_structp png = png_create_read_struct(
        PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    png_infop info = png_create_info_struct(png);

    png_init_io(png, fp);
    png_set_sig_bytes(png, 0);

    png_read_png(png, info, (PNG_TRANSFORM_STRIP_16 |
                             PNG_TRANSFORM_PACKING | PNG_TRANSFORM_EXPAND), nullptr);

    int bit_depth, color_type;
    png_get_IHDR(png, info, &ctx->imageWidth, &ctx->imageHeight,
                 &bit_depth, &color_type, NULL, NULL, NULL);

    unsigned int row_bytes = png_get_rowbytes(png, info);
    ctx->imageData = (unsigned char *)malloc(row_bytes * ctx->imageHeight);

    png_bytepp rows = png_get_rows(png, info);
    for (unsigned int i = 0; i < ctx->imageHeight; ++i) {
        memcpy(ctx->imageData + (row_bytes * i), rows[i], row_bytes);
    }

    png_destroy_read_struct(&png, &info, nullptr);
    fclose(fp);
    return (0);
}

void deletePng(AppCtx *ctx) {
    free(ctx->imageData);
}

static void drawImage(AppCtx *ctx, char *filename) {

    if (loadPng(ctx, filename) != 0) {

        GLclampf red = ((float)rand()/(float)(RAND_MAX)) * 1.0;
        GLclampf green = ((float)rand()/(float)(RAND_MAX)) * 1.0;
        GLclampf blue = ((float)rand()/(float)(RAND_MAX)) * 1.0;

        glClearColor(red, green, blue, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    else {

        glClearColor(0.0f, 0.0f, 0.0f, 0.1f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                     ctx->imageWidth, ctx->imageHeight,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, ctx->imageData);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glUseProgram(ctx->program);

        glVertexAttribPointer(ctx->texcoord, 2, GL_FLOAT, GL_FALSE, 0, texcoords);
        glVertexAttribPointer(ctx->position, 3, GL_FLOAT, GL_FALSE, 0, vertices);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        deletePng(ctx);
    }
    eglSwapBuffers(ctx->eglDisplay, ctx->eglSurfaceWindow);
}

static NativeWindowType createNativeWindow(AppCtx *ctx)
{
    uint32_t width = 200, height = 100;

    VC_RECT_T dst_rect;
    vc_dispmanx_rect_set(&dst_rect, 200, 100, width, height);

    VC_RECT_T src_rect;
    vc_dispmanx_rect_set(&src_rect, 0, 0, (width << 16), (height << 16));

    DISPMANX_DISPLAY_HANDLE_T dispman_display = vc_dispmanx_display_open(0);
    DISPMANX_UPDATE_HANDLE_T dispman_update = vc_dispmanx_update_start(0);
    DISPMANX_ELEMENT_HANDLE_T dispman_element =
        vc_dispmanx_element_add(dispman_update,
                                dispman_display,
                                1 /*layer*/,
                                &dst_rect,
                                0 /*src*/,
                                &src_rect,
                                DISPMANX_PROTECTION_NONE,
                                0 /*alpha*/,
                                0 /*clamp*/,
                                DISPMANX_NO_ROTATE);
    static EGL_DISPMANX_WINDOW_T nativewindow;
    nativewindow.element = dispman_element;
    nativewindow.width = width;
    nativewindow.height = height;
    vc_dispmanx_update_submit_sync(dispman_update);
    return (NativeWindowType)&nativewindow;
}

static void destroyNativeWindow(AppCtx *ctx) {
    /* TBD */
}

static void createSurface(AppCtx *ctx) {

    NativeWindowType native = createNativeWindow(ctx);
    ctx->eglSurfaceWindow = eglCreateWindowSurface(
        ctx->eglDisplay, ctx->eglConfig, native, NULL);
    eglMakeCurrent(
        ctx->eglDisplay, ctx->eglSurfaceWindow,
        ctx->eglSurfaceWindow, ctx->eglContext);

    eglQuerySurface(ctx->eglDisplay,
                    ctx->eglSurfaceWindow, EGL_WIDTH, &ctx->surfaceWidth);
    eglQuerySurface(ctx->eglDisplay,
                    ctx->eglSurfaceWindow, EGL_HEIGHT, &ctx->surfaceHeight);
}

static void destroySurface(AppCtx *ctx) {

    eglMakeCurrent(ctx->eglDisplay,
                   EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(ctx->eglDisplay, ctx->eglSurfaceWindow);
    destroyNativeWindow(ctx);
}

EGLint const attrib_list[] = {
    EGL_RED_SIZE,
    8,
    EGL_GREEN_SIZE,
    8,
    EGL_BLUE_SIZE,
    8,
    EGL_ALPHA_SIZE,
    8,
    EGL_STENCIL_SIZE,
    0,
    EGL_BUFFER_SIZE,
    32,
    EGL_SURFACE_TYPE,
    EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
    EGL_COLOR_BUFFER_TYPE,
    EGL_RGB_BUFFER,
    EGL_CONFORMANT,
    EGL_OPENGL_ES2_BIT,
    EGL_RENDERABLE_TYPE,
    EGL_OPENGL_ES2_BIT,
    EGL_NONE
};

EGLint context_attrib_list[] = {
    EGL_CONTEXT_CLIENT_VERSION,
    2,
    EGL_NONE,
};

static void setupEGL(AppCtx *ctx) {

    ctx->eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(ctx->eglDisplay, NULL, NULL);

    EGLint configCount = 0;
    eglChooseConfig(ctx->eglDisplay,
                    attrib_list, &ctx->eglConfig, 1, &configCount);

    ctx->eglContext = eglCreateContext(
        ctx->eglDisplay, ctx->eglConfig, EGL_NO_CONTEXT, context_attrib_list);
}

static void termEGL(AppCtx *ctx) {

    eglDestroyContext(ctx->eglDisplay, ctx->eglContext);
    eglTerminate(ctx->eglDisplay);
}

int main(void) {

    AppCtx ctx;
    memset(&ctx, 0, sizeof(AppCtx));

    bcm_host_init();
    setupEGL(&ctx);
    createSurface(&ctx);
    setupGL(&ctx);

    while (1) {
        char c;
        scanf("%c", &c);
        if (c == 'x') {
            break;
        }
        drawImage(&ctx, (char *)"png_transparency.png");
    }

    termGL(&ctx);
    destroySurface(&ctx);
    termEGL(&ctx);
}
