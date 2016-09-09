#include "common.h"

#include "tribuf.h"
#include "pallet.h"

#include "audio/audio.h"

#include "software/pixmisc.h"
#include "software/map.h"
#include "software/maxsrc.h"

#include <SDL.h>
#include <SDL_thread.h>

#include "sdlhelp.h"
#include "sdl/sdlsetup.h"

#ifdef _WIN32
#define set_threadname(name)
#elif defined(__linux__)
#include <sys/prctl.h>
void set_threadname(const char *name)
{
	prctl(PR_SET_NAME, name);
}
#else
#include <pthread.h>
void set_threadname(const char *name)
{
	pthread_t tid = pthread_self();
	pthread_setname_np(tid, name);
}
#endif

#define IM_SIZE (512)

static opt_data opts;
static int im_w = 0, im_h = 0;
static int running = 1;
static float map_fps=0;
uint32_t maxfrms = 0;

static struct maxsrc *maxsrc = NULL;
static soft_map_func map_func = soft_map_interp;

static int run_map_thread(tribuf *tb)
{
	set_threadname("render");

	struct point_data *pd = new_point_data(opts.rational_julia?4:2);
	unsigned int beats = beat_get_count();
	unsigned int tick0, fps_oldtime, frmcnt=0, last_beat_time = 0;
	tick0 = fps_oldtime = SDL_GetTicks();

	unsigned int fpstimes[40]; for(int i=0; i<40; i++) fpstimes[i] = 0;

	uint16_t *map_src = tribuf_get_read_nolock(tb);
    while(running) {
		frmcnt++;

		if((tick0-SDL_GetTicks())*opts.maxsrc_rate + (maxfrms*1000) > 1000) {
			audio_data ad; audio_get_samples(&ad);
			maxsrc_update(maxsrc, ad.data, ad.len);
			audio_finish_samples();
			maxfrms++;
		}

		uint16_t *map_dest = tribuf_get_write(tb);
		map_func(map_dest, map_src, im_w, im_h, pd);
		maxblend(map_dest, maxsrc_get(maxsrc), im_w, im_h);
		tribuf_finish_write(tb);
		map_src=map_dest;

		unsigned int now = SDL_GetTicks() - tick0;
		float fpsd = (now - fpstimes[frmcnt%40])/1000.0f;
		fpstimes[frmcnt%40] = now;
		map_fps = 40.0f / fpsd;

		unsigned int newbeat = beat_get_count();
		if(newbeat != beats && now - last_beat_time > 1000) {
			last_beat_time = now;
			update_points(pd, now, 1);
		} else update_points(pd, now, 0);
		beats = newbeat;

		if(map_fps > 750)
			SDL_Delay(1); // hard limit ourselves because 1500FPS is just pointless use of CPU (except of course to say that we can do it)
							// also if we run at more that 1000FPS the point motion code might blow up without the microsecond accurate timers...
							// high threshhold because we want it high enough that we don't notice if we jitter back
							// and fourth across it
    }
	return 0;
}

#define FPS_HIST_LEN 32

int main(int argc, char **argv)
{
	optproc(argc, argv, &opts); if(audio_init(&opts) < 0) exit(1);
	SDL_Surface *screen = sdl_setup(&opts, IM_SIZE);
	im_w = screen->w - screen->w%16; im_h = screen->h - screen->h%16;
	printf("running with %dx%d bufs\n", im_w, im_h);

	if(strcmp(opts.map_name, "rational") == 0) {
		map_func = soft_map_rational_interp;
		if(opts.quality >= 1)  map_func = soft_map_rational;
	}
	else if(strcmp(opts.map_name, "butterfly") == 0) {
//		map_func = soft_map_butterfly_interp;
//		if(opts.quality >= 1)
			map_func = soft_map_butterfly;
	}
	else if(opts.quality >= 1)  map_func = soft_map;

	maxsrc = maxsrc_new(im_w, im_h);
	struct pal_ctx *pal_ctx = pal_ctx_new(screen->format->BitsPerPixel == 8);

	uint16_t *map_surf[3];
	void *map_surf_mem = aligned_alloc(64, 3 * im_w * im_h * sizeof(uint16_t));
	for(int i=0; i<3; i++)
		map_surf[i] = map_surf_mem + i * im_w * im_h * sizeof(uint16_t);
	memset(map_surf_mem, 0, 3 * im_w * im_h * sizeof(uint16_t));

	tribuf *map_tb = tribuf_new((void **)map_surf, 1);

	int beats = 0;
	int frmcnt = 0;
	int lastdrawn=0;

	SDL_Thread *map_thread = SDL_CreateThread((void *)&run_map_thread, map_tb);
	SDL_Delay(100);

	uint32_t frametimes[FPS_HIST_LEN]; for(int i=1; i<FPS_HIST_LEN; i++) frametimes[i] = 0;
	uint32_t totframetime = frametimes[0] = MIN(1000/opts.draw_rate, 1);

	Uint32 tick0 = SDL_GetTicks();
	Uint32 lastpalstep, lastupdate, fps_oldtime; fps_oldtime = lastpalstep = lastupdate = tick0;
	SDL_Event event;
	while(SDL_PollEvent(&event) >= 0)
	{
		if(event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
			break;
		} else if(tribuf_check_fresh(map_tb)) {
			Uint32 now = SDL_GetTicks();
			if(now - lastpalstep >= 2048/256) { // want pallet switch to take ~2 seconds
				pal_ctx_step(pal_ctx, IMIN((now - lastpalstep)*256/2048, 64));
				lastpalstep = now;
			}

			pallet_blit_SDL(screen, tribuf_get_read(map_tb), im_w, im_h, pal_ctx_get_active(pal_ctx));
			tribuf_finish_read(map_tb);

			char buf[64];
			sprintf(buf,"%6.1f FPS %6.1f UPS %6.1f", map_fps, FPS_HIST_LEN*1000.0f/totframetime, maxfrms*1000.0f/(now-tick0));
			DrawText(screen, buf);
			SDL_Flip(screen);

			int newbeat = beat_get_count();
			if(newbeat != beats) pal_ctx_start_switch(pal_ctx, newbeat);
			beats = newbeat;

			now = SDL_GetTicks();
			int delay =  (tick0 + frmcnt*1000/opts.draw_rate) - now;
			if(delay > 0) SDL_Delay(delay);

			totframetime -= frametimes[frmcnt%FPS_HIST_LEN];
			totframetime += (frametimes[frmcnt%FPS_HIST_LEN] = now - fps_oldtime);
			fps_oldtime = now;
			frmcnt++;
		} else {
			SDL_Delay(1000/opts.draw_rate/2); // wait half a frame and hope we get a new buffer
		}
	}
	running = 0;

	int status;
	SDL_WaitThread(map_thread, &status);
	aligned_free(map_surf_mem);
	audio_shutdown();
	SDL_Quit();
    return 0;
}

