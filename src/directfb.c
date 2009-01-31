#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <malloc.h>

//~ #include <pthread.h>
#include <sys/types.h>
//#include <sys/timerfd.h> // doesn't exist in my glibc :( (need glibc 2.8 have 2.6)
#include <time.h>
#include <signal.h>

#include <mm_malloc.h>

#include <directfb.h>
#include <direct/clock.h>
#include <direct/thread.h>

#include "tribuf.h"
#include "common.h"

#include "pixmisc.h"
#include "map.h"

#define IM_SIZE (512)

// set up source for maxblending
static uint16_t *setup_maxsrc(int w, int h) 
{
	uint16_t *max_src = valloc(w * h * sizeof(uint16_t));

	for(int y=0; y < h; y++)  {
		for(int x=0; x < w; x++) {
			float u = 2*(float)x/w - 1; float v = 2*(float)y/h - 1;
			float d = sqrtf(u*u + v*v);
			max_src[y*w + x] = (uint16_t)((1.0f - fminf(d, 0.25)*4)*UINT16_MAX);
		}
	}
	return max_src;
}

static opt_data opts;
static IDirectFB *dfb = NULL;
static IDirectFBSurface *primary = NULL;
static int im_w = 0, im_h = 0;
static uint16_t *max_src;
static float map_fps=0;

//~ static void* run_map_thread(void *parm)  
static void* run_map_thread(DirectThread *thread, void *parm) 
{
	tribuf *tb = parm;
	//~ int oldcanceltype;
	
	float t0 = 0, t1 = 0;
	
	long long tick0, fps_oldtime;
	fps_oldtime = tick0 = direct_clock_get_abs_millis();
	float frametime = 100;
	
	uint16_t *map_src = tribuf_get_read(tb);
    while(1) 
	{
		uint16_t *map_dest = tribuf_get_write(tb);

		soft_map_interp(map_dest, map_src, im_w, im_h, sin(t0), sin(t1));
		maxblend(map_dest, max_src, im_w, im_h);
		
		tribuf_finish_write(tb);
		map_src=map_dest;
		
		//~ int tmp;
		//~ pthread_setcanceltype(oldcanceltype, &tmp);
		long long now = direct_clock_get_abs_millis();
		//~ pthread_setcanceltype(tmp, &oldcanceltype);

		frametime = 0.02f * (now - fps_oldtime) + (1.0f - 0.02f) * frametime;
		map_fps = 1000.0f / frametime;
		float dt = (now - tick0) * 0.001f;
		t0=0.05f*dt; t1=0.35f*dt;

		fps_oldtime = now;
    }
	return NULL;
}

IDirectFBFont *font = NULL;

void dfb_setup(int argc, char **argv) 
{
	DFBFontDescription font_dsc;
	DFBSurfaceDescription dsc;
	
	DFBCHECK (DirectFBInit (&argc, &argv));
	DFBCHECK (DirectFBCreate (&dfb));
	DFBCHECK (dfb->SetCooperativeLevel (dfb, DFSCL_FULLSCREEN));
	dsc.flags = DSDESC_CAPS;
	dsc.caps  = DSCAPS_PRIMARY;
	DFBCHECK (dfb->CreateSurface( dfb, &dsc, &primary ));
	DFBCHECK (primary->GetSize (primary, &im_w, &im_h));
	
	font_dsc.flags = DFDESC_HEIGHT;
	font_dsc.height = 16;
	DFBCHECK (dfb->CreateFont (dfb, "font.ttf", &font_dsc, &font));
	DFBCHECK (primary->SetFont (primary, font));
}

int main(int argc, char **argv) 
{    
	dfb_setup(argc, argv);
	optproc(argc, argv, &opts);
	if(opts.w < 0 && opts.h < 0) opts.w = opts.h = IM_SIZE;
	else if(opts.w < 0) opts.w = opts.h;
	else if(opts.h < 0) opts.h = opts.w;
	im_w = IMIN(opts.w, im_w); im_h = IMIN(opts.h, im_h);
	im_w = im_w - im_w%8; im_h = im_h - im_h%8;
	
	printf("running with %dx%d bufs\n", im_w, im_h);
	
	max_src = setup_maxsrc(im_w, im_h);
	
	uint16_t *map_surf[3];
	map_surf[0] = valloc(im_w * im_h * sizeof(uint16_t));
	map_surf[1] = valloc(im_w * im_h * sizeof(uint16_t));
	map_surf[2] = valloc(im_w * im_h * sizeof(uint16_t));
	for(int i=0; i< im_w*im_h; i++) {
		map_surf[0][i] = max_src[i];
		map_surf[1][i] = max_src[i];
		map_surf[2][i] = max_src[i];
	}
	
	uint32_t *pal = _mm_malloc(257 * sizeof(uint32_t), 64); // p4 has 64 byte cache line
	for(int i = 0; i < 256; i++) pal[i] = ((2*abs(i-127))<<16) | (i<<8) | ((255-i));
	pal[256] = pal[255];

	tribuf *map_tb = tribuf_new((void **)map_surf);
	
	
	DirectThread *map_thread =direct_thread_create(DTT_DEFAULT, &run_map_thread, map_tb, "MAP_THREAD");
	//~ pthread_t map_thread;
	//~ pthread_create(&map_thread, NULL, &run_map_thread, map_tb);

	
	IDirectFBEventBuffer *eb;
	DFBCHECK (dfb->CreateInputEventBuffer(dfb,  DICAPS_KEYS , DFB_FALSE, &eb));
	
	
	//~ signal(
	//~ timer_t blit_timer;
	//~ if(!timer_create(CLOCK_REALTIME, NULL, &blit_timer)) { fprintf(stderr, "failed to create timer"); exit(1); }
	//~ struct itimerspec tm = { {0,1}, {0, 33333333} }; // 1/30 of a second
	//~ timer_settime(blit_timer, 0, &tm, NULL);
	
	
	while(1) {
		DFBInputEvent event;
		DFBResult r = eb->WaitForEventWithTimeout(eb, 0, 1000/60);
		
		if(r == DFB_TIMEOUT) {
			pallet_blit_DFB(primary, tribuf_get_read(map_tb), im_w, im_h, pal);
			char buf[32];
			sprintf(buf,"%6.1f FPS", map_fps);
			DFBCHECK (primary->SetColor (primary, 0x0, 0x0, 0x0, 0xFF));
			primary->DrawString (primary, buf, -1, 0, 10, DSTF_LEFT);
		} else if(r == DFB_OK) {
			DFBCHECK(eb->GetEvent(eb, (DFBEvent *)&event));
			if((event.type == DIET_KEYPRESS && event.key_id == DIKI_ESCAPE)) 
				break;
		} else DFBCHECK(r);
	}

	
	//~ pthread_cancel(map_thread);
	//~ pthread_join(map_thread, NULL);
	direct_thread_destroy(map_thread);
	
	font->Release (font);
	primary->Release (primary);
	dfb->Release (dfb);

    return 0;
}
