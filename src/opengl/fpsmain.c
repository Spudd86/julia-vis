#include "common.h"

#include <time.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <poll.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "glmisc.h"
#include <GL/glx.h>
#include <GL/glxext.h>

#include "audio/audio.h"
#include "fpsservo/fpsservo.h"

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

typedef struct {
	int event_type;	      /* GLX_EXCHANGE_COMPLETE_INTEL,
				 GLX_COPY_COMPLETE, GLX_FLIP_COMPLETE */
	unsigned long serial; /* # of last request processed by server */
	Bool send_event;      /* event was generated by a SendEvent request */
    Display *display;     /* display the event was read from */
    GLXDrawable drawable; /* i.d. of Drawable */
	uint64_t ust;           /* UST from when the swap occurred */
	uint64_t msc;           /* MSC from when the swap occurred */
	uint64_t sbc;           /* SBC from when the swap occurred */
} GLXBufferSwapEventINTEL;
#endif /* GLX_INTEL_swap_event */

static struct fps_data *fps_data = NULL;

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

int arm_timer(int tfd, int64_t time);

int gcd(int a, int b) {
	while(b != 0) {
		int t = b;
		b = a % b;
		a = t;
	}
	return a;
}

static Bool WaitForNotify(Display *d, XEvent *e, char *arg) {
	return (e->type == MapNotify) && (e->xmap.window == (Window)arg);
}

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
    
    Window xwin = 0, root = DefaultRootWindow(dpy);
    int numReturned = 0;
    GLXFBConfig *fbConfigs = NULL;
    fbConfigs = glXChooseFBConfig( dpy, DefaultScreen(dpy), fbattrib, &numReturned );
    
   	if(fbConfigs == NULL) {  //TODO: handle this?
   		printf("No suitable fbconfigs!\n");
   		exit(EXIT_FAILURE);
   	}
   	
   	XVisualInfo  *vinfo = glXGetVisualFromFBConfig( dpy, fbConfigs[0] );
   	
   	/* window attributes */
   	XSetWindowAttributes attrs;
	attrs.background_pixel = 0;
	attrs.border_pixel = 0;
	attrs.colormap = XCreateColormap(dpy, root, vinfo->visual, AllocNone);
	attrs.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask;
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
    
    glxWin = glXCreateWindow(dpy, fbConfigs[0], xwin, NULL );
    
    XMapWindow(dpy, xwin);
    XIfEvent(dpy, &event, WaitForNotify, (XPointer) xwin);
    
    glXMakeContextCurrent(dpy, glxWin, glxWin, context);
	
	init_gl(&opts, w, h);
	
	const GLubyte *glx_ext_str = glXQueryExtensionsString(dpy, 0);
	
	if(strstr(glx_ext_str, "GLX_MESA_swap_control")) {
		PFNGLXSWAPINTERVALMESAPROC swap_interval = glXGetProcAddressARB("glXSwapIntervalMESA");
		printf("INTERVAL SET\n");
		opts.draw_rate = 300;
		swap_interval(1);
	}
	if(strstr(glx_ext_str, "GLX_INTEL_swap_event")) {
    	glXSelectEvent(dpy, glxWin, GLX_BUFFER_SWAP_COMPLETE_INTEL_MASK);
    }
    
    // start off buffer swapping 
	glXSwapBuffers(dpy, glxWin);
	
	{ // init fps servo
		int64_t ust, msc, sbc;
		struct fps_period swap_period;
		glXGetMscRateOML(dpy, glxWin, &swap_period.n, &swap_period.d);
		glXGetSyncValuesOML(dpy, glxWin, &ust, &msc, &sbc);
		
		int tmp = gcd(swap_period.n, swap_period.d);
		swap_period.n /= tmp; swap_period.d /= tmp;
		fps_data = fps_data_new(swap_period, msc, uget_ticks());
	}
	
	int xfd = ConnectionNumber(dpy);
	int tfd = timerfd_create(CLOCK_MONOTONIC, 0);
	
	struct pollfd pfds[] = {
		{xfd, POLLIN, 0 },
		{tfd, POLLIN, 0 },
	};
	
	int debug_maxsrc = 0, debug_pal = 0, show_mandel = 0, show_fps_hist = 0;
	
	while(1) {
		if(poll(pfds, 2, -1) < 0) continue;
		
		if(pfds[1].revents) {
			uint64_t timeouts;
			read(tfd, &timeouts, sizeof(timeouts));			
			//printf("Render (timeouts = %" PRId64 ")\n", timeouts);
			render_frame(debug_maxsrc, debug_pal, show_mandel, show_fps_hist);
		}
		
		if(!pfds[0].revents) continue;
	
		int clear_key = 1;
		while (XPending(dpy) > 0) 
		{
			XNextEvent(dpy, &event);
			
			if(event.type == glxEventBase + GLX_BufferSwapComplete) {
				int64_t now = uget_ticks();
				
				//int64_t ust, msc, sbc;
				//glXGetSyncValuesOML(dpy, glxWin, &ust, &msc, &sbc);
				//int delay = swap_complete(fps_data, now, msc, sbc);
				GLXBufferSwapEventINTEL *swap_event = &event;
				int delay = swap_complete(fps_data, now, swap_event->msc, swap_event->sbc);
				
				//printf("swap_complete: delay = %d\n", delay);
				if(delay <= 0) render_frame(debug_maxsrc, debug_pal, show_mandel, show_fps_hist);
				else arm_timer(tfd, now+delay); // schedual next frame
				
				continue;
			}
			
			switch (event.type) {
				case Expose:
				/* we'll redraw when we feel like it :P */
				break;
				case KeyPress:
				{
					clear_key = 0;
					char buffer[10];
					int r, code;
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
	
	XDestroyWindow(dpy, xwin);
	XCloseDisplay(dpy);
	
	return 0;
}

void swap_buffers(void)
{
	int msc = 0;
	if(fps_data)
		swap_begin(fps_data, uget_ticks());
	//TODO: when intel swap buffer event isn't present wait for msc
	// then call swap complete
	glXSwapBuffersMscOML(dpy, glxWin, 0, 0, 0);
}

#define NSECS_IN_SEC INT64_C(1000000000)
static int timespec_subtract(struct timespec *result, struct timespec x, struct timespec y)
{
	/* Perform the carry for the later subtraction by updating y. */
	if(x.tv_nsec < y.tv_nsec) {
		int64_t secs = (y.tv_nsec - x.tv_nsec) / NSECS_IN_SEC + 1;
		y.tv_nsec -= NSECS_IN_SEC * secs;
		y.tv_sec += secs;
	}
	if(x.tv_nsec - y.tv_nsec > NSECS_IN_SEC) {
		int64_t secs = (x.tv_nsec - y.tv_nsec) / NSECS_IN_SEC;
		y.tv_nsec += NSECS_IN_SEC * secs;
		y.tv_sec -= secs;
	}

	/* Compute the time remaining to wait.
	tv_usec is certainly positive. */
	result->tv_sec = x.tv_sec - y.tv_sec;
	result->tv_nsec = x.tv_nsec - y.tv_nsec;

	/* Return 1 if result is negative. */
	return x.tv_sec < y.tv_sec;
}

static void timespec_add(struct timespec *result, struct timespec x, struct timespec y)
{
	int64_t nsec = x.tv_nsec + y.tv_nsec;
	result->tv_sec = x.tv_sec + y.tv_sec + nsec/NSECS_IN_SEC;
	result->tv_nsec = nsec%NSECS_IN_SEC;
}

static struct timespec starttime = {0, 0};

uint64_t uget_ticks(void) {
	if(!starttime.tv_sec) {
		clock_gettime(CLOCK_MONOTONIC, &starttime);
		return 0;
	}

	struct timespec now, tv;
	clock_gettime(CLOCK_MONOTONIC, &now);
	timespec_subtract(&tv, now, starttime);
	return (1000000*(int64_t)tv.tv_sec + (int64_t)tv.tv_nsec/1000);
}

void udodelay(uint64_t us) {
	//usleep(us);
}

uint32_t get_ticks(void) {
	return uget_ticks() / 1000;
}

void dodelay(uint32_t ms) {
	//usleep(ms*1000);
}

int arm_timer(int tfd, int64_t time) 
{
	struct timespec t = {time/1000000, (time%1000000)*1000};
	struct itimerspec new = {
		{0, 0},
		{0, 0}
	};
	
	timespec_add(&new.it_value, starttime, t);
	return timerfd_settime(tfd, TFD_TIMER_ABSTIME, &new, NULL);
}

