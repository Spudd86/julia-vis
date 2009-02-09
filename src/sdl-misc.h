#ifndef SDL_MISC_H
#define SDL_MISC_H

#include <SDL.h>

SDL_Surface *sdl_setup(opt_data *opts, int im_size);
void DrawText(SDL_Surface* screen, const char* text);

void draw_line(SDL_Surface *s, 
          int x1, int y1, 
          int x2, int y2, 
          Uint32 color);

#endif

