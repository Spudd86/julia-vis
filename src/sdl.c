#include <unistd.h>
#include <stdio.h>
#include <stdlib.h> /* for exit() */
#include <stdbool.h>
#include <math.h>
#include <malloc.h>

#include <SDL.h>

#include "pixmisc.h"
#include "map.h"

#include "common.h"

#define MAP soft_map_interp8x8
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

#define IM_SIZE (512)

int main() {
    
    printf("Initializing SDL.\n");
    
    if((SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTTHREAD | SDL_INIT_TIMER | SDL_INIT_NOPARACHUTE)==-1)) { 
        printf("Could not initialize SDL: %s.\n", SDL_GetError());
        exit(-1);
    }
	atexit(SDL_Quit);

    printf("SDL initialized.\n");
    
	//SDL_VideoInfo *vid_info = SDL_GetVideoInfo();
	
	SDL_Surface *screen;

    screen = SDL_SetVideoMode(IM_SIZE, IM_SIZE, 32, SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_FULLSCREEN);
    if ( screen == NULL ) {
        fprintf(stderr, "Unable to set video: %s\n", SDL_GetError());
        exit(1);
    }
	
	SDL_WM_SetCaption("SDL test for fractal map", "sdl-test");
	
	uint16_t *max_src = setup_maxsrc(IM_SIZE, IM_SIZE);
	
	uint16_t *map_surf[2];
	map_surf[0] = valloc(IM_SIZE * IM_SIZE * sizeof(uint16_t));
	map_surf[1] = valloc(IM_SIZE * IM_SIZE * sizeof(uint16_t));
	for(int i=0; i < IM_SIZE*IM_SIZE; i++) {
		map_surf[0][i] = max_src[i];
		map_surf[1][i] = max_src[i];
	}
	
	uint32_t *pal = memalign(64, 257 * sizeof(uint32_t));
	for(int i = 0; i < 256; i++) pal[i] = 0xFF000000|((2*abs(i-127))<<16) | (i<<8) | ((255-i));
	pal[256] = pal[255];

	
	int m = 0;
	float t0 = 0, t1 = 0;
	SDL_Event	event;
	Uint32 tick1, tick2;
	Uint32 fps_lasttime;
	Uint32 fps_delta;
	Uint32 fps_oldtime = fps_lasttime = tick1 = SDL_GetTicks();
	float frametime = 100/3.0f;
	
	while(SDL_PollEvent(&event) >= 0)
	{
		if(event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_QUIT 
			|| (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE))
			break;
		
		MAP(map_surf[(m+1)&0x1], map_surf[m], IM_SIZE, IM_SIZE, sin(t0)*0.75, sin(t1)*0.75);
		m = (m+1)&0x1; 
		
		maxblend(map_surf[m], max_src, IM_SIZE, IM_SIZE); 
		
		PALLET_BLIT(screen, map_surf[m], IM_SIZE, IM_SIZE, pal);
		
		SDL_Flip(screen);

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


    printf("Quitting SDL.\n");
    
    /* Shutdown all subsystems */
    SDL_Quit();
    
    printf("Quitting...\n");

    exit(0);
}
