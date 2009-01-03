#ifndef PIXMISC_H
#define PIXMISC_H

#include <unistd.h>
#include <stdint.h>
#include <SDL.h>

void pallet_blit(void *dest, int dst_stride, uint16_t *src, int w, int h, uint32_t *pal);
void pallet_blit_SDL(SDL_Surface *dst, uint16_t *src, int w, int h, uint32_t *pal);

void maxblend(void *dest, void *src, int w, int h);

#endif
