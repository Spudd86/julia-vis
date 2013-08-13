/**
 * palletblit.c
 *
 */

#include "common.h"
#include "pixmisc.h"
#include "pallet.h"
#include "mymm.h"


// pallet must have 257 entries (for easier interpolation on 16 bit indicies)
// output pix = (pallet[in/256]*(in%256) + pallet[in/256+1]*(255-in%256])/256
// for each colour component

// the above is not done in 16 bit modes they just do out = pallet[in/256] (and conver the pallet)

//TODO: load pallets from files of some sort

#ifdef __MMX__
static void pallet_blit32(uint32_t *restrict dest, unsigned int dst_stride, const uint16_t *restrict src, unsigned int src_stride, unsigned int w, unsigned int h, const uint32_t *restrict pal)
{
	const __m64 zero = _mm_cvtsi32_si64(0ll);
	const __m64 mask = (__m64)(0x00ff00ff00ff);
	dst_stride /= 4;

	for(unsigned int y = 0; y < h; y++) {
		for(unsigned int x = 0; x < w; x+=4) {
			int v = src[y*src_stride + x];
			__builtin_prefetch(src + y*src_stride + x + 4, 0, 0);
			__builtin_prefetch(dest + y*dst_stride + x + 4, 1, 0);

			__m64 col1 = *(__m64 *)(pal+(v/256));
			__m64 col2 = col1;
			col1 = _mm_unpacklo_pi8(col1, zero);
    		col2 = _mm_unpackhi_pi8(col2, zero);

		    //col1 = (col2*v + col1*(0xff-v))/256;
			__m64 vt = _mm_set1_pi16(v);
			vt = _mm_and_si64(vt, mask);
			col2 = _mm_mullo_pi16(col2, vt);
			vt = _mm_andnot_si64(vt, mask); // vt = 255 - vt
			col1 = _mm_mullo_pi16(col1, vt);
    		col1 = _mm_add_pi16(col1, col2);
    		col1 = _mm_srli_pi16(col1, 8);

			__m64 tmp = col1;

			v = src[y*src_stride + x + 1];
			col1 = *(__m64 *)(pal+(v/256));
			col2 = col1;
			col1 = _mm_unpacklo_pi8(col1, zero);
    		col2 = _mm_unpackhi_pi8(col2, zero);

			vt = _mm_set1_pi16(v);
			vt = _mm_and_si64(vt, mask);
			col2 = _mm_mullo_pi16(col2, vt);
			vt = _mm_andnot_si64(vt, mask); // vt = 255 - vt
			col1 = _mm_mullo_pi16(col1, vt);
    		col1 = _mm_add_pi16(col1, col2);
    		col1 = _mm_srli_pi16(col1, 8);

			tmp = _mm_packs_pu16(tmp, col1);
			//_mm_stream_pi((__m64 *)(dest + y*dst_stride + x), tmp);
			*(__m64 *)(dest + y*dst_stride + x) = tmp;

			v = src[y*src_stride + x + 2];
			col1 = *(__m64 *)(pal+(v/256));
			col2 = col1;
			col1 = _mm_unpacklo_pi8(col1, zero);
    		col2 = _mm_unpackhi_pi8(col2, zero);

			vt = _mm_set1_pi16(v);
			vt = _mm_and_si64(vt, mask);
			col2 = _mm_mullo_pi16(col2, vt);
			vt = _mm_andnot_si64(vt, mask); // vt = 255 - vt
			col1 = _mm_mullo_pi16(col1, vt);
    		col1 = _mm_add_pi16(col1, col2);
    		col1 = _mm_srli_pi16(col1, 8);

			tmp = col1;

			v = src[y*src_stride + x + 3];
			col1 = *(__m64 *)(pal+(v/256));
			col2 = col1;
			col1 = _mm_unpacklo_pi8(col1, zero);
    		col2 = _mm_unpackhi_pi8(col2, zero);

			vt = _mm_set1_pi16(v);
			vt = _mm_and_si64(vt, mask);
			col2 = _mm_mullo_pi16(col2, vt);
			vt = _mm_andnot_si64(vt, mask); // vt = 255 - vt
			col1 = _mm_mullo_pi16(col1, vt);
    		col1 = _mm_add_pi16(col1, col2);
    		col1 = _mm_srli_pi16(col1, 8);

			tmp = _mm_packs_pu16(tmp, col1);
			//_mm_stream_pi((__m64 *)(dest + y*dst_stride + x+2), tmp);
			*(__m64 *)(dest + y*dst_stride + x + 2) = tmp;
		}
	}
}
#else
static void pallet_blit32(uint8_t *restrict dest, unsigned int dst_stride, const uint16_t *restrict src, unsigned int src_stride, unsigned int w, unsigned int h, const uint32_t *restrict pal)
{
	for(unsigned int y = 0; y < h; y++)
		for(unsigned int x = 0; x < w; x++)
			*(uint32_t *)(dest + y*dst_stride + x*4) = pal[src[y*src_stride + x]>>8];
}
#endif

// needs _mm_shuffle_pi16 no other sse/3dnow stuff used
#if defined(__SSE__) || defined(__3dNOW__)
static void pallet_blit565(uint8_t *restrict dest, unsigned int dst_stride, const uint16_t *pbattr src, unsigned int src_stride, unsigned int w, unsigned int h, const uint32_t *restrict pal)
{
	for(unsigned int y = 0; y < h; y++) {
		for(unsigned int x = 0; x < w; x+=4) {
			__builtin_prefetch(src + y*src_stride + x + 4, 0, 0);
			__builtin_prefetch(dest + y*dst_stride + x + 4, 1, 0);

			__m64 r1, r2, g1, g2, b1, b2, c;
			int v = src[y*src_stride + x];
			c  = _mm_cvtsi32_si64(pal[v/256]);
			r1 = _mm_shuffle_pi16(c, 0xfd);
			g1 = _mm_shuffle_pi16(c, 0xfc);
			c  = _mm_slli_pi32(c, 8);
			b1 = _mm_shuffle_pi16(c, 0xfc);

			v=src[y*src_stride + x+1];
			c  = _mm_cvtsi32_si64(pal[v/256]);
			r2 = _mm_shuffle_pi16(c, 0xf7);
			g2 = _mm_shuffle_pi16(c, 0xf3);
			c  = _mm_slli_pi32(c, 8);
			b2 = _mm_shuffle_pi16(c, 0xf3);
			r1 = _mm_or_si64(r1, r2);
			g1 = _mm_or_si64(g1, g2);
			b1 = _mm_or_si64(b1, b2);

			v=src[y*src_stride + x+2];
			c  = _mm_cvtsi32_si64(pal[v/256]);
			r2 = _mm_shuffle_pi16(c, 0xdf);
			g2 = _mm_shuffle_pi16(c, 0xcf);
			c  = _mm_slli_pi32(c, 8);
			b2 = _mm_shuffle_pi16(c, 0xcf);
			r1 = _mm_or_si64(r1, r2);
			g1 = _mm_or_si64(g1, g2);
			b1 = _mm_or_si64(b1, b2);

			v=src[y*src_stride + x+3];
			c  = _mm_cvtsi32_si64(pal[v/256]);
			r2 = _mm_shuffle_pi16(c, 0x7f);
			g2 = _mm_shuffle_pi16(c, 0x3f);
			c  = _mm_slli_pi32(c, 8);
			b2 = _mm_shuffle_pi16(c, 0x3f);
			r1 = _mm_or_si64(r1, r2);
			g1 = _mm_or_si64(g1, g2);
			b1 = _mm_or_si64(b1, b2);

			r1 = _mm_srli_pi16(r1, 3);
			g1 = _mm_srli_pi16(g1, 10);
			b1 = _mm_srli_pi16(b1, 11);

			r1 = _mm_slli_pi16(r1, 11);
			g1 = _mm_slli_pi16(g1, 5);

			r1 = _mm_or_si64(r1, g1);
			r1 = _mm_or_si64(r1, b1);

			*(__m64 *)(dest + y*dst_stride + x*2) = r1;
		}
	}
}

static void pallet_blit555(uint8_t *restrict dest, unsigned int dst_stride, const uint16_t *pbattr src, unsigned int src_stride, unsigned int w, unsigned int h, const uint32_t *restrict pal)
{
	for(unsigned int y = 0; y < h; y++) {
		for(unsigned int x = 0; x < w; x+=4) {
			__builtin_prefetch(src + y*src_stride + x + 4, 0, 0);
			__builtin_prefetch(dest + y*dst_stride + x + 4, 1, 0);
			int v;
			__m64 r1, r2, g1, g2, b1, b2, c;

			v  = src[y*src_stride + x];
			c  = _mm_cvtsi32_si64(pal[v/256]);
			r1 = _mm_shuffle_pi16(c, 0xfd);
			g1 = _mm_shuffle_pi16(c, 0xfc);
			c  = _mm_slli_pi32(c, 8);
			b1 = _mm_shuffle_pi16(c, 0xfc);

			v  = src[y*src_stride + x+1];
			c  = _mm_cvtsi32_si64(pal[v/256]);
			r2 = _mm_shuffle_pi16(c, 0xf7);
			g2 = _mm_shuffle_pi16(c, 0xf3);
			c  = _mm_slli_pi32(c, 8);
			b2 = _mm_shuffle_pi16(c, 0xf3);
			r1 = _mm_or_si64(r1, r2);
			g1 = _mm_or_si64(g1, g2);
			b1 = _mm_or_si64(b1, b2);

			v  = src[y*src_stride + x+2];
			c  = _mm_cvtsi32_si64(pal[v/256]);
			r2 = _mm_shuffle_pi16(c, 0xdf);
			g2 = _mm_shuffle_pi16(c, 0xcf);
			c  = _mm_slli_pi32(c, 8);
			b2 = _mm_shuffle_pi16(c, 0xcf);
			r1 = _mm_or_si64(r1, r2);
			g1 = _mm_or_si64(g1, g2);
			b1 = _mm_or_si64(b1, b2);

			v  = src[y*src_stride + x+3];
			c  = _mm_cvtsi32_si64(pal[v/256]);
			r2 = _mm_shuffle_pi16(c, 0x7f);
			g2 = _mm_shuffle_pi16(c, 0x3f);
			c  = _mm_slli_pi32(c, 8);
			b2 = _mm_shuffle_pi16(c, 0x3f);
			r1 = _mm_or_si64(r1, r2);
			g1 = _mm_or_si64(g1, g2);
			b1 = _mm_or_si64(b1, b2);

			r1 = _mm_srli_pi16(r1, 3);
			g1 = _mm_srli_pi16(g1, 11);
			b1 = _mm_srli_pi16(b1, 11);

			r1 = _mm_slli_pi16(r1, 10);
			g1 = _mm_slli_pi16(g1, 5);

			r1 = _mm_or_si64(r1, g1);
			r1 = _mm_or_si64(r1, b1);

			*(__m64 *)(dest + y*dst_stride + x*2) = r1;
		}
	}
}
#else //TODO: test these
#ifdef __MMX__
#warning no mmx for 16-bit modes (needs extras added in 3dnow or SSE)!
#endif
static void pallet_blit565(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal)
{
	for(int y = 0; y < h; y++) {
		for(int x = 0; x < w; x++) {
			uint32_t cl = pal[src[y*src_stride + x]>>8];
			uint16_t px = (cl>>3)&0x1f;
			px = px | (((cl>>10)&0x3f)<<5);
			px = px | (((cl>>19)&0x1f)<<11);
			*(uint16_t *)(dest + y*dst_stride + x*4) = px;
		}
	}
}
static void pallet_blit555(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal)
{
	for(int y = 0; y < h; y++) {
		for(int x = 0; x < w; x++) {
			uint32_t cl = pal[src[y*src_stride + x]>>8];
			uint16_t px = (cl>>3)&0x1f;
			px = px | (((cl>>11)&0x1f)<<5);
			px = px | (((cl>>19)&0x1f)<<10);
			*(uint16_t *)(dest + y*dst_stride + x*4) = px;
		}
	}
}
#endif

#if defined(__MMX__)
static void pallet_blit8(uint8_t* restrict dest, unsigned int dst_stride,
		const uint16_t *pbattr src, unsigned int src_stride, unsigned int w, unsigned int h)
{
	for(unsigned int y = 0; y < h; y++) {
		for(unsigned int x = 0; x < w; x+=16) {
			__builtin_prefetch(src + y*src_stride + x + 16, 0, 0);
			__builtin_prefetch(dest + y*dst_stride + x + 16, 1, 0);

			__m64 p1, p2;

			p1 = *(__m64 *)(src + y*src_stride + x);
			p1  = _mm_srli_pi16(p1, 8);
			p2 = *(__m64 *)(src + y*src_stride + x+4);
			p2  = _mm_srli_pi16(p2, 8);

			p1 = _mm_packs_pu16(p1, p2);
			*(__m64 *)(dest + y*dst_stride + x) = p1;

			p1 = *(__m64 *)(src + y*src_stride + x+8);
			p1  = _mm_srli_pi16(p1, 8);
			p2 = *(__m64 *)(src + y*src_stride + x+12);
			p2  = _mm_srli_pi16(p2, 8);

			p1 = _mm_packs_pu16(p1, p2);
			*(__m64 *)(dest + y*dst_stride + x+ 8) = p1;
		}
	}
}
#else
static void pallet_blit8(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h)
{
	for(int y = 0; y < h; y++)
		for(int x = 0; x < w; x++)
			*(dest + y*dst_stride + x*4) = src[y*src_stride + x]>>8;
}
#endif

void pallet_blit_Pixbuf(Pixbuf* dst, const uint16_t* restrict src, int w, int h, const uint32_t *restrict pal)
{
	const int src_stride = w;
	w = IMIN(w, dst->w);
	h = IMIN(h, dst->h);

	if(dst->bpp == 32) pallet_blit32(dst->data, dst->pitch, src, src_stride, w, h, pal);
	else if(dst->bpp == 16) pallet_blit565(dst->data, dst->pitch, src, src_stride, w, h, pal);
	else if(dst->bpp == 15) pallet_blit555(dst->data, dst->pitch, src, src_stride, w, h, pal);
	else if(dst->bpp == 8) { // need to set surface's pallet
		pallet_blit8(dst->data, dst->pitch, src, src_stride, w, h);
	}
#ifdef __MMX__
	_mm_empty();
#endif
}

#ifdef USE_SDL
#include <SDL.h>
void pallet_blit_SDL(SDL_Surface *dst, const uint16_t* restrict src, int w, int h, const uint32_t *restrict pal)
{
	const int src_stride = w;
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
#ifdef __MMX__
	_mm_empty();
#endif
	if(SDL_MUSTLOCK(dst)) SDL_UnlockSurface(dst);
}
#endif

