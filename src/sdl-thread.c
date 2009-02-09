#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <malloc.h>

#include "mtwist/mtwist.h"

#include <mm_malloc.h>

#include <SDL.h>
#include <SDL_thread.h>
#include <SDL_ttf.h>

#include "tribuf.h"
#include "common.h"

#include "pixmisc.h"
#include "sdl-misc.h"
#include "map.h"

#include "audio/audio.h"

#define IM_SIZE (384)

#define MAP soft_map_interp
#define PALLET_BLIT pallet_blit_SDL

static opt_data opts;
static int im_w = 0, im_h = 0;
static int running = 1;
static float map_fps=0;

static int run_map_thread(tribuf *tb) 
{
	float vx, vy, x, y, xt, yt;
	
	mt_goodseed(); // seed our PSRNG
	vx = vy = 0;
	xt = 0.5f*((mt_lrand()%im_w)*2.0f/im_w - 1.0f); yt = 0.5f*((mt_lrand()%im_h)*2.0f/im_h - 1.0f);
	x = 0.5f*((mt_lrand()%im_w)*2.0f/im_w - 1.0f); y = 0.5f*((mt_lrand()%im_h)*2.0f/im_h - 1.0f);
	
	Uint64 tick0, fps_oldtime;
	fps_oldtime = tick0 = SDL_GetTicks();
	float frametime = 100;
	int beats = beat_get_count();
	int done_time = tick0;
	unsigned int last_beat_time = tick0;
	
	uint16_t *map_src = tribuf_get_read(tb);
    while(running) 
	{
		uint16_t *map_dest = tribuf_get_write(tb);

		MAP(map_dest, map_src, im_w, im_h, x, y);
		maxblend(map_dest, maxsrc_get(), im_w, im_h);
		
		tribuf_finish_write(tb);
		map_src=map_dest;

		Uint32 now = SDL_GetTicks();

		frametime = 0.02f * (now - fps_oldtime) + (1.0f - 0.02f) * frametime;
		map_fps = 1000.0f / frametime;
		fps_oldtime = now;
		
		int newbeat = beat_get_count();
		if(newbeat != beats && now - last_beat_time > 1000) {
			xt = 0.5f*((mt_lrand()%im_w)*2.0f/im_w - 1.0f); yt = 0.5f*((mt_lrand()%im_h)*2.0f/im_h - 1.0f);
			last_beat_time = now;
		}
		beats = newbeat;
		
		const float delt = 30.0f/200.0f;
		const float tsped = 0.005;
		const int dt = 1000/200;
		
		while(done_time <= now) {
			float xtmp = xt - x, ytmp = yt-y;
			float mag = xtmp*xtmp+ytmp*ytmp;
				mag=(mag>0)?delt*0.4f/sqrtf(mag):0;
			vx=(vx+xtmp*mag*tsped)/(tsped+1);
			vy=(vy+ytmp*mag*tsped)/(tsped+1);
			x=x+vx*delt;
			y=y+vy*delt;
			
			done_time += dt;
		}
    }
	return 0;
}

int audio_setup_pa();
int jack_setup();

static SDL_Event user_event;
static Uint32 timercallback(Uint32 t, void *data) {SDL_PushEvent(&user_event); return t; }
int main(int argc, char **argv) 
{    
	optproc(argc, argv, &opts);
	SDL_Surface *screen = sdl_setup(&opts, IM_SIZE);
	im_w = screen->w - screen->w%16; im_h = screen->h - screen->h%8;
	
#ifdef HAVE_JACK
	if(opts.use_jack)
		jack_setup();
	else
#endif
		audio_setup_pa();
	
	maxsrc_setup(im_w, im_h);
	
	uint16_t *map_surf[3];
	for(int i=0; i<3; i++) {
		map_surf[i] = valloc(im_w * im_h * sizeof(uint16_t));
		memset(map_surf[i], 0, im_w * im_h * sizeof(uint16_t));
	}
	
	uint32_t *pal = _mm_malloc(257 * sizeof(uint32_t), 64); // p4 has 64 byte cache line
	for(int i = 0; i < 256; i++) pal[i] = ((2*abs(i-127))<<16) | (i<<8) | ((255-i));
	pal[256] = pal[255];

	tribuf *map_tb = tribuf_new((void **)map_surf);
	
	user_event.type=SDL_USEREVENT;
	user_event.user.code=2;
	user_event.user.data1=NULL;
	user_event.user.data2=NULL;

	printf("running with %dx%d bufs\n", im_w, im_h);
	
	usleep(1000);
	
	SDL_Thread *map_thread = SDL_CreateThread(&run_map_thread, map_tb);
	SDL_AddTimer(1000/opts.draw_rate, &timercallback, NULL);
	
	SDL_Event	event;
	int prevfrm = 0, cnt = 0;
	while(SDL_WaitEvent(&event) >= 0)
	{
		if(event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_QUIT 
				|| (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
			break;
		} else if(event.type == SDL_USEREVENT) {
			PALLET_BLIT(screen, tribuf_get_read(map_tb), im_w, im_h, pal);
			//PALLET_BLIT(screen, maxsrc_get(), im_w, im_h, pal);
			char buf[32];
			sprintf(buf,"%6.1f FPS", map_fps);
			DrawText(screen, buf);
			SDL_Flip(screen);
			//if(tribuf_get_frmnum(map_tb) != prevfrm && !(cnt%2)) {
			if(!(cnt%2)) {
				maxsrc_update();
			}
			cnt++;
		}
	}
	running = 0;
	
	int status;
	SDL_WaitThread(map_thread, &status);

    return 0;
}

