#ifndef PIXMISC_H
#define PIXMISC_H

#ifdef USE_SDL
#include <SDL.h>
void pallet_blit_SDL(SDL_Surface *dst, const uint16_t * restrict src, int w, int h);
#endif

// require w%16 == 0
// requires (h*w)%32 == 0
void maxblend(void *dest, void *src, int w, int h);
void fade_pix(void *buf, int w, int h, uint8_t fade);


uint16_t *maxsrc_get(void);
void maxsrc_setup(int w, int h);
void maxsrc_update(void);

void pallet_init(int bswap);
int pallet_step(int step);
void pallet_start_switch(int nextpal);
int get_pallet_changing(void);

void pallet_blit_Pixbuf(Pixbuf* dst, const uint16_t* restrict src, int w, int h);

#endif
