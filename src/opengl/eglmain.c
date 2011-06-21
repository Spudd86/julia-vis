/**
 * eglmain.c
 *
 */

#include "common.h"

#include "egl/eglmisc.h"
#include "glmisc.h"

#include "audio/audio.h"

static EGLint const attribute_list[] = {
    EGL_RED_SIZE, 1,
    EGL_GREEN_SIZE, 1,
    EGL_BLUE_SIZE, 1,
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT, // only care about window surfaces
    EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
    EGL_NONE
};

static EGLDisplay display;
static EGLSurface surface;

int main(int argc, char **argv)
{
	opt_data opts; optproc(argc, argv, &opts);
	if(audio_init(&opts) < 0) exit(1);

    eglBindAPI(EGL_OPENGL_API);
	
	EGLNativeDisplayType native_dpy = egl_get_native_display();
	EGLint egl_major, egl_minor;
	display = eglGetDisplay(EGL_DEFAULT_DISPLAY); CHECK_EGL_ERR;
	if(display == EGL_NO_DISPLAY) {
		printf("eglGetDisplay failed!\n");
		return 1;
	}
	
	eglInitialize(display, &egl_major, &egl_minor); CHECK_EGL_ERR;
	
	printf("EGL API version: %d.%d\n", egl_major, egl_minor);
	printf("EGL vendor string: %s\n", eglQueryString(display, EGL_VENDOR));
	printf("EGL_VERSION = %s\n", eglQueryString(display, EGL_VERSION));
	
	/* get an appropriate EGL frame buffer configuration */
	EGLint num_config;
	EGLConfig config;
    if(!eglChooseConfig(display, attribute_list, &config, 1, &num_config)) {
    	CHECK_EGL_ERR;
    	printf("ChooseConfig failed!\n");
    	return 1;
    }
    
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, NULL); CHECK_EGL_ERR;
	
	EGLNativeWindowType native_window = egl_native_window(native_dpy, display, config, &opts, 512);
	if(!native_window) {
		printf("egl_native_window failed!\n");
		return 1;
	}
	
	surface = eglCreateWindowSurface(display, config, native_window, NULL); CHECK_EGL_ERR;
	if(surface  == EGL_NO_SURFACE) {
		printf("CreateWindowSurface failed!\n");
		return 1;
	}
	
	egl_map_window(display, native_window);
	
	eglMakeCurrent(display, surface, surface, context); CHECK_EGL_ERR;
	
	init_gl(&opts, opts.w, opts.h);

	int debug_maxsrc = 0, debug_pal = 0, show_mandel = 0, show_fps_hist = 0;
	int lastframe_key = 0;	
	while(1) {
		//TODO: event loop
		egl_event event;
		while(egl_event_poll(display, native_window, &event) > 0) {
			if(event == EKEY_ESC || event == E_QUIT)  goto egl_main_quit;
			if(event == EKEY_F1) { if(!lastframe_key) { debug_maxsrc = !debug_maxsrc; } lastframe_key = 1; }
			else if(event == EKEY_F2) { if(!lastframe_key) { debug_pal = !debug_pal; } lastframe_key = 1; }
			else if(event == EKEY_F3) { if(!lastframe_key) { show_mandel = !show_mandel; } lastframe_key = 1; }
			else if(event == EKEY_F4) { if(!lastframe_key) { show_fps_hist = !show_fps_hist; } lastframe_key = 1; }
			else lastframe_key = 0;
		}
		
		render_frame(debug_maxsrc, debug_pal, show_mandel, show_fps_hist);
	}

egl_main_quit:

	audio_shutdown();
	eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglTerminate(display);
	egl_destroy_native_window(display, native_window);
	egl_destroy_native_display(display);
	
	return 0;
}

void render_debug_overlay(void) {

}

void swap_buffers(void) {
	eglSwapBuffers(display, surface);
}

const char *egl_get_error_str(EGLint code)
{

static const char * const errors[] = {
	"Success (0x3000)",                 // No tr
	"Not initialized (0x3001)",         // No tr
	"Bad access (0x3002)",              // No tr
	"Bad alloc (0x3003)",               // No tr
	"Bad attribute (0x3004)",           // No tr
	"Bad config (0x3005)",              // No tr
	"Bad context (0x3006)",             // No tr
	"Bad current surface (0x3007)",     // No tr
	"Bad display (0x3008)",             // No tr
	"Bad match (0x3009)",               // No tr
	"Bad native pixmap (0x300A)",       // No tr
	"Bad native window (0x300B)",       // No tr
	"Bad parameter (0x300C)",           // No tr
	"Bad surface (0x300D)",             // No tr
	"Context lost (0x300E)"             // No tr
};

	if (code >= 0x3000 && code <= 0x300E) {
		return errors[code - 0x3000];
	} else {
		return "UNKNOWN ERROR";
	}
}
