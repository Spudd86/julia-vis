#include "common.h"
#include <stdio.h>
#include <mm_malloc.h>

#include <directfb.h>
#include <direct/clock.h>
#include <direct/thread.h>

#include "tribuf.h"
#include "pixmisc.h"
#include "map.h"
#include "audio/audio.h"

#define IM_SIZE (512)

static opt_data opts;
static IDirectFB *dfb = NULL;
static IDirectFBSurface *primary = NULL;
static int im_w = 0, im_h = 0;

static float map_fps=0;

//~ static void* run_map_thread(void *parm)
static void* run_map_thread(DirectThread *thread, void *parm)
{
	tribuf *tb = parm;

	struct point_data *pd = new_point_data(opts.rational_julia?4:2);
	uint64_t tick0, last_beat_time = 0;
	uint64_t fpstimes[40]; for(int i=0; i<40; i++) fpstimes[i] = 0;

	tick0 = direct_clock_get_abs_millis();
	uint16_t *map_src = tribuf_get_read_nolock(tb);
    while(1)
	{
    	uint16_t *map_dest = tribuf_get_write(tb);
		if(!opts.rational_julia)
			soft_map_interp(map_dest, map_src, im_w, im_h, pd);
		else // really want to do maxblend first here, but can't because we'd have to modify map_src and it's shipped off for reading
			soft_map_rational(map_dest, map_src, im_w, im_h, pd);
		maxblend(map_dest, maxsrc_get(), im_w, im_h);
		tribuf_finish_write(tb);
		map_src=map_dest;

		uint64_t now = direct_clock_get_abs_millis() - tick0;

		float fpsd = (now - fpstimes[frmcnt%40])/1000.0f;
		fpstimes[frmcnt%40] = now;
		map_fps = 40.0f / fpsd;
		int newbeat = beat_get_count();
		if(newbeat != beats && now - last_beat_time > 1000) {
			last_beat_time = now;
			update_points(pd, now, 1);
		} else update_points(pd, now, 0);
		beats = newbeat;
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

	audio_init(&opts);
	maxsrc_setup(im_w, im_h);
	pallet_init(screen->format->BitsPerPixel == 8);
	maxsrc_update();

	uint16_t *map_surf[3];
	void *map_surf_mem = valloc(3 * im_w * im_h * sizeof(uint16_t));
	for(int i=0; i<3; i++)
		map_surf[i] = map_surf_mem + i * im_w * im_h * sizeof(uint16_t);
	memset(map_surf_mem, 0, 3 * im_w * im_h * sizeof(uint16_t));
	tribuf *map_tb = tribuf_new((void **)map_surf);

	DirectThread *map_thread =direct_thread_create(DTT_DEFAULT, &run_map_thread, map_tb, "MAP_THREAD");

	const int maxsrc_ps = opts.maxsrc_rate;
	uint32_t beats = 0, prevfrm = 0, frmcnt = 0, lastdrawn=0, maxfrms = 0;
	uint64_t lastpalstep, lastupdate, lasttime; lastpalstep = lastupdate = lasttime = 0;
	uint64_t fpstimes[opts.draw_rate]; for(int i=0; i<opts.draw_rate; i++) fpstimes[i] = tick0;
	uint64_t tick0 = direct_clock_get_abs_millis();
	float scr_fps = 0;

	IDirectFBEventBuffer *eb;
	DFBCHECK (dfb->CreateInputEventBuffer(dfb,  DICAPS_KEYS , DFB_FALSE, &eb));
	while(1) {
		DFBInputEvent event; // TODO: do this right...
		DFBResult r = eb->WaitForEventWithTimeout(eb, 0, 1000/60);

		uint64_t nextfrm = tribuf_get_frmnum(map_tb);
		if(r == DFB_TIMEOUT) {
			lastdrawn = nextfrm;
			uint32_t now = direct_clock_get_abs_millis() - tick0;
			if(now - lastpalstep >= 2048/256) { // want pallet switch to take ~2 seconds
				pallet_step(IMIN((now - lastpalstep)*256/2048, 64));
				lastpalstep = now;
			}

			pallet_blit_DFB(primary, tribuf_get_read(map_tb), im_w, im_h, pal);
			tribuf_finish_read(map_tb);

			char buf[32];
			sprintf(buf,"%6.1f FPS", map_fps);
			DFBCHECK (primary->SetColor (primary, 0x0, 0x0, 0x0, 0xFF));
			primary->DrawString (primary, buf, -1, 0, 10, DSTF_LEFT);

			int newbeat = beat_get_count();
			if(newbeat != beats) pallet_start_switch(newbeat);
			beats = newbeat;

			now = direct_clock_get_abs_millis() - tick0;
			if(tribuf_get_frmnum(map_tb) - prevfrm > 1 && (tick0+(maxfrms*1000)/maxsrc_ps) - now > 1000/maxsrc_ps) {
				maxsrc_update();
				maxfrms++;
				prevfrm = tribuf_get_frmnum(map_tb);
				lasttime = now;
			}

			now = direct_clock_get_abs_millis() - tick0;
			int delay =  (frmcnt*1000/opts.draw_rate) - now;
			if(delay > 0) usleep(delay*100); //TODO: check delay (needs *1000?)
			float fpsd = now - fpstimes[frmcnt%opts.draw_rate];
			fpstimes[frmcnt%opts.draw_rate] = now;
			scr_fps = opts.draw_rate * 1000.0f/ fpsd;
			frmcnt++;
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
