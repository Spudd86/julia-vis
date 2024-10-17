
#include "common.h"

#include "pallet.h"

#include "audio/audio.h"

#include "software/pixmisc.h"
#include "software/maxsrc.h"
#include "software/map.h"
#include "software/scope_render.h"

#include <SDL.h>
#include "sdl/sdlsetup.h"
#include "sdlhelp.h"


#define IM_SIZE (512)

static opt_data opts;

int main(int argc, char **argv)
{
	optproc(argc, argv, &opts);
	if(audio_init(&opts) < 0) exit(1);
	SDL_Surface *screen = sdl_setup(&opts, IM_SIZE);
	int im_w = screen->w - screen->w%16, im_h = screen->h - screen->h%16;
	printf("running with %dx%d bufs\n", im_w, im_h);

	uint16_t *surf;
	surf = aligned_alloc(512, 512 + im_w * im_h * sizeof(uint16_t));
	memset(surf, 0, im_w * im_h * sizeof(uint16_t));

	struct scope_renderer *scope = scope_renderer_new(im_w, im_h);

	int m = 0, cnt = 0;

	struct pal_ctx *pal_ctx = pal_ctx_new(screen->format->BitsPerPixel == 8); //TODO: write a helper that picks the right pallet channel order based on screen->format

	Uint32 tick0, fps_oldtime;
	fps_oldtime = tick0 = SDL_GetTicks();
	float frametime = 100;
	int beats = beat_get_count();
	uint32_t last_beat_time = tick0;
	uint32_t lastpalstep = tick0;
	uint32_t now = tick0;

	float tx=0, ty=0, tz=0;

	bool show_fps_hist = false, show_fps = false;
	bool lastframe_key = false;
	bool pause_scope = false;

	SDL_Event event;
	memset(&event, 0, sizeof(event));
	while(SDL_PollEvent(&event) >= 0) {
		if(event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) break;
		if((event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_SPACE)) { if(!lastframe_key) { pause_scope = !pause_scope; } lastframe_key = 1; }
		else if((event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F4)) { if(!lastframe_key) { show_fps_hist = !show_fps_hist; } lastframe_key = 1; }
		else if((event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F5)) { if(!lastframe_key) { show_fps = !show_fps; } lastframe_key = 1; }
		else lastframe_key = 0;

		if((now - lastpalstep)*256/1024 >= 1) { // want pallet switch to take ~2 seconds
			pal_ctx_step(pal_ctx, IMIN((now - lastpalstep)*256/1024, 32));
			lastpalstep = now;
		}

		if(!pause_scope)
		{
			memset(surf, 0, im_w * im_h * sizeof(uint16_t));
			audio_data ad; audio_get_samples(&ad);
			scope_render(scope, surf, tx, ty, tz, ad.data, ad.len);
			audio_finish_samples();
			tx+=0.02f; ty+=0.01f; tz-=0.003f;
		}

		pallet_blit_SDL(screen, surf, im_w, im_h, pal_ctx_get_active(pal_ctx));

		if(show_fps)
		{
			char buf[32];
			sprintf(buf,"%6.1f FPS", 1000.0f / frametime);
			DrawText(screen, buf);
		}

		SDL_Flip(screen);

		now = SDL_GetTicks();
		int newbeat = beat_get_count();
		if(newbeat != beats) pal_ctx_start_switch(pal_ctx, newbeat);

		if(newbeat != beats && now - last_beat_time > 1000) {
			last_beat_time = now;
		}
		beats = newbeat;

		now = SDL_GetTicks();
		// if(now - fps_oldtime < 10) SDL_Delay(10 - (now - fps_oldtime)); // stay below 1000FPS
		frametime = 0.02f * (now - fps_oldtime) + (1.0f - 0.02f) * frametime;
		fps_oldtime = SDL_GetTicks();
		cnt++;
	}

	pal_ctx_delete(pal_ctx);
	scope_renderer_delete(scope);
	audio_shutdown();
	SDL_Quit();
	return 0;
}

