#ifndef PIXMISC_H
#define PIXMISC_H

#ifdef USE_SDL
#include <SDL.h>
void pallet_blit_SDL(SDL_Surface *dst, const uint16_t * restrict src, int w, int h, const uint32_t *restrict pal);
#endif

// require w%16 == 0
// requires (h*w)%32 == 0
void maxblend(void *dest, const void *src, int w, int h);

void pallet_blit_Pixbuf(Pixbuf* dst, const uint16_t* restrict src, int w, int h, const uint32_t *restrict pal);

#endif
