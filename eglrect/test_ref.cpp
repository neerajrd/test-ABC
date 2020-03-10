///
// OpenGLES First Window Display example.
//  Code stolen from belows:
//    https://jan.newmarch.name/LinuxSound/Diversions/RaspberryPiOpenGL/rectangle.c
//    https://code.google.com/archive/p/opengles-book-samples/source/default/source
//
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <bcm_host.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

typedef struct tagEsContext
{
    uint32_t width;
    uint32_t height;

    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
} EsContext;
EsContext state;

static void get_native_display_size(uint32_t* width, uint32_t* height)
{
    // get display size
    if (graphics_get_display_size(0 /*LCD*/,
                                  width,
                                  height) < 0) {
        perror("Failed to graphics_get_display_size");
        exit(EXIT_FAILURE);
    }
}

static NativeWindowType
create_native_window(uint32_t width, uint32_t height)
{
    VC_RECT_T dst_rect;
    vc_dispmanx_rect_set(&dst_rect, 0, 0, width, height);

    VC_RECT_T src_rect;
    vc_dispmanx_rect_set(&src_rect, 0, 0, (width<<16), (height<<16));

    DISPMANX_DISPLAY_HANDLE_T dispman_display = vc_dispmanx_display_open(0 /*LCD*/);
    DISPMANX_UPDATE_HANDLE_T dispman_update = vc_dispmanx_update_start(0);
    DISPMANX_ELEMENT_HANDLE_T dispman_element =
        vc_dispmanx_element_add(dispman_update,
                                dispman_display,
                                0/*layer*/,
                                &dst_rect,
                                0/*src*/,
                                &src_rect,
                                DISPMANX_PROTECTION_NONE,
                                0/*alpha*/,
                                0/*clamp*/,
                                DISPMANX_NO_ROTATE);
    static EGL_DISPMANX_WINDOW_T nativewindow;
    nativewindow.element = dispman_element;
    nativewindow.width = width;
    nativewindow.height = height;
    vc_dispmanx_update_submit_sync(dispman_update);
    return (NativeWindowType)&nativewindow;
}

void init_ogl(EsContext *state)
{
    // get an EGL display connection
    state->display = eglGetDisplay(state->display);

    // initialize the EGL display connection
    if (eglInitialize(state->display, NULL, NULL) == EGL_FALSE) {
        perror("Failed to eglInitialize");
        exit(EXIT_FAILURE);
    }

    // get an appropriate EGL frame buffer configuration
    static const EGLint attribute_list[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_BUFFER_SIZE, 8,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_NONE
    };
    EGLint num_config = 0;
    EGLConfig config;
    if (eglChooseConfig(state->display, attribute_list,
                        &config, 1, &num_config) == EGL_FALSE) {
        perror("Failed to eglChooseConfig");
        exit(EXIT_FAILURE);
    }

    // bind to OpenGLES API
    if (eglBindAPI(EGL_OPENGL_ES_API) == EGL_FALSE) {
        perror("Failed to eglBindAPI");
        exit(EXIT_FAILURE);
    }

    // create an EGL rendering context
    static const EGLint context_attributes[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    state->context = eglCreateContext(state->display, config,
                                      EGL_NO_CONTEXT, context_attributes);
    if (state->context == EGL_NO_CONTEXT) {
        perror("Failed to eglCreateContext");
        exit(EXIT_FAILURE);
    }

    // create an EGL Window surface
    get_native_display_size(&state->width, &state->height);
    NativeWindowType nativewindow =
        create_native_window(state->width, state->height);
    state->surface = eglCreateWindowSurface(state->display, config,
                                            nativewindow, NULL);
    if (state->surface == EGL_NO_SURFACE) {
        perror("Failed to eglCreateWindowSurface");
        exit(EXIT_FAILURE);
    }

    // connect the context to the surface
    if (eglMakeCurrent(state->display, state->surface,
                       state->surface, state->context) == EGL_FALSE) {
        perror("Failed to eglMakeCurrent");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char* argv[])
{
    bcm_host_init();
    init_ogl(&state);

    glClearColor(1.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glFlush();

    eglSwapBuffers(state.display, state.surface);

    while (1) {
        sleep(10);
    }
    return 0;
}
