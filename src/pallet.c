#include <unistd.h>
#include <stdint.h>

#include <SDL.h>

#include <mmintrin.h>
#ifdef __SSE__
#include <xmmintrin.h>
#endif
#ifdef __3dNOW__
#include <mm3dnow.h>
static inline void _mm_stream_pi(__m64 *a, __m64 b) { __builtin_ia32_movntq((int64_t *)(a), (int64_t)b); }
#endif 

#include "pixmisc.h"
#include "common.h"

// pallet must have 257 entries

// stride is for dest
void pallet_blit(void *dest, int dst_stride, uint16_t *src, int w, int h, uint32_t *pal)
{
	for(int y = 0; y < h; y++) {
		for(int x = 0; x < w; x++) {
			uint16_t v = src[y*w + x];

			__m64 col1 = _mm_unpacklo_pi8(_mm_cvtsi32_si64(pal[(v>>8)]), _mm_cvtsi32_si64(0));
    		__m64 col2 = _mm_unpacklo_pi8(_mm_cvtsi32_si64(pal[(v>>8)+1]), _mm_cvtsi32_si64(0));

    		__m64 vt = _mm_set1_pi16(v & 0xff);
    		__m64 t  = _mm_set1_pi16(0xff);

		    //col1 = (col1*v + col2*(0xff-v))/256;
    		t = _mm_sub_pi16(t, vt);
    		col1 = _mm_mullo_pi16(col1, vt);
    		col2 = _mm_mullo_pi16(col2, t);
    		col1 = _mm_add_pi16(col1, col2);
    		col1 = _mm_srli_pi16(col1, 8);
    		col1 = _mm_packs_pu16(col1, col1);

			*(int32_t *)(dest + y*dst_stride + x*4) = _mm_cvtsi64_si32(col1);
		}
	}
	_mm_empty();
}

//~ //doesn't work yet
void pallet_blit16_SDL(SDL_Surface *dst, uint16_t *src, int w, int h, uint32_t *pal)
{
	if(SDL_MUSTLOCK(dst) && SDL_LockSurface(dst) < 0) return;
	
	//const __m64 maskr = (__m64)0x001f0000001f0000;
	//const __m64 maskg = (__m64)0x00001f0000001f00;
	//const __m64 maskb = (__m64)0x0000001f0000001f;
	const int dst_stride = dst->pitch/2;
	const int src_stride = w;
	
	const uint16_t *dest = dst->pixels;
	w = IMIN(w, dst->w);
	h = IMIN(h, dst->h);
	
	__m64 msk = (__m64)0x0000001f0000001f;
	for(int y = 0; y < h; y++) {
		for(int x = 0; x < w; x+=2) 
		{
			int v = src[y*src_stride + x]; //TODO make this fast
			
			__m64 col1 = (__m64)*(int64_t *)(pal+(v/256));
			__m64 col2 = col1;
			col1 = _mm_unpacklo_pi8(col1, _mm_cvtsi32_si64(0));
    		col2 = _mm_unpackhi_pi8(col2, _mm_cvtsi32_si64(0));

		    //col1 = (col1*v + col2*(0xff-v))/256;
    		col1 = _mm_mullo_pi16(col1, _mm_set1_pi16(v & 0xff));
    		col2 = _mm_mullo_pi16(col2, _mm_set1_pi16(0xff-(v&0xff)));
    		col1 = _mm_add_pi16(col1, col2);
    		col1 = _mm_srli_pi16(col1, 11);
    		
			__m64 tmp = col1;
			
			v = src[y*src_stride + x + 1];
			col1 = (__m64)*(int64_t *)(pal+(v/256));
			col2 = col1;
			col1 = _mm_unpacklo_pi8(col1, _mm_cvtsi32_si64(0));
    		col2 = _mm_unpackhi_pi8(col2, _mm_cvtsi32_si64(0));

		    //col1 = (col1*v + col2*(0xff-v))/256;
    		col1 = _mm_mullo_pi16(col1, _mm_set1_pi16(v & 0xff));
    		col2 = _mm_mullo_pi16(col2, _mm_set1_pi16(0xff-(v&0xff)));
    		col1 = _mm_add_pi16(col1, col2);
    		col1 = _mm_srli_pi16(col1, 11);
			
			
			tmp = _mm_packs_pu16(tmp, col1);
			__m64 tmp2 = tmp;
			__m64 tmp3 = tmp;
			tmp = _mm_and_si64(tmp, msk);
			msk = _mm_slli_pi32(msk, 8);
			
			tmp2 = _mm_and_si64(tmp2, msk);
			tmp2 = _mm_srli_pi32(tmp2, 3);
			tmp = _mm_or_si64(tmp, tmp2);
			msk = _mm_slli_pi32(msk, 8);
			tmp3 = _mm_and_si64(tmp3, msk);
			tmp3 = _mm_srli_pi32(tmp3, 6);
			tmp = _mm_or_si64(tmp, tmp3);
			__m64 res = tmp;
			
			v = src[y*src_stride + x + 2];
			
			col1 = (__m64)*(int64_t *)(pal+(v/256));
			col2 = col1;
			col1 = _mm_unpacklo_pi8(col1, _mm_cvtsi32_si64(0));
    		col2 = _mm_unpackhi_pi8(col2, _mm_cvtsi32_si64(0));

		    //col1 = (col1*v + col2*(0xff-v))/256;
    		col1 = _mm_mullo_pi16(col1, _mm_set1_pi16(v & 0xff));
    		col2 = _mm_mullo_pi16(col2, _mm_set1_pi16(0xff-(v&0xff)));
    		col1 = _mm_add_pi16(col1, col2);
    		col1 = _mm_srli_pi16(col1, 11);
    		
			__m64 tmpb = col1;
			
			v = src[y*src_stride + x + 3];
			col1 = (__m64)*(int64_t *)(pal+(v/256));
			col2 = col1;
			col1 = _mm_unpacklo_pi8(col1, _mm_cvtsi32_si64(0));
    		col2 = _mm_unpackhi_pi8(col2, _mm_cvtsi32_si64(0));

		    //col1 = (col1*v + col2*(0xff-v))/256;
    		col1 = _mm_mullo_pi16(col1, _mm_set1_pi16(v & 0xff));
    		col2 = _mm_mullo_pi16(col2, _mm_set1_pi16(0xff-(v&0xff)));
    		col1 = _mm_add_pi16(col1, col2);
    		col1 = _mm_srli_pi16(col1, 11);
			
			tmp2 = _mm_packs_pu16(tmp2, col1);
			tmp2 = tmp;
			tmp3 = tmp;
			
			tmp3 = _mm_and_si64(tmp3, msk);
			msk = _mm_srli_pi32(msk, 8);
			tmp = _mm_and_si64(tmp, msk);
			tmp2 = _mm_and_si64(tmp2, msk);
			msk = _mm_srli_pi32(msk, 8);
			
			tmp2 = _mm_srli_pi32(tmp2, 3);
			tmp = _mm_or_si64(tmp, tmp2);

			tmp3 = _mm_srli_pi32(tmp3, 6);
			tmp = _mm_or_si64(tmp, tmp3);
			
			res = _mm_packs_pi32(res, tmp);
			
			_mm_stream_pi((__m64 *)(dest + y*dst_stride + x), res);
		}
	}
	_mm_empty();
	if(SDL_MUSTLOCK(dst)) SDL_UnlockSurface(dst);
	//SDL_UpdateRect(dst, 0, 0, w, h);
}

void pallet_blit_SDL(SDL_Surface *dst, uint16_t *src, int w, int h, uint32_t *pal)
{
	if(SDL_MUSTLOCK(dst) && SDL_LockSurface(dst) < 0) return;
	
	const int dst_stride = dst->pitch/4;
	const int src_stride = w;
	
	const uint32_t *dest = dst->pixels;
	w = IMIN(w, dst->w);
	h = IMIN(h, dst->h);
	
	for(int y = 0; y < h; y++) {
		for(int x = 0; x < w; x+=2) 
		{
			int v = src[y*src_stride + x];
			__builtin_prefetch(y*src_stride + x + 4, 1, 2);
			
			__m64 col1 = (__m64)*(int64_t *)(pal+(v/256));
			__m64 col2 = col1;
			col1 = _mm_unpacklo_pi8(col1, _mm_cvtsi32_si64(0));
    		col2 = _mm_unpackhi_pi8(col2, _mm_cvtsi32_si64(0));

		    //col1 = (col1*v + col2*(0xff-v))/256;
    		col1 = _mm_mullo_pi16(col1, _mm_set1_pi16(v & 0xff));
    		col2 = _mm_mullo_pi16(col2, _mm_set1_pi16(0xff-(v&0xff)));
    		col1 = _mm_add_pi16(col1, col2);
    		col1 = _mm_srli_pi16(col1, 8);
    		
			__m64 tmp = col1;
			
			v = src[y*src_stride + x + 1];
			col1 = (__m64)*(int64_t *)(pal+(v/256));
			col2 = col1;
			col1 = _mm_unpacklo_pi8(col1, _mm_cvtsi32_si64(0));
    		col2 = _mm_unpackhi_pi8(col2, _mm_cvtsi32_si64(0));
    		col1 = _mm_mullo_pi16(col1, _mm_set1_pi16(v & 0xff));
    		col2 = _mm_mullo_pi16(col2, _mm_set1_pi16(0xff-(v&0xff)));
    		col1 = _mm_add_pi16(col1, col2);
    		col1 = _mm_srli_pi16(col1, 8);
			
			tmp = _mm_packs_pu16(tmp, col1);
			_mm_stream_pi((__m64 *)(dest + y*dst_stride + x), tmp);
			
			v = src[y*src_stride + x + 2];
			col1 = (__m64)*(int64_t *)(pal+(v/256));
			col2 = col1;
			col1 = _mm_unpacklo_pi8(col1, _mm_cvtsi32_si64(0));
    		col2 = _mm_unpackhi_pi8(col2, _mm_cvtsi32_si64(0));
    		col1 = _mm_mullo_pi16(col1, _mm_set1_pi16(v & 0xff));
    		col2 = _mm_mullo_pi16(col2, _mm_set1_pi16(0xff-(v&0xff)));
    		col1 = _mm_add_pi16(col1, col2);
    		col1 = _mm_srli_pi16(col1, 8);
    		
			tmp = col1;
			
			v = src[y*src_stride + x + 3];
			col1 = (__m64)*(int64_t *)(pal+(v/256));
			col2 = col1;
			col1 = _mm_unpacklo_pi8(col1, _mm_cvtsi32_si64(0));
    		col2 = _mm_unpackhi_pi8(col2, _mm_cvtsi32_si64(0));
    		col1 = _mm_mullo_pi16(col1, _mm_set1_pi16(v & 0xff));
    		col2 = _mm_mullo_pi16(col2, _mm_set1_pi16(0xff-(v&0xff)));
    		col1 = _mm_add_pi16(col1, col2);
    		col1 = _mm_srli_pi16(col1, 8);
			
			tmp = _mm_packs_pu16(tmp, col1);
			_mm_stream_pi((__m64 *)(dest + y*dst_stride + x+2), tmp);
		}
	}
	_mm_empty();
	if(SDL_MUSTLOCK(dst)) SDL_UnlockSurface(dst);
	//SDL_UpdateRect(dst, 0, 0, w, h);
}

void pallet_blit_SDL8x8(SDL_Surface *dst, uint16_t *src, int w, int h, uint32_t *pal)
{
	if(SDL_MUSTLOCK(dst) && SDL_LockSurface(dst) < 0) return;
	
	int dst_stride = dst->pitch/4;
	uint32_t *dest = dst->pixels;
	w = IMIN(w, dst->w);
	h = IMIN(h, dst->h);
	
	for(int y = 0; y < h/8; y++) {
		for(int x = 0; x < w/8; x++) {
			for(int yt=0; yt<8; yt++) {
				for(int xt=0; xt<8; xt+=2) {
					int v = *(src++); //FIXME: need to calculate address properly (if dst smaller than src this is wrong)
					int v2 = *(src++);
					__builtin_prefetch(pal+v2/256,1,0);

					__m64 col1 = (__m64)*(int64_t *)(pal+(v/256));
					__m64 col2 = col1;
					col1 = _mm_unpacklo_pi8(col1, _mm_cvtsi32_si64(0));
					col2 = _mm_unpackhi_pi8(col2, _mm_cvtsi32_si64(0));

					//col1 = (col1*v + col2*(0xff-v))/256;
					col1 = _mm_mullo_pi16(col1, _mm_set1_pi16(v & 0xff));
					col2 = _mm_mullo_pi16(col2, _mm_set1_pi16(0xff-(v&0xff)));
					col1 = _mm_add_pi16(col1, col2);
					col1 = _mm_srli_pi16(col1, 8);
					
					__m64 tmp = col1;
					
					v = v2;
					col1 = (__m64)*(int64_t *)(pal+(v/256));
					col2 = col1;
					col1 = _mm_unpacklo_pi8(col1, _mm_cvtsi32_si64(0));
					col2 = _mm_unpackhi_pi8(col2, _mm_cvtsi32_si64(0));

					//col1 = (col1*v + col2*(0xff-v))/256;
					col1 = _mm_mullo_pi16(col1, _mm_set1_pi16(v & 0xff));
					col2 = _mm_mullo_pi16(col2, _mm_set1_pi16(0xff-(v&0xff)));
					col1 = _mm_add_pi16(col1, col2);
					col1 = _mm_srli_pi16(col1, 8);
					
					tmp = _mm_packs_pu16(tmp, col1);
					_mm_stream_pi((__m64 *)(dest + (y*8+yt)*dst_stride + (x*8+xt)), tmp);
				}
			}
		}
	}
	_mm_empty();
	if(SDL_MUSTLOCK(dst)) SDL_UnlockSurface(dst);
	//SDL_UpdateRect(dst, 0, 0, w, h);
}

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


