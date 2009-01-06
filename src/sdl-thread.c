#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <malloc.h>

#include <mm_malloc.h>

#include <SDL.h>
#include <SDL_thread.h>
#include <SDL_ttf.h>

#include "tribuf.h"
#include "common.h"

#include "pixmisc.h"

#include "map.h"

#define IM_SIZE (1024)

#define MAP soft_map_interp
#define PALLET_BLIT pallet_blit_SDL

// set up source for maxblending
static uint16_t *setup_maxsrc(int w, int h) 
{
	uint16_t *max_src = valloc(w * h * sizeof(uint16_t));

	for(int y=0; y < h; y++)  {
		for(int x=0; x < w; x++) {
			float u = 2*(float)x/w - 1; float v = 2*(float)y/h - 1;
			float d = sqrtf(u*u + v*v);
			max_src[y*w + x] = (uint16_t)((1.0f - fminf(d, 0.25)*4)*UINT16_MAX);
		}
	}
	return max_src;
}

static int im_w = 0, im_h = 0;
static volatile bool running = true;
static uint16_t *max_src;
static float map_fps=0;

static int run_map_thread(tribuf *tb) 
{
	float t0 = 0, t1 = 0;
	
	Uint32 tick0, fps_oldtime;
	fps_oldtime = tick0 = SDL_GetTicks();
	float frametime = 100;
	
	uint16_t *map_src = tribuf_get_read(tb);
    while(running) 
	{
		uint16_t *map_dest = tribuf_get_write(tb);

		MAP(map_dest, map_src, im_w, im_h, sin(t0), sin(t1));
		maxblend(map_dest, max_src, im_w, im_h);
		
		tribuf_finish_write(tb);
		map_src=map_dest;

		Uint32 now = SDL_GetTicks();

		frametime = 0.02f * (now - fps_oldtime) + (1.0f - 0.02f) * frametime;
		map_fps = 1000.0f / frametime;
		float dt = (now - tick0) * 0.001f;
		t0=0.05f*dt; t1=0.35f*dt;

		fps_oldtime = now;
    }
	return 0;
}

SDL_Surface *sdl_setup(int im_size);
void DrawText(SDL_Surface* screen, const char* text);

static SDL_Event user_event;
static Uint32 timercallback(Uint32 t, void *data) {SDL_PushEvent(&user_event); return t; }
int main() 
{    
	SDL_Surface *screen = sdl_setup(IM_SIZE);
	im_w = screen->w - screen->w%8; im_h = screen->h - screen->h%8;
	
	max_src = setup_maxsrc(im_w, im_h);
	
	uint16_t *map_surf[3];
	map_surf[0] = valloc(im_w * im_h * sizeof(uint16_t));
	map_surf[1] = valloc(im_w * im_h * sizeof(uint16_t));
	map_surf[2] = valloc(im_w * im_h * sizeof(uint16_t));
	for(int i=0; i< im_w*im_h; i++) {
		map_surf[0][i] = max_src[i];
		map_surf[1][i] = max_src[i];
		map_surf[2][i] = max_src[i];
	}
	
	uint32_t *pal = _mm_malloc(257 * sizeof(uint32_t), 64); // p4 has 64 byte cache line
	for(int i = 0; i < 256; i++) pal[i] = ((2*abs(i-127))<<16) | (i<<8) | ((255-i));
	pal[256] = pal[255];

	tribuf *map_tb = tribuf_new((void **)map_surf);
	
	SDL_Thread *map_thread = SDL_CreateThread(&run_map_thread, map_tb);
	
	user_event.type=SDL_USEREVENT;
	user_event.user.code=2;
	user_event.user.data1=NULL;
	user_event.user.data2=NULL;

	SDL_AddTimer(1000/60, &timercallback, NULL);
	
	SDL_Event	event;
	while(SDL_WaitEvent(&event) >= 0)
	{
		if(event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_QUIT 
				|| (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
			break;
		} else if(event.type == SDL_USEREVENT) {
			PALLET_BLIT(screen, tribuf_get_read(map_tb), im_w, im_h, pal);
			char buf[32];
			sprintf(buf,"%6.1f FPS", map_fps);
			DrawText(screen, buf);
			SDL_Flip(screen);
		}
	}
	running = false;
	
	int status;
	SDL_WaitThread(map_thread, &status);

    return 0;
}

