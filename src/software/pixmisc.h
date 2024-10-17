#ifndef PIXMISC_H
#define PIXMISC_H

// TODO: switch over to something like:

#if 0
__attribute__((target("arch=athlon,3dnow") ))
void pallet_blit32(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal);
__attribute__((target("mmx,no-sse") ))
void pallet_blit32(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal);
__attribute__((target("sse,no-sse2") ))
void pallet_blit32(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal);
__attribute__((target("sse2,no-sse3")))
void pallet_blit32(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal);
__attribute__((target("avx2,no-avx512f")))
void pallet_blit32(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal);
__attribute__((target("default")))
void pallet_blit32(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal);

// This lets GCC generate a resolver function
// Would need prototype of "default" visible in every implementation file
#endif

#include "pixformat.h"
typedef void (*maxblend_fn)(void *restrict dest, const void *restrict src, size_t n);

// require w%16 == 0
// requires (h*w)%32 == 0
void maxblend_mmx(void *restrict dest, const void *restrict src, size_t n);
void maxblend_3dnow(void *restrict dest, const void *restrict src, size_t n);
void maxblend_sse(void *restrict dest, const void *restrict src, size_t n);
void maxblend_sse2(void *restrict dest, const void *restrict src, size_t n);
void maxblend_sse4_1(void *restrict dest, const void *restrict src, size_t n);
#ifndef __EMSCRIPTEN__
// Currently no AVX2 on emscripten
void maxblend_avx2(void *restrict dest, const void *restrict src, size_t n);
#endif
void maxblend_fallback(void *restrict dest, const void *restrict src, size_t n);

void maxblend(void *dest, const void *src, int w, int h);


typedef void (*pallet_blit8_fn)(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h);

typedef void (*pallet_blit555_fn)(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal);

typedef void (*pallet_blit565_fn)(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal);

typedef void (*pallet_blit32_fn)(uint8_t *restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal);

typedef void (*pallet_blit101010_fn)(uint8_t *restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal);


void pallet_blit32_fallback(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal);
void pallet_blit32_mmx(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal);
void pallet_blit32_3dnow(uint8_t * restrict dest, unsigned int dst_stride,
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
void pallet_blit32_ssse3(uint8_t *restrict dest, unsigned int dst_stride,
                        const uint16_t *restrict src, unsigned int src_stride,
                        unsigned int w, unsigned int h,
                        const uint32_t *restrict pal);
#ifndef __EMSCRIPTEN__
// Currently no AVX2 on emscripten
void pallet_blit32_avx2(uint8_t *restrict dest, unsigned int dst_stride,
                        const uint16_t *restrict src, unsigned int src_stride,
                        unsigned int w, unsigned int h,
                        const uint32_t *restrict pal);
#endif

void pallet_blit565_fallback(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal);
void pallet_blit565_mmx(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal);
void pallet_blit565_3dnow(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal);
void pallet_blit565_sse(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal);

void pallet_blit555_fallback(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal);
void pallet_blit555_mmx(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal);
void pallet_blit555_3dnow(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal);
void pallet_blit555_sse(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal);

void pallet_blit8_fallback(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h);
void pallet_blit8_mmx(uint8_t* restrict dest, unsigned int dst_stride,
		const uint16_t *restrict src, unsigned int src_stride,
		unsigned int w, unsigned int h);
void pallet_blit8_3dnow(uint8_t* restrict dest, unsigned int dst_stride,
		const uint16_t *restrict src, unsigned int src_stride,
		unsigned int w, unsigned int h);
void pallet_blit8_sse(uint8_t* restrict dest, unsigned int dst_stride,
		const uint16_t *restrict src, unsigned int src_stride,
		unsigned int w, unsigned int h);

extern pallet_blit8_fn pallet_blit8;
extern pallet_blit555_fn pallet_blit555;
extern pallet_blit565_fn pallet_blit565;
extern pallet_blit32_fn pallet_blit32;
extern pallet_blit101010_fn pallet_blit101010;

void pallet_blit_Pixbuf(Pixbuf* dst, const uint16_t* restrict src, int w, int h, const uint32_t *restrict pal);

#endif
