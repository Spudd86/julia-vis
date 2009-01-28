#include <unistd.h>
#include <stdint.h>

#include "pixmisc.h"

#include "mymm.h"
//TODO: sse2 version of this (8 pixels at once!)

void fade_pix(void *restrict dest, void *restrict src, int w, int h, uint8_t fade)
{
	__m64 *mbdst = dest, *mbsrc = src;
	const __m64 fd = _mm_set1_pi16(256-fade);
	for(unsigned int i=0; i < 2*w*h/sizeof(__m64); i+=4) { // TODO see if the prefeting is helping 
		__builtin_prefetch(mbdst+i+4, 1, 0); __builtin_prefetch(mbsrc+i+4, 0, 0); 
		__m64 v,t;
		
		v = t = mbsrc[i]; 
		v = _mm_mulhi_pi16(v, fd); t = _mm_mullo_pi16(t, fd);
		v = _mm_slli_pi16(v, 8);   t = _mm_srli_pi16(t, 8);
		v = _mm_or_si64(v,t);
		mbdst[i]=v;
		
		v = t = mbsrc[i+1]; 
		v = _mm_mulhi_pi16(v, fd); t = _mm_mullo_pi16(t, fd);
		v = _mm_slli_pi16(v, 8);   t = _mm_srli_pi16(t, 8);
		v = _mm_or_si64(v,t);
		mbdst[i+1]=v;
		
		v = t = mbsrc[i+2]; 
		v = _mm_mulhi_pi16(v, fd); t = _mm_mullo_pi16(t, fd);
		v = _mm_slli_pi16(v, 8);   t = _mm_srli_pi16(t, 8);
		v = _mm_or_si64(v,t);
		mbdst[i+2]=v;

		v = t = mbsrc[i+3]; 
		v = _mm_mulhi_pi16(v, fd); t = _mm_mullo_pi16(t, fd);
		v = _mm_slli_pi16(v, 8);   t = _mm_srli_pi16(t, 8);
		v = _mm_or_si64(v,t);
		
		mbdst[i+3]=v;
	}
	_mm_empty();
}

// require w%8 == 0
void maxblend_stride(void *restrict dest, int dest_stride, void *restrict src, int w, int h)
{
	__m64 *mbdst = dest, *mbsrc = src;
	const __m64 off = _mm_set1_pi16(0x8000);
	for(int y=0; y < h; y++) {
		int dst_off = dest_stride*y;
		int src_off = w*y;
		for(unsigned int i=0; i < w/sizeof(__m64); i+=2) { // TODO see if the prefeting is helping 
			__builtin_prefetch(mbdst+i+2, 1, 0); __builtin_prefetch(mbsrc+i+2, 0, 0); 
			
			__m64 v = mbdst[dst_off+i], t = mbsrc[src_off+i]; 
			v = _mm_add_pi16(v, off); t = _mm_add_pi16(t, off);
			v = _mm_max_pi16(v, t);
			v = _mm_sub_pi16(v, off);
			mbdst[dst_off+i]=v;
			
			v = mbdst[dst_off+i+1]; t = mbsrc[src_off+i+1]; 
			v = _mm_add_pi16(v, off); t = _mm_add_pi16(t, off);
			v = _mm_max_pi16(v, t);
			v = _mm_sub_pi16(v, off);
			mbdst[dst_off+i+1]=v;
		}
	}
	_mm_empty();
}

// requires w%16 == 0
void maxblend(void *restrict dest, void *restrict src, int w, int h)
{
	__m64 *mbdst = dest, *mbsrc = src;
	const __m64 off = _mm_set1_pi16(0x8000);
	for(unsigned int i=0; i < 2*w*h/sizeof(__m64); i+=4) { // TODO see if the prefeting is helping 
		__builtin_prefetch(mbdst+i+4, 1, 0); __builtin_prefetch(mbsrc+i+4, 0, 0); 
		
		__m64 v = mbdst[i], t = mbsrc[i]; 
		v = _mm_add_pi16(v, off); t = _mm_add_pi16(t, off);
		v = _mm_max_pi16(v, t);
		v = _mm_sub_pi16(v, off);
		mbdst[i]=v;
		//~ _mm_stream_pi(mbdst+i, v);
		
		v = mbdst[i+1]; t = mbsrc[i+1]; 
		v = _mm_add_pi16(v, off); t = _mm_add_pi16(t, off);
		v = _mm_max_pi16(v, t);
		v = _mm_sub_pi16(v, off);
		mbdst[i+1]=v;
		//~ _mm_stream_pi(mbdst+i+1, v);
		
		v = mbdst[i+2]; t = mbsrc[i+2]; 
		v = _mm_add_pi16(v, off); t = _mm_add_pi16(t, off);
		v = _mm_max_pi16(v, t);
		v = _mm_sub_pi16(v, off);
		mbdst[i+2]=v;
		//~ _mm_stream_pi(mbdst+i+2, v);

		v = mbdst[i+3]; t = mbsrc[i+3]; 
		v = _mm_add_pi16(v, off); t = _mm_add_pi16(t, off);
		v = _mm_max_pi16(v, t);
		v = _mm_sub_pi16(v, off);
		
		mbdst[i+3]=v;
		//~ _mm_stream_pi(mbdst+i+3, v); // TODO: see if non-caching write is helping
	}
	_mm_empty();
}


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
