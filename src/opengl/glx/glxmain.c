#include "common.h"

#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "opengl/glmisc.h"
#include "glx_gen.h"

// for some reason this is in glx.h but not glxew.h
//#define GLX_BufferSwapComplete	1

#include "audio/audio.h"

#ifndef GLX_INTEL_swap_event
# define GLX_INTEL_swap_event 1
# ifndef GLX_EXCHANGE_COMPLETE_INTEL
#  define GLX_EXCHANGE_COMPLETE_INTEL 0x8180
# endif
# ifndef GLX_COPY_COMPLETE_INTEL
#  define GLX_COPY_COMPLETE_INTEL 0x8181
# endif
# ifndef GLX_FLIP_COMPLETE_INTEL
#  define GLX_FLIP_COMPLETE_INTEL 0x8182
# endif
# ifndef GLX_BUFFER_SWAP_COMPLETE_INTEL_MASK
#  define GLX_BUFFER_SWAP_COMPLETE_INTEL_MASK 0x04000000
# endif
#endif

static Display *dpy = NULL;
static GLXWindow glxWin;

static const int fbattrib[] = {
    GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
    GLX_RENDER_TYPE,   GLX_RGBA_BIT,
    GLX_DOUBLEBUFFER,  True,  /* Request a double-buffered color buffer with */
    GLX_RED_SIZE,      1,     /* the maximum number of bits per component    */
    GLX_GREEN_SIZE,    1, 
    GLX_BLUE_SIZE,     1,
    None
};

static Bool WaitForNotify(Display *d, XEvent *e, char *arg) {
	(void)d;
	return (e->type == MapNotify) && (e->xmap.window == (Window)arg);
}

static GLboolean have_intel_swap_event = GL_FALSE;

int main(int argc, char **argv)
{
	opt_data opts; optproc(argc, argv, &opts);
	if(audio_init(&opts) < 0) exit(1);
	int x = 0, y = 0, w, h;
	if(opts.w < 0 && opts.h < 0) opts.w = opts.h = 512;
	else if(opts.w < 0) opts.w = opts.h;
	else if(opts.h < 0) opts.h = opts.w;
	w = opts.w; h = opts.h;
	
	XEvent event;
	
	dpy = XOpenDisplay( NULL );
	if(dpy == NULL) {
        printf("Error: couldn't open display %s\n", getenv("DISPLAY"));
        exit(EXIT_FAILURE);
    }
    
    int glx_major, glx_minor;
    if(!glXQueryVersion(dpy, &glx_major, &glx_minor)) {
    	printf("GLX extension missing!\n");
    	XCloseDisplay(dpy);
    	exit(EXIT_FAILURE); 
    }
    printf("GLX version %i.%i\n", glx_major, glx_minor);

    int glxErrBase, glxEventBase;
    glXQueryExtension(dpy, &glxErrBase, &glxEventBase);
    printf("GLX: errorBase = %i, eventBase = %i\n", glxErrBase, glxEventBase);
    
    
    Window xwin, root;
    int numReturned;
    GLXFBConfig *fbConfigs;
    fbConfigs = glXChooseFBConfig( dpy, DefaultScreen(dpy), fbattrib, &numReturned );
    
   	if(fbConfigs == NULL) {  //TODO: handle this?
   		printf("No suitable fbconfigs!\n");
   		exit(EXIT_FAILURE);
   	}
   	
   	XVisualInfo  *vinfo = glXGetVisualFromFBConfig( dpy, fbConfigs[0] );
   	
	root = DefaultRootWindow(dpy);
   	
   	/* window attributes */
   	XSetWindowAttributes attrs;
	attrs.background_pixel = 0;
	attrs.border_pixel = 0;
	attrs.colormap = XCreateColormap(dpy, root, vinfo->visual, AllocNone);
	//attrs.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask;
	attrs.event_mask = StructureNotifyMask | KeyPressMask;
	unsigned long mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;
	xwin = XCreateWindow(dpy, root, x, y, w, h,
		                 0, vinfo->depth, InputOutput,
		                 vinfo->visual, mask, &attrs);
	XFree(vinfo);
	
	// Set hints and properties:
	{
		XSizeHints sizehints;
		sizehints.x = x;
		sizehints.y = y;
		sizehints.width  = w;
		sizehints.height = h;
		sizehints.flags = USSize | USPosition;
		XSetNormalHints(dpy, xwin, &sizehints);
		XSetStandardProperties(dpy, xwin, "Julia-vis", "Julia-vis",
				               None, (char **)NULL, 0, &sizehints);
	}
   	
   	/* Create a GLX context for OpenGL rendering */
    GLXContext context = glXCreateNewContext(dpy, fbConfigs[0], GLX_RGBA_TYPE, NULL, True );
    
#if 0
	GLXContext context = 0;
	
	glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 0;
	glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)glXGetProcAddressARB( (const GLubyte *) "glXCreateContextAttribsARB" );

	if(strstr(glXQueryExtensionsString(dpy, 0), "GLX_ARB_create_context") || !glXCreateContextAttribsARB) {
		printf("glXCreateContextAttribsARB() not found ... using old-style GLX context\n");
		context = glXCreateNewContext(dpy, fbConfigs[0], GLX_RGBA_TYPE, 0, True);
	} else {
		const int context_attribs[] = {
			GLX_RENDER_TYPE, GLX_RGBA_TYPE,
			GLX_CONTEXT_MAJOR_VERSION_ARB, 2,
			GLX_CONTEXT_MINOR_VERSION_ARB, 1,
			None
		};
		context = glXCreateContextAttribsARB(dpy, fbConfigs[0], NULL, True, context_attribs);
    }
    
    if(context == NULL) {
    	printf("Failed to create context!\n");
    	return EXIT_FAILURE;
    }
#endif
    
    glxWin = glXCreateWindow(dpy, fbConfigs[0], xwin, NULL );
    
    XMapWindow(dpy, xwin);
    XIfEvent(dpy, &event, WaitForNotify, (XPointer) xwin);
    
    glXMakeContextCurrent(dpy, glxWin, glxWin, context);
	
	init_gl(&opts, w, h);
	
	if(strstr(glXQueryExtensionsString(dpy, 0), "GLX_MESA_swap_control")) {
		PFNGLXSWAPINTERVALMESAPROC swap_interval = glXGetProcAddressARB("glXSwapIntervalMESA");
		swap_interval(1);
		opts.draw_rate = 600;
	}
	
	if(strstr(glXQueryExtensionsString(dpy, 0), "GLX_INTEL_swap_event")) {
    	glXSelectEvent(dpy, glxWin, GLX_BUFFER_SWAP_COMPLETE_INTEL_MASK);
    	have_intel_swap_event = GL_TRUE;
    }

	int debug_maxsrc = 0, debug_pal = 0, show_mandel = 0, show_fps_hist = 0;
	
	if(have_intel_swap_event)
		render_frame(debug_maxsrc, debug_pal, show_mandel, show_fps_hist);
	
	while(1) {
		if(!have_intel_swap_event) render_frame(debug_maxsrc, debug_pal, show_mandel, show_fps_hist);
		
		//int clear_key = 1;
		while (XPending(dpy) > 0) 
		{
			XNextEvent(dpy, &event);
			
			if(event.type == glxEventBase + GLX_BufferSwapComplete) {
				render_frame(debug_maxsrc, debug_pal, show_mandel, show_fps_hist);
				continue;
			}
			
			switch (event.type) {
				case Expose:
				/* we'll redraw below */
				break;
				/*case ConfigureNotify:
				window_w = event.xconfigure.width;
				window_h = event.xconfigure.height;
				if (surface_type == EGL_WINDOW_BIT)
				reshape(window_w, window_h);
				break;*/
				case KeyPress:
				{
					//clear_key = 0;
					//char buffer[10];
					int code;
					code = XLookupKeysym(&event.xkey, 0);
					if (code == XK_F1) {
						debug_maxsrc = !debug_maxsrc;
					} else if (code == XK_F2) {
						debug_pal = !debug_pal;
					} else if (code == XK_F3) {
						show_mandel = !show_mandel;
					} else if (code == XK_F4) {
						show_fps_hist = !show_fps_hist;
					} else {
						code = XLookupKeysym(&event.xkey, 1);
						if(code == XK_Escape) {
							goto glx_main_loop_quit;
						}
					}
				}
				break;
				
				default:
					//printf("Bar %i!\n", event.type);
					
					break;
			}
		}
	}
glx_main_loop_quit:
	audio_shutdown();
	
	XDestroyWindow(dpy, xwin);
	XCloseDisplay(dpy);
	
	return 0;
}

void render_debug_overlay(void) {

}

void swap_buffers(void) {
	glXSwapBuffers( dpy, glxWin );
	//glXSwapBuffersMscOML(dpy, glxWin, 0, 0, 0);
}


//TODO: use same clock stuff as fpsmain.c? (clock_gettime)

static int timeval_subtract(struct timeval *result, struct timeval x, struct timeval y)
{
	/* Perform the carry for the later subtraction by updating y. */
	if(x.tv_usec < y.tv_usec) {
		int nsec = (y.tv_usec - x.tv_usec) / 1000000 + 1;
		y.tv_usec -= 1000000 * nsec;
		y.tv_sec += nsec;
	}
	if(x.tv_usec - y.tv_usec > 1000000) {
		int nsec = (x.tv_usec - y.tv_usec) / 1000000;
		y.tv_usec += 1000000 * nsec;
		y.tv_sec -= nsec;
	}

	/* Compute the time remaining to wait.
	tv_usec is certainly positive. */
	result->tv_sec = x.tv_sec - y.tv_sec;
	result->tv_usec = x.tv_usec - y.tv_usec;

	/* Return 1 if result is negative. */
	return x.tv_sec < y.tv_sec;
}

static struct timeval starttime = {0, 0};

uint64_t uget_ticks(void) {
	if(!starttime.tv_sec) {
		gettimeofday(&starttime, NULL);
		return 0;
	}

	struct timeval now, tv;
	gettimeofday(&now, NULL);
	timeval_subtract(&tv, now, starttime);
	return ((uint64_t)tv.tv_sec*1000*1000 + (uint64_t)tv.tv_usec);
}

void udodelay(uint64_t us) {
	(void)us;
	//TODO: use clock_nanosleep so we can sleep for an absolute time?
	//usleep(us);
}

uint32_t get_ticks(void) {
	return uget_ticks() / 1000;
	//return (uint32_t)(tv.tv_sec*1000 + tv.tv_usec/1000);
}

void dodelay(uint32_t ms) {
	(void)ms;
	//usleep(ms*1000);
}

