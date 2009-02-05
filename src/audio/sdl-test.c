#include "../config.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h> /* for exit() */
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
int jack_setup();

int main(int argc, char **argv) 
{    
	optproc(argc, argv, &opts);
	SDL_Surface *screen = sdl_setup(&opts, IM_SIZE);
	int im_w = screen->w - screen->w%8, im_h = screen->h - screen->h%8;
#ifdef HAVE_JACK
	if(opts.use_jack)
		jack_setup();
	else
#else
		audio_setup_pa();
#endif
	
	usleep(1000);
	
	Uint32 frmcnt = 0;
	Uint32 tick0;
	Uint32 fps_oldtime = tick0 = SDL_GetTicks();
	float frametime = 100;
	SDL_Event	event;
	
	audio_data beat_data = {0, NULL};
	
	int oldbc = 0;
	uint8_t *beats = malloc(sizeof(uint8_t)*im_w);
	memset(beats, 0, sizeof(uint8_t)*im_w);
	
	while(SDL_PollEvent(&event) >= 0)
	{
		if(event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_QUIT 
			|| (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE))
			break;
		
		SDL_Rect r = {0,0, im_w, im_h};
		SDL_FillRect(screen, &r, 0);
		
		audio_data d;
		audio_get_samples(&d);
		beat_band_get_counts(&beat_data);
		
		if(SDL_MUSTLOCK(screen) && SDL_LockSurface(screen) < 0) break;
		
		for(int i=0; i<d.len; i++) // draw a simple 'scope
			putpixel(screen, im_w*3/4-1 + i*im_w/(d.len*4), im_h/8 - d.data[i]*im_h/8, 0xffffff);
		
		audio_get_fft(&d);
		for(int i=0; i<d.len; i++) // draw a simple 'scope
			putpixel(screen, 0.125f*im_w+i*0.75f*im_w/d.len, (1.0f-2*d.data[i])*im_h*0.95, 0xff00);
		
		int beat_count = beat_get_count();
		int beat_off = ((fps_oldtime-tick0)*982/10000)%im_w;
		
		for(int i=0; i < im_w; i++) {
			if(beats[(i+beat_off)%im_w]) putpixel(screen, i, im_h/2, 0xff00);
		}
		
		
		if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
		
		char buf[128];
		sprintf(buf,"%6.1f FPS %i beats", 1000.0f / frametime, beat_count);
		DrawText(screen, buf);
		
		SDL_Flip(screen);
		
		frmcnt++;
		Uint32 now = SDL_GetTicks();
		int delay =  (tick0 + frmcnt*10000/982) - now;
		beats[beat_off] = (oldbc != beat_count);
		oldbc = beat_count;
		
		if(delay > 0)
			SDL_Delay(delay);
		
		frametime = 0.02f * (now - fps_oldtime) + (1.0f - 0.02f) * frametime;
		fps_oldtime = now;
	}

	frmcnt++;
    return 0;
}
