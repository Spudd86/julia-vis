#include "common.h"
#include "sdl-misc.h"
#include "audio.h"

#define IM_SIZE (768)

static opt_data opts;

static inline int inrange(int c, int a, int b) { return (unsigned)(c-a) <= (unsigned)(b-a); }

static inline void putpixel(SDL_Surface *surface, int x, int y, Uint32 pixel)
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

static inline void putpixel_mono(SDL_Surface *surface, int x, int y, int bri) {
	const int pixbits = surface->format->BitsPerPixel;

	if(pixbits == 16) {
		int rb = bri >> 3;
		int g = bri >> 2;
		putpixel(surface, x, y, ((rb<<11) | (g<<5) | rb));
	} else if(pixbits == 15) {
		int p = bri >> 3;
		putpixel(surface, x, y, ((p<<10) | (p<<5) | p));
	} else {
		putpixel(surface, x, y, ((bri<<16) | (bri<<8) | bri));
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

static inline float sigmoid(float x) {
	float e = expf(x);
	return e/(1+e);
}

int main(int argc, char **argv)
{
	optproc(argc, argv, &opts);
	SDL_Surface *screen = sdl_setup(&opts, IM_SIZE);
	int im_w = screen->w, im_h = screen->h ;

	SDL_Surface *voice_print = SDL_CreateRGBSurface(SDL_SWSURFACE,
	                                                im_w,
	                                                im_h/2,
	                                                screen->format->BitsPerPixel,
	                                                screen->format->Rmask,
	                                                screen->format->Gmask,
	                                                screen->format->Bmask,
	                                                screen->format->Amask);
	if(!voice_print) { printf("failed to create voice_print\n"); exit(1); }

	audio_init(&opts);

	unsigned int frmcnt = 0;
	unsigned int tick0, fps_oldtime = tick0 = SDL_GetTicks();
	float frametime = 100;
	SDL_Event	event;
	float beat_throb = 0.0f;
	int ovpx = -1;
	int oldbc = 0;

	const int pixbits = screen->format->BitsPerPixel;
	const int green = (pixbits == 32)?0xff00:(0x3f<<5);
	const int blue = (pixbits == 32)?0xff:0x1f;

	int beath[16][im_w];
	memset(beath, 0, sizeof(beath));

	while(SDL_PollEvent(&event) >= 0)
	{
		if(event.type == SDL_QUIT
			|| (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE))
			break;

		const int vpx = audio_get_buf_count() % im_w;
		if(ovpx == vpx) SDL_Delay(2000/982);

		SDL_Rect r = {0,0, im_w, im_h/2+1};
		SDL_FillRect(screen, &r, 0);

		audio_data d;
		beat_data bd;
		int beat_count = beat_get_count();
		beat_get_data(&bd);
		audio_get_fft(&d);
		//TODO: make copy of fft so we can finish read sooner

		if(SDL_MUSTLOCK(voice_print) && SDL_LockSurface(voice_print) < 0) { printf("failed to lock voice_print\n"); break; }

		for(int i=0; i < IMIN(d.len,im_h/2); i++) {
			int bri = 255*log2f(d.data[i*d.len/IMIN(d.len,im_h/2)]*255+1.0f)/8;
			putpixel_mono(voice_print, vpx, i, bri);
		}
		ovpx = vpx;
		if(SDL_MUSTLOCK(voice_print)) SDL_UnlockSurface(voice_print);

		if(oldbc != beat_count)
			draw_line(voice_print, vpx, im_h/4-5, vpx, im_h/4+5, 0xffffffff);

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
			int x = 0.125f*im_w + i*0.75f*im_w/bd.bands;
			int y = (1.0f - 2*bd.means[i])*(im_h-2);

			y = abs(y)%im_h;
			x = abs(x)%im_w;
			draw_line(screen, ox, oy, x, y, green);
			ox = x; oy = y;

			x = 0.125f*im_w + i*0.75f*im_w/bd.bands;
			y = (1.0f - 2*bd.stddev[i])*(im_h-2);
			putpixel(screen, x, y, 0xffff00);
		}

		for(int b=0; b < 16; b++) {
			const float s = 1 - 0.2f*log2f(1+31*beat_gethist(&bd, b, bd.histlen-1));
			beath[b][vpx] = IMAX((b + s)*im_h/32, 0);
			ox = 0; oy = beath[b][(vpx+1)%im_w];
			for(int i=1; i < im_w; i++) {
				int y = beath[b][(vpx+i+1)%im_w];
				draw_line(screen, ox, oy, i, y, 0xffffffff);
				ox = i; oy = y;
			}
		}

		draw_line(screen, 2, im_h-10, 2, (sigmoid(-0.1*beat_throb)+0.5)*(im_h-12), 0xffffffff);
		draw_line(screen, 3, im_h-10, 3, (sigmoid(-0.1*beat_throb)+0.5)*(im_h-12), 0xffffffff);

		audio_fft_finsih_read();

		if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);

		char buf[128];
		sprintf(buf,"%6.1f FPS %4i beats %6.1f", 1000.0f / frametime, beat_count, beat_throb);
		DrawText(screen, buf);

		SDL_Flip(screen);

		frmcnt++;
		Uint32 now = SDL_GetTicks();
		int delay =  (tick0 + frmcnt*10000/982) - now;
		beat_throb = beat_throb*(0.996) + (oldbc != beat_count);
		oldbc = beat_count;

		if(delay > 0)
			SDL_Delay(delay);

		frametime = 0.02f * (now - fps_oldtime) + (1.0f - 0.02f) * frametime;
		fps_oldtime = now;
	}

	SDL_FreeSurface(voice_print);
	frmcnt++;
    return 0;
}
