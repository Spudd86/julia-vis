#include "common.h"
#include <stdio.h>

#include "sdl-misc.h"

#include <SDL_ttf.h>

static TTF_Font *font = NULL;

// TODO: improve automatic mode selection
SDL_Surface *sdl_setup(opt_data *opts, int im_size)
{
	printf("Initializing SDL.\n");
	putenv("SDL_NOMOUSE=1");
	
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

	const int vidflags = SDL_HWSURFACE | SDL_HWACCEL | SDL_ASYNCBLIT | SDL_ANYFORMAT | SDL_HWPALETTE | ((opts->doublebuf)?(SDL_DOUBLEBUF):0);
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
	SDL_WM_SetCaption("SDL test for fractal map", "sdl-test");

	vid_info = SDL_GetVideoInfo();
	printf("\nusing mode %dx%d-%d\n", vid_info->current_w, vid_info->current_h, vid_info->vfmt->BitsPerPixel);
	printf("Rshift=%i\nGShift=%i\nBShift=%i\n", screen->format->Rshift, screen->format->Gshift, screen->format->Bshift);
	printf("Rmask=%x\nGmask=%x\nBmask=%x\n\n", screen->format->Rmask, screen->format->Gmask, screen->format->Bmask);

	return screen;
}

void DrawText(SDL_Surface* screen, const char* text)
{
    SDL_Surface *text_surface = TTF_RenderText_Solid(font, text, (SDL_Color){255,255,255});
    if (text_surface == NULL) return;

	SDL_BlitSurface(text_surface, NULL, screen, NULL);
	SDL_FreeSurface(text_surface);
}

