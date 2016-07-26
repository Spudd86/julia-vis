#include "common.h"

#include "pallet.h"
#include "software/pixmisc.h"

#include <SDL.h>
#include "sdlhelp.h"
#include "sdl/sdlsetup.h"

#define IM_SIZE (512)

#define NUM_SRC 5

static opt_data opts;

int main(int argc, char **argv)
{
	optproc(argc, argv, &opts);

	SDL_Surface *screen = sdl_setup(&opts, IM_SIZE);

	int im_w = screen->w, im_h = screen->h;
	im_w = im_w - im_w%16, im_h = im_h - im_h%16;

	uint16_t *blit_srcs[NUM_SRC];
	for(int i = 0; i<NUM_SRC; i++) {
		blit_srcs[i] = aligned_alloc(64, im_w * im_h * sizeof(uint16_t));
	}
	uint16_t fillstep = UINT16_MAX/im_h;
	uint16_t fillcur = 0;
	for(size_t y = 0; y < im_h; y++, fillcur+=fillstep) {
		for(size_t x = 0; x < im_w; x++) {
			blit_srcs[0][y*im_w + x] = fillcur - ((x*fillcur)/im_w);
			blit_srcs[1][y*im_w + x] = (x*UINT16_MAX)/im_w;
			blit_srcs[2][y*im_w + x] = (y*UINT16_MAX)/im_h;
			blit_srcs[3][y*im_w + x] = (x<<10)/im_w;
			blit_srcs[4][y*im_w + x] = (y<<10)/im_h;
		}
	}

	struct pal_ctx *pal_ctx = pal_ctx_new(screen->format->BitsPerPixel == 8);

	SDL_Event event;
	memset(&event, 0, sizeof(event));
	int lastframe_key = 0;
	int cur_pal = 0;
	int cur_src = 0;
	uint32_t tick0 = SDL_GetTicks();
	uint32_t lastpalstep = tick0;
	uint32_t now = tick0;
	while(SDL_PollEvent(&event) >= 0) {
		if(event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) break;

		if(event.type == SDL_KEYDOWN) {
			if(event.key.keysym.sym==SDLK_LEFT && !lastframe_key) { cur_pal = (cur_pal + 10-1) % 10; pal_ctx_start_switch(pal_ctx, cur_pal); }
			else if(event.key.keysym.sym==SDLK_RIGHT && !lastframe_key) { cur_pal = (cur_pal + 1) % 10; pal_ctx_start_switch(pal_ctx, cur_pal); }
			else if(event.key.keysym.sym==SDLK_DOWN && !lastframe_key) cur_src = (cur_src + NUM_SRC-1) % NUM_SRC;
			else if(event.key.keysym.sym==SDLK_UP && !lastframe_key) cur_src = (cur_src + 1) % NUM_SRC;
			lastframe_key = 1;
		} else {
			lastframe_key = 0;
		}

		pallet_blit_SDL(screen, blit_srcs[cur_src], im_w, im_h, pal_ctx_get_active(pal_ctx));

		char buf[128];
		sprintf(buf, "pal %d src %d", cur_pal, cur_src);
		DrawText(screen, buf);

		SDL_Flip(screen);

		now = SDL_GetTicks();
		if((now - lastpalstep)*256/1024 >= 1) { // want pallet switch to take ~2 seconds
			pal_ctx_step(pal_ctx, IMIN((now - lastpalstep)*256/1024, 32));
			lastpalstep = now;
		}

	}

	return 0;
}
