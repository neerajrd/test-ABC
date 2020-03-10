#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <math.h>

#include <bcm_host.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

typedef struct _AppCtx {

    EGLDisplay eglDisplay;
    EGLConfig eglConfig;
    EGLContext eglContext;
    EGLSurface eglSurfaceWindow;

    EGLint surfaceWidth;
    EGLint surfaceHeight;

} AppCtx;

float getIntensity(int frame, float rate) {
    float radian = 2 * M_PI * frame / rate;
    return 0.5 + 0.5 * sin(radian);
}

static void drawFrame(AppCtx *ctx) {

#if 0

    GLclampf red = ((float)rand()/(float)(RAND_MAX)) * 1.0;
    GLclampf green = ((float)rand()/(float)(RAND_MAX)) * 1.0;
    GLclampf blue = ((float)rand()/(float)(RAND_MAX)) * 1.0;

    glClearColor(red, green, blue, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glFlush();

    eglSwapBuffers(ctx->eglDisplay, ctx->eglSurfaceWindow);

#else

    static int frame = 0;
    frame++;

    glEnable(GL_SCISSOR_TEST);

    float radian = 2 * M_PI * frame / 600.0f;
    int offset_x = ctx->surfaceHeight * sin(radian) / 3.6;
    int offset_y = ctx->surfaceHeight * cos(radian) / 3.6;

    int block_width = ctx->surfaceWidth / 16;
    int block_height = ctx->surfaceWidth / 9;

    int center_x = (ctx->surfaceWidth - block_width) / 2;
    int center_y = (ctx->surfaceHeight - block_height) / 2;

    glScissor(center_x + offset_x, center_y + offset_y, block_width,
              block_height);

    glClearColor(getIntensity(frame, 55.0f), getIntensity(frame, 60.0f),
                 getIntensity(frame, 62.5f), 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glFlush();

    eglSwapBuffers(ctx->eglDisplay, ctx->eglSurfaceWindow);

#endif

}

static NativeWindowType createNativeWindow(AppCtx *ctx)
{
    uint32_t width, height;
    graphics_get_display_size(0, &width, &height);

    VC_RECT_T dst_rect;
    vc_dispmanx_rect_set(&dst_rect, 0, 0, width, height);

    VC_RECT_T src_rect;
    vc_dispmanx_rect_set(&src_rect, 0, 0, (width<<16), (height<<16));

    DISPMANX_DISPLAY_HANDLE_T dispman_display = vc_dispmanx_display_open(0);
    DISPMANX_UPDATE_HANDLE_T dispman_update = vc_dispmanx_update_start(0);
    DISPMANX_ELEMENT_HANDLE_T dispman_element =
        vc_dispmanx_element_add(dispman_update,
                                dispman_display,
                                0 /*layer*/,
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

#define RED_SIZE (8)
#define GREEN_SIZE (8)
#define BLUE_SIZE (8)
#define ALPHA_SIZE (8)
#define DEPTH_SIZE (0)

static void setupEGL(AppCtx *ctx) {

    ctx->eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(ctx->eglDisplay, NULL, NULL);

#if 1

    EGLint configCount = 0;
    eglChooseConfig(ctx->eglDisplay,
                    attrib_list, &ctx->eglConfig, 1, &configCount);
#else

    int i;
    EGLConfig *eglConfigs;
    EGLint configCount = 0;
    EGLint redSize, greenSize, blueSize, alphaSize, depthSize;

    eglGetConfigs(ctx->eglDisplay, NULL, 0, &configCount);
    eglConfigs = (EGLConfig*)malloc(configCount*sizeof(EGLConfig));

    eglChooseConfig(
        ctx->eglDisplay, attrib_list, eglConfigs, configCount, &configCount);
    for (i = 0; i < configCount; ++i) {
        eglGetConfigAttrib(ctx->eglDisplay,
                           eglConfigs[i], EGL_RED_SIZE, &redSize);
        eglGetConfigAttrib(ctx->eglDisplay,
                           eglConfigs[i], EGL_GREEN_SIZE, &greenSize);
        eglGetConfigAttrib(ctx->eglDisplay,
                           eglConfigs[i], EGL_BLUE_SIZE, &blueSize);
        eglGetConfigAttrib(ctx->eglDisplay,
                           eglConfigs[i], EGL_ALPHA_SIZE, &alphaSize);
        eglGetConfigAttrib(ctx->eglDisplay,
                           eglConfigs[i], EGL_DEPTH_SIZE, &depthSize);

        if ((redSize == RED_SIZE) && (greenSize == GREEN_SIZE) &&
            (blueSize == BLUE_SIZE) && (alphaSize == ALPHA_SIZE) &&
            (depthSize >= DEPTH_SIZE)) {
            break;
        }
    }

    if (i == configCount) {
        printf("No suitable configuration available\n");
    }

    ctx->eglConfig = eglConfigs[i];
    free(eglConfigs);

#endif

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

    while (1) {
        char c;
        scanf("%c", &c);
        if (c == 'x') {
            break;
        }
        drawFrame(&ctx);
    }

    destroySurface(&ctx);
    termEGL(&ctx);
}
