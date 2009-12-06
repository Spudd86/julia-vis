#include "common.h"
#include <stdio.h>
#include <string.h>

#include "sdl-misc.h"

//#include <SDL_ttf.h>
//static TTF_Font *font = NULL;

static SDL_Surface *real_sdl_setup(opt_data *opts, int im_size, int enable_gl);

SDL_Surface *sdl_setup(opt_data *opts, int im_size) {
	return real_sdl_setup(opts, im_size, 0);

//	if(TTF_Init()==-1) {
//		printf("TTF_Init: %s\n", TTF_GetError());
//		exit(2);
//	}
//	font = TTF_OpenFont("font.ttf", 16);
}

SDL_Surface *sdl_setup_gl(opt_data *opts, int im_size) {
	return real_sdl_setup(opts, im_size, 1); //TODO: make gl stuff not need SDL_ttf
}

// TODO: improve automatic mode selection
static SDL_Surface *real_sdl_setup(opt_data *opts, int im_size, int enable_gl)
{
	printf("Initializing SDL.\n");
	putenv("SDL_NOMOUSE=1");

    if((SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER)==-1)) {
        printf("Could not initialize SDL: %s.\n", SDL_GetError());
        exit(-1);
    }
	atexit(SDL_Quit);
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

	int vidflags = SDL_ASYNCBLIT | SDL_ANYFORMAT | SDL_HWPALETTE;
	if(opts->doublebuf) vidflags |= SDL_DOUBLEBUF;
	if(vid_info->hw_available) vidflags |= SDL_HWSURFACE;
	if(vid_info->blit_hw) vidflags |= SDL_HWACCEL;
	if(opts->hw_pallet) vidflags |= SDL_HWPALETTE;

	if(enable_gl) {
		vidflags |= SDL_OPENGL;
	}

	SDL_Rect **modes = SDL_ListModes(vid_info->vfmt, vidflags);
	if (modes == (SDL_Rect**)0) {
		printf("No modes available!\n");
		exit(-1);
	}

	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1);

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

SDL_Surface* render_string(const char *str);
void DrawText(SDL_Surface* screen, const char* text)
{
//    SDL_Surface *text_surface = TTF_RenderText_Solid(font, text, (SDL_Color){255,255,255,255});
	SDL_Surface *text_surface = render_string(text);
    if (text_surface == NULL) return;

	SDL_BlitSurface(text_surface, NULL, screen, NULL);
	SDL_FreeSurface(text_surface);
}

#include "terminusIBM.h"
//TODO: write this
SDL_Surface* render_string(const char *str)
{
	int len = strlen(str);

	SDL_Surface *surf = SDL_AllocSurface(SDL_SWSURFACE, len*8, 16, 8, 0, 0, 0, 0);
    if( surf == NULL ) {
        return NULL;
    }
    SDL_Palette* palette = surf->format->palette;
    palette->colors[1].r = 255;
    palette->colors[1].g = 255;
    palette->colors[1].b = 255;
    palette->colors[0].r = 0;
    palette->colors[0].g = 0;
    palette->colors[0].b = 0;
    SDL_SetColorKey(surf, SDL_SRCCOLORKEY, 0);


	const char *c = str;
	int x = 0;
	while(*c) {

		const uint8_t * restrict src = terminusIBM + 16 * *c;;
		for(int y=0; y < 16; y++) {
			uint8_t *dst = ((uint8_t*)surf->pixels) + surf->pitch*y + x*8;
			uint8_t line = *src++;
			for(int o=0; o < 8; o++) {
				if(line & (1<<(7-o))) dst[o] = 1;
			}
		}
		c++; x++;
	}

	return surf;
}




