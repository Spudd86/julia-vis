
#include "common.h"

#include "pallet.h"
#include "audio/audio.h"
#include "software/softcore.h"

#include <SDL.h>
#include "sdl/sdlsetup.h"
#include "sdlhelp.h"

//TODO: switch to using the julia_vis_pixel_format based pallet init function

int main(int argc, char **argv)
{
	opt_data opts;
	optproc(argc, argv, &opts);

	simple_soft_map_func map_func = SOFT_MAP_FUNC_NORMAL_INTERP;
	if(strcmp(opts.map_name, "rational") == 0) {
		if(opts.quality == 0) map_func = SOFT_MAP_FUNC_RATIONAL_INTERP;
		else if(opts.quality >= 1)  map_func = SOFT_MAP_FUNC_RATIONAL;
	}
	else if(strcmp(opts.map_name, "butterfly") == 0) map_func = SOFT_MAP_FUNC_BUTTERFLY;
	else if(opts.quality >= 1)  map_func = SOFT_MAP_FUNC_NORMAL;

	if(audio_init(&opts) < 0) exit(1);

	SDL_Surface *screen = sdl_setup(&opts, 768);

	struct softcore_ctx * core = softcore_init(screen->w, screen->h, map_func);

	int im_w, im_h;
	softcore_get_buffer_dims(core, &im_w, &im_h);
	printf("running with %dx%d bufs\n", im_w, im_h);

	struct pal_ctx *pal_ctx = pal_ctx_new(screen->format->BitsPerPixel == 8); //TODO: write a helper that picks the right pallet channel order based on screen->format

	Uint32 tick0, fps_oldtime;
	fps_oldtime = tick0 = SDL_GetTicks();
	float frametime = 100;
	int beats = beat_get_count();
	uint32_t lastpalstep = tick0;
	uint32_t now = tick0;

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

		now = SDL_GetTicks();
		int newbeat = beat_get_count();
		audio_data ad; audio_get_samples(&ad);
		const uint16_t *src_buf = softcore_render(core, now, tick0, beat_get_count(), ad.data, ad.len);
		audio_finish_samples();
		if(newbeat != beats) pal_ctx_start_switch(pal_ctx, newbeat);
		if(debug_maxsrc) src_buf = get_last_maxsrc_buffer(core);
		pallet_blit_SDL(screen, src_buf, im_w, im_h, pal_ctx_get_active(pal_ctx));

		char buf[32];
		sprintf(buf,"%6.1f FPS", 1000.0f / frametime);
		DrawText(screen, buf);
		SDL_Flip(screen);

		now = SDL_GetTicks();
		if(now - fps_oldtime < 10) SDL_Delay(10 - (now - fps_oldtime)); // stay below 1000FPS
		frametime = 0.02f * (now - fps_oldtime) + (1.0f - 0.02f) * frametime;
		fps_oldtime = SDL_GetTicks();
	}

	pal_ctx_delete(pal_ctx);
	softcore_destroy(core);
	audio_shutdown();
	SDL_Quit();
    return 0;
}

