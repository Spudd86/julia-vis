#include "common.h"

#include "audio/audio.h"
#include "audio/beat.h"
#include "getsamp.h"

#include <SDL.h>
#include "software/sdl/sdlhelp.h"
#include "sdlsetup.h"

#define IM_SIZE (768)

static opt_data opts;

void split_radix_real_complex_fft(float *x, uint32_t n);

static void get_next_fft(audio_data *d);
static int get_buf_count(void);

static inline void putpixel(SDL_Surface *surface, int x, int y, Uint32 pixel);
static inline void putpixel_mono(SDL_Surface *surface, int x, int y, int bri);

typedef void (*audio_drv_shutdown_t)(void);
static audio_drv_shutdown_t audio_drv_shutdown = NULL;

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

	uint64_t frmcnt = 0;
	uint32_t tick0, fps_oldtime = tick0 = SDL_GetTicks();
	float frametime = 100;
	SDL_Event	event;
	float beat_throb = 0.0f;
	int ovpx = 0;
	int oldbc = 0;

	const int pixbits = screen->format->BitsPerPixel;
	const int green = (pixbits == 32)?0xff00:(0x3f<<5);
	const int blue = (pixbits == 32)?0xff:0x1f;

	int beath[16][im_w];
	memset(beath, 0, sizeof(beath));

	struct beat_ctx *beat_ctx = beat_new();
	int beat_nbands = beat_ctx_bands(beat_ctx);

	uint32_t now = SDL_GetTicks();
	audio_data d;
	get_next_fft(&d);
	while(SDL_PollEvent(&event) >= 0)
	{
		if(event.type == SDL_QUIT
			|| (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE))
			break;

		uint64_t next_frame =  (tick0 + frmcnt*10000/982);
		//uint64_t next_frame =  (tick0 + frmcnt*10000/491);
		if(next_frame > now) SDL_Delay(next_frame - now);

		//TODO: when we get new buffers make sure we process all of them before we attempt
		// to actually update the front buffer, so that if we're running slow we don't
		// waste time drawing frames that won't be seen

		int avpx = get_buf_count() % im_w;
		//if(ovpx == avpx) continue;
		//while(ovpx == avpx) { //SDL_Delay(0);
		//	avpx = audio_get_buf_count() % im_w;
		//}
		int vpx = ovpx;

		while (ovpx != avpx) {
			vpx = (ovpx+1) % im_w;

			SDL_Rect r = {0,0, im_w, im_h/2+1};
			SDL_FillRect(screen, &r, 0);

			get_next_fft(&d);

			beat_ctx_update(beat_ctx, d.data, d.len);
			int beat_count = beat_ctx_count(beat_ctx);

			for(int b=0; b < 16; b++) {
				float samp = getsamp(d.data, d.len, b*d.len/(beat_nbands*2), d.len/(beat_nbands*4));
				float s = 1 - 0.2f*log2f(1+31*samp);
				beath[b][ovpx] = MAX((b + s)*(im_h/32), b);
			}

			if(SDL_MUSTLOCK(voice_print) && SDL_LockSurface(voice_print) < 0) { printf("failed to lock voice_print\n"); break; }

			for(int i=0; i < MIN(d.len,im_h/2); i++) {
				int bri = 255*log2f(d.data[i*d.len/MIN(d.len,im_h/2)]*255+1.0f)/8;
				putpixel_mono(voice_print, vpx, i, bri);
			}

			if(SDL_MUSTLOCK(voice_print)) SDL_UnlockSurface(voice_print);

			if(oldbc != beat_count)
				draw_line(voice_print, vpx, im_h/4-5, vpx, im_h/4+5, 0xffffffff);

			beat_throb = beat_throb*(0.997f) + (oldbc != beat_count);
			oldbc = beat_count;
			ovpx = vpx;
		}

		// screen update down here

		SDL_Rect blit_rect = { 0, 0, vpx, im_h/2 };
		SDL_Rect blit_rect2 = { im_w-vpx-1, im_h/2+1, vpx, im_h/2 };
		SDL_BlitSurface(voice_print, &blit_rect, screen, &blit_rect2);
		blit_rect2.w = blit_rect.w = blit_rect2.x; blit_rect2.x = 0; blit_rect.x = vpx+1;
		SDL_BlitSurface(voice_print, &blit_rect, screen, &blit_rect2);

		if(SDL_MUSTLOCK(screen) && SDL_LockSurface(screen) < 0) { printf("failed to lock screen\n"); break; }

		int ox, oy;
		ox = 0.125f*im_w; oy = (1.0f-2*getsamp(d.data, d.len, 0, d.len/(beat_nbands*4)))*(im_h-2);
		for(int i=0; i<beat_nbands; i++) {// draw a simple 'scope
			int x = 0.125f*im_w+i*0.75f*im_w/(beat_nbands);
			int y = (1.0f - 2*getsamp(d.data, d.len, i*d.len/(beat_nbands*2), d.len/(beat_nbands*4)))*(im_h-2);
			draw_line(screen, ox, oy, x, y, blue);
			ox=x; oy=y;
		}

		beat_data bd;
		beat_ctx_get_data(beat_ctx, &bd);
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
			ox = 0; oy = beath[b][(vpx+1)%im_w];
			for(int i=1; i < im_w; i++) {
				int y = beath[b][(vpx+i+1)%im_w];
				draw_line(screen, ox, oy, i, y, 0xffffffff);
				ox = i; oy = y;
			}
		}

		draw_line(screen, 2, im_h-10, 2, (sigmoid(-0.1f*beat_throb)+0.5f)*(im_h-12), 0xffffffff);
		draw_line(screen, 3, im_h-10, 3, (sigmoid(-0.1f*beat_throb)+0.5f)*(im_h-12), 0xffffffff);

		if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);

		char buf[128];
		sprintf(buf,"%6.1f FPS %6.1f", 1000.0f / frametime, beat_throb);
		DrawText(screen, buf);

		SDL_Flip(screen);

		frmcnt++;
		now = SDL_GetTicks();
		frametime = 0.02f * (now - fps_oldtime) + (1.0f - 0.02f) * frametime;
		fps_oldtime = now;
	}

	SDL_FreeSurface(voice_print);

	audio_drv_shutdown();

    return 0;
}

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


#include "audio/rb.h"
#include "audio/audio-private.h"

int audio_init(const opt_data *od)
{
	printf("\nAudio input starting...\n");

	int rc;
	switch(od->audio_driver) {
	#ifdef HAVE_JACK
		case AUDIO_JACK:
			rc = jack_setup(od);
			audio_drv_shutdown = jack_shutdown;
			break;
	#endif
	#ifdef HAVE_PULSE
		case AUDIO_PULSE:
			audio_drv_shutdown = pulse_shutdown;
			rc = pulse_setup(od);
			break;
	#endif
	#ifdef HAVE_PORTAUDIO
		case AUDIO_PORTAUDIO:
			rc = audio_setup_pa(od);
			audio_drv_shutdown = audio_stop_pa;
			break;
	#endif
	#ifdef HAVE_SNDFILE
		case AUDIO_SNDFILE:
			rc = filedecode_setup(od);
			audio_drv_shutdown = filedecode_shutdown;
			break;
	#endif
		default:
			printf("No Audio driver!\n");
			//rc = audio_setup(48000);
			rc = -1;
	}

	if(rc < 0) printf("Audio setup failed!\n");
	else printf("Finished audio setup\n\n");
	return rc;
}

#define MAX_SAMP 2048

static uint64_t buf_count = 0;
static size_t nr_samp = 0;
static float fft_tmp[MAX_SAMP];
static float samp_bufs[2][MAX_SAMP];
static float *prev_samp = NULL;
static float *cur_samp = NULL;
static rb_t *samp_rb = NULL;

int audio_setup(int sr)
{
	nr_samp = (sr<50000)?MAX_SAMP/2:MAX_SAMP;

	printf("Sample Rate %i\nUsing %zu samples/buffer\n", sr, nr_samp/2);

	for(int i=0; i<MAX_SAMP; i++) {
		fft_tmp[i] = samp_bufs[0][i] = samp_bufs[1][i] = 0;
	}

	prev_samp = samp_bufs[0];
	cur_samp = samp_bufs[1];

	samp_rb = rb_create(8*MAX_SAMP*sizeof(float));

	return 0;
}

void audio_update(const float *in, int n)
{
	rb_write(samp_rb, (const char*)in, n*sizeof(*in));
}

static int get_buf_count()
{
	return buf_count + rb_read_space(samp_rb)/(sizeof(float)*nr_samp/2);
}

static float *do_fft(float *in1, float *in2)
{
	for(int i=0; i<nr_samp;i++) { // window samples
		// Hanning window
		float w = 0.5f*(1.0f - cosf((2*M_PI_F*i)/(nr_samp-1)));
		fft_tmp[i] = ((i < nr_samp/2)?in1[i]:in2[i-nr_samp/2])*w;
	}

	split_radix_real_complex_fft(fft_tmp, nr_samp);
	float *fft = fft_tmp;

	const float scl = 2.0f/nr_samp;
	fft[0] = fabsf(fft_tmp[0])*scl;
	for(int i=1; i < nr_samp/2; i++)
		fft[i] = sqrtf(fft_tmp[i]*fft_tmp[i] + fft_tmp[nr_samp-i]*fft_tmp[nr_samp-i])*scl;
	fft[nr_samp/2] = fabsf(fft_tmp[nr_samp/2])*scl;

	return fft;
}

static void get_next_fft(audio_data *d)
{
	d->len = nr_samp/2+1;
	d->data = fft_tmp;

	size_t buf_len = sizeof(float)*(nr_samp/2);
	if(rb_read_space(samp_rb)/sizeof(float) >= (nr_samp/2)) {
		rb_read(samp_rb, (void *)cur_samp, buf_len);

		d->data = do_fft(cur_samp, prev_samp);

		float *tmp = cur_samp;
		cur_samp = prev_samp;
		prev_samp = tmp;

		buf_count++;
	}
}

#include "software/pixmisc.h"
// so that link doesn't fail, we don't actually need any pallet blitting
// but we need functions out of sdlhelp.c, but pallet_blit_SDL() is in that file
// and needs these symbols defined
pallet_blit8_fn pallet_blit8 = NULL;
pallet_blit555_fn pallet_blit555 = NULL;
pallet_blit565_fn pallet_blit565 = NULL;
pallet_blit32_fn pallet_blit32 = NULL;
