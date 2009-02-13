#include <unistd.h>
#include <stdio.h>
#include <stdlib.h> /* for exit() */
#include <math.h>
#include <malloc.h>

#include "mtwist/mtwist.h"

#include <SDL.h>
#include <SDL_ttf.h>

#include "map.h"

#include "common.h"
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
	float vx1, vy1, x1, y1, xt1, yt1;
	float vx2, vy2, x2, y2, xt2, yt2;
	
	mt_goodseed(); // seed our PSRNG
	vx1 = vy1 = vx2 = vy2 = 0;
	xt1 = 0.5f*((mt_lrand()%im_w)*2.0f/im_w - 1.0f); yt1 = 0.5f*((mt_lrand()%im_h)*2.0f/im_h - 1.0f);
	x1 = 0.5f*((mt_lrand()%im_w)*2.0f/im_w - 1.0f); y1 = 0.5f*((mt_lrand()%im_h)*2.0f/im_h - 1.0f);
	xt2 = 0.5f*((mt_lrand()%im_w)*2.0f/im_w - 1.0f); yt2 = 0.5f*((mt_lrand()%im_h)*2.0f/im_h - 1.0f);
	x2 = 0.5f*((mt_lrand()%im_w)*2.0f/im_w - 1.0f); y2 = 0.5f*((mt_lrand()%im_h)*2.0f/im_h - 1.0f);
	
	Uint32 tick0;
	Uint32 fps_oldtime = tick0 = SDL_GetTicks();
	float frametime = 100;
	int beats = beat_get_count();
	int done_time = tick0;
	unsigned int last_beat_time = tick0;

	SDL_Event	event;
	while(SDL_PollEvent(&event) >= 0)
	{
		if(event.type == SDL_QUIT 
			|| (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE))
			break;
		
		if(!(cnt%3)) {
			maxsrc_update();
		}
		cnt++;
		
		//~ soft_map_butterfly(map_surf[(m+1)&0x1], map_surf[m], im_w, im_h, x1, y1);
		//~ MAP(map_surf[(m+1)&0x1], map_surf[m], im_w, im_h, x1, y1);
		//~ m = (m+1)&0x1; 
		//~ maxblend(map_surf[m], maxsrc_get(), im_w, im_h);
		//~ MAP(map_surf[(m+1)&0x1], map_surf[m], im_w, im_h, x1, y1);
		//~ m = (m+1)&0x1; 
		maxblend(map_surf[m], maxsrc_get(), im_w, im_h);
		
		soft_map_rational(map_surf[(m+1)&0x1], map_surf[m], im_w, im_h, x1, y1, x2, y2);
		m = (m+1)&0x1;
		maxblend(map_surf[m], maxsrc_get(), im_w, im_h);
		soft_map_rational(map_surf[(m+1)&0x1], map_surf[m], im_w, im_h, x1, y1, x2, y2);
		m = (m+1)&0x1;
		
		pallet_blit_SDL(screen, map_surf[m], im_w, im_h);
		
		char buf[32];
		sprintf(buf,"%6.1f FPS", 1000.0f / frametime);
		DrawText(screen, buf);
		
		SDL_Flip(screen);
		
		pallet_step(2);
		
		Uint32 now = SDL_GetTicks();
		frametime = 0.02f * (now - fps_oldtime) + (1.0f - 0.02f) * frametime;
		fps_oldtime = now;
		
		int newbeat = beat_get_count();
		if(newbeat != beats) pallet_start_switch(newbeat%4);
		if(newbeat != beats && now - last_beat_time > 1000) {
			xt1 = 0.5f*((mt_lrand()%im_w)*2.0f/im_w - 1.0f); yt1 = 0.5f*((mt_lrand()%im_h)*2.0f/im_h - 1.0f);
			xt2 = 0.5f*((mt_lrand()%im_w)*2.0f/im_w - 1.0f); yt2 = 0.5f*((mt_lrand()%im_h)*2.0f/im_h - 1.0f);
			last_beat_time = now;
		}
		beats = newbeat;
		
		const float delt = 30.0f/200.0f;
		const float tsped = 0.002;
		const int dt = 1000/200;
		
		while(done_time <= now) {
			float xtmp1 = xt1 - x1, ytmp1 = yt1-y1;
			float xtmp2 = xt2 - x2, ytmp2 = yt2-y2;
			float mag = xtmp1*xtmp1+ytmp1*ytmp1+xtmp2*xtmp2+ytmp2*ytmp2;
				mag=(mag>0)?delt*0.4f/sqrtf(mag):0;
			vx1=(vx1+xtmp1*mag*tsped)/(tsped+1);
			vy1=(vy1+ytmp1*mag*tsped)/(tsped+1);
			vx2=(vx2+xtmp2*mag*tsped)/(tsped+1);
			vy2=(vy2+ytmp2*mag*tsped)/(tsped+1);
			x1=x1+vx1*delt;
			y1=y1+vy1*delt;
			x2=x2+vx2*delt;
			y2=y2+vy2*delt;
			
			done_time += dt;
		}
	}

    return 0;
}
