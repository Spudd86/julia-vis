#ifndef PIXMISC_H
#define PIXMISC_H

#include "config.h"

#include <unistd.h>
#include <stdint.h>

#ifdef USE_SDL
#include <SDL.h>
void pallet_blit_SDL(SDL_Surface *dst, uint16_t * restrict src, int w, int h, uint32_t *restrict pal);
#endif

#ifdef HAVE_DIRECTFB
#include <directfb.h>
void pallet_blit_DFB(IDirectFBSurface *dst, uint16_t * restrict src, int w, int h, uint32_t *restrict pal);
#endif

void maxblend_stride(void *restrict dest, int dest_stride, void *restrict src, int w, int h);

// require w%16 == 0
void maxblend(void *restrict dest, void *restrict src, int w, int h);
void fade_pix(void *restrict dest, void *restrict src, int w, int h, uint8_t fade);


uint16_t *maxsrc_get(void);
void maxsrc_setup(int w, int h);
void maxsrc_update(void);
#endif
