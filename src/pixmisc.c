#include <unistd.h>
#include <stdint.h>
#include <mmintrin.h>
#include <xmmintrin.h>

#include "pixmisc.h"

typedef int v4hi __attribute__ ((vector_size (8)));


void maxblend(void *dest, void *src, int w, int h)
{
	__m64 *mbdst = dest, *mbsrc = src;
	const __m64 off = _mm_set1_pi16(0x8000);
	for(unsigned int i=0; i < 2*w*h/sizeof(__m64); i++) { // TODO see if the prefeting is helping 
		_mm_prefetch(mbsrc+i+4, _MM_HINT_NTA); _mm_prefetch(mbdst+i+4, _MM_HINT_NTA);
		__m64 v = mbdst[i], t = mbsrc[i]; 
		v = _mm_add_pi16(v, off); t = _mm_add_pi16(t, off);
		v = _mm_max_pi16(v, t);
		v = _mm_sub_pi16(v, off);
		//~ mbdst[i]=v;
		_mm_stream_pi(mbdst+i, v);
		i++;
		v = mbdst[i]; t = mbsrc[i]; 
		v = _mm_add_pi16(v, off); t = _mm_add_pi16(t, off);
		v = _mm_max_pi16(v, t);
		v = _mm_sub_pi16(v, off);
		//~ mbdst[i]=v;
		_mm_stream_pi(mbdst+i, v);
		i++;
		v = mbdst[i]; t = mbsrc[i]; 
		v = _mm_add_pi16(v, off); t = _mm_add_pi16(t, off);
		v = _mm_max_pi16(v, t);
		v = _mm_sub_pi16(v, off);
		//~ mbdst[i]=v;
		_mm_stream_pi(mbdst+i, v);
		i++;
		v = mbdst[i]; t = mbsrc[i]; 
		v = _mm_add_pi16(v, off); t = _mm_add_pi16(t, off);
		v = _mm_max_pi16(v, t);
		v = _mm_sub_pi16(v, off);
		
		//~ mbdst[i]=v;
		_mm_stream_pi(mbdst+i, v); // TODO: see if non-caching write is helping
	}
	_mm_empty();
}