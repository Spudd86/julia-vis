/**
 * eglmain.c
 *
 */

#include "common.h"

#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "eglmisc.h"

EGLNativeDisplayType egl_get_native_display()
{
	Display *dpy = XOpenDisplay(NULL);
    if (dpy == NULL) {
        printf("Error: couldn't open display %s\n", getenv("DISPLAY"));
        exit(EXIT_FAILURE);
    }
	return dpy;
}

//TODO: figure out what this should return
EGLNativeWindowType egl_native_window(EGLNativeDisplayType ndpy, EGLDisplay edpy, EGLConfig conf, opt_data *opts, int im_size)
{
	Display *dpy = ndpy;
	XVisualInfo vinfo_template, *vinfo = NULL;
	EGLint val, num_vinfo;
	Window root, xwin;
	XSetWindowAttributes attrs;
	unsigned long mask;
	const char *name = "EGL Test";
	EGLint x = 0, y = 0, w, h;
	
	if(opts->w < 0 && opts->h < 0) opts->w = opts->h = im_size;
	else if(opts->w < 0) opts->w = opts->h;
	else if(opts->h < 0) opts->h = opts->w;
	w = opts->w; h = opts->h;
    
	if(!eglGetConfigAttrib(edpy, conf, EGL_NATIVE_VISUAL_ID, &val)) {
		printf("eglGetConfigAttrib() failed\n");
		goto egl_native_window_error;
	}
	if(val) {
		vinfo_template.visualid = (VisualID) val;
		vinfo = XGetVisualInfo(dpy, VisualIDMask, &vinfo_template, &num_vinfo);
	}
    
	if(!vinfo) {
		printf("XGetVisualInfo() failed\n");
		goto egl_native_window_error;
	}
    
    root = DefaultRootWindow(dpy);
    
	/* window attributes */
	attrs.background_pixel = 0;
	attrs.border_pixel = 0;
	attrs.colormap = XCreateColormap(dpy, root, vinfo->visual, AllocNone);
	//attrs.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask;
	attrs.event_mask = ExposureMask | KeyPressMask;
	attrs.override_redirect = EGL_FALSE;
	mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask | CWOverrideRedirect;

	xwin = XCreateWindow(dpy, root, x, y, w, h,
		                 0, vinfo->depth, InputOutput,
		                 vinfo->visual, mask, &attrs);
	XFree(vinfo);
	
	/* set hints and properties */
	{
		XSizeHints sizehints;
		sizehints.x = x;
		sizehints.y = y;
		sizehints.width  = w;
		sizehints.height = h;
		sizehints.flags = USSize | USPosition;
		XSetNormalHints(dpy, xwin, &sizehints);
		XSetStandardProperties(dpy, xwin, name, name,
				               None, (char **)NULL, 0, &sizehints);
	}
    
    return xwin;
    
egl_native_window_error:
	
	return 0;
}

void egl_map_window(EGLNativeDisplayType dpy, EGLNativeWindowType win) {
	XMapWindow(dpy, win);
}

void egl_destroy_native_window(EGLNativeDisplayType dpy, EGLNativeWindowType win)
{
	XDestroyWindow(dpy, win);
}

void egl_destroy_native_display(EGLNativeDisplayType dpy)
{
	XCloseDisplay(dpy);
}

int egl_event_poll(EGLNativeDisplayType dpy, EGLNativeWindowType win, egl_event *ev)
{
	int nevent = XPending(dpy);
	*ev = E_NO_EVENT;
	if(nevent > 0) {
		XEvent event;
		XNextEvent(dpy, &event);
		switch (event.type) {
			case Expose:
			/* we'll redraw below */
			break;
			case KeyPress:
			{
				char buffer[10];
				int r, code;
				code = XLookupKeysym(&event.xkey, 0);
				if(code == XK_F1) {
					*ev = EKEY_F1;
				} else if(code == XK_F2) {
					*ev = EKEY_F2;
				} else if(code == XK_F3) {
					*ev = EKEY_F3;
				} else if(code == XK_F4) {
					*ev = EKEY_F4;
				} else {
					r = XLookupString(&event.xkey, buffer, sizeof(buffer), NULL, NULL);
					if (buffer[0] == 27) {
						// escape
						*ev = EKEY_ESC;
					}
				}
			}
			default:
				break;
		}
	}
	return nevent - 1;
}

static struct timeval starttime = {0, 0};

uint32_t get_ticks(void) {

	//TODO:
	if(starttime.tv_sec) {
	}

	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (uint32_t)(tv.tv_sec*1000 + tv.tv_usec/1000);
}

void dodelay(uint32_t ms) {
	usleep(ms*1000);
	//SDL_Delay(ms);
}

