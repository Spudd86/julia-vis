#include <unistd.h>
#include <stdint.h>

#include "pixmisc.h"

#include "mymm.h"
//TODO: sse2 version of this (8 pixels at once!)

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
