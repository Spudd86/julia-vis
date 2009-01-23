#ifndef SDL_MISC_H
#define SDL_MISC_H

#include <SDL.h>

SDL_Surface *sdl_setup(opt_data *opts, int im_size);
void DrawText(SDL_Surface* screen, const char* text);

#endif

