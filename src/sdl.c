
#include "common.h"
#include <stdio.h>
#include <SDL.h>

#include "map.h"
#include "sdl-misc.h"
#include "pixmisc.h"
#include "audio/audio.h"

#define MAP soft_map_interp

#define IM_SIZE (384)

static opt_data opts;

int main(int argc, char **argv) 
{    
	optproc(argc, argv, &opts);
	SDL_Surface *screen = sdl_setup(&opts, IM_SIZE);
	int im_w = screen->w - screen->w%16, im_h = screen->h - screen->h%8;
	printf("running with %dx%d bufs\n", im_w, im_h);
	
	audio_init(&opts);
	
	maxsrc_setup(im_w, im_h);
	pallet_init(screen->format->BitsPerPixel == 8);

	uint16_t *map_surf[2];
	map_surf[0] = valloc(2 * im_w * im_h * sizeof(uint16_t));
	memset(map_surf[0], 0, 2 * im_w * im_h * sizeof(uint16_t));
	map_surf[1] = map_surf[0] + im_w * im_h;
	
	int m = 0, cnt = 0;

	struct point_data *pd = new_point_data(4);
	
	Uint32 tick0, fps_oldtime;
	fps_oldtime = tick0 = SDL_GetTicks();
	float frametime = 100;
	int beats = beat_get_count();
	Uint32 last_beat_time = tick0;
	Uint32  maxfrms = 0;
	

	SDL_Event	event;
	while(SDL_PollEvent(&event) >= 0)
	{
		if(event.type == SDL_QUIT 
			|| (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE))
			break;
		
		//~ soft_map_butterfly(map_surf[(m+1)&0x1], map_surf[m], im_w, im_h, pd);
		//~ MAP(map_surf[(m+1)&0x1], map_surf[m], im_w, im_h, pd);
		//~ m = (m+1)&0x1; 
		//~ maxblend(map_surf[m], maxsrc_get(), im_w, im_h);
		MAP(map_surf[(m+1)&0x1], map_surf[m], im_w, im_h, pd);
		m = (m+1)&0x1; 
		maxblend(map_surf[m], maxsrc_get(), im_w, im_h);
		
		//~ soft_map_rational(map_surf[(m+1)&0x1], map_surf[m], im_w, im_h,  pd);
		//~ m = (m+1)&0x1;
		//~ maxblend(map_surf[m], maxsrc_get(), im_w, im_h);
		//~ soft_map_rational(map_surf[(m+1)&0x1], map_surf[m], im_w, im_h, pd);
		//~ m = (m+1)&0x1;
		
		pallet_step(2);
		pallet_blit_SDL(screen, map_surf[m], im_w, im_h);
		
		Uint32 now = SDL_GetTicks();
		int newbeat = beat_get_count();
		if(newbeat != beats) pallet_start_switch(newbeat%5);
		if(newbeat != beats && now - last_beat_time > 1000) {
			last_beat_time = now;
			update_points(pd, (now - tick0), 1);
		} else update_points(pd, (now - tick0), 0);
		beats = newbeat;
		
		char buf[32];
		sprintf(buf,"%6.1f FPS", 1000.0f / frametime);
		DrawText(screen, buf);
		
		SDL_Flip(screen);
		
		if((tick0+(maxfrms*1000)/opts.maxsrc_rate) - now > 1000/opts.maxsrc_rate) {
			maxsrc_update();
			maxfrms++;
		}
		cnt++;
		
		now = SDL_GetTicks();
		
		frametime = 0.02f * (now - fps_oldtime) + (1.0f - 0.02f) * frametime;
		fps_oldtime = now;
	}

    return 0;
}
