/**
 * eglmisc.h
 *
 */

#ifndef EGLMISC_H_
#define EGLMISC_H_

#include <EGL/egl.h>
#include <EGL/eglext.h>

typedef enum {
	E_NO_EVENT,
	EKEY_F1,
	EKEY_F2,
	EKEY_F3,
	EKEY_F4,
	EKEY_ESC,
	E_QUIT
} egl_event;

EGLNativeDisplayType egl_get_native_display(void);
EGLNativeWindowType egl_native_window(EGLNativeDisplayType dpy, EGLDisplay edpy, EGLConfig conf, opt_data *opts, int im_size);
void egl_destroy_native_window(EGLNativeDisplayType dpy, EGLNativeWindowType win);
void egl_destroy_native_display(EGLNativeDisplayType dpy);
int egl_event_poll(EGLNativeDisplayType dpy, EGLNativeWindowType win, egl_event *ev);
void egl_map_window(EGLNativeDisplayType dpy, EGLNativeWindowType win);

const char *egl_get_error_str(EGLint err);


#define CHECK_EGL_ERR do { EGLint eglerr = eglGetError(); if(eglerr != EGL_SUCCESS)\
	fprintf(stderr, "%s: In function '%s':\n%s:%d: Warning: %s\n", \
		__FILE__, __func__, __FILE__, __LINE__, egl_get_error_str(eglerr)); \
		eglerr = eglGetError();\
	} while(0)

#endif

