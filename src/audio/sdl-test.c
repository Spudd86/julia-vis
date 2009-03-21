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


#define IM_SIZE (768)

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

static inline float getsamp(float *data, int len, int i, int w) {
	float res = 0;
	int l = IMAX(i-w, 1); // skip sample 0 it's average for energy for entire interval
	int u = IMIN(i+w, len);
	for(int i = l; i < u; i++) {
		res += data[i];
	}
	return res / (u-l);
}

static float sigmoid(float x) {
	float e = expf(x);
	return e/(1+e);
}

int main(int argc, char **argv) 
{    
	optproc(argc, argv, &opts);
	SDL_Surface *screen = sdl_setup(&opts, IM_SIZE);
	int im_w = screen->w - screen->w%8, im_h = screen->h - screen->h%8;
	
	audio_init(&opts);
	
	usleep(1000);
	
	Uint32 frmcnt = 0;
	Uint32 tick0;
	Uint32 fps_oldtime = tick0 = SDL_GetTicks();
	float frametime = 100;
	SDL_Event	event;
	float beat_throb = 0.0f;
	
	int oldbc = 0;
	uint8_t *beats = malloc(sizeof(uint8_t)*im_w);
	memset(beats, 0, sizeof(uint8_t)*im_w);
	
	SDL_Surface *voice_print = SDL_CreateRGBSurface(SDL_HWSURFACE, im_w, im_h/2, screen->format->BitsPerPixel, screen->format->Rmask, screen->format->Gmask, screen->format->Bmask, screen->format->Amask);
	
	const int pixbits = screen->format->BitsPerPixel;
	const int green = (pixbits == 32)?0xff00:(0x3f<<5);
	const int blue = (pixbits == 32)?0xff:0x1f;
	
	while(SDL_PollEvent(&event) >= 0)
	{
		if(event.type == SDL_QUIT 
			|| (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE))
			break;
		
		SDL_Rect r = {0,0, im_w, im_h/2};
		SDL_FillRect(screen, &r, 0);
		
		audio_data d;
		beat_data bd;
		beat_get_data(&bd);
		audio_get_fft(&d);
		
		//SDL_LockSurface(voice_print);
		if(SDL_MUSTLOCK(voice_print) && SDL_LockSurface(voice_print) < 0) { printf("failed to lock voice_print\n"); break; }
		int vpx = audio_get_buf_count() % im_w;
		for(int i=0; i < im_h/2; i++) {
			int bri = 255*log2f(d.data[i*d.len/im_h]*255+1.0f)/8;
			if(pixbits == 16) {
				int rb = bri >> 3;
				int g = bri >> 2;
				putpixel(voice_print, vpx, i, ((rb<<11) | (g<<5) | rb));
			} else if(pixbits == 15) {
				int p = bri >> 3;
				putpixel(voice_print, vpx, i, ((p<<10) | (p<<5) | p));
			} else {
				putpixel(voice_print, vpx, i, ((bri<<16) | (bri<<8) | bri));
			}
		}
		if(SDL_MUSTLOCK(voice_print)) SDL_UnlockSurface(voice_print);

		SDL_Rect blit_rect = { 0, 0, vpx, im_h/2 };
		SDL_Rect blit_rect2 = { im_w-vpx-1, im_h/2+1, vpx, im_h/2 };
		SDL_BlitSurface(voice_print, &blit_rect, screen, &blit_rect2);
		blit_rect2.w = blit_rect.w = blit_rect2.x; blit_rect2.x = 0; blit_rect.x = vpx+1;
		SDL_BlitSurface(voice_print, &blit_rect, screen, &blit_rect2);
		
		
		if(SDL_MUSTLOCK(screen) && SDL_LockSurface(screen) < 0) { printf("failed to lock screen\n"); break; }
		
		int ox, oy;
		
		ox = 0.125f*im_w; oy = (1.0f-2*getsamp(d.data, d.len, 0, d.len/(bd.bands*4)))*(im_h-2);
		for(int i=0; i<bd.bands; i++) {// draw a simple 'scope
			int x = 0.125f*im_w+i*0.75f*im_w/(bd.bands);
			int y = (1.0f - 2*getsamp(d.data, d.len, i*d.len/(bd.bands*2), d.len/(bd.bands*4)))*(im_h-2);
			draw_line(screen, ox, oy, x, y, blue);
			ox=x; oy=y;
		}
		
		ox = 0.125f*im_w;
		oy = (1.0f-2*bd.means[0])*(im_h-2);
		oy = abs(oy)%im_h;
		ox = abs(ox)%im_w;
		putpixel(screen, ox, (1.0f - 2*bd.stddev[0])*(im_h-2), 0xffffff00);
		for(int i=1; i < bd.bands; i++) {
			int x = 0.125f*im_w + (i+1)*0.75f*im_w/bd.bands;
			int y = (1.0f - 2*bd.means[i])*(im_h-2);
			
			y = abs(y)%im_h;
			x = abs(x)%im_w;
			draw_line(screen, ox, oy, x, y, green);
			ox = x; oy = y;
			
			x = 0.125f*im_w + i*0.75f*im_w/bd.bands;
			y = (1.0f - 2*bd.stddev[i])*(im_h-2);
			putpixel(screen, x, y, 0xffff00);
		}
		
		int bh = (im_h/bd.bands)*2;
		for(int b=0; b < bd.bands/4; b++) {
			int by = b*bh;
			ox = 0;
			oy = (1.0f - log2f(beat_gethist(&bd, b, 0)*31+1.0f)/5)*bh+by; oy = IMAX(oy, 0);
			for(int i=1; i < bd.histlen; i++) {
				int x = (i+1)*(im_w-1)/bd.histlen;
				float s = log2f(beat_gethist(&bd, b, i)*31+1.0f)/5;
				int y = (1.0f - s)*bh+by;
				y = IMAX(y, 0);
				draw_line(screen, ox, oy, x, y, 0xffffffff);
				ox = x; oy = y;
			}
		}
		
		int beat_count = beat_get_count();
		int beat_off = ((fps_oldtime-tick0)*982/10000)%im_w;
		//int beat_off = ((fps_oldtime-tick0)*1000/10000)%im_w;
		//int beat_off = audio_get_buf_count() % im_w;
		
		for(int i=0; i < im_w; i++) {
			if(beats[(i+beat_off)%im_w]) draw_line(screen, i, im_h*3/4-5, i, im_h*3/4+5, 0xffffffff);
		}
		
		draw_line(screen, 2, im_h-10, 2, (sigmoid(-0.1*beat_throb)+0.5)*(im_h-12), 0xffffffff);
		draw_line(screen, 3, im_h-10, 3, (sigmoid(-0.1*beat_throb)+0.5)*(im_h-12), 0xffffffff);
		
		if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
		
		char buf[128];
		sprintf(buf,"%6.1f FPS %4i beats %6.1f", 1000.0f / frametime, beat_count, beat_throb);
		DrawText(screen, buf);
		
		SDL_Flip(screen);
		
		frmcnt++;
		Uint32 now = SDL_GetTicks();
		//int delay =  (tick0 + frmcnt*10000/1000) - now;
		int delay =  (tick0 + frmcnt*10000/982) - now;
		beats[beat_off] = (oldbc != beat_count);
		beat_throb = beat_throb*(0.996) + (oldbc != beat_count);
		oldbc = beat_count;
		
		if(delay > 0)
			SDL_Delay(delay);
		
		frametime = 0.02f * (now - fps_oldtime) + (1.0f - 0.02f) * frametime;
		fps_oldtime = now;
	}

	frmcnt++;
    return 0;
}
