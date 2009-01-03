#include <unistd.h>
#include <stdint.h>

#include <SDL.h>

#include "mymm.h"

#include "pixmisc.h"
#include "common.h"

// pallet must have 257 entries

void pallet_blit_SDL(SDL_Surface *dst, uint16_t *src, int w, int h, uint32_t *pal)
{
	if((SDL_MUSTLOCK(dst) && SDL_LockSurface(dst) < 0) || w < 0 || h < 0) return;
	
	const __m64 zero = _mm_set1_pi16(0);;
	const __m64 mask = _mm_set1_pi16(0x00ff);;
	const unsigned int dst_stride = dst->pitch/4;
	const unsigned int src_stride = w;
	
	const uint32_t *dest = dst->pixels;
	w = IMIN(w, dst->w);
	h = IMIN(h, dst->h);
	
	for(unsigned int y = 0; y < h; y++) {
		for(unsigned int x = 0; x < w; x+=2) 
		{
			int v = src[y*src_stride + x];
			__builtin_prefetch(src + y*src_stride + x + 4, 0, 0);
			__builtin_prefetch(dest + y*src_stride + x + 4, 1, 0);
			
			__m64 col1 = *(__m64 *)(pal+(v/256));
			__m64 col2 = col1;
			col1 = _mm_unpacklo_pi8(col1, zero);
    		col2 = _mm_unpackhi_pi8(col2, zero);
			//col1 = (__m64) __builtin_ia32_punpcklbw ((__v8qi)col1, (__v8qi)zero);
    		//col2 = (__m64) __builtin_ia32_punpckhbw ((__v8qi)col2, (__v8qi)zero);
			
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

	_mm_empty();
	if(SDL_MUSTLOCK(dst)) SDL_UnlockSurface(dst);
}

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
			//~ __m64 tmp1 = (__m64)*(uint64_t *)(pal+(v>>8));
			//~ __m64 tmp2 = (__m64)*(uint64_t *)(pal+(v2>>8));
			//~ __m64 col1 = _mm_unpacklo_pi32(tmp1, tmp2);
			//~ __m64 col2 = _mm_unpackhi_pi32(tmp1, tmp2);
			
			//~ tmp1 = _mm_unpacklo_pi32(_mm_set1_pi8(v & 0xff), _mm_set1_pi8(v2 & 0xff));
			//~ tmp2 = _mm_sub_pi8(_mm_set1_pi8(0xff), tmp1);
		    //~ //col1 = (col1*v + col2*(0xff-v))/256;
    		//~ col1 = _mm_mulhi_pi8(col1, tmp1);
    		//~ col2 = _mm_mulhi_pi8(col2, tmp2);
    		//~ col1 = _mm_add_pi8(col1, col2);
			
			//~ _mm_stream_pi((__m64 *)(dest + y*dst_stride + x), tmp);
		//~ }
	//~ }
	//~ _mm_empty();
	//~ if(SDL_MUSTLOCK(dst)) SDL_UnlockSurface(dst);
	//~ SDL_UpdateRect(dst, 0, 0, w, h);
//~ }


