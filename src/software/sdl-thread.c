#include "common.h"
#include <stdio.h>

#include <SDL.h>
#include <SDL_thread.h>

#include <mm_malloc.h>

#include "tribuf.h"
#include "pallet.h"
#include "pixmisc.h"
#include "sdl-misc.h"
#include "map.h"
#include "audio/audio.h"

#define IM_SIZE (384)

static opt_data opts;
static int im_w = 0, im_h = 0;
static int running = 1;
static float map_fps=0;

MAP_FUNC_ATTR void soft_map_line_buff(uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd);

static int run_map_thread(tribuf *tb)
{
	struct point_data *pd = new_point_data(opts.rational_julia?4:2);
	unsigned int beats = beat_get_count();
	unsigned int tick0, fps_oldtime, frmcnt=0, last_beat_time = 0;
	tick0 = fps_oldtime = SDL_GetTicks();

	unsigned int fpstimes[40]; for(int i=0; i<40; i++) fpstimes[i] = 0;

	uint16_t *map_src = tribuf_get_read_nolock(tb);
    while(running) {
		frmcnt++;

		uint16_t *map_dest = tribuf_get_write(tb);
		if(!opts.rational_julia)
//			soft_map_line_buff(map_dest, map_src, im_w, im_h, pd);
			soft_map_interp(map_dest, map_src, im_w, im_h, pd);
		else // really want to do maxblend first here, but can't because we'd have to modify map_src and it's shipped off for reading
			soft_map_rational(map_dest, map_src, im_w, im_h, pd);

		maxblend(map_dest, maxsrc_get(), im_w, im_h);

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

		if(map_fps > 250)
			SDL_Delay(3); // hard limit ourselves to ~250FPS because 1500FPS is just pointless use of CPU (except of course to say that we can do it)
							// also if we run at more that 1000FPS the point motion code might blow up without the microsecond accurate timers...
							// high threshhold because we want it high enough that we don't notice if we jitter back
							// and fourth across it
    }
	return 0;
}

int main(int argc, char **argv)
{
	optproc(argc, argv, &opts);
	SDL_Surface *screen = sdl_setup(&opts, IM_SIZE);
	im_w = screen->w - screen->w%16; im_h = screen->h - screen->h%8;
	printf("running with %dx%d bufs\n", im_w, im_h);

	audio_init(&opts);

	maxsrc_setup(im_w, im_h);
	pallet_init(screen->format->BitsPerPixel == 8);

	uint16_t *map_surf[3];
	//void *map_surf_mem = valloc(3 * im_w * im_h * sizeof(uint16_t));
	void *map_surf_mem = _mm_malloc(3 * im_w * im_h * sizeof(uint16_t), 32);
	for(int i=0; i<3; i++)
		map_surf[i] = map_surf_mem + i * im_w * im_h * sizeof(uint16_t);
	memset(map_surf_mem, 0, 3 * im_w * im_h * sizeof(uint16_t));

	tribuf *map_tb = tribuf_new((void **)map_surf, 1);

	maxsrc_update();

	int beats = 0;
	int prevfrm = 0;
	int frmcnt = 0;
	int lastdrawn=0;
	int maxfrms = 0;
	Uint32 tick0 = SDL_GetTicks();
	Uint32 lastpalstep, lastupdate; lastpalstep = lastupdate = tick0;
	Uint32 fpstimes[opts.draw_rate]; for(int i=0; i<opts.draw_rate; i++) fpstimes[i] = tick0;
	float scr_fps = 0;

	SDL_Thread *map_thread = SDL_CreateThread((void *)&run_map_thread, map_tb);
	SDL_Event event;
	while(SDL_PollEvent(&event) >= 0)
	{
		int nextfrm = tribuf_get_frmnum(map_tb);
		if(event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
			break;
		} else if(lastdrawn < nextfrm) {
			lastdrawn = nextfrm;
			Uint32 now = SDL_GetTicks();
			if(now - lastpalstep >= 2048/256) { // want pallet switch to take ~2 seconds
				pallet_step(IMIN((now - lastpalstep)*256/2048, 64));
				lastpalstep = now;
			}
			//pallet_blit_SDL(screen, maxsrc_get(), im_w, im_h);
			pallet_blit_SDL(screen, tribuf_get_read(map_tb), im_w, im_h);
			tribuf_finish_read(map_tb);

			char buf[64];
			sprintf(buf,"%6.1f FPS %6.1f UPS", map_fps, scr_fps);
			DrawText(screen, buf);
			SDL_Flip(screen);

			int newbeat = beat_get_count();
			if(newbeat != beats) pallet_start_switch(newbeat);
			beats = newbeat;

			const unsigned int maxsrc_ps = opts.maxsrc_rate;
			now = SDL_GetTicks();

			if(tribuf_get_frmnum(map_tb) - prevfrm > 1 && (tick0+(maxfrms*1000)/maxsrc_ps) - now > 1000/maxsrc_ps) {
				maxsrc_update();
				maxfrms++;
				prevfrm = tribuf_get_frmnum(map_tb);
			}

			now = SDL_GetTicks();
			int delay =  (tick0 + frmcnt*1000/opts.draw_rate) - now;
			if(delay > 0) SDL_Delay(delay);
			float fpsd = now - fpstimes[frmcnt%opts.draw_rate];
			fpstimes[frmcnt%opts.draw_rate] = now;
			scr_fps = opts.draw_rate * 1000.0f/ fpsd;
			frmcnt++;
		} else  {
			SDL_Delay(1000/map_fps); // hope we gete a new frame
		}


	}
	running = 0;

	int status;
	SDL_WaitThread(map_thread, &status);

	_mm_free(map_surf_mem);

    return 0;
}
