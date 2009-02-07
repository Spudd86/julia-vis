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
static void line16(SDL_Surface *s, 
                   int x1, int y1, 
                   int x2, int y2, 
                   Uint32 color);

static inline float getsamp(float *data, int len, int i, int w) {
	float res = 0;
	int l = IMAX(i-w, 1); // skip sample 0 it's average for energy for entire interval
	int u = IMIN(i+w, len);
	for(int i = l; i < u; i++) {
		res += data[i];
	}
	return res / (u-l);
}

int main(int argc, char **argv) 
{    
	optproc(argc, argv, &opts);
	SDL_Surface *screen = sdl_setup(&opts, IM_SIZE);
	int im_w = screen->w - screen->w%8, im_h = screen->h - screen->h%8;
#ifdef HAVE_JACK
	if(opts.use_jack) {
		printf("staring jack\n");
		jack_setup();
	} else
#endif
		audio_setup_pa();
	
	usleep(1000);
	
	Uint32 frmcnt = 0;
	Uint32 tick0;
	Uint32 fps_oldtime = tick0 = SDL_GetTicks();
	float frametime = 100;
	SDL_Event	event;
	
	int oldbc = 0;
	uint8_t *beats = malloc(sizeof(uint8_t)*im_w);
	memset(beats, 0, sizeof(uint8_t)*im_w);
	
	while(SDL_PollEvent(&event) >= 0)
	{
		if(event.type == SDL_QUIT 
			|| (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE))
			break;
		
		SDL_Rect r = {0,0, im_w, im_h};
		SDL_FillRect(screen, &r, 0);
		
		audio_data d;
		beat_data bd;
		beat_get_data(&bd);
		audio_get_samples(&d);
		
		if(SDL_MUSTLOCK(screen) && SDL_LockSurface(screen) < 0) break;
		
		int ox, oy;
		
		for(int i=0; i<d.len; i++) // draw a simple 'scope
			putpixel(screen, im_w*3/4-1 + i*im_w/(d.len*4), im_h/8 - d.data[i]*im_h/8, 0xffffff);
		
		audio_get_fft(&d);
		//~ ox = 0.125f*im_w; oy = (1.0f-2*d.data[0])*im_h*0.95;
		//~ for(int i=0; i<d.len; i++) {// draw a simple 'scope
			//~ int x = 0.125f*im_w+i*0.75f*im_w/d.len;
			//~ int y = (1.0f-2*d.data[i])*im_h*0.95;
			//~ line16(screen, ox, oy, x, y, 0x1f);
			//~ ox=x; oy=y;
			//~ //putpixel(screen, 0.125f*im_w+i*0.75f*im_w/d.len, (1.0f-2*d.data[i])*im_h*0.95, 0xff00);
		//~ }
		ox = 0.125f*im_w; oy = (1.0f-getsamp(d.data, d.len, 0, d.len/(bd.bands*4)))*im_h*0.95;
		for(int i=0; i<bd.bands; i++) {// draw a simple 'scope
			int x = 0.125f*im_w+i*0.75f*im_w/(bd.bands);
			//int y = 0.75*im_h;
			int y = (1.0f - getsamp(d.data, d.len, i*d.len/(bd.bands*2), d.len/(bd.bands*4)))*im_h*0.95;
			line16(screen, ox, oy, x, y, 0x1f);
			//putpixel(screen,x, y, 0x1f);
			ox=x; oy=y;
			//putpixel(screen, 0.125f*im_w+i*0.75f*im_w/d.len, (1.0f-2*d.data[i])*im_h*0.95, 0xff00);
		}
		int beat_count = beat_get_count();
		int beat_off = ((fps_oldtime-tick0)*982/10000)%im_w;
		
		for(int i=0; i < im_w; i++) {
			if(beats[(i+beat_off)%im_w]) line16(screen, i, im_h/2-10, i, im_h/2 + 10, 0xffff);
		}
		
		ox = 0.125f*im_w;
		//int oy = (1.0f-bd.means[0])*im_h*0.95;
		oy = (0.5 - 2*bd.means[0]*0.5)*im_h;
		
		oy = abs(oy)%im_h;
		ox = abs(ox)%im_w;
		
		for(int i=1; i < bd.bands; i++) {
			int x = 0.125f*im_w + i*0.75f*im_w/bd.bands;
			//int y = (1.0f-bd.means[i])*im_h*0.95;
			int y = (0.5 - 2*bd.means[i]*0.5)*im_h;
			//putpixel(screen, x, y, 0x1f<<5);
			
			y = abs(y)%im_h;
			x = abs(x)%im_w;
			line16(screen, ox, oy, x, y, 0x3f<<5);
			ox = x; oy = y;
		}
		
		ox = 0.125f*im_w;
		oy = (0.5 - bd.stddev[0]*0.5)*im_h;
		
		for(int i=1; i < bd.bands; i++) {
			int x = 0.125f*im_w + i*0.75f*im_w/bd.bands;
			int y = (0.5 - bd.stddev[i]*0.5)*im_h;
			putpixel(screen, x, y, 0x1f);
			//line16(screen, abs(ox)/1000, oy, abs(x)/1000, y, 0x3ff);
			ox = x; oy = y;
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

#define sign(a) (((a)<0) ? -1 : (a)>0 ? 1 : 0)
#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))
#define abs(a) (((a)<0) ? -(a) : (a))

static void line16(SDL_Surface *s, 
                   int x1, int y1, 
                   int x2, int y2, 
                   Uint32 color)
{
  int d;
  int x;
  int y;
  int ax;
  int ay;
  int sx;
  int sy;
  int dx;
  int dy;

  Uint8 *lineAddr;
  Sint32 yOffset;

  dx = x2 - x1;  
  ax = abs(dx) << 1;  
  sx = sign(dx);

  dy = y2 - y1;  
  ay = abs(dy) << 1;  
  sy = sign(dy);
  yOffset = sy * s->pitch;

  x = x1;
  y = y1;

  lineAddr = ((Uint8 *)s->pixels) + (y * s->pitch);
  if (ax>ay)
  {                      /* x dominant */
    d = ay - (ax >> 1);
    for (;;)
    {
      *((Uint16 *)(lineAddr + (x << 1))) = (Uint16)color;

      if (x == x2)
      {
        return;
      }
      if (d>=0)
      {
        y += sy;
        lineAddr += yOffset;
        d -= ax;
      }
      x += sx;
      d += ay;
    }
  }
  else
  {                      /* y dominant */
    d = ax - (ay >> 1);
    for (;;)
    {
      *((Uint16 *)(lineAddr + (x << 1))) = (Uint16)color;

      if (y == y2)
      {
        return;
      }
      if (d>=0) 
      {
        x += sx;
        d -= ay;
      }
      y += sy;
      lineAddr += yOffset;
      d += ax;
    }
  }
}

