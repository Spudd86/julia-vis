#ifndef SDLHELP_H__
#define SDLHELP_H__

void DrawText(SDL_Surface* screen, const char* text);
void pallet_blit_SDL(SDL_Surface *dst, const uint16_t * restrict src, int w, int h, const uint32_t *restrict pal);

void draw_line(SDL_Surface *s,
          int x1, int y1,
          int x2, int y2,
          Uint32 color);

#endif
