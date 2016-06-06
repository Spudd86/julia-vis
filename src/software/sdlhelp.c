
#include "common.h"

#include <SDL.h>

#include "sdlhelp.h"
#include "pixmisc.h"

void pallet_blit_SDL(SDL_Surface *dst, const uint16_t* restrict src, int w, int h, const uint32_t *restrict pal)
{
	const unsigned int src_stride = w;
	w = IMIN(w, dst->w);
	h = IMIN(h, dst->h);

	if((SDL_MUSTLOCK(dst) && SDL_LockSurface(dst) < 0) || w < 0 || h < 0) return;
	if(dst->format->BitsPerPixel == 32) pallet_blit32(dst->pixels, dst->pitch, src, src_stride, w, h, pal);
	else if(dst->format->BitsPerPixel == 16) pallet_blit565(dst->pixels, dst->pitch, src, src_stride, w, h, pal);
	else if(dst->format->BitsPerPixel == 15) pallet_blit555(dst->pixels, dst->pitch, src, src_stride, w, h, pal);
	else if(dst->format->BitsPerPixel == 8) { // need to set surface's pallet
		pallet_blit8(dst->pixels, dst->pitch, src, src_stride, w, h);
		SDL_SetColors(dst, (void *)pal, 0, 256);
	}
	if(SDL_MUSTLOCK(dst)) SDL_UnlockSurface(dst);
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
