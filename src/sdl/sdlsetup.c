#include "common.h"
#include <stdio.h>
#include <string.h>

#include <SDL.h>
#include "sdlsetup.h"

static SDL_Surface *real_sdl_setup(opt_data *opts, int im_size, int enable_gl);

SDL_Surface *sdl_setup(opt_data *opts, int im_size) {
	return real_sdl_setup(opts, im_size, 0);
}

SDL_Surface *sdl_setup_gl(opt_data *opts, int im_size) {
	return real_sdl_setup(opts, im_size, 1);
}

// TODO: improve automatic mode selection
static SDL_Surface *real_sdl_setup(opt_data *opts, int im_size, int enable_gl)
{
	printf("Initializing SDL.\n");
	putenv(strdup("SDL_NOMOUSE=1")); // because SUSv2 says it can't be const...

    if((SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER)==-1)) {
        printf("Could not initialize SDL: %s.\n", SDL_GetError());
        exit(-1);
    }
	if(SDL_WasInit(SDL_INIT_AUDIO)) {
		SDL_CloseAudio();
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
	}

	SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
	SDL_EventState(SDL_MOUSEBUTTONDOWN, SDL_IGNORE);
	SDL_EventState(SDL_MOUSEBUTTONUP, SDL_IGNORE);
	SDL_EventState(SDL_JOYAXISMOTION, SDL_IGNORE);
	SDL_EventState(SDL_JOYBALLMOTION, SDL_IGNORE);
	SDL_EventState(SDL_JOYHATMOTION, SDL_IGNORE);
	SDL_EventState(SDL_JOYBUTTONDOWN, SDL_IGNORE);
	SDL_EventState(SDL_JOYBUTTONUP, SDL_IGNORE);
	SDL_EventState(SDL_VIDEORESIZE, SDL_IGNORE);
	SDL_EventState(SDL_VIDEOEXPOSE, SDL_IGNORE);
	SDL_EventState(SDL_USEREVENT, SDL_IGNORE);
	SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);
	SDL_EventState(SDL_ACTIVEEVENT, SDL_IGNORE);

    printf("SDL initialized.\n");

    const SDL_VideoInfo *vid_info = SDL_GetVideoInfo();

	int vidflags = SDL_ASYNCBLIT | SDL_ANYFORMAT;
	if(opts->doublebuf) vidflags |= SDL_DOUBLEBUF;
	if(vid_info->hw_available) vidflags |= SDL_HWSURFACE;
	if(vid_info->blit_hw) vidflags |= SDL_HWACCEL;
	if(opts->hw_pallet) vidflags |= SDL_HWPALETTE;

	if(enable_gl) {
		vidflags |= SDL_OPENGL;
//		if(opts->doublebuf)
			SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
		SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 0);
	} else {

	}

	SDL_Rect **modes = SDL_ListModes(vid_info->vfmt, vidflags);
	if (modes == (SDL_Rect**)0) {
		printf("No modes available!\n");
		exit(-1);
	}

	SDL_Surface *screen;
	if (modes == (SDL_Rect**)-1) {
		if(opts->w < 0 && opts->h < 0) opts->w = opts->h = im_size;
		else if(opts->w < 0) opts->w = opts->h;
		else if(opts->h < 0) opts->h = opts->w;
		screen = SDL_SetVideoMode(opts->w, opts->h, (opts->hw_pallet?8:32), vidflags | ((opts->fullscreen)?SDL_FULLSCREEN:0));

	} else {
		if(opts->w < 0 && opts->h < 0) opts->h = im_size;
		int mode=-1;
		for (int i=0; modes[i]; i++) {
			printf("  %d x %d\n", modes[i]->w, modes[i]->h);
			if(modes[i]->w >= opts->w && modes[i]->h >= opts->h)
				mode = i;
		}
		if(mode == -1) {
			printf("No usable modes available!\n");
			exit(-1);
		}
		screen = SDL_SetVideoMode(modes[mode]->w, modes[mode]->h, (opts->hw_pallet?8:32), vidflags);
	}

    if ( screen == NULL ) {
        fprintf(stderr, "Unable to set video: %s\n", SDL_GetError());
        exit(1);
    }
	SDL_WM_SetCaption("Julia Set Fractal Visualizer", "sdl-test");

	vid_info = SDL_GetVideoInfo();
	printf("\nusing mode %dx%d-%d\n", vid_info->current_w, vid_info->current_h, vid_info->vfmt->BitsPerPixel);
	printf("Rshift=%2i\nGShift=%2i\nBShift=%2i\n", screen->format->Rshift, screen->format->Gshift, screen->format->Bshift);
	printf("Rmask=%04x\nGmask=%04x\nBmask=%04x\n\n", screen->format->Rmask, screen->format->Gmask, screen->format->Bmask);

	return screen;
}






