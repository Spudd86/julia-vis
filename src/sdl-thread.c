#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <malloc.h>
#include <time.h>

#include "mtwist/mtwist.h"

#include <mm_malloc.h>

#include <SDL.h>
#include <SDL_thread.h>
#include <SDL_ttf.h>

#include "tribuf.h"
#include "common.h"

#include "pixmisc.h"
#include "sdl-misc.h"
#include "map.h"

#include "audio/audio.h"

#define IM_SIZE (384)

#define MAP soft_map_interp

static opt_data opts;
static int im_w = 0, im_h = 0;
static int running = 1;
static float map_fps=0;

#include <pthread.h>

static int run_map_thread(tribuf *tb) 
{
	//pthread_setschedprio(pthread_self(), 5);
	
	struct point_data *pd = new_point_data(2);
	
	Uint32 tick0, fps_oldtime, frmcnt=0;
	struct timespec tm0;
	clock_gettime(CLOCK_MONOTONIC, &tm0);
	tick0 = fps_oldtime = tm0.tv_sec*1000 + tm0.tv_nsec/1000000;
	int beats = beat_get_count();
	unsigned int last_beat_time = tick0;
	
	struct timespec fpstimes[40]; for(int i=0; i<40; i++) fpstimes[i] = tm0;
	
	uint16_t *map_src = tribuf_get_read_nolock(tb);
    while(running) 
	{
		frmcnt++;
		
		uint16_t *map_dest = tribuf_get_write(tb);
		
		MAP(map_dest, map_src, im_w, im_h, pd);
		maxblend(map_dest, maxsrc_get(), im_w, im_h);
		
		tribuf_finish_write(tb);
		map_src=map_dest;

		struct timespec tm;
		clock_gettime(CLOCK_MONOTONIC, &tm);
		Uint32 now = tm.tv_sec*1000 + tm.tv_nsec/1000000;
		
		float fpsd = (tm.tv_sec - fpstimes[frmcnt%40].tv_sec)+(tm.tv_nsec - fpstimes[frmcnt%40].tv_nsec)/1000000000.0f;
		fpstimes[frmcnt%40] = tm;
		map_fps = 40.0f / fpsd;
		
		const uint64_t del = (tm.tv_sec - tm0.tv_sec)*1000000 + (tm.tv_nsec - tm0.tv_nsec)/1000;
		int newbeat = beat_get_count();
		if(newbeat != beats && now - last_beat_time > 1000) {
			last_beat_time = now;
			update_points(pd, del, 1);
		} else update_points(pd, del, 0);
		beats = newbeat;
		
		if(map_fps > 250) 
			SDL_Delay(3); // hard limit ourselves to ~350FPS because 1500FPS is just pointless use of CPU (except of course as bragging rights that we can do it)
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
	void *map_surf_mem = valloc(3 * im_w * im_h * sizeof(uint16_t));
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
	Uint32 lastupdate, lasttime; lastupdate = lasttime = tick0;
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
			pallet_step(2);
			pallet_blit_SDL(screen, tribuf_get_read(map_tb), im_w, im_h);
			tribuf_finish_read(map_tb);
			
			char buf[64];
			sprintf(buf,"%6.1f FPS %6.1f UPS", map_fps, scr_fps);
			DrawText(screen, buf);
			SDL_Flip(screen);
			
			int newbeat = beat_get_count();
			if(newbeat != beats) pallet_start_switch(mt_lrand()%5);
			beats = newbeat;

			const int maxsrc_ps = opts.maxsrc_rate;
			Uint32 now = SDL_GetTicks();
			
			float fpsd = now - fpstimes[frmcnt%opts.draw_rate];
			fpstimes[frmcnt%opts.draw_rate] = now;
			scr_fps = opts.draw_rate * 1000.0f/ fpsd;
			
			if(tribuf_get_frmnum(map_tb) - prevfrm > 2 && (tick0+(maxfrms*1000)/maxsrc_ps) - now > 1000/maxsrc_ps) {
				maxsrc_update();
				maxfrms++;
				prevfrm = tribuf_get_frmnum(map_tb);
				lasttime = now;
			}
			
			now = SDL_GetTicks();
			int delay =  (tick0 + frmcnt*1000/opts.draw_rate) - now;
			if(delay > 0) SDL_Delay(delay);
			frmcnt++;
		} else  {
			SDL_Delay(1000/map_fps); // hope we gete a new frame
		}
		
		
	}
	running = 0;
	
	int status;
	SDL_WaitThread(map_thread, &status);

    return 0;
}

