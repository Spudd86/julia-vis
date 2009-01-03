#include <unistd.h>
#include <stdio.h>
#include <stdlib.h> /* for exit() */
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

#define IM_SIZE (256)

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

static volatile bool running = true;
static uint16_t *max_src;

static float map_fps=0;

static int run_map_thread(tribuf *tb) 
{
	float t0 = 0, t1 = 0;
	
	
	Uint32 tick1, tick2;
	Uint32 fps_delta;
	Uint32 fps_oldtime = tick1 = SDL_GetTicks();
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
		frametime = 0.02f * fps_delta + (1.0f - 0.02f) * frametime;
		map_fps = 1000.0f / frametime;
		float dt = (tick2 - tick1) * 0.001f;
		t0=0.05f*dt; t1=0.35f*dt;
    }
	
	return 0;
}

static SDL_Surface *sdl_setup() 
{
	SDL_VideoInfo *vid_info = SDL_GetVideoInfo();
	SDL_Rect **modes = SDL_ListModes(vid_info->vfmt, SDL_HWSURFACE|SDL_DOUBLEBUF);
	if (modes == (SDL_Rect**)0) {
		printf("No modes available!\n");
		exit(-1);
	}
	
	int vidflags = SDL_HWSURFACE | SDL_HWACCEL | SDL_DOUBLEBUF | SDL_FULLSCREEN;
	SDL_Surface *screen;
	if (modes == (SDL_Rect**)-1) {
		screen = SDL_SetVideoMode(IM_SIZE, IM_SIZE, vid_info->vfmt->BitsPerPixel, vidflags);
	} else {
		int mode=0;
		for (int i=0; modes[i]; i++) {
			printf("  %d x %d\n", modes[i]->w, modes[i]->h);
			if(modes[i]->w >= IM_SIZE && modes[i]->h >= IM_SIZE && modes[i]->h <= modes[mode]->h) 
				mode = i;
		}
		if(modes[mode]->w < IM_SIZE && modes[mode]->h < IM_SIZE) {
			printf("No usable modes available!\n");
			exit(-1);
		}
		printf("\nusing %d x %d\n", modes[mode]->w, modes[mode]->h);
		screen = SDL_SetVideoMode(modes[mode]->w, modes[mode]->h, vid_info->vfmt->BitsPerPixel, vidflags);
	}

    if ( screen == NULL ) {
        fprintf(stderr, "Unable to set video: %s\n", SDL_GetError());
        exit(1);
    }
	SDL_WM_SetCaption("SDL test for fractal map", "sdl-test");
	
	return screen;
}


static void DrawText(SDL_Surface* screen, TTF_Font* font, const char* text)
{
    SDL_Surface *text_surface = TTF_RenderText_Solid(font, text, (SDL_Color){255,255,255});
    if (text_surface == NULL) return;
    
	SDL_BlitSurface(text_surface, NULL, screen, NULL);
	SDL_FreeSurface(text_surface);
}

static SDL_Event user_event;
static Uint32 timercallback(Uint32 t, void *data) {SDL_PushEvent(&user_event); return t; }
int main() 
{    
    printf("Initializing SDL.\n");
    if((SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTTHREAD | SDL_INIT_TIMER)==-1)) { 
        printf("Could not initialize SDL: %s.\n", SDL_GetError());
        exit(-1);
    }
	atexit(SDL_Quit);
	if(TTF_Init()==-1) {
		printf("TTF_Init: %s\n", TTF_GetError());
		exit(2);
	}
	TTF_Font *font = TTF_OpenFont("font.ttf", 16);
	
    printf("SDL initialized.\n");

	SDL_Surface *screen = sdl_setup();
	
	max_src = setup_maxsrc(IM_SIZE, IM_SIZE);
	
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
	for(int i = 0; i < 256; i++) pal[i] = ((2*abs(i-127))<<16) | (i<<8) | ((255-i));
	pal[256] = pal[255];

	tribuf *map_tb = tribuf_new((void **)map_surf);
	
	SDL_Thread *map_thread = SDL_CreateThread(&run_map_thread, map_tb);
	
	user_event.type=SDL_USEREVENT;
	user_event.user.code=2;
	user_event.user.data1=NULL;
	user_event.user.data2=NULL;

	SDL_AddTimer(1000/30, &timercallback, NULL);
	
	SDL_Event	event;
	while(SDL_WaitEvent(&event) >= 0)
	{
		if(event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_QUIT 
				|| (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
			break;
		} else if(event.type == SDL_USEREVENT) {
			PALLET_BLIT(screen, tribuf_get_read(map_tb), IM_SIZE, IM_SIZE, pal);
			char buf[32];
			sprintf(buf,"%6.1f FPS", map_fps);
			DrawText(screen, font, buf);
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

