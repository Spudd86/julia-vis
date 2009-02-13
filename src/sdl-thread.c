#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <malloc.h>

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

static int run_map_thread(tribuf *tb) 
{
	float vx, vy, x, y, xt, yt;
	
	mt_goodseed(); // seed our PSRNG
	vx = vy = 0;
	xt = 0.5f*((mt_lrand()%im_w)*2.0f/im_w - 1.0f); yt = 0.5f*((mt_lrand()%im_h)*2.0f/im_h - 1.0f);
	x = 0.5f*((mt_lrand()%im_w)*2.0f/im_w - 1.0f); y = 0.5f*((mt_lrand()%im_h)*2.0f/im_h - 1.0f);
	
	Uint64 tick0, fps_oldtime;
	fps_oldtime = tick0 = SDL_GetTicks();
	float frametime = 100;
	int beats = beat_get_count();
	int done_time = tick0;
	unsigned int last_beat_time = tick0;
	
	uint16_t *map_src = tribuf_get_read(tb);
    while(running) 
	{
		uint16_t *map_dest = tribuf_get_write(tb);

		MAP(map_dest, map_src, im_w, im_h, x, y);
		maxblend(map_dest, maxsrc_get(), im_w, im_h);
		
		tribuf_finish_write(tb);
		map_src=map_dest;

		Uint32 now = SDL_GetTicks();

		frametime = 0.02f * (now - fps_oldtime) + (1.0f - 0.02f) * frametime;
		map_fps = 1000.0f / frametime;
		fps_oldtime = now;
		
		int newbeat = beat_get_count();
		if(newbeat != beats && now - last_beat_time > 1000) {
			xt = 0.5f*((mt_lrand()%im_w)*2.0f/im_w - 1.0f); yt = 0.5f*((mt_lrand()%im_h)*2.0f/im_h - 1.0f);
			last_beat_time = now;
		}
		beats = newbeat;
		
		const float delt = 30.0f/200.0f;
		const float tsped = 0.002;
		const int dt = 1000/200;
		
		while(done_time <= now) {
			float xtmp = xt - x, ytmp = yt-y;
			float mag = xtmp*xtmp+ytmp*ytmp;
				mag=(mag>0)?delt*0.4f/sqrtf(mag):0;
			vx=(vx+xtmp*mag*tsped)/(tsped+1);
			vy=(vy+ytmp*mag*tsped)/(tsped+1);
			x=x+vx*delt;
			y=y+vy*delt;
			
			done_time += dt;
		}
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
	
	tribuf *map_tb = tribuf_new((void **)map_surf);
	
	maxsrc_update();

	int beats = 0;
	int prevfrm = 0;
	int frmcnt = 0;
	int lastdrawn=0;
	Uint32 tick0 = SDL_GetTicks();
	Uint32 lasttime = tick0;
	
	SDL_Thread *map_thread = SDL_CreateThread(&run_map_thread, map_tb);
	SDL_Event event;
	while(SDL_PollEvent(&event) >= 0)
	{
		if(event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
			break;
		} else if(lastdrawn < tribuf_get_frmnum(map_tb)) {
			lastdrawn = tribuf_get_frmnum(map_tb);
			pallet_blit_SDL(screen, tribuf_get_read(map_tb), im_w, im_h);
			//pallet_blit_SDL(screen, maxsrc_get(), im_w, im_h);
			char buf[32];
			sprintf(buf,"%6.1f FPS", map_fps);
			DrawText(screen, buf);
			SDL_Flip(screen);
			
			int newbeat = beat_get_count();
			if(newbeat != beats) pallet_start_switch(newbeat%4);
			pallet_step(2);
			beats = newbeat;

			Uint64 now = SDL_GetTicks();
			if(tribuf_get_frmnum(map_tb) - prevfrm > 1 && now - lasttime > 1000/16) {
				maxsrc_update();
				prevfrm = tribuf_get_frmnum(map_tb);
				lasttime = now;
			}
		}
		
		Uint64 now = SDL_GetTicks();
		int delay =  (tick0 + frmcnt*1000/opts.draw_rate) - now;
		if(delay > 0) SDL_Delay(delay);
		frmcnt++;
	}
	running = 0;
	
	int status;
	SDL_WaitThread(map_thread, &status);

    return 0;
}

