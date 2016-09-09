#include "common.h"

#include <SDL.h>

#include "sdlhelp.h"
#include "software/pixmisc.h"

#if NO_PARATASK
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
#else

#include "paratask/paratask.h"

struct pallet_blit_work_args {
	void (*task_fn)(uint8_t * restrict dest, unsigned int dst_stride,
	                const uint16_t *restrict src, unsigned int src_stride,
	                unsigned int w, unsigned int h,
	                const uint32_t *restrict pal);
	SDL_Surface *dst;
	const uint16_t* restrict src;
	int w, h;
	const uint32_t *restrict pal;
	size_t span;
};
static void paratask_func(size_t work_item_id, void *arg_)
{
	struct pallet_blit_work_args *a = arg_;

	const int ystart = work_item_id * a->span;
	const int yend   = IMIN(ystart + a->span, (unsigned int)a->h);
	a->task_fn(a->dst->pixels + ystart*a->dst->pitch, a->dst->pitch, a->src + ystart*a->w, a->w, a->w, yend - ystart, a->pal);
}

void pallet_blit_SDL(SDL_Surface *dst, const uint16_t* restrict src, int w, int h, const uint32_t *restrict pal)
{
	static int first_time = 1;
	const unsigned int src_stride = w;
	int span = 2;

	if((SDL_MUSTLOCK(dst) && SDL_LockSurface(dst) < 0) || w < 0 || h < 0) return;
	if(first_time) { // the cpu dispatch code is not thread safe so first blit is done single threaded
		//TODO: do the first time thing per format?
		if(dst->format->BitsPerPixel == 32) pallet_blit32(dst->pixels, dst->pitch, src, src_stride, w, h, pal);
		else if(dst->format->BitsPerPixel == 16) pallet_blit565(dst->pixels, dst->pitch, src, src_stride, w, h, pal);
		else if(dst->format->BitsPerPixel == 15) pallet_blit555(dst->pixels, dst->pitch, src, src_stride, w, h, pal);
		first_time = 0;
	} else if(dst->format->BitsPerPixel != 8) {
		struct pallet_blit_work_args args = {
			NULL, dst, src, w, h, pal, span
		};
		if(dst->format->BitsPerPixel == 32) args.task_fn = pallet_blit32;
		else if(dst->format->BitsPerPixel == 16) args.task_fn = pallet_blit565;
		else if(dst->format->BitsPerPixel == 15) args.task_fn = pallet_blit555;

		paratask_call(paratask_default_instance(), 0, h/span, paratask_func, &args);
	}
	
	if(dst->format->BitsPerPixel == 8) { // need to set surface's pallet
		//TODO: make this work in parallel too
		pallet_blit8(dst->pixels, dst->pitch, src, src_stride, w, h);
		SDL_SetColors(dst, (void *)pal, 0, 256);
	}
	if(SDL_MUSTLOCK(dst)) SDL_UnlockSurface(dst);
}
#endif