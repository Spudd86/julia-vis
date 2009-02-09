#include <unistd.h>
#include <stdio.h>
#include <stdlib.h> /* for exit() */
#include <math.h>
#include <malloc.h>

#include <SDL.h>
#include <SDL_ttf.h>

#include "map.h"

#include "common.h"
#include "sdl-misc.h"
#include "pixmisc.h"

#define MAP soft_map_interp
#define PALLET_BLIT pallet_blit_SDL


#define IM_SIZE (384)

//~ // set up source for maxblending
//~ static uint16_t *setup_maxsrc(int w, int h) 
//~ {
	//~ uint16_t *max_src = valloc(w * h * sizeof(uint16_t));

	//~ for(int y=0; y < h; y++)  {
		//~ for(int x=0; x < w; x++) {
			//~ float u = 2*(float)x/w - 1; float v = 2*(float)y/h - 1;
			//~ float d = sqrtf(u*u + v*v);
			//~ max_src[y*w + x] = (uint16_t)((1.0f - fminf(d, 0.5)*2)*UINT16_MAX);
		//~ }
	//~ }
	//~ return max_src;
//~ }

static opt_data opts;

int main(int argc, char **argv) 
{    
	optproc(argc, argv, &opts);
	SDL_Surface *screen = sdl_setup(&opts, IM_SIZE);
	int im_w = screen->w - screen->w%16, im_h = screen->h - screen->h%8;
	
#ifdef HAVE_JACK
	if(opts.use_jack)
		jack_setup();
	else
#endif
		audio_setup_pa();
	
	//uint16_t *maxsrc = setup_maxsrc(im_w, im_h);
	maxsrc_setup(im_w, im_h);

	uint16_t *map_surf[2];
	for(int i=0; i<2; i++) {
		map_surf[i] = valloc(im_w * im_h * sizeof(uint16_t));
		memset(map_surf[i], 0, im_w * im_h * sizeof(uint16_t));
	}
	//~ map_surf[0] = valloc(im_w * im_h * sizeof(uint16_t));
	//~ map_surf[1] = valloc(im_w * im_h * sizeof(uint16_t));
	//~ for(int i=0; i < im_w * im_h; i++) {
		//~ map_surf[0][i] = max_src[i];
		//~ map_surf[1][i] = max_src[i];
	//~ }
	
	uint32_t *pal = memalign(64, 257 * sizeof(uint32_t));
	for(int i = 0; i < 256; i++) pal[i] = ((2*abs(i-127))<<16) | (i<<8) | ((255-i));
	pal[256] = pal[255];

	
	int m = 0, cnt = 0;
	float t0=0, t1=0.5, t2=1.0f, t3=-1.5;
	
	Uint32 tick0;
	Uint32 fps_oldtime = tick0 = SDL_GetTicks();
	float frametime = 100;

	SDL_Event	event;
	while(SDL_PollEvent(&event) >= 0)
	{
		if(event.type == SDL_QUIT 
			|| (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE))
			break;
		
		//~ MAP(map_surf[(m+1)&0x1], map_surf[m], im_w, im_h, sin(t0)*0.75, sin(t1)*0.75);
		//~ m = (m+1)&0x1; 
		//~ maxblend(map_surf[m], max_src, im_w, im_h);
		maxblend(map_surf[m], maxsrc_get(), im_w, im_h);
		soft_map_rational(map_surf[(m+1)&0x1], map_surf[m], im_w, im_h, sin(t0)*0.5, cos(t1)*0.5, sin(t2)*0.5, sin(t3));
		m = (m+1)&0x1;
		
		PALLET_BLIT(screen, map_surf[m], im_w, im_h, pal);
		
		char buf[32];
		sprintf(buf,"%6.1f FPS", 1000.0f / frametime);
		DrawText(screen, buf);
		
		SDL_Flip(screen);
		
		//if(!(cnt%2)) {
			maxsrc_update();
		//}
		cnt++;

		Uint32 now = SDL_GetTicks();
		frametime = 0.02f * (now - fps_oldtime) + (1.0f - 0.02f) * frametime;
		float dt = (now - tick0) * 0.001f;
		t0=0.07f*dt; t1=-0.09f*dt; t2=0.1f*dt; t3=0.05f*dt;
		fps_oldtime = now;
	}

    return 0;
}
