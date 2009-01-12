#ifndef PIXMISC_H
#define PIXMISC_H

#include <unistd.h>
#include <stdint.h>


void pallet_blit(void *dest, int dst_stride, uint16_t *src, int w, int h, uint32_t *pal);
#ifdef USE_SDL
#include <SDL.h>
void pallet_blit_SDL(SDL_Surface *dst, uint16_t * restrict src, int w, int h, uint32_t *restrict pal);
#endif

#ifdef USE_DIRECTFB
#include <directfb.h>
void pallet_blit_DFB(IDirectFBSurface *dst, uint16_t * restrict src, int w, int h, uint32_t *restrict pal);
#endif

void maxblend(void *restrict dest, void *restrict src, int w, int h);

#endif
