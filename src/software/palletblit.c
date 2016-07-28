
#include "common.h"
#include "pixmisc.h"
#include "pallet.h"
#include "pixformat.h"

#include <assert.h>


// pallet must have 257 entries (for easier interpolation on 16 bit indices)
// output pix = (pallet[in/256]*(in%256) + pallet[in/256+1]*(255-in%256])/256
// for each colour component

// the above is not done in 16 bit modes they just do out = pallet[in/256] (and convert the pallet)

//TODO: load pallets from files of some sort

//TODO: find a way to do approximate gamma correct colour interpolation
// http://chilliant.blogspot.ca/2012/08/srgb-approximations-for-hlsl.html

void pallet_blit32_fallback(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal)
{
	for(unsigned int y = 0; y < h; y++) {
		uint32_t * restrict d = (uint32_t *restrict)(dest + y*dst_stride);
		const uint16_t *restrict s = src + y*src_stride;
		for(unsigned int x = 0; x < w; x++) {
			uint16_t v = *(s++);
			const uint8_t *p = (const uint8_t *)(pal + (v>>8));
			v = v & 0xff;
			uint8_t *db = (uint8_t *)(d++);
			db[0] = ((uint16_t)p[0]*(256-v) + (uint16_t)p[4]*v) >> 8;
			db[1] = ((uint16_t)p[1]*(256-v) + (uint16_t)p[5]*v) >> 8;
			db[2] = ((uint16_t)p[2]*(256-v) + (uint16_t)p[6]*v) >> 8;
			db[3] = ((uint16_t)p[3]*(256-v) + (uint16_t)p[7]*v) >> 8;
		}
	}
}

#if 0
static inline float a_dither_flt(float input, uint32_t x, uint32_t y, int c, uint32_t levels)
{
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
}
#endif

static inline uint8_t a_dither4(uint8_t input, uint32_t x, uint32_t y, int c, uint16_t bits)
{
	// dither_line = (c*67*119 + y*236*119) ;
	// dither = (x*119 + dither_line) & 255 ;
	// note stuff inside bracket can be stepped across a line
	// saturating add with pixel
	uint8_t mask = ((x+c*67) + y * 236) * 119 & 255;
	return MIN( (uint16_t)input + (mask>>bits), UINT8_MAX );

}

void pallet_blit565_fallback(uint8_t * restrict dest, unsigned int dst_stride,
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

void pallet_blit555_fallback(uint8_t * restrict dest, unsigned int dst_stride,
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

void pallet_blit8_fallback(uint8_t * restrict dest, unsigned int dst_stride,
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

#if !defined(__x86_64__)
	if(feat & X86FEAT_MMX) pallet_blit8 = pallet_blit8_mmx;
	if(feat & X86FEAT_MMXEXT) pallet_blit8 = pallet_blit8_sse;
#endif
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

#if !defined(__x86_64__)
	if(feat & X86FEAT_MMX) pallet_blit555 = pallet_blit555_mmx;
	if(feat & X86FEAT_MMXEXT) pallet_blit555 = pallet_blit555_sse;
#endif
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

#if !defined(__x86_64__)
	if(feat & X86FEAT_MMX) pallet_blit565 = pallet_blit565_mmx;
	if(feat & X86FEAT_MMXEXT) pallet_blit565 = pallet_blit565_sse;
#endif
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

#if !defined(__x86_64__)
	if(feat & X86FEAT_MMX) pallet_blit32 = pallet_blit32_mmx;
	if(feat & X86FEAT_MMXEXT) pallet_blit32 = pallet_blit32_sse;
#endif
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

#ifndef NDEBUG
#define unreachable() do { assert(0); __builtin_unreachable(); } while(0)
#else
#define unreachable __builtin_unreachable
#endif

void pallet_blit_Pixbuf(Pixbuf* dst, const uint16_t* restrict src, int w, int h, const uint32_t *restrict pal)
{
	const unsigned  int src_stride = w;
	w = IMIN(w, dst->w);
	h = IMIN(h, dst->h);

	switch(dst->format) { // pallet code takes care of channel order
		case SOFT_PIX_FMT_RGBx8888:
		case SOFT_PIX_FMT_BGRx8888:
		case SOFT_PIX_FMT_xRGB8888:
		case SOFT_PIX_FMT_xBGR8888:
			pallet_blit32(dst->data, dst->pitch, src, src_stride, w, h, pal);
			break;
		case SOFT_PIX_FMT_RGB565:
		case SOFT_PIX_FMT_BGR565:
			pallet_blit565(dst->data, dst->pitch, src, src_stride, w, h, pal);
			break;
		case SOFT_PIX_FMT_RGB555:
		case SOFT_PIX_FMT_BGR555:
			pallet_blit555(dst->data, dst->pitch, src, src_stride, w, h, pal);
			break;
		case SOFT_PIX_FMT_8_RGB_PAL:
		case SOFT_PIX_FMT_8_BGR_PAL:
		pallet_blit8(dst->data, dst->pitch, src, src_stride, w, h);
			break;
		default:
			unreachable();
	}
}

