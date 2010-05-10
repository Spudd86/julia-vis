#include "common.h"
#include <stdio.h>
#include <mm_malloc.h>

#include <directfb.h>
#include <direct/clock.h>
#include <direct/thread.h>

#include "tribuf.h"
#include "pallet.h"
#include "pixmisc.h"
#include "map.h"
#include "audio/audio.h"

#define IM_SIZE (512)

static opt_data opts;
static IDirectFB *dfb = NULL;
static IDirectFBSurface *primary = NULL;
static DFBBoolean exclusive = DFB_FALSE;
static int im_w = 0, im_h = 0;
#define FPS_HIST_LEN 32
static float map_fps = 0.0f;

//~ static void* run_map_thread(void *parm)
static void* run_map_thread(DirectThread *thread, void *parm)
{
	tribuf *tb = parm;

	struct point_data *pd = new_point_data(opts.rational_julia?4:2);
	uint64_t tick0, last_beat_time = 0;
	uint64_t fpstimes[FPS_HIST_LEN]; for(int i=0; i<FPS_HIST_LEN; i++) fpstimes[i] = 0;
	uint32_t frmcnt = 0;
	uint32_t beats = 0;
	uint32_t maxfrms = 0, totframetime = 0, fps_oldtime;

	fps_oldtime = tick0 = direct_clock_get_abs_millis();
	uint16_t *map_src = tribuf_get_read_nolock(tb);
    while(1) {
//    	if((tick0-direct_clock_get_abs_millis())*opts.maxsrc_rate + maxfrms*1000 > 1000) {
//			maxsrc_update();
//			maxfrms++;
//		}

    	uint16_t *map_dest = tribuf_get_write(tb);
		if(!opts.rational_julia)
			soft_map_interp(map_dest, map_src, im_w, im_h, pd);
		else // really want to do maxblend first here, but can't because we'd have to modify map_src and it's shipped off for reading
			soft_map_rational(map_dest, map_src, im_w, im_h, pd);
		maxblend(map_dest, maxsrc_get(), im_w, im_h);
		tribuf_finish_write(tb);
		map_src=map_dest;

		uint64_t now = direct_clock_get_abs_millis() - tick0;
		totframetime -= fpstimes[frmcnt%FPS_HIST_LEN];
		totframetime += (fpstimes[frmcnt%FPS_HIST_LEN] = now - fps_oldtime);
		fps_oldtime = now;
		map_fps = FPS_HIST_LEN*1000.0f/totframetime;
		frmcnt++;

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
	DFBResult r = dfb->SetCooperativeLevel (dfb, DFSCL_EXCLUSIVE);
	if(r == DFB_ACCESSDENIED) printf("Failed to set exclusive!\n");
	else if(r != DFB_OK) DFBCHECK(r);
	exclusive = (r == DFB_OK);
//	DFBCHECK (dfb->SetCooperativeLevel (dfb, DFSCL_NORMAL));
	dsc.flags = DSDESC_CAPS;
	dsc.caps  = DSCAPS_PRIMARY;
	if(!exclusive) dsc.caps = dsc.caps | DSCAPS_FLIPPING;
	DFBCHECK (dfb->CreateSurface( dfb, &dsc, &primary ));
	DFBCHECK (primary->GetSize (primary, &im_w, &im_h));

//	primary->SetBlittingFlags();
//	IDirectFBScreen *scrn; scrn->AddRef;

	font_dsc.flags = DFDESC_HEIGHT;
	font_dsc.height = 16;
	DFBCHECK (dfb->CreateFont (dfb, "font.ttf", &font_dsc, &font));
	DFBCHECK (primary->SetFont (primary, font));
}

void pallet_blit_DFB(IDirectFBSurface *dst, const uint16_t * restrict src, int w, int h);

int main(int argc, char **argv)
{
	dfb_setup(argc, argv);
	optproc(argc, argv, &opts);
	if(opts.w < 0 && opts.h < 0) opts.w = opts.h = IM_SIZE;
	else if(opts.w < 0) opts.w = opts.h;
	else if(opts.h < 0) opts.h = opts.w;
	im_w = IMIN(opts.w, im_w); im_h = IMIN(opts.h, im_h);
	im_w = im_w - im_w%16; im_h = im_h - im_h%16;

	printf("running with %dx%d bufs\n", im_w, im_h);

	DFBSurfaceCapabilities prim_caps; primary->GetCapabilities(primary, &prim_caps);
	DFBSurfacePixelFormat pix_format; primary->GetPixelFormat(primary, &pix_format);


	audio_init(&opts);
	maxsrc_setup(im_w, im_h);
	pallet_init(0);
	maxsrc_update();

	uint16_t *map_surf[3];
	void *map_surf_mem = valloc(3 * im_w * im_h * sizeof(uint16_t));
	for(int i=0; i<3; i++)
		map_surf[i] = map_surf_mem + i * im_w * im_h * sizeof(uint16_t);
	memset(map_surf_mem, 0, 3 * im_w * im_h * sizeof(uint16_t));
	tribuf *map_tb = tribuf_new((void **)map_surf, 1);

	DirectThread *map_thread = direct_thread_create(DTT_DEFAULT, &run_map_thread, map_tb, "MAP_THREAD");

	uint32_t beats = 0, prevfrm = 0, maxfrms = 0, lastdrawn=0;
	uint64_t lastpalstep, lastupdate, lasttime; lastpalstep = lastupdate = lasttime = 0;
	uint64_t tick0 = direct_clock_get_abs_millis();

	IDirectFBEventBuffer *eb;
	DFBCHECK (dfb->CreateInputEventBuffer(dfb,  DICAPS_KEYS , DFB_FALSE, &eb));
	while(1) {
		DFBInputEvent event;
		DFBResult r = eb->HasEvent(eb);
		if(r == DFB_OK) {
			DFBCHECK(eb->GetEvent(eb, (DFBEvent *)&event));
			if((event.type == DIET_KEYPRESS && event.key_id == DIKI_ESCAPE))
				break;
		} else if(r != DFB_BUFFEREMPTY ) DFBCHECK(r);

		uint32_t now = direct_clock_get_abs_millis() - tick0;
		if(now - lastpalstep >= 2048/256) { // want pallet switch to take ~2 seconds
			pallet_step(IMIN((now - lastpalstep)*256/2048, 64));
			lastpalstep = now;
		}

		if(!(prim_caps & DSCAPS_FLIPPING) || exclusive) {
//				printf("waiting for vsync\n");
			DFBCHECK(dfb->WaitForSync(dfb)); // broken!
		}

		float fps = map_fps;
		fps = (fps>0.1f)?fps:0.1f;
		uint64_t nextfrm = tribuf_get_frmnum(map_tb);
		if(nextfrm != lastdrawn) { // skip drawing if nothing has changed
			pallet_blit_DFB(primary, tribuf_get_read(map_tb), im_w, im_h);
			tribuf_finish_read(map_tb);
			lastdrawn = nextfrm;

			char buf[32];
			sprintf(buf,"%6.1f FPS", fps);
			DFBCHECK (primary->SetColor (primary, 0xFF, 0xFF, 0xFF, 0xFF));
			primary->DrawString (primary, buf, -1, 0, 20, DSTF_LEFT);

			if(prim_caps & DSCAPS_FLIPPING) primary->Flip(primary, NULL, DSFLIP_WAITFORSYNC);

			if((tick0-direct_clock_get_abs_millis())*opts.maxsrc_rate + maxfrms*1000 > 1000) {
				maxsrc_update();
				maxfrms++;
			}

		} else {
			//TODO: need tp delay here a little bit so we don't burn CPU for no reason
			usleep(10000/(2*fps)); // 10 milliseconds
		}

		int newbeat = beat_get_count();
		if(newbeat != beats) pallet_start_switch(newbeat);
		beats = newbeat;
	}

	direct_thread_destroy(map_thread);
	audio_shutdown();

	font->Release (font);
	primary->Release (primary);
	dfb->Release (dfb);

    return 0;
}
