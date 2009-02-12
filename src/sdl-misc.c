#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL.h>
#include <SDL_thread.h>
#include <SDL_ttf.h>

#include "common.h"
#include "sdl-misc.h"

static TTF_Font *font = NULL;


// TODO: improve automatic mode selection
SDL_Surface *sdl_setup(opt_data *opts, int im_size)
{
	printf("Initializing SDL.\n");
    if((SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTTHREAD | SDL_INIT_TIMER)==-1)) {
        printf("Could not initialize SDL: %s.\n", SDL_GetError());
        exit(-1);
    }
	atexit(SDL_Quit);
	if(TTF_Init()==-1) {
		printf("TTF_Init: %s\n", TTF_GetError());
		exit(2);
	}
	font = TTF_OpenFont("font.ttf", 16);

    printf("SDL initialized.\n");


	const int vidflags = SDL_HWSURFACE | SDL_HWACCEL | SDL_ASYNCBLIT | ((opts->doublebuf)?(SDL_DOUBLEBUF):0);
	const SDL_VideoInfo *vid_info = SDL_GetVideoInfo();
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
		screen = SDL_SetVideoMode(opts->w, opts->h, vid_info->vfmt->BitsPerPixel, vidflags | ((opts->fullscreen)?SDL_FULLSCREEN:0));

	} else {
		if(opts->w < 0 && opts->h < 0) opts->h = im_size;
		int mode=0;
		for (int i=0; modes[i]; i++) {
			printf("  %d x %d\n", modes[i]->w, modes[i]->h);
			if(modes[i]->w >= opts->w && modes[i]->h >= opts->h && modes[i]->h <= modes[mode]->h)
				mode = i;
		}
		if(modes[mode]->w < im_size && modes[mode]->h < im_size) {
			printf("No usable modes available!\n");
			exit(-1);
		}
		screen = SDL_SetVideoMode(modes[mode]->w, modes[mode]->h, vid_info->vfmt->BitsPerPixel, vidflags);
	}

    if ( screen == NULL ) {
        fprintf(stderr, "Unable to set video: %s\n", SDL_GetError());
        exit(1);
    }
	SDL_WM_SetCaption("SDL test for fractal map", "sdl-test");

	vid_info = SDL_GetVideoInfo();
	printf("\nusing mode %dx%d-%d\n", vid_info->current_w, vid_info->current_h, vid_info->vfmt->BitsPerPixel);

	return screen;
}

void DrawText(SDL_Surface* screen, const char* text)
{
    SDL_Surface *text_surface = TTF_RenderText_Solid(font, text, (SDL_Color){255,255,255});
    if (text_surface == NULL) return;

	SDL_BlitSurface(text_surface, NULL, screen, NULL);
	SDL_FreeSurface(text_surface);
}

