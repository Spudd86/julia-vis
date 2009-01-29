#include <unistd.h>
#include <stdio.h>
#include <stdlib.h> /* for exit() */
#include <stdbool.h>
#include <math.h>
#include <malloc.h>

#include <SDL.h>

#include "../common.h"
#include "../sdl-misc.h"
#include "audio.h"


#define IM_SIZE (512)

static opt_data opts;

static inline int inrange(int c, int a, int b) { return (unsigned)(c-a) <= (unsigned)(b-a); }

static void putpixel(SDL_Surface *surface, int x, int y, Uint32 pixel)
{
	if(!inrange(x,0,surface->w-1) || !inrange(y, 0, surface->h-1)) return;
	
    int bpp = surface->format->BitsPerPixel/8;
    /* Here p is the address to the pixel we want to set */
    Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;

    switch(bpp) {
    case 1:
        *p = pixel;
        break;

    case 2:
        *(Uint16 *)p = pixel;
        break;

    case 3:
        if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
            p[0] = (pixel >> 16) & 0xff;
            p[1] = (pixel >> 8) & 0xff;
            p[2] = pixel & 0xff;
        } else {
            p[0] = pixel & 0xff;
            p[1] = (pixel >> 8) & 0xff;
            p[2] = (pixel >> 16) & 0xff;
        }
        break;

    case 4:
        *(Uint32 *)p = pixel;
        break;
    }
}

int audio_setup_pa();

int main(int argc, char **argv) 
{    
	optproc(argc, argv, &opts);
	SDL_Surface *screen = sdl_setup(&opts, IM_SIZE);
	int im_w = screen->w - screen->w%8, im_h = screen->h - screen->h%8;
	
	audio_setup_pa();
	usleep(1000);
	
	Uint32 tick0;
	Uint32 fps_oldtime = tick0 = SDL_GetTicks();
	float frametime = 100;
	SDL_Event	event;
	while(SDL_PollEvent(&event) >= 0)
	{
		if(event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_QUIT 
			|| (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE))
			break;
		
		SDL_Rect r = {0,0, im_w, im_h};
		SDL_FillRect(screen, &r, 0);
		
		audio_data d;
		audio_get_samples(&d);
		
		if(SDL_MUSTLOCK(screen) && SDL_LockSurface(screen) < 0) break;
		
		for(int i=0; i<d.len; i++) // draw a simple 'scope
			putpixel(screen, i*im_w/d.len, d.data[i]*im_h/4 + im_h/4, 0xffffff);
		
		audio_get_fft(&d);
		for(int i=0; i<d.len; i++) // draw a simple 'scope
			putpixel(screen, 0.125f*im_w+i*0.75f*im_w/d.len, (1.0f-2*d.data[i])*im_h*0.95, 0xff00);
		
		if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
		
		char buf[128];
		sprintf(buf,"%6.1f FPS %i beats", 1000.0f / frametime, beat_get_count());
		DrawText(screen, buf);
		
		SDL_Flip(screen);
		
		Uint32 now = SDL_GetTicks();
		frametime = 0.02f * (now - fps_oldtime) + (1.0f - 0.02f) * frametime;
		fps_oldtime = now;
	}

    return 0;
}
