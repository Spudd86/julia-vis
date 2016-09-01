#include "common.h"

#include <time.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <poll.h>
#include <limits.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

#include "glmisc.h"
#include "glx_gen.h"

#include "audio/audio.h"
#include "fpsservo/fpsservo.h"

#define USE_GLX_INTEL_swap_event 1

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

#define GLX_BufferSwapCompleteINTEL 1

typedef struct {
	int type;
	unsigned long serial; /* # of last request processed by server */
	Bool send_event;      /* true if this came from a SendEvent request */
	Display *display;     /* Display the event was read from */
	GLXDrawable drawable; /* drawable on which event was requested in event mask */
	int event_type;       /* GLX_EXCHANGE_COMPLETE_INTEL, GLX_COPY_COMPLETE, GLX_FLIP_COMPLETE */
	int64_t ust;          /* UST from when the swap occurred */
	int64_t msc;          /* MSC from when the swap occurred */
	int64_t sbc;          /* SBC from when the swap occurred */
} GLXBufferSwapEventINTEL;
#endif /* GLX_INTEL_swap_event */

#define _NET_WM_STATE_REMOVE        0
#define _NET_WM_STATE_ADD           1
#define _NET_WM_STATE_TOGGLE        2

static struct fps_data *fps_data = NULL;

static Window root;
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

#define HIST_LEN 128
static int delayhist_total = 0;
static int delayhist[HIST_LEN];
static int framecnt = 0;

static struct fps_period swap_period;

static int debug_maxsrc = 0, debug_pal = 0, show_mandel = 0, show_fps_hist = 0;

static opt_data opts;

static int timer_fd = 0;

static Atom          wmPing;            // _NET_WM_PING atom
static Atom          wmState;           // _NET_WM_STATE atom
static Atom          wmStateFullscreen; // _NET_WM_STATE_FULLSCREEN atom
static Atom          wmActiveWindow;    // _NET_ACTIVE_WINDOW atom

static unsigned long getWindowProperty( Window window,
                                        Atom property,
                                        Atom type,
                                        unsigned char** value )
{
    Atom actualType;
    int actualFormat;
    unsigned long itemCount, bytesAfter;

    XGetWindowProperty( dpy,
                        window,
                        property,
                        0,
                        LONG_MAX,
                        False,
                        type,
                        &actualType,
                        &actualFormat,
                        &itemCount,
                        &bytesAfter,
                        value );

    if( actualType != type )
    {
        return 0;
    }

    return itemCount;
}

static Atom getSupportedAtom( Atom* supportedAtoms,
                              unsigned long atomCount,
                              const char* atomName )
{
    Atom atom = XInternAtom( dpy, atomName, True );
    if( atom != None )
    {
        unsigned long i;

        for( i = 0;  i < atomCount;  i++ )
        {
            if( supportedAtoms[i] == atom )
            {
                return atom;
            }
        }
    }

    return None;
}

static GLboolean checkForEWMH( void )
{
    Window *windowFromRoot = NULL;
    Window *windowFromChild = NULL;

    // Hey kids; let's see if the window manager supports EWMH!

    // First we need a couple of atoms, which should already be there
    Atom supportingWmCheck = XInternAtom( dpy,
                                          "_NET_SUPPORTING_WM_CHECK",
                                          True );
    Atom wmSupported = XInternAtom( dpy,
                                    "_NET_SUPPORTED",
                                    True );
    if( supportingWmCheck == None || wmSupported == None )
    {
        return GL_FALSE;
    }

    // Then we look for the _NET_SUPPORTING_WM_CHECK property of the root window
    if( getWindowProperty( root,
                           supportingWmCheck,
                           XA_WINDOW,
                           (unsigned char**) &windowFromRoot ) != 1 )
    {
        XFree( windowFromRoot );
        return GL_FALSE;
    }

    // It should be the ID of a child window (of the root)
    // Then we look for the same property on the child window
    if( getWindowProperty( *windowFromRoot,
                           supportingWmCheck,
                           XA_WINDOW,
                           (unsigned char**) &windowFromChild ) != 1 )
    {
        XFree( windowFromRoot );
        XFree( windowFromChild );
        return GL_FALSE;
    }

    // It should be the ID of that same child window
    if( *windowFromRoot != *windowFromChild )
    {
        XFree( windowFromRoot );
        XFree( windowFromChild );
        return GL_FALSE;
    }

    XFree( windowFromRoot );
    XFree( windowFromChild );

    // We are now fairly sure that an EWMH-compliant window manager is running

    Atom *supportedAtoms;
    unsigned long atomCount;

    // Now we need to check the _NET_SUPPORTED property of the root window
    atomCount = getWindowProperty( root,
                                   wmSupported,
                                   XA_ATOM,
                                   (unsigned char**) &supportedAtoms );

    // See which of the atoms we support that are supported by the WM

    wmState = getSupportedAtom( supportedAtoms,
                                         atomCount,
                                         "_NET_WM_STATE" );

    wmStateFullscreen = getSupportedAtom( supportedAtoms,
                                                   atomCount,
                                                   "_NET_WM_STATE_FULLSCREEN" );

    wmPing = getSupportedAtom( supportedAtoms,
                                        atomCount,
                                        "_NET_WM_PING" );

    wmActiveWindow = getSupportedAtom( supportedAtoms,
                                                atomCount,
                                                "_NET_ACTIVE_WINDOW" );

    XFree( supportedAtoms );

    return GL_TRUE;
}


int main(int argc, char **argv)
{
	optproc(argc, argv, &opts);
	if(audio_init(&opts) < 0) exit(1);
	int x = 0, y = 0, w, h;
	if(opts.w < 0 && opts.h < 0) opts.w = opts.h = 512;
	else if(opts.w < 0) opts.w = opts.h;
	else if(opts.h < 0) opts.h = opts.w;
	w = opts.w; h = opts.h;
	
	memset(delayhist, 0, sizeof(delayhist));
	
	XEvent event;
	
	dpy = XOpenDisplay( NULL );
	if(dpy == NULL) {
        fprintf(stderr, "Error: couldn't open display %s\n", getenv("DISPLAY"));
        exit(EXIT_FAILURE);
    }
    
    if(!glx_LoadFunctions(dpy, DefaultScreen(dpy))) {
    	fprintf(stderr, "Failed to load GLX extensions, exiting\n");
    	exit(EXIT_FAILURE);
    }
    
    int glx_major, glx_minor;
    if(!glXQueryVersion(dpy, &glx_major, &glx_minor)) {
    	fprintf(stderr, "GLX extension missing!\n");
    	XCloseDisplay(dpy);
    	exit(EXIT_FAILURE); 
    }
    printf("GLX version %i.%i\n", glx_major, glx_minor);

    int glxErrBase, glxEventBase;
    glXQueryExtension(dpy, &glxErrBase, &glxEventBase);
    printf("GLX: errorBase = %i, eventBase = %i\n", glxErrBase, glxEventBase);
    
    Window xwin = 0; root = DefaultRootWindow(dpy);
    int numReturned = 0;
    GLXFBConfig *fbConfigs = NULL;
    fbConfigs = glXChooseFBConfig( dpy, DefaultScreen(dpy), fbattrib, &numReturned );
    
   	if(fbConfigs == NULL) {  //TODO: handle this?
   		printf("No suitable fbconfigs!\n");
   		exit(EXIT_FAILURE);
   	}
   	
   	XVisualInfo  *vinfo = glXGetVisualFromFBConfig( dpy, fbConfigs[0] );
   	
   	bool haveEWMH = checkForEWMH();
   	
   	if( opts.fullscreen && haveEWMH)
   	{
   		h = opts.h = DisplayHeight(dpy, DefaultScreen(dpy));
   		w = opts.w = DisplayWidth(dpy, DefaultScreen(dpy));
   	}
   	
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
    //XIfEvent(dpy, &event, WaitForNotify, (XPointer) xwin);
    
    if( opts.fullscreen && haveEWMH)
    {
            // Ask the window manager to raise and focus our window
            // Only focused windows with the _NET_WM_STATE_FULLSCREEN state end
            // up on top of all other windows ("Stacking order" in EWMH spec)

            XEvent event;
            memset( &event, 0, sizeof(event) );

            event.type = ClientMessage;
            event.xclient.window = xwin;
            event.xclient.format = 32; // Data is 32-bit longs
            event.xclient.message_type = wmActiveWindow;
            event.xclient.data.l[0] = 1; // Sender is a normal application
            event.xclient.data.l[1] = 0; // We don't really know the timestamp

            XSendEvent( dpy,
                        root,
                        False,
                        SubstructureNotifyMask | SubstructureRedirectMask,
                        &event );

        // Ask the window manager to make our window a fullscreen window
        // Fullscreen windows are undecorated and, when focused, are kept
        // on top of all other windows

        //XEvent event;
        memset( &event, 0, sizeof(event) );

        event.type = ClientMessage;
        event.xclient.window = xwin;
        event.xclient.format = 32; // Data is 32-bit longs
        event.xclient.message_type = wmState;
        event.xclient.data.l[0] = _NET_WM_STATE_ADD;
        event.xclient.data.l[1] = wmStateFullscreen;
        event.xclient.data.l[2] = 0; // No secondary property
        event.xclient.data.l[3] = 1; // Sender is a normal application

        XSendEvent( dpy,
                    root,
                    False,
                    SubstructureNotifyMask | SubstructureRedirectMask,
                    &event );
    }
    
    glXMakeContextCurrent(dpy, glxWin, glxWin, context);
	
	const GLubyte *glx_ext_str = glXQueryExtensionsString(dpy, 0);
	
	bool have_glx_intel_swap_event = false;

#if USE_GLX_INTEL_swap_event
	if(strstr(glx_ext_str, "GLX_INTEL_swap_event")) {
		glXSelectEvent(dpy, glxWin, GLX_BUFFER_SWAP_COMPLETE_INTEL_MASK);
		have_glx_intel_swap_event = true;
    }
#endif

	if(strstr(glx_ext_str, "GLX_EXT_swap_control")) {
		glXSwapIntervalEXT(dpy, glxWin, 1);
	} else if(strstr(glx_ext_str, "GLX_SGI_swap_control")) {
		glXSwapIntervalSGI(1);
	}

	init_gl(&opts, w, h);
	
	{ // init fps servo
		int64_t ust, msc, sbc;
		
		glXGetMscRateOML(dpy, glxWin, &swap_period.n, &swap_period.d);
		
		int tmp = gcd(swap_period.n, swap_period.d);
		swap_period.n /= tmp; swap_period.d /= tmp;
		glXGetSyncValuesOML(dpy, glxWin, &ust, &msc, &sbc);
		glXWaitForMscOML(dpy, glxWin, msc + 1, 0, 0, &ust, &msc, &sbc);
		uint64_t now = uget_ticks();
		glXGetSyncValuesOML(dpy, glxWin, &ust, &msc, &sbc); // values of msc set by glXWaitForMscOML() seem to be loony toons
		fps_data = fps_data_new(swap_period, msc, now);
	}
	
	int xfd = ConnectionNumber(dpy);
	int tfd = timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);

	struct pollfd pfds[] = {
		{tfd, POLLIN, 0 },
		{xfd, POLLIN | POLLPRI, 0 },
	};
	
	arm_timer(tfd, 0);

	while(1) {
		int err;
		if( (err = poll(pfds, 2, 100)) < 0) continue;
		//if((err = poll(pfds, 2, -1)) < 0) continue;
		
		if(pfds[0].revents) {
			uint64_t now = uget_ticks();
			uint64_t timeouts;
			read(tfd, &timeouts, sizeof(timeouts));
			
			frame_start(fps_data, now);
			render_frame(debug_maxsrc, debug_pal, show_mandel, show_fps_hist);

			if(!have_glx_intel_swap_event) {
				int64_t ust, msc, sbc;
				glXGetSyncValuesOML(dpy, glxWin, &ust, &msc, &sbc);
				glXWaitForMscOML(dpy, glxWin, fps_get_target_msc(fps_data), 0, 0, &ust, &msc, &sbc); // Mesa seems to give us a junk value for msc in this function

				now = uget_ticks();
				glXGetSyncValuesOML(dpy, glxWin, &ust, &msc, &sbc);
				int delay = swap_complete(fps_data, now, msc, sbc);
				delayhist_total -= delayhist[framecnt%HIST_LEN];
				delayhist_total += (delayhist[framecnt%HIST_LEN] = delay);
				framecnt++;

				//TODO: if delay is 0 arrange to poll X events once and then immediately start next frame
				arm_timer(tfd, now+delay); // schedule next frame
			}
		}
		
		//if(!pfds[1].revents) continue;
	
		//int clear_key = 1;
		while (XPending(dpy) > 0) 
		{
			XNextEvent(dpy, &event);

			if(event.type == glxEventBase + GLX_BufferSwapCompleteINTEL) {
				GLXBufferSwapEventINTEL *swap_event = &event;

				int64_t now = uget_ticks();
				int delay = swap_complete(fps_data, now, swap_event->msc, swap_event->sbc);
							
				delayhist_total -= delayhist[framecnt%HIST_LEN];
				delayhist_total += (delayhist[framecnt%HIST_LEN] = delay);
				framecnt++;

				arm_timer(tfd, now+delay); // schedule next frame
				continue;
			}
			
			switch (event.type) {
				case Expose:
					/* we'll redraw when we feel like it :P */
				break;

				case KeyPress:
				{
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
				
				default: break;
			}
		}
	}
glx_main_loop_quit:
	
	XDestroyWindow(dpy, xwin);
	XCloseDisplay(dpy);
	
	return 0;
}

static void gl_hline(float y, float x1, float x2) {
	glVertex2f(x1, y); glVertex2f(x2, y);
}

//TODO: add hotkey to turn this on and off
void render_fps_hist(void)
{
	int64_t fpstotal, swaptotal, slacktotal, fpsdelaytotal, fpslen;
	const int *fpsworktimes = NULL;
	const int *fpsframetimes = NULL;
	const int *fpsdelays = NULL;
	const int *fpsslacks = NULL;
	fpslen = fps_get_hist_len(fps_data);
	fps_get_worktimes(fps_data, &fpstotal, &fpsworktimes);
	fps_get_frametimes(fps_data, &swaptotal, &fpsframetimes);
	fps_get_delays(fps_data, &fpsdelaytotal, &fpsdelays);
	fps_get_slacks(fps_data, &slacktotal, &fpsslacks);

	float scl = swap_period.n/(swap_period.d*1000000.0f);
	int frame_int = (int)(swap_period.d*INT64_C(1000000)/swap_period.n);
	
	int max_worktime = 0;
	int totslen = MIN(fpslen, HIST_LEN);
	int tots[MAX(fpslen, HIST_LEN)];
	for(int i=0; i < totslen; i++) {
		tots[totslen - i - 1] = fpsdelays[fpslen-i-1] + fpsworktimes[fpslen-i-1];
		max_worktime = MAX(max_worktime, fpsworktimes[fpslen-i-1]);
	}
	
	glColor3f(1.0f, 1.0f, 1.0f);
	char buf[256];
	sprintf(buf,
	        "AVG delay %7.1f\n"
	        "    delay %7.1f\n"
	        "    slack %7.1f\n"
	        "    swap  %7.1f\n"
	        " worktime %7.1f\n"
	        "  wrk max %d\n",
	        (float)delayhist_total/HIST_LEN,
	        (float)fpsdelaytotal/fpslen,
	        (float)slacktotal/fpslen,
	        (float)swaptotal/fpslen,
	        (float)fpstotal/fpslen,
	        max_worktime);
	draw_string(buf); DEBUG_CHECK_GL_ERR;

	// draw the nice big graph
	glPushMatrix();
	//glScalef(1.0f, 0.25f, 1.0f); glTranslatef(0, -4, 0);
	glScalef(1.0f, 0.5f, 1.0f); glTranslatef(0, -2, 0);
	
	float px = 1.0f/(opts.h*0.5f);
	glBegin(GL_QUADS);
	glColor3f(1.0f, 0.4f, 0.4f);gl_hline(1.0, 1.0f, -10*px);
	glColor3f(0.4f, 0.2f, 0.2f);gl_hline((frame_int-2000)*scl, -10*px, 1.0f);
	glEnd();
	glBegin(GL_LINES);
	//glColor3f(1.0f, 0.6f, 0.6f); gl_hline((frame_int-2000)*scl, -10*px, 1.0f);
	glColor3f(0.4f, 0.4f, 0.4f);
	for(int lnpos = frame_int-4000; lnpos >= 0; lnpos -= 2000) 
		gl_hline(lnpos*scl, -10*px, 1.0f);
	glColor3f(1.0f, 0.0f, 1.0f); gl_hline(scl*(frame_int - slacktotal/fpslen), -px, -9*px);
	glColor3f(0.0f, 1.0f, 0.0f); gl_hline(delayhist_total*scl/HIST_LEN, -px, -7*px);
	glColor3f(1.0f, 1.0f, 0.0f); gl_hline(fpstotal*scl/fpslen, -px, -5*px);
	glEnd();
	
	draw_hist_array_col(framecnt, scl, delayhist, HIST_LEN, 0.0f, 1.0f, 0.0f);
	draw_hist_array_col(framecnt, scl, fpsworktimes, fpslen, 1.0f, 1.0f, 0.0f);
	draw_hist_array_xlate(framecnt, -scl, frame_int*scl, fpsslacks, fpslen, 1.0f, 0.0f, 1.0f);
	draw_hist_array_col(framecnt, scl, tots, totslen, 0.0f, 1.0f, 1.0f);
	glPopMatrix();
	
	glColor3f(0.0f, 1.0f, 0.0f);
	glRasterPos2f(0.0f, -0.5f);// - 20.0f/(opts.h*0.5f));
	draw_string("delay"); DEBUG_CHECK_GL_ERR;
	
	glColor3f(1.0f, 0.0f, 1.0f);
	glRasterPos2f(0.0f, -0.5f);// - 20.0f/(opts.h*0.5f));
	draw_string("      -slack"); DEBUG_CHECK_GL_ERR;
	
	glColor3f(1.0f, 1.0f, 0.0f);
	glRasterPos2f(0.0f, -0.5f);// - 20.0f/(opts.h*0.5f));
	draw_string("             worktimes"); DEBUG_CHECK_GL_ERR;
	
	glColor3f(0.0f, 1.0f, 1.0f);
	glRasterPos2f(0.0f, -0.5f); 
	draw_string("                       wkt+delay"); DEBUG_CHECK_GL_ERR;
	
	// draw the others

/*	glPushMatrix();*/
/*	glScalef(0.5, 0.25, 1); glTranslatef(-2, -4, 0);*/
/*	draw_hist_array_xlate(framecnt, -scl, frame_int*scl, fpsslacks, fpslen, 1.0f, 0.0f, 0.0f);*/
/*	draw_hist_array_col(framecnt, scl, fpsdelays, fpslen, 0.0f, 1.0f, 0.0f);*/
/*	//for(int i=0; i < fpslen; i++) tots[i] = fpsdelays[i] + fpsworktimes[i];*/
/*	draw_hist_array_col(framecnt, scl, tots, fpslen, 0.0f, 1.0f, 1.0f);*/
/*	glPopMatrix();*/
/*	glColor3f(0.0f, 1.0f, 1.0f);*/
/*	glRasterPos2f(-1, -0.8); draw_string("wkt+delay"); DEBUG_CHECK_GL_ERR;*/
/*	glColor3f(1.0f, 0.0f, 0.0f);*/
/*	glRasterPos2f(-1, -0.8); draw_string("          -slack"); DEBUG_CHECK_GL_ERR;*/
	
	glColor3f(1.0f, 1.0f, 1.0f);
	glPushMatrix();
	glScalef(0.5f, 0.25f, 1.0f); glTranslatef(-2, -3, 0);
	glColor3f(0.6f, 0.6f, 0.6f); glBegin(GL_LINES); gl_hline(0.5f, 0.0f, 1.05f); glEnd();
	draw_hist_array_xlate(framecnt, scl/2, 0.5f, fpsframetimes, fpslen, 1.0f,0.0f,1.0f);
	glPopMatrix();
	
	glColor3f(1.0f, 0.0f, 1.0f);
	glRasterPos2f(-1, -0.75);
	draw_string("swaptimes"); DEBUG_CHECK_GL_ERR;

	glPushMatrix();
	glScalef(0.5, 0.25, 1); glTranslatef(-2, -2, 0);
	glColor3f(0.6f, 0.6f, 0.6f); glBegin(GL_LINES); gl_hline(0.5f, 0.0f, 1.05f); glEnd();
	float fps_scl_avg = fpslen*0.5f/MAX(fpstotal, fpslen);
	draw_hist_array_col(framecnt, fps_scl_avg, fpsworktimes, fpslen, 0.0f,1.0f,0.0f);
	glPopMatrix();
	
	glColor3f(0.0f, 1.0f, 0.0f);
	glRasterPos2f(-1, -0.25);
	draw_string("worktimes"); DEBUG_CHECK_GL_ERR;
	
	glColor3f(1.0f, 1.0f, 1.0f);
}

void render_debug_overlay(void)
{
	if(show_fps_hist) render_fps_hist();
}

#if 1
void swap_buffers(void)
{
	int64_t targetmsc = 0;
	if(fps_data) targetmsc = swap_begin(fps_data, uget_ticks());
	glXSwapBuffersMscOML(dpy, glxWin, targetmsc, 0, 0);
}
#elif 0 && !USE_GLX_INTEL_swap_event
void swap_buffers(void)
{
	int64_t targetmsc = 0;
	if(fps_data) targetmsc = swap_begin(fps_data, uget_ticks());

	int64_t targetsbc = glXSwapBuffersMscOML(dpy, glxWin, targetmsc, 0, 0);

	int64_t ust, msc, sbc;
	//glXWaitForMscOML(dpy, glxWin, targetmsc, 0, 0, &ust, &msc, &sbc); // the msc we get back from this function seems to be insane
	//glXGetSyncValuesOML(dpy, glxWin, &ust, &msc, &sbc);
	glXWaitForSbcOML(dpy, glxWin, targetsbc, &ust, &msc, &sbc);
	int64_t now = uget_ticks();
	int delay = swap_complete(fps_data, now, msc, sbc);
	delayhist_total -= delayhist[framecnt%HIST_LEN];
	delayhist_total += (delayhist[framecnt%HIST_LEN] = delay);
	framecnt++;
	arm_timer(timer_fd, now+delay);
}
#endif


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

