
#include "common.h"

#include "pallet.h"

#include "audio/audio.h"

#include "software/pixmisc.h"
#include "software/maxsrc.h"
#include "software/map.h"

#include <SDL.h>
#include "sdl/sdlsetup.h"
#include "sdlhelp.h"


//TODO: switch to using the julia_vis_pixel_format based pallet init function

#define IM_SIZE (512)

static opt_data opts;

static soft_map_func map_func = soft_map_interp;

int main(int argc, char **argv)
{
	optproc(argc, argv, &opts);
	if(strcmp(opts.map_name, "rational") == 0) {
		if(opts.quality == 0) map_func = soft_map_rational_interp;
		else if(opts.quality >= 1)  map_func = soft_map_rational;
	}
	else if(strcmp(opts.map_name, "butterfly") == 0) map_func = soft_map_butterfly;
	else if(opts.quality >= 1)  map_func = soft_map;
	if(audio_init(&opts) < 0) exit(1);
	SDL_Surface *screen = sdl_setup(&opts, IM_SIZE);
	int im_w = screen->w - screen->w%16, im_h = screen->h - screen->h%16;
	printf("running with %dx%d bufs\n", im_w, im_h);

	uint16_t *map_surf[2];
	map_surf[0] = aligned_alloc(512, 512 + im_w * im_h * sizeof(uint16_t));
	memset(map_surf[0], 0, im_w * im_h * sizeof(uint16_t));
	map_surf[1] = aligned_alloc(512, 512 + im_w * im_h * sizeof(uint16_t));
	memset(map_surf[0], 0, im_w * im_h * sizeof(uint16_t));

	int m = 0, cnt = 0;

	struct maxsrc *maxsrc = maxsrc_new(im_w, im_h);
	struct pal_ctx *pal_ctx = pal_ctx_new(screen->format->BitsPerPixel == 8); //TODO: write a helper that picks the right pallet channel order based on screen->format
	struct point_data *pd = new_point_data(opts.rational_julia?4:2);

	Uint32 tick0, fps_oldtime;
	fps_oldtime = tick0 = SDL_GetTicks();
	float frametime = 100;
	int beats = beat_get_count();
	uint32_t last_beat_time = tick0;
	uint32_t lastpalstep = tick0;
	uint32_t now = tick0;
	uint32_t maxfrms = 0;

	int debug_maxsrc = 0, debug_pal = 0, show_mandel = 0, show_fps_hist = 0;
	int lastframe_key = 0;

	SDL_Event event;
	memset(&event, 0, sizeof(event));
	while(SDL_PollEvent(&event) >= 0) {
		if(event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) break;
		if((event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F1)) { if(!lastframe_key) { debug_maxsrc = !debug_maxsrc; } lastframe_key = 1; }
		else if((event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F2)) { if(!lastframe_key) { debug_pal = !debug_pal; } lastframe_key = 1; }
		else if((event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F3)) { if(!lastframe_key) { show_mandel = !show_mandel; } lastframe_key = 1; }
		else if((event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F4)) { if(!lastframe_key) { show_fps_hist = !show_fps_hist; } lastframe_key = 1; }
		else lastframe_key = 0;

		m = (m+1)&0x1;

		if(!opts.rational_julia) {
			map_func(map_surf[m], map_surf[(m+1)&0x1], im_w, im_h, pd);
			maxblend(map_surf[m], maxsrc_get(maxsrc), im_w, im_h);
		}

		if(opts.rational_julia) {
			maxblend(map_surf[(m+1)&0x1], maxsrc_get(maxsrc), im_w, im_h);
			map_func(map_surf[m], map_surf[(m+1)&0x1], im_w, im_h,  pd);
		}

		if((now - lastpalstep)*256/1024 >= 1) { // want pallet switch to take ~2 seconds
			pal_ctx_step(pal_ctx, IMIN((now - lastpalstep)*256/1024, 32));
			lastpalstep = now;
		}
		if(debug_maxsrc)
			pallet_blit_SDL(screen, maxsrc_get(maxsrc), im_w, im_h, pal_ctx_get_active(pal_ctx));
		else
			pallet_blit_SDL(screen, map_surf[m], im_w, im_h, pal_ctx_get_active(pal_ctx));

		char buf[32];
		sprintf(buf,"%6.1f FPS", 1000.0f / frametime);
		DrawText(screen, buf);
		SDL_Flip(screen);

		now = SDL_GetTicks();
		int newbeat = beat_get_count();
		if(newbeat != beats) pal_ctx_start_switch(pal_ctx, newbeat);

		if(newbeat != beats && now - last_beat_time > 1000) {
			last_beat_time = now;
			update_points(pd, (now - tick0), 1);
		} else update_points(pd, (now - tick0), 0);
		beats = newbeat;

		if((tick0+(maxfrms*1000)/opts.maxsrc_rate) - now > 1000/opts.maxsrc_rate) {
			audio_data ad; audio_get_samples(&ad);
			maxsrc_update(maxsrc, ad.data, ad.len);
			audio_finish_samples();
			maxfrms++;
		}

		now = SDL_GetTicks();
		if(now - fps_oldtime < 10) SDL_Delay(10 - (now - fps_oldtime)); // stay below 1000FPS
		frametime = 0.02f * (now - fps_oldtime) + (1.0f - 0.02f) * frametime;
		fps_oldtime = SDL_GetTicks();
		cnt++;
	}

	pal_ctx_delete(pal_ctx);
	maxsrc_delete(maxsrc);
	audio_shutdown();
	SDL_Quit();
    return 0;
}

