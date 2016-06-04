
#pragma GCC optimize "3,inline-functions,merge-all-constants"

#include "common.h"
#include "pixmisc.h"
#include "pallet.h"

#include <assert.h>


// pallet must have 257 entries (for easier interpolation on 16 bit indices)
// output pix = (pallet[in/256]*(in%256) + pallet[in/256+1]*(255-in%256])/256
// for each colour component

// the above is not done in 16 bit modes they just do out = pallet[in/256] (and convert the pallet)

//TODO: load pallets from files of some sort

//TODO: optimize 16 bit modes
//  - move y*w calculations outside of the loop
//  - non-temporal store version if sse is available

//TODO: dispatch based on availability of instructions
//   except maybe on x86_64 since sse2 is always available there

//TODO: find a way to do approximate gamma correct colour interpolation

static void pallet_blit32_fallback(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal)
{
	for(unsigned int y = 0; y < h; y++) {
		uint32_t * restrict d = (uint32_t *restrict)(dest + y*dst_stride);
		const uint16_t *restrict s = src + y*src_stride;
		for(unsigned int x = 0; x < w; x++) {
			*(d++) = pal[*(s++)>>8];
		}
	}
}

static inline uint16_t a_dither1(uint16_t input, uint32_t x, uint32_t y, uint16_t bits)
{
	uint32_t mask = ((x ^ y * 149) * 1234& 511)/511.0f;
	return MIN( (((uint32_t)input<<bits) + mask)>>bits, UINT16_MAX);
}

static inline uint8_t a_dither4(uint8_t input, uint32_t x, uint32_t y, int c, uint16_t bits)
{
#if 0
	float mask;
	switch (pattern) {
		case 1: mask = ((x ^ y * 149) * 1234& 511)/511.0f; break;
		case 2: mask = (((x+c*17) ^ y * 149) * 1234 & 511)/511.0f; break;
		case 3: mask = ((x + y * 237) * 119 & 255)/255.0f; break;
		case 4: mask = (((x+c*67) + y * 236) * 119 & 255)/255.0f; break;
		case 5: mask = 0.5; break;
		default: return input;
	}
	//return mask*255;
	return (levels * input + mask)/levels;
#endif
	// dither_line = c*67*119 + y*236*119;
	// dither = (x*119 + dither_line) & 255; // note stuff inside bracket can be stepped across a line
	//
	uint8_t mask = ((x+c*67) + y * 236) * 119 & 255;
	return MIN( (((uint16_t)input<<bits) + mask)>>bits, 255 );
	//return MIN( input + (mask>>(8-bits)), 255 );

}

static void pallet_blit565_fallback(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal)
{
	for(size_t y = 0; y < h; y++) {
		const uint16_t *restrict s = src + y*src_stride;
		uint16_t *restrict d = (uint16_t *restrict)(dest + y*dst_stride);
		for(size_t x = 0; x < w; x++, s++, d++) {
			uint32_t p = pal[s[0] >> 8];
			//uint32_t p = pal[a_dither1(s[0], x, y, 8) >> 8];
			//uint16_t b = p, g = p >> 8, r = p>>16;
			uint16_t b = a_dither4(p, x, y, 0, 5), g = a_dither4(p >> 8, x, y, 1, 6), r = a_dither4(p >> 16, x, y, 3, 5);
			b = b >> 3;
			g = g << 3;
			r = r << 8;
			uint16_t px = (r&0xf800) + (g&0x07e0) + (b&0x001f);
			*d = px;
		}
	}
}

static void pallet_blit555_fallback(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal)
{
	for(size_t y = 0; y < h; y++) {
		const uint16_t *restrict s = src + y*src_stride;
		uint16_t *restrict d = (uint16_t *restrict)(dest + y*dst_stride);
		for(size_t x = 0; x < w; x++, s++, d++) {
			uint32_t p = pal[s[0] >> 8];
			//uint32_t p = pal[a_dither1(s[0], x, y, 8) >> 8];
			//uint16_t b = p, g = p >> 8, r = p>>16;
			uint16_t b = a_dither4(p, x, y, 0, 5), g = a_dither4(p >> 8, x, y, 1, 5), r = a_dither4(p >> 16, x, y, 3, 5);
			b = b >> 3;
			g = g << 2;
			r = r << 7;
			uint16_t px = (r&0x7c00) + (g&0x03e0) + (b&0x001f);
			*d = px;
		}
	}
}

static void pallet_blit8_fallback(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h)
{
	for(unsigned int y = 0; y < h; y++) {
		const uint16_t *restrict s = src + y*src_stride;
		uint8_t *restrict d = dest + y*dst_stride;
		for(unsigned int x = 0; x < w; x++)
			*(d++) = *(s++)>>8;
	}
}

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

#if (__x86_64__ || __i386__)

#include "x86/x86_features.h"

static void pallet_blit8_dispatch(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h);
static void pallet_blit555_dispatch(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal);
static void pallet_blit565_dispatch(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal);
static void pallet_blit32_dispatch(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal);

pallet_blit8_fn pallet_blit8 = pallet_blit8_dispatch;
pallet_blit555_fn pallet_blit555 = pallet_blit555_dispatch;
pallet_blit565_fn pallet_blit565 = pallet_blit565_dispatch;
pallet_blit32_fn pallet_blit32 = pallet_blit32_dispatch;

static void pallet_blit8_dispatch(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h)
{
	pallet_blit8 = pallet_blit8_fallback;

	uint64_t feat = x86feat_get_features();

	if(feat & X86FEAT_MMX) pallet_blit8 = pallet_blit8_mmx;
	if(feat & X86FEAT_MMXEXT) pallet_blit8 = pallet_blit8_sse;
	if(feat & X86FEAT_SSE) pallet_blit8 = pallet_blit8_sse;

	pallet_blit8(dest, dst_stride, src, src_stride, w, h);
}

static void pallet_blit555_dispatch(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal)
{
	pallet_blit555 = pallet_blit555_fallback;

	uint64_t feat = x86feat_get_features();

	if(feat & X86FEAT_MMX) pallet_blit555 = pallet_blit555_mmx;
	if(feat & X86FEAT_MMXEXT) pallet_blit555 = pallet_blit555_sse;
	if(feat & X86FEAT_SSE) pallet_blit555 = pallet_blit555_sse;

	pallet_blit555(dest, dst_stride, src, src_stride, w, h, pal);
}

static void pallet_blit565_dispatch(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal)
{
	pallet_blit565 = pallet_blit565_fallback;

	uint64_t feat = x86feat_get_features();

	if(feat & X86FEAT_MMX) pallet_blit565 = pallet_blit565_mmx;
	if(feat & X86FEAT_MMXEXT) pallet_blit565 = pallet_blit565_sse;
	if(feat & X86FEAT_SSE) pallet_blit565 = pallet_blit565_sse;

	pallet_blit565(dest, dst_stride, src, src_stride, w, h, pal);
}

static void pallet_blit32_dispatch(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal)
{
	pallet_blit32 = pallet_blit32_fallback;

	uint64_t feat = x86feat_get_features();

	if(feat & X86FEAT_MMX) pallet_blit32 = pallet_blit32_mmx;
	if(feat & X86FEAT_MMXEXT) pallet_blit32 = pallet_blit32_sse;
	if(feat & X86FEAT_SSE) pallet_blit32 = pallet_blit32_sse;
	if(feat & X86FEAT_SSE2) pallet_blit32 = pallet_blit32_sse2;

	pallet_blit32(dest, dst_stride, src, src_stride, w, h, pal);
}

#else
pallet_blit8_fn pallet_blit8 = pallet_blit8_fallback;
pallet_blit555_fn pallet_blit555 = pallet_blit555_fallback;
pallet_blit565_fn pallet_blit565 = pallet_blit565_fallback;
pallet_blit32_fn pallet_blit32 = pallet_blit32_fallback;
#endif


void pallet_blit_Pixbuf(Pixbuf* dst, const uint16_t* restrict src, int w, int h, const uint32_t *restrict pal)
{
	const unsigned  int src_stride = w;
	w = IMIN(w, dst->w);
	h = IMIN(h, dst->h);

	if(dst->bpp == 32) pallet_blit32(dst->data, dst->pitch, src, src_stride, w, h, pal);
	else if(dst->bpp == 16) pallet_blit565(dst->data, dst->pitch, src, src_stride, w, h, pal);
	else if(dst->bpp == 15) pallet_blit555(dst->data, dst->pitch, src, src_stride, w, h, pal);
	else if(dst->bpp == 8) { // need to set surface's pallet
		pallet_blit8(dst->data, dst->pitch, src, src_stride, w, h);
	}
}

#ifdef USE_SDL
#include <SDL.h>
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
#endif

