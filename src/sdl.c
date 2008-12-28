#include <unistd.h>
#include <stdio.h>
#include <stdlib.h> /* for exit() */
#include <stdbool.h>
#include <math.h>
#include <malloc.h>

#include <cairo.h>
#include <SDL.h>

#include "pallet.h"
#include "map.h"

#include "common.h"

#define IM_SIZE (512)
#define IM_MID  (IM_SIZE/2)

static uint8_t *setup_maxsrc(int w, int h, bool tile) 
{
	// set up source for maxblending
	cairo_surface_t *static_surf = cairo_image_surface_create (CAIRO_FORMAT_A8, IM_SIZE, IM_SIZE);
	uint8_t *source = cairo_image_surface_get_data(static_surf);
	int src_strd = cairo_image_surface_get_stride(static_surf);
	memset(source, 0, IM_SIZE * src_strd);
	cairo_t *cr = cairo_create(static_surf);
	cairo_pattern_t *pat = cairo_pattern_create_radial (IM_MID, IM_MID, 0, IM_MID,  IM_MID, IM_SIZE/8);
	cairo_pattern_add_color_stop_rgba (pat, 0, 1, 1, 1, 1);
	cairo_pattern_add_color_stop_rgba (pat, 1, 0, 0, 0, 0);
	cairo_set_source (cr, pat);

	cairo_arc(cr, IM_MID, IM_MID, IM_SIZE/8, 0, 2*M_PI);
	cairo_fill(cr);
	cairo_pattern_destroy (pat);
	cairo_destroy(cr);
	
	uint8_t *max_src = valloc(IM_SIZE * IM_SIZE * sizeof(uint8_t));
	if(!tile) {
		for(int y=0; y < IM_SIZE; y++) 
			for(int x=0; x < IM_SIZE; x++) 
				max_src[y*IM_SIZE + x] = source[y*src_strd + x];
	} else {
		for(int y=0; y < IM_SIZE/8; y++) {
			for(int x=0; x < IM_SIZE/8; x++) {
				for(int yt=0; yt<8; yt++) 
					for(int xt=0; xt<8; xt++) 
						max_src[y*IM_SIZE*64/8 + x*64 + yt*8+xt] = source[(y*8+yt)*src_strd + (x*8+xt)];
			}
		}
	}
	
	cairo_surface_destroy(static_surf);
	return max_src;
}

int main() {
    
    printf("Initializing SDL.\n");
    
    /* Initialize defaults, Video and Audio subsystems */
    if((SDL_Init(SDL_INIT_VIDEO)==-1)) { 
        printf("Could not initialize SDL: %s.\n", SDL_GetError());
        exit(-1);
    }
	atexit(SDL_Quit);

    printf("SDL initialized.\n");
    
	SDL_Surface *screen;

    screen = SDL_SetVideoMode(IM_SIZE, IM_SIZE, 32, SDL_HWSURFACE);//SDL_DOUBLEBUF);
    if ( screen == NULL ) {
        fprintf(stderr, "Unable to set video: %s\n", SDL_GetError());
        exit(1);
    }
	
	SDL_WM_SetCaption("SDL test for fractal map", "sdl-test");
	
	uint8_t *max_src = setup_maxsrc(IM_SIZE, IM_SIZE, false);
	
	uint16_t *map_surf[2];
	map_surf[0] = valloc(IM_SIZE * IM_SIZE * sizeof(uint16_t));
	memset(map_surf[0], 0, IM_SIZE * IM_SIZE * sizeof(uint16_t));
	map_surf[1] = valloc(IM_SIZE * IM_SIZE * sizeof(uint16_t));
	memset(map_surf[1], 0, IM_SIZE * IM_SIZE * sizeof(uint16_t));
	
	uint32_t *pal = memalign(32, 257 * sizeof(uint32_t));
	for(int i = 0; i < 256; i++) pal[i] = 0xFF000000|((2*abs(i-127))<<16) | (i<<8) | ((255-i));
	pal[256] = pal[255];

	int m = 0;
	float t0 = 0, t1 = 0;
	SDL_Event	event;
	long		tick1, tick2;
	tick1 = SDL_GetTicks();
	
	for(int i=0; i< IM_SIZE*IM_SIZE; i++) {
		map_surf[m][i] = IMAX(map_surf[m][i], max_src[i]*256);
	}
	
	while (SDL_PollEvent(&event) >= 0)
	{
		tick2 = SDL_GetTicks();
		float dt = (tick2 - tick1) * 0.001f;
		tick1 = tick2;
		/* Click to exit */
		if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_QUIT)
			break;
		
		soft_map_bl(map_surf[(m+1)&0x1], map_surf[m], IM_SIZE, IM_SIZE, sin(t0), sin(t1));
		m = (m+1)&0x1; t0+=0.05*dt; t1+=0.35*dt;
		
		for(int i=0; i< IM_SIZE*IM_SIZE; i++) {
			map_surf[m][i] = IMAX(map_surf[m][i], max_src[i]*256);
		}

		pallet_blit_SDL(screen, map_surf[m], IM_SIZE, IM_SIZE, pal);
		
		SDL_Flip(screen);

		/* let operating system breath */
		SDL_Delay(1);
	}


    printf("Quitting SDL.\n");
    
    /* Shutdown all subsystems */
    SDL_Quit();
    
    printf("Quitting...\n");

    exit(0);
}
