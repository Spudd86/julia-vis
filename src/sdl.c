
#include "common.h"
#include <stdio.h>
#include <SDL.h>
#include <mm_malloc.h>

#include "map.h"
#include "sdl-misc.h"
#include "pixmisc.h"
#include "audio/audio.h"

#define MAP soft_map_interp

#define IM_SIZE (384)

static opt_data opts;

MAP_FUNC_ATTR void soft_map_line_buff(uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd);

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
	map_surf[0] = _mm_malloc(2 * im_w * im_h * sizeof(uint16_t), 32);
	//map_surf[0] = valloc(2 * im_w * im_h * sizeof(uint16_t));
	memset(map_surf[0], 0, 2 * im_w * im_h * sizeof(uint16_t));
	map_surf[1] = map_surf[0] + im_w * im_h;

	int m = 0, cnt = 0;

	struct point_data *pd = new_point_data(opts.rational_julia?4:2);

	Uint32 tick0, fps_oldtime;
	fps_oldtime = tick0 = SDL_GetTicks();
	float frametime = 100;
	int beats = beat_get_count();
	Uint32 last_beat_time = tick0;
	Uint32 lastpalstep = tick0;
	Uint32 now = tick0;
	Uint32  maxfrms = 0;

	SDL_Event	event;
	while(SDL_PollEvent(&event) >= 0) {
		if(event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) break;

		m = (m+1)&0x1;

		if(!opts.rational_julia) {
			soft_map_interp(map_surf[m], map_surf[(m+1)&0x1], im_w, im_h, pd);
//			soft_map_line_buff(map_surf[m], map_surf[(m+1)&0x1], im_w, im_h, pd);
			maxblend(map_surf[m], maxsrc_get(), im_w, im_h);
		}

		if(opts.rational_julia) {
			maxblend(map_surf[(m+1)&0x1], maxsrc_get(), im_w, im_h);
			//~ soft_map_butterfly(map_surf[m], map_surf[(m+1)&0x1], im_w, im_h, pd);
			soft_map_rational(map_surf[m], map_surf[(m+1)&0x1], im_w, im_h,  pd);
		}

		if((now - lastpalstep)*256/1024 >= 1) { // want pallet switch to take ~2 seconds
			pallet_step(IMIN((now - lastpalstep)*256/1024, 32));
			lastpalstep = now;
		}
		pallet_blit_SDL(screen, map_surf[m], im_w, im_h);

		char buf[32];
		sprintf(buf,"%6.1f FPS", 1000.0f / frametime);
		DrawText(screen, buf);
		SDL_Flip(screen);

		now = SDL_GetTicks();
		int newbeat = beat_get_count();
		if(newbeat != beats && !get_pallet_changing()) {
			pallet_start_switch(newbeat);
		}
		if(newbeat != beats && now - last_beat_time > 1000) {
			last_beat_time = now;
			update_points(pd, (now - tick0), 1);
		} else update_points(pd, (now - tick0), 0);
		beats = newbeat;

		if((tick0+(maxfrms*1000)/opts.maxsrc_rate) - now > 1000/opts.maxsrc_rate) {
			maxsrc_update();
			maxfrms++;
		}

		now = SDL_GetTicks();
		if(now - fps_oldtime < 10) SDL_Delay(10 - (now - fps_oldtime)); // stay below 1000FPS
		frametime = 0.02f * (now - fps_oldtime) + (1.0f - 0.02f) * frametime;
		fps_oldtime = SDL_GetTicks();
		cnt++;
	}

    return 0;
}
