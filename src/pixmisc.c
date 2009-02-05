#include <unistd.h>
#include <stdint.h>

#include "pixmisc.h"
#include "common.h"

#include "mymm.h"

void maxblend_stride(void *restrict dest, int dest_stride, void *restrict source, int w, int h)
{
	uint16_t *restrict dst = dest, *restrict src = source;
	for(int y=0; y < h; y++) 
		for(int x=0; x < w; x++)
			dst[dest_stride*y+x] = IMAX(dst[dest_stride*y+x], src[w*y+x]);
}

//TODO: portable version (no x86 stuff)

#ifndef __SSE2__
//FIXME: currently this loses two bits of precision
void fade_pix(void *restrict dest, void *restrict src, int w, int h, uint8_t fade)
{
	__m64 *mbdst = dest, *mbsrc = src;
	const __m64 fd = _mm_set1_pi16(fade<<7);
	//const __m64 fd = _mm_set1_pi16(fade);
	for(unsigned int i=0; i < 2*w*h/sizeof(__m64); i+=4) { // TODO see if the prefeting is helping 
		__builtin_prefetch(mbdst+i+4, 1, 0); __builtin_prefetch(mbsrc+i+4, 0, 0); 
		__m64 v1, v2, v3, v4;//,t;
		
		v1 = mbsrc[i];
		v1 = _mm_srli_pi16(v1, 1);
		v1 = _mm_mulhi_pi16(v1, fd);
		v1 = _mm_slli_pi16(v1, 2);
		mbdst[i]=v1;
		
		v2 = mbsrc[i+1]; 
		v2 = _mm_srli_pi16(v2, 1);
		v2 = _mm_mulhi_pi16(v2, fd);
		v2 = _mm_slli_pi16(v2, 2);
		mbdst[i+1]=v2;
		
		v3 = mbsrc[i+2]; 
		v3 = _mm_srli_pi16(v3, 1);
		v3 = _mm_mulhi_pi16(v3, fd);
		v3 = _mm_slli_pi16(v3, 2);
		mbdst[i+2]=v3;

		v4 = mbsrc[i+3]; 
		v4 = _mm_srli_pi16(v4, 1);
		v4 = _mm_mulhi_pi16(v4, fd);
		v4 = _mm_slli_pi16(v4, 2);
		mbdst[i+3]=v4;
	}
	_mm_empty();
}

// requires w%16 == 0
void maxblend(void *restrict dest, void *restrict src, int w, int h)
{
	__m64 *mbdst = dest, *mbsrc = src;
	const __m64 off = _mm_set1_pi16(0x8000);
	for(unsigned int i=0; i < 2*w*h/sizeof(__m64); i+=4) { // TODO see if the prefeting is helping 
		__builtin_prefetch(&(mbsrc[i+4]), 0, 0); __builtin_prefetch(&(mbdst[i+4]), 1, 0);
		
		__m64 v1, v2, v3, v4, t1, t2, t3, t4;
		
		v1 = mbdst[i], t1 = mbsrc[i]; 
		v1 = _mm_add_pi16(v1, off); t1 = _mm_add_pi16(t1, off);
		v1 = _mm_max_pi16(v1, t1);
		v1 = _mm_sub_pi16(v1, off);
		mbdst[i]=v1;
		
		v2 = mbdst[i+1], t2 = mbsrc[i+1]; 
		v2 = _mm_add_pi16(v2, off); t2 = _mm_add_pi16(t2, off);
		v2 = _mm_max_pi16(v2, t2);
		v2 = _mm_sub_pi16(v2, off);
		mbdst[i+1]=v2;
		
		v3 = mbdst[i+2], t3 = mbsrc[i+2]; 
		v3 = _mm_add_pi16(v3, off); t3 = _mm_add_pi16(t3, off);
		v3 = _mm_max_pi16(v3, t3);
		v3 = _mm_sub_pi16(v3, off);
		mbdst[i+2]=v3;

		v4 = mbdst[i+3], t4 = mbsrc[i+3]; 
		v4 = _mm_add_pi16(v4, off); t4 = _mm_add_pi16(t4, off);
		v4 = _mm_max_pi16(v4, t4);
		v4 = _mm_sub_pi16(v4, off);
		mbdst[i+3]=v4;
	}
	_mm_empty();
}

#else
#include <emmintrin.h>

// requires w%16 == 0
void maxblend(void *restrict dest, void *restrict src, int w, int h)
{
	__m128i *mbdst = dest, *mbsrc = src;
	const __m128i off = _mm_set1_epi16(0x8000);
	for(unsigned int i=0; i < 2*w*h/sizeof(__m128); i+=2) { // TODO see if the prefeting is helping 
		__builtin_prefetch(&(mbsrc[i+2]), 0, 0); __builtin_prefetch(&(mbdst[i+2]), 1, 0);
		
		__m128i v1, v2, t1, t2;
		
		v1 = mbdst[i], t1 = mbsrc[i]; 
		v1 = _mm_add_epi16(v1, off); t1 = _mm_add_epi16(t1, off);
		v1 = _mm_max_epi16(v1, t1);
		v1 = _mm_sub_epi16(v1, off);
		mbdst[i]=v1;
		
		v2 = mbdst[i+1], t2 = mbsrc[i+1]; 
		v2 = _mm_add_epi16(v2, off); t2 = _mm_add_epi16(t2, off);
		v2 = _mm_max_epi16(v2, t2);
		v2 = _mm_sub_epi16(v2, off);
		mbdst[i+1]=v2;
	}
}

void fade_pix(void *restrict dest, void *restrict src, int w, int h, uint8_t fade)
{
	__m128i *mbdst = dest, *mbsrc = src;
	const __m128i fd = _mm_set1_epi16(fade<<7);

	for(unsigned int i=0; i < 2*w*h/sizeof(__m128i); i+=2) { // TODO see if the prefeting is helping 
		__builtin_prefetch(mbdst+i+2, 1, 0); __builtin_prefetch(mbsrc+i+2, 0, 0); 
		__m128i v1, v2;//,t;
		
		v1 = mbsrc[i];
		v1 = _mm_srli_epi16(v1, 1);
		v1 = _mm_mulhi_epi16(v1, fd);
		v1 = _mm_slli_epi16(v1, 2);
		mbdst[i]=v1;
		
		v2 = mbsrc[i+1]; 
		v2 = _mm_srli_epi16(v2, 1);
		v2 = _mm_mulhi_epi16(v2, fd);
		v2 = _mm_slli_epi16(v2, 2);
		mbdst[i+1]=v2;
	}
	_mm_empty();
}

#endif

//~ __attribute__((fastcall)) void maxblend(void *dest, void *src, int w, int h)
//~ {
	//~ __m64 *mbdst = dest, *mbsrc = src;
	//~ const __m64 off = _mm_set1_pi16(0x8000);
	//~ for(unsigned int i=0; i < 2*w*h/sizeof(__m64); i++) { // TODO see if the prefeting is helping 
		//~ //__builtin_prefetch(mbdst+i+4, 1, 1); __builtin_prefetch(mbsrc+i+4, 0, 1); 
		
		//~ __m64 v = mbdst[i], t = mbsrc[i]; 
		//~ v = _mm_add_pi16(v, off); t = _mm_add_pi16(t, off);
		//~ v = _mm_max_pi16(v, t);
		//~ v = _mm_sub_pi16(v, off);
		//~ _mm_stream_pi(mbdst+i, v);
	//~ }
	//~ _mm_empty();
//~ }
