/**
 * sdlgl.c
 *
 */

#include "common.h"
#include "opengl/glmisc.h"
#include "audio/audio.h"

#include <SDL.h>
#include "sdlsetup.h"

int main(int argc, char **argv)
{
	opt_data opts; optproc(argc, argv, &opts);
	if(audio_init(&opts) < 0) exit(1);
	SDL_Surface *screen = sdl_setup_gl(&opts, 512);
	init_gl(&opts, screen->w, screen->h);

	int debug_maxsrc = 0, debug_pal = 0, show_mandel = 0, show_fps_hist = 0;
	int lastframe_key = 0;
	SDL_Event event;
	while(SDL_PollEvent(&event) >= 0) {
		if(event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) break;
		if((event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F1)) { if(!lastframe_key) { debug_maxsrc = !debug_maxsrc; } lastframe_key = 1; }
		else if((event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F2)) { if(!lastframe_key) { debug_pal = !debug_pal; } lastframe_key = 1; }
		else if((event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F3)) { if(!lastframe_key) { show_mandel = !show_mandel; } lastframe_key = 1; }
		else if((event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F4)) { if(!lastframe_key) { show_fps_hist = !show_fps_hist; } lastframe_key = 1; }
		else lastframe_key = 0;
		render_frame(debug_maxsrc, debug_pal, show_mandel, show_fps_hist);
	}
	audio_shutdown();
	SDL_Quit();
	return 0;
}

void render_debug_overlay(void) {

}

void swap_buffers(void) {
	SDL_GL_SwapBuffers();
}

uint32_t get_ticks(void) {
	return SDL_GetTicks();
}

void dodelay(uint32_t ms) {
	SDL_Delay(ms);
}

uint64_t uget_ticks(void) {
	return (uint64_t)SDL_GetTicks() * 1000;
}

void udodelay(uint64_t us) {
	SDL_Delay(us/1000);
}

