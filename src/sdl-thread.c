#include <unistd.h>
#include <stdio.h>
#include <stdlib.h> /* for exit() */
#include <stdbool.h>
#include <math.h>
#include <malloc.h>

#include <mm_malloc.h>

#include <SDL.h>
#include <SDL_thread.h>

#include "pixmisc.h"
#include "map.h"
#include "tribuf.h"

#include "common.h"

#define IM_SIZE (512)

#if TILED
#define MAP soft_map_interp8x8
#define PALLET_BLIT pallet_blit_SDL8x8
#else
#define MAP soft_map_interp8x8
#define PALLET_BLIT pallet_blit_SDL
#endif

// set up source for maxblending
static uint16_t *setup_maxsrc(int w, int h, bool tile) 
{
	uint16_t col(int x, int y) {
		float u = 2*(float)x/w - 1; float v = 2*(float)y/h - 1;
		float d = sqrtf(u*u + v*v);
		
		return (uint16_t)((1.0f - fminf(d, 0.25)*4)*UINT16_MAX);
	}
	uint16_t *max_src = valloc(w * h * sizeof(uint16_t));
	if(!tile) {
		for(int y=0; y < h; y++) 
			for(int x=0; x < w; x++) 
				max_src[y*w + x] = col(x,y);
	} else {
		for(int y=0; y < h/8; y++) {
			for(int x=0; x < w/8; x++) {
				for(int yt=0; yt<8; yt++) 
					for(int xt=0; xt<8; xt++) 
						max_src[y*w*64/8 + x*64 + yt*8+xt] = col(x*8+xt, y*8+yt);
			}
		}
	}
	
	return max_src;
}

static volatile bool running = true;
static uint16_t *max_src;

static int run_map_thread(tribuf *tb) 
{
	float t0 = 0, t1 = 0;
	
	
	Uint32 tick1, tick2;
	Uint32 fps_lasttime;
	Uint32 fps_delta;
	Uint32 fps_oldtime = fps_lasttime = tick1 = SDL_GetTicks();
	float frametime = 100;
	
	uint16_t *map_src = tribuf_get_read(tb);
    while(running) 
	{
		uint16_t *map_dest = tribuf_get_write(tb);

		MAP(map_dest, map_src, IM_SIZE, IM_SIZE, sin(t0), sin(t1));
		maxblend(map_dest, max_src, IM_SIZE, IM_SIZE);
		
		tribuf_finish_write(tb);
		map_src=map_dest;

		tick2 = SDL_GetTicks();
		fps_delta = tick2 - fps_oldtime;
		fps_oldtime = tick2;
		frametime = 0.1f * fps_delta + (1.0f - 0.1f) * frametime;
		if (fps_lasttime+1000 < tick2) {
			printf("FPS: %3.1f\n", 1000.0f / frametime);
			fps_lasttime = tick2;
		}
		float dt = (tick2 - tick1) * 0.001f;
		t0=0.05f*dt; t1=0.35f*dt;
    }
	
	return 0;
}

#define TICK_INTERVAL    30
int main() 
{    
    printf("Initializing SDL.\n");
    if((SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTTHREAD | SDL_INIT_TIMER)==-1)) { 
        printf("Could not initialize SDL: %s.\n", SDL_GetError());
        exit(-1);
    }
	atexit(SDL_Quit);

    printf("SDL initialized.\n");
	
	SDL_Surface *screen;
	screen = SDL_SetVideoMode(IM_SIZE, IM_SIZE, 32, SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_FULLSCREEN);
    if ( screen == NULL ) {
        fprintf(stderr, "Unable to set video: %s\n", SDL_GetError());
        exit(1);
    }
	SDL_WM_SetCaption("SDL test for fractal map", "sdl-test");
	
	max_src = setup_maxsrc(IM_SIZE, IM_SIZE, TILED);
	
	uint16_t *map_surf[3];
	map_surf[0] = valloc(IM_SIZE * IM_SIZE * sizeof(uint16_t));
	map_surf[1] = valloc(IM_SIZE * IM_SIZE * sizeof(uint16_t));
	map_surf[2] = valloc(IM_SIZE * IM_SIZE * sizeof(uint16_t));
	for(int i=0; i< IM_SIZE*IM_SIZE; i++) {
		map_surf[0][i] = max_src[i];
		map_surf[1][i] = max_src[i];
		map_surf[2][i] = max_src[i];
	}
	
	uint32_t *pal = _mm_malloc(257 * sizeof(uint32_t), 64); // p4 has 64 byte cache line
	for(int i = 0; i < 256; i++) pal[i] = 0xFF000000|((2*abs(i-127))<<16) | (i<<8) | ((255-i));
	pal[256] = pal[255];

	tribuf *map_tb = tribuf_new((void **)map_surf);
	
	SDL_Thread *map_thread = SDL_CreateThread(&run_map_thread, map_tb);
	
	SDL_Event user_event;

	user_event.type=SDL_USEREVENT;
	user_event.user.code=2;
	user_event.user.data1=NULL;
	user_event.user.data2=NULL;

	Uint32 timercallback(Uint32 t, void *data) {SDL_PushEvent(&user_event); return t; }
	
	SDL_AddTimer(1000/30, &timercallback, NULL);
	
	SDL_Event	event;
	while(SDL_WaitEvent(&event) >= 0)
	{
		if(event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_QUIT 
				|| (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
			break;
		} else if(event.type == SDL_USEREVENT) {
			PALLET_BLIT(screen, tribuf_get_read(map_tb), IM_SIZE, IM_SIZE, pal);
			SDL_Flip(screen);
		}
	}
	running = false;
	
	int status;
	SDL_WaitThread(map_thread, &status);

    printf("Quitting SDL.\n");
    
    /* Shutdown all subsystems */
    SDL_Quit();
    
    printf("Quitting...\n");

    exit(0);
}

