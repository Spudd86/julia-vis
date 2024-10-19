
#include "common.h"

#include "pallet.h"

#include "audio/audio.h"

#include "software/pixmisc.h"
#include "software/scope_render.h"
#include "getsamp.h"

#include <SDL.h>
#include "sdl/sdlsetup.h"
#include "sdlhelp.h"

#define IM_SIZE (512)

static opt_data opts;

//#define POINT_SPAN_DEBUG_LOG 1

#if POINT_SPAN_DEBUG_LOG
extern int point_span_scope_debug_level;
#endif

struct old_scope_renderer {
	int iw, ih;
	int samp;
	float tx, ty, tz;

	uint16_t *restrict tex_data;
	int tex_w;
	int tex_h;
};

/**************************************************************************************************************************************
 * Old dot based code
 */

static struct old_scope_renderer *init_old_scope(int w, int h, int samp)
{
	struct old_scope_renderer *self = calloc(sizeof(*self), 1);

	self->iw = w; self->ih = h;
	self->samp = samp;
	
	int line_width = IMAX(IMAX(w/24, 8), IMAX(h/24, 8));
	int tex_w = self->tex_w = line_width;
	int tex_h = self->tex_h = line_width;
	
	self->tex_data = calloc(sizeof(uint16_t), tex_w*(tex_h+1)); // add extra padding to be able to run past the end
	
	for(int y=0; y < tex_h; y++)  {
		for(int x=0; x < tex_w; x++) {
			float u = (2.0f*x)/(tex_w-1) - 1, v = (2.0f*y)/(tex_h-1) - 1;
			self->tex_data[y*tex_w + x] = (uint16_t)(expf(-4.5f*0.5f*log2f((u*u+v*v) + 1.0f))*(UINT16_MAX));
		}
	}
	
	return self;
}

static void draw_points(struct old_scope_renderer *self, void *restrict dest, const uint32_t *pnts)
{
	const int pnt_stride = self->tex_w;
	const int iw = self->iw;

	for(int i=0; i<self->samp; i++) {
		const uint32_t ipx = pnts[i*2+0], ipy = pnts[i*2+1];
		const uint32_t yf = ipy&0xff, xf = ipx&0xff;

		uint32_t a00 = (yf*xf);
		uint32_t a01 = (yf*(256-xf));
		uint32_t a10 = ((256-yf)*xf);
		uint32_t a11 = ((256-yf)*(256-xf));

		uint32_t off = (ipy/256u)*(unsigned)iw + ipx/256u;

		const uint16_t *s0 = self->tex_data;
		const uint16_t *s1 = self->tex_data + pnt_stride;
		for(int y=0; y < self->tex_h; y++) {
			uint16_t *restrict dst_line = (uint16_t *restrict)dest + off + iw*y;
			for(int x=0; x < self->tex_w; x++) {
#if 0
				uint16_t res = (s0[x]*a00 + s0[x+1]*a01
				              + s1[x]*a10 + s1[x+1]*a11)>>16;
#else
				uint16_t res = s0[x];
#endif
				res = IMAX(res, dst_line[x]);
				dst_line[x] = res;
			}
			s0 += pnt_stride;
			s1 += pnt_stride;
		}
	}
}

void old_scope_render(struct old_scope_renderer *self,
                  void *restrict dst,
                  float tx, float ty, float tz,
                  const float *audio,
                  int audiolen)
{
	int samp = self->samp;
	int iw = self->iw, ih = self->ih;

	float cx=cosf(tx), cy=cosf(ty), cz=cosf(tz);
	float sx=sinf(tx), sy=sinf(ty), sz=sinf(tz);

	const float R[][3] = {
		{cz*cy-sz*sx*sy, -sz*cx, -sy*cz-cy*sz*sx},
		{sz*cy+cz*sx*sy,  cz*cx, -sy*sz+cy*cz*sx},
		{cx*sy         ,    -sx,  cy*cx}
	};

	uint32_t pnts[samp*2]; // TODO: if we do dynamically choose number of points based on spacing move allocating this into context object

	for(int i=0; i<samp; i++) {
		//TODO: maybe change the spacing of the points depending on
		// the rate of change so that the distance in final x,y coords is
		// approximately constant?
		// maybe step with samples spaced nicely for for the straight line and
		// let it insert up to n extra between any two?
		// might want initial spacing to be a little tighter, probably need to tweak 
		// that and max number of extra to insert.
		// also should check spacing post transform
		// probably want to shoot for getting about 3 pixels apart at 512x512
		float s = getsamp(audio, audiolen, i*audiolen/(samp-1), audiolen/96);

		// shape the waveform a bit, more slope near zero so queit stuff still makes it wiggle
		s=copysignf(log2f(fabsf(s)*3+1)/2, s);

		// xt ∈ [-0.5, 0.5] ∀∃∄∈∉⊆⊈⊂⊄
		float xt = (float)i/(float)(samp - 1) - 0.5f; // (i - (samp-1)/2.0f)*(1.0f/(samp-1));
		float yt = 0.2f*s;
		float zt = 0.0f;

		float x = R[0][0]*xt + R[1][0]*yt + R[2][0]*zt;
		float y = R[0][1]*xt + R[1][1]*yt + R[2][1]*zt;
		float z = R[0][2]*xt + R[1][2]*yt + R[2][2]*zt;
		float zvd = 0.75f/(z+2);

		float xi = x*zvd*iw+(iw - self->tex_w)/2.0f;
		float yi = y*zvd*ih+(ih - self->tex_h)/2.0f;

		pnts[i*2+0] = (uint32_t)(xi*256);
		pnts[i*2+1] = (uint32_t)(yi*256);
	}
	draw_points(self, dst, pnts);
}

/**************************************************************************************************************************************
 * Main
 */


int main(int argc, char **argv)
{
	optproc(argc, argv, &opts);
	if(audio_init(&opts) < 0) exit(1);
	SDL_Surface *screen = sdl_setup(&opts, IM_SIZE);
	int im_w = screen->w - screen->w%16, im_h = screen->h - screen->h%16;
	printf("running with %dx%d bufs\n", im_w, im_h);

	uint16_t *point_surf;
	point_surf = aligned_alloc(512, 512 + im_w * im_h * sizeof(uint16_t));
	float *audio = calloc(sizeof(float), 2048);
	int num_audio_samples = 1024;
	
	float tx = 0, ty = 0, tz = 0;
	
	struct scope_renderer* scope1 = scope_renderer_new(im_w, im_h, 2);
	struct scope_renderer* scope2 = scope_renderer_new(im_w, im_h, MIN(MIN(im_w/8, im_h/8), 128));
	struct old_scope_renderer *old_scope = init_old_scope(im_w, im_h, MIN(MIN(im_w/8, im_h/8), 128)*4);

	int m = 0, cnt = 0;

	struct pal_ctx *pal_ctx = pal_ctx_new(screen->format->BitsPerPixel == 8); //TODO: write a helper that picks the right pallet channel order based on screen->format

	Uint32 tick0, fps_oldtime;
	fps_oldtime = tick0 = SDL_GetTicks();
	float frametime = 100;
	int beats = beat_get_count();
	uint32_t last_beat_time = tick0;
	uint32_t lastpalstep = tick0;
	uint32_t now = tick0;
	uint32_t maxfrms = 0;

	int scope_mode = 1, pause_ticks = 0, cur_pal = 0;;
	int lastframe_key = 0;

	SDL_Event event;
	memset(&event, 0, sizeof(event));
	while(SDL_PollEvent(&event) >= 0) {
	#if POINT_SPAN_DEBUG_LOG
		point_span_scope_debug_level = 0;
	#endif
		if(event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) break;
		
		if(event.type == SDL_KEYDOWN)
		{
			if(event.key.keysym.sym == SDLK_F1 && !lastframe_key) { scope_mode = 0; }
			else if(event.key.keysym.sym == SDLK_F2 && !lastframe_key) { scope_mode = 1; }
			else if(event.key.keysym.sym == SDLK_F3 && !lastframe_key) { scope_mode = 2; }
			else if(event.key.keysym.sym == SDLK_SPACE && !lastframe_key) { if(pause_ticks) {tick0 = now;} pause_ticks= !pause_ticks; }
#if POINT_SPAN_DEBUG_LOG
			else if(event.key.keysym.sym == SDLK_1 && !lastframe_key) { point_span_scope_debug_level = 1; }
			else if(event.key.keysym.sym == SDLK_2 && !lastframe_key) { point_span_scope_debug_level = 2; }
			else if(event.key.keysym.sym == SDLK_3 && !lastframe_key) { point_span_scope_debug_level = 3; }
			else if(event.key.keysym.sym == SDLK_4 && !lastframe_key) { point_span_scope_debug_level = 4; }
#endif
			else if(event.key.keysym.sym==SDLK_LEFT && !lastframe_key) { cur_pal = (cur_pal + 10-1) % 10; pal_ctx_start_switch(pal_ctx, cur_pal); }
			else if(event.key.keysym.sym==SDLK_RIGHT && !lastframe_key) { cur_pal = (cur_pal + 1) % 10; pal_ctx_start_switch(pal_ctx, cur_pal); }

			lastframe_key = 1;
		}
		else
		{
			lastframe_key = 0;
		}

		m = (m+1)&0x1;

		memset(point_surf, 0, im_w * im_h * sizeof(uint16_t));
		switch(scope_mode)
		{
			case 0:
				scope_render(scope1, point_surf, tx,ty,tz, audio, num_audio_samples);
				break;
			case 1:
				scope_render(scope2, point_surf, tx,ty,tz, audio, num_audio_samples);
				break;
			case 2:
				old_scope_render(old_scope, point_surf, tx,ty,tz, audio, num_audio_samples);
				break;
			default:
				break;
		}

		if((now - lastpalstep)*256/1024 >= 1) { // want pallet switch to take ~2 seconds
			pal_ctx_step(pal_ctx, IMIN((now - lastpalstep)*256/1024, 32));
			lastpalstep = now;
		}
		pallet_blit_SDL(screen, point_surf, im_w, im_h, pal_ctx_get_active(pal_ctx));

		char buf[64];
		if(pause_ticks) sprintf(buf,"%6.1f FPS (ticks paused)", 1000.0f / frametime);
		else sprintf(buf,"%6.1f FPS", 1000.0f / frametime);
		DrawText(screen, buf);
		SDL_Flip(screen);

		now = SDL_GetTicks();
		if(!pause_ticks)
		{
    		int newbeat = beat_get_count();
    		if(newbeat != beats) pal_ctx_start_switch(pal_ctx, newbeat);
    
    		if(newbeat != beats && now - last_beat_time > 1000) {
    			last_beat_time = now;
    		}
    		beats = newbeat;
    		
    		if((tick0+(maxfrms*1000)/opts.maxsrc_rate) - now > 1000/opts.maxsrc_rate) {
    			audio_data ad; audio_get_samples(&ad);
    			memcpy(audio, ad.data, sizeof(float)*ad.len);
    			num_audio_samples = ad.len;
    			audio_finish_samples();
    			tx+=0.02f; ty+=0.01f; tz-=0.003f;
    			maxfrms++;
    		}
        }

		now = SDL_GetTicks();
		if(now - fps_oldtime < 10) SDL_Delay(10 - (now - fps_oldtime)); // stay below 1000FPS
		frametime = 0.02f * (now - fps_oldtime) + (1.0f - 0.02f) * frametime;
		fps_oldtime = SDL_GetTicks();
		cnt++;
	}

	pal_ctx_delete(pal_ctx);
	audio_shutdown();
	SDL_Quit();
	return 0;
}

