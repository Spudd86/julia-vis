#ifndef PALLET_H
#define PALLET_H

#include <unistd.h>
#include <stdint.h>
//#include <cairo.h>
#include <SDL.h>

void pallet_blit(void *dest, int dst_stride, uint16_t *src, int w, int h, uint32_t *pal);
//void pallet_blit_cairo(cairo_surface_t *dst, uint16_t *src, int w, int h, uint32_t *pal);
//void pallet_blit_cairo_unroll(cairo_surface_t *dst, uint16_t *src, int w, int h, uint32_t *pal);
//void pallet_blit_cairo8x8(cairo_surface_t *dst, uint16_t *src, int w, int h, uint32_t *pal);
//void pallet_blit_cairo_simple(cairo_surface_t *dst, uint16_t *src, int w, int h, uint32_t *pal);
void pallet_blit_SDL(SDL_Surface *dst, uint16_t *src, int w, int h, uint32_t *pal);
void pallet_blit_SDL8x8(SDL_Surface *dst, uint16_t *src, int w, int h, uint32_t *pal);

#endif