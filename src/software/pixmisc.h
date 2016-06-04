#ifndef PIXMISC_H
#define PIXMISC_H

#ifdef USE_SDL
#include <SDL.h>
void pallet_blit_SDL(SDL_Surface *dst, const uint16_t * restrict src, int w, int h, const uint32_t *restrict pal);
#endif

typedef void (*maxblend_fn)(void *restrict dest, const void *restrict src, int w, int h);

// require w%16 == 0
// requires (h*w)%32 == 0
void maxblend_mmx(void *restrict dest, const void *restrict src, int w, int h);
void maxblend_sse(void *restrict dest, const void *restrict src, int w, int h);
void maxblend_sse2(void *restrict dest, const void *restrict src, int w, int h);
void maxblend_sse4_1(void *restrict dest, const void *restrict src, int w, int h);
void maxblend_fallback(void *restrict dest, const void *restrict src, int w, int h);

void maxblend(void *dest, const void *src, int w, int h);

void pallet_blit32_mmx(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal);
void pallet_blit32_sse(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal);
void pallet_blit32_sse2(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal);

void pallet_blit565_mmx(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal);
void pallet_blit565_sse(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal);

void pallet_blit555_mmx(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal);
void pallet_blit555_sse(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal);

void pallet_blit8_mmx(uint8_t* restrict dest, unsigned int dst_stride,
		const uint16_t *restrict src, unsigned int src_stride,
		unsigned int w, unsigned int h);
void pallet_blit8_sse(uint8_t* restrict dest, unsigned int dst_stride,
		const uint16_t *restrict src, unsigned int src_stride,
		unsigned int w, unsigned int h);

void pallet_blit_Pixbuf(Pixbuf* dst, const uint16_t* restrict src, int w, int h, const uint32_t *restrict pal);

#endif
