#include "common.h"
#include <SDL.h>

#include "sdlhelp.h"
#include "sdlsetup.h"
#include "pallet.h"
#include "pixmisc.h"

#define IM_SIZE (512)

static opt_data opts;

int main(int argc, char **argv)
{
	optproc(argc, argv, &opts);

	SDL_Surface *screen = sdl_setup(&opts, IM_SIZE);

	int im_w = screen->w/2, im_h = screen->h/2;
	im_w = im_w - im_w%16, im_h = im_h - im_h%16;

	SDL_Surface *srfpal = NULL, *srf555 = NULL, *srf565 = NULL, *srf888 = NULL;
	srfpal = SDL_CreateRGBSurface(SDL_SWSURFACE,
	                              screen->w/2, screen->h/2,
	                              8,
	                              0, 0, 0, 0);

	srf555 = SDL_CreateRGBSurface(SDL_SWSURFACE,
	                              screen->w/2, screen->h/2,
	                              15,
	                              0x7c00, 0x03e0, 0x001f, 0);

	srf565 = SDL_CreateRGBSurface(SDL_SWSURFACE,
	                              screen->w/2, screen->h/2,
	                              16,
	                              0xf800, 0x07e0, 0x001f, 0);

	srf888 = SDL_CreateRGBSurface(SDL_SWSURFACE,
	                              screen->w/2, screen->h/2,
	                              32,
	                              0xff0000, 0x00ff00, 0x0000ff, 0);

	uint16_t *blit_src = aligned_alloc(64, im_w * im_h * sizeof(uint16_t));
	uint16_t fillstep = UINT16_MAX/im_h;
	uint16_t fillcur = 0;
	for(size_t y = 0; y < im_h; y++, fillcur+=fillstep) {
		for(size_t x = 0; x < im_w; x++) {
			blit_src[y*im_w + x] = fillcur - ((x*fillcur)/im_w);
			//blit_src[y*im_w + x] = fillcur;
		}
	}

	uint32_t *pals[5];
	uint32_t *pals_swap[5];
	for(int i = 0; i<5; i++) {
		pals[i] = aligned_alloc(16, 257 * sizeof(uint32_t));
		pals_swap[i] = aligned_alloc(16, 257 * sizeof(uint32_t));
	}
	for(unsigned i=0; i < 256; i++) {
		pals[0][i] = (i<<16) + (i<<8) + i;
		pals[1][i] = i<<16;
		pals[2][i] = i<<8;
		pals[3][i] = i;
		pals[4][i] = (255 - i) + (i<<8) + (abs(127 - i) << 16);
	}
	pals[0][256] = pals[0][255];
	pals[1][256] = pals[1][255];
	pals[2][256] = pals[2][255];
	pals[3][256] = pals[3][255];
	pals[4][256] = pals[4][255];


	for(unsigned i=0; i < 5; i++) {
		for(unsigned j=0; j <= 256; j++) {
			uint32_t p = pals[i][j];
			pals_swap[i][j] = ((p>>16)&0x0000ff) | ((p>>0)&0x00ff00) | ((p<<16)&0xff0000);
		}
	}

	//TODO: once we have runtime CPU detection for palletblit
	// this should be able to test all the versions that will run on
	// the CPU

	int cur_pal = 4;
	SDL_Event event;
	memset(&event, 0, sizeof(event));
	int lastframe_key = 0;
	while(SDL_PollEvent(&event) >= 0) {
		if(event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) break;

		if(event.type == SDL_KEYDOWN) {
			if(event.key.keysym.sym==SDLK_LEFT) { if(!lastframe_key) cur_pal = (cur_pal + 4) % 5; lastframe_key = 1; }
			else if(event.key.keysym.sym==SDLK_RIGHT) { if(!lastframe_key) cur_pal = (cur_pal + 1) % 5; lastframe_key = 1; }
		} else {
			lastframe_key = 0;
		}

		pallet_blit_SDL(srfpal, blit_src, im_w, im_h, pals_swap[cur_pal]);
		pallet_blit_SDL(srf555, blit_src, im_w, im_h, pals[cur_pal]);
		pallet_blit_SDL(srf565, blit_src, im_w, im_h, pals[cur_pal]);
		pallet_blit_SDL(srf888, blit_src, im_w, im_h, pals[cur_pal]);

		SDL_Rect blit_rect = { 0,           0,           screen->w/2, screen->h/2 };
		SDL_Rect _pal_rect = { 0,           0,           srfpal->w, srfpal->h };
		SDL_Rect _555_rect = { screen->w/2, 0,           srf555->w, srf555->h };
		SDL_Rect _565_rect = { 0,           screen->h/2, srf565->w, srf565->h };
		SDL_Rect _888_rect = { screen->w/2, screen->h/2, srf888->w, srf888->h };
		SDL_BlitSurface(srfpal, &blit_rect, screen, &_pal_rect);
		SDL_BlitSurface(srf555, &blit_rect, screen, &_555_rect);
		SDL_BlitSurface(srf565, &blit_rect, screen, &_565_rect);
		SDL_BlitSurface(srf888, &blit_rect, screen, &_888_rect);

		SDL_Flip(screen);
	}

	return 0;
}


