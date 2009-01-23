#include <unistd.h>
#include <stdint.h>

#include "mymm.h"

#include "pixmisc.h"
#include "common.h"

// pallet must have 257 entries

static void pallet_blit_SDL32(uint32_t  * restrict dest, unsigned int dst_stride, uint16_t *restrict src, unsigned int src_stride, int w, int h, uint32_t *restrict pal)
{
	__m64 zero = _mm_cvtsi32_si64(0ll);
	__m64 mask = (__m64)(0x00ff00ff00ff);
	dst_stride /= 4;
	
	for(unsigned int y = 0; y < h; y++) {
		for(unsigned int x = 0; x < w; x+=4) {
			int v = src[y*src_stride + x];
			__builtin_prefetch(src + y*src_stride + x + 4, 0, 0);
			//__builtin_prefetch(dest + y*dst_stride + x + 4, 1, 0);
			
			__m64 col1 = *(__m64 *)(pal+(v/256));
			__m64 col2 = col1;
			col1 = _mm_unpacklo_pi8(col1, zero);
    		col2 = _mm_unpackhi_pi8(col2, zero);
			
		    //col1 = (col1*v + col2*(0xff-v))/256;
			__m64 vt = _mm_set1_pi16(v);
			vt = _mm_and_si64(vt, mask);
    		col1 = _mm_mullo_pi16(col1, vt);
			vt = _mm_andnot_si64(vt, mask); // vt = 255 - vt
    		col2 = _mm_mullo_pi16(col2, vt);
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
    		col1 = _mm_mullo_pi16(col1, vt);
			vt = _mm_andnot_si64(vt, mask); // vt = 255 - vt
    		col2 = _mm_mullo_pi16(col2, vt);
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
    		col1 = _mm_mullo_pi16(col1, vt);
			vt = _mm_andnot_si64(vt, mask); // vt = 255 - vt
    		col2 = _mm_mullo_pi16(col2, vt);
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
    		col1 = _mm_mullo_pi16(col1, vt);
			vt = _mm_andnot_si64(vt, mask); // vt = 255 - vt
    		col2 = _mm_mullo_pi16(col2, vt);
    		col1 = _mm_add_pi16(col1, col2);
    		col1 = _mm_srli_pi16(col1, 8);
			
			tmp = _mm_packs_pu16(tmp, col1);
			//_mm_stream_pi((__m64 *)(dest + y*dst_stride + x+2), tmp);
			*(__m64 *)(dest + y*dst_stride + x + 2) = tmp;
		}
	}
}

static void pallet_blit_SDL565(uint8_t  * restrict dest, unsigned int dst_stride, uint16_t *restrict src, unsigned int src_stride, int w, int h, uint32_t *restrict pal)
{
	for(unsigned int y = 0; y < h; y++) {
		for(unsigned int x = 0; x < w; x+=4) {
			__builtin_prefetch(src + y*src_stride + x + 4, 0, 0);
			//__builtin_prefetch(dest + y*dst_stride + x + 4, 1, 0);
			
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

static void pallet_blit_SDL555(uint8_t  * restrict dest, unsigned int dst_stride, uint16_t *restrict src, unsigned int src_stride, int w, int h, uint32_t *restrict pal)
{	
	for(unsigned int y = 0; y < h; y++) {
		for(unsigned int x = 0; x < w; x+=4) {
			__builtin_prefetch(src + y*src_stride + x + 4, 0, 0);
			//__builtin_prefetch(dest + y*dst_stride + x + 4, 1, 0);
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

#ifdef USE_SDL

#include <SDL.h>

void pallet_blit_SDL(SDL_Surface *dst, uint16_t * restrict src, int w, int h, uint32_t *restrict pal)
{
	const int src_stride = w;
	w = IMIN(w, dst->w);
	h = IMIN(h, dst->h);
	
	if((SDL_MUSTLOCK(dst) && SDL_LockSurface(dst) < 0) || w < 0 || h < 0) return;
	if(dst->format->BitsPerPixel == 32) pallet_blit_SDL32(dst->pixels, dst->pitch, src, src_stride, w, h, pal);
	else if(dst->format->BitsPerPixel == 16) pallet_blit_SDL565(dst->pixels, dst->pitch, src, src_stride, w, h, pal);
	else if(dst->format->BitsPerPixel == 15) pallet_blit_SDL555(dst->pixels, dst->pitch, src, src_stride, w, h, pal);
	_mm_empty();
	if(SDL_MUSTLOCK(dst)) SDL_UnlockSurface(dst);
}

#endif

#ifdef USE_DIRECTFB

#include <directfb.h>

void pallet_blit_DFB(IDirectFBSurface *dst, uint16_t * restrict src, int w, int h, uint32_t *restrict pal)
{
	const int src_stride = w;
	DFBSurfacePixelFormat dst_format;
	void *dst_pixels = NULL;
	int dst_pitch, dst_w, dst_h;
	
	dst->GetSize(dst, &dst_w, &dst_h);
	dst->GetPixelFormat(dst, &dst_format);
	
	w = IMIN(w, dst_w);
	h = IMIN(h, dst_h);
	dst->Lock(dst, DSLF_WRITE, &dst_pixels, &dst_pitch); // TODO: error check
	if(dst_format == DSPF_RGB32) pallet_blit_SDL32(dst_pixels, dst_pitch, src, src_stride, w, h, pal);
	else if(dst_format == DSPF_RGB16) pallet_blit_SDL565(dst_pixels, dst_pitch, src, src_stride, w, h, pal);
	else if(dst_format == DSPF_RGB555) pallet_blit_SDL555(dst_pixels, dst_pitch, src, src_stride, w, h, pal);
	_mm_empty();
	dst->Unlock(dst);
}
#endif

// stride is for dest
//~ void pallet_blit(void *dest, int dst_stride, uint16_t *src, int w, int h, uint32_t *pal)
//~ {
	//~ for(int y = 0; y < h; y++) 
		//~ for(int x = 0; x < w; x++) 
			//~ *(int32_t *)(dest + y*dst_stride + x*4) = pal[src[y*w + x]>>8];
//~ }

//TODO load/generate pallets

// this one is sort of what I'd like to do but there is no 8 bit mmx mul :(
//~ void pallet_blitSDL(SDL_Surface *dst, uint16_t *src, int w, int h, uint32_t *pal)
//~ {
	//~ if(SDL_MUSTLOCK(dst) && SDL_LockSurface(dst) < 0) return;

	//~ int dst_stride = dst->pitch/4;
	//~ uint32_t *dest = dst->pixels;
	//~ w = IMIN(w, dst->w);
	//~ h = IMIN(h, dst->h);
	
	//~ for(int y = 0; y < h; y++) {
		//~ for(int x = 0; x < w; x+=2) {
			//~ int v = src[y*src_stride + x];
			//~ int v2 = src[y*src_stride + x+1];
			//~ __m64 col1, col2;
			//~ col1 = col2 = (__m64)*(uint64_t *)(pal+(v>>8));
			//~ __m64 tmp   = (__m64)*(uint64_t *)(pal+(v2>>8));
			//~ col1 = _mm_unpacklo_pi32(col1, tmp);
			//~ col2 = _mm_unpackhi_pi32(col2, tmp);
			//~ tmp = _mm_unpacklo_pi32(_mm_set1_pi8(v & 0xff), _mm_set1_pi8(v2 & 0xff));
			//~ col1 = _mm_mulhi_pi8(col1, tmp);
			//~ tmp = _mm_andnot_si64(tmp, mask)
			//~ col2 = _mm_mulhi_pi8(col2, tmp);
    		//~ col1 = _mm_add_pi8(col1, col2);
			
			//~ _mm_stream_pi((__m64 *)(dest + y*dst_stride + x), tmp);
		//~ }
	//~ }
	//~ _mm_empty();
	//~ if(SDL_MUSTLOCK(dst)) SDL_UnlockSurface(dst);
	//~ SDL_UpdateRect(dst, 0, 0, w, h);
//~ }


