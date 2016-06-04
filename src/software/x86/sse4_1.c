
#if (__x86_64__ || __i386__)
#pragma GCC target("no-sse4.2")

#include "common.h"
#include "../pixmisc.h"
#include <mmintrin.h>
#include <xmmintrin.h>
#include <emmintrin.h>
#include <immintrin.h>
#include <smmintrin.h>

#include <assert.h>

#ifndef NDEBUG
#define unreachable() assert(0)
#else
#define unreachable __builtin_unreachable
#endif

// requires w%16 == 0 && h%16 == 0
__attribute__((hot))
void maxblend_sse4_1(void *restrict dest, const void *restrict src, int w, int h)
{
	__m128i * restrict mbdst = dest; const __m128i * restrict mbsrc = src;
	const size_t npix = (size_t)w*(size_t)h;
#if 0
	//TODO: try to work out why this is slower
	//_mm_lfence();
	for(size_t i=0; i < (unsigned)w*h; i+=32, mbdst+=4, mbsrc+=4) {
		__m128i v1, v2, t1, t2, v3, t3, v4, t4;

		v1 = _mm_stream_load_si128(mbdst + 0), t1 = _mm_stream_load_si128(mbsrc + 0);
		v1 = _mm_max_epu16(v1, t1);
		_mm_stream_si128(mbdst + 0, v1);

		v2 = _mm_stream_load_si128(mbdst + 1), t2 = _mm_stream_load_si128(mbsrc + 1);
		v2 = _mm_max_epu16(v2, t2);
		_mm_stream_si128(mbdst + 1, v2);

		v3 = _mm_stream_load_si128(mbdst + 2), t3 = _mm_stream_load_si128(mbsrc + 2);
		v3 = _mm_max_epu16(v3, t3);
		_mm_stream_si128(mbdst + 2, v3);

		v4 = _mm_stream_load_si128(mbdst + 3), t4 = _mm_stream_load_si128(mbsrc + 3);
		v4 = _mm_max_epu16(v4, t4);
		_mm_stream_si128(mbdst + 3, v4);
	}
	_mm_sfence(); // needed because of the non-temporal stores.
#else
	_mm_mfence();
	_mm_prefetch(mbdst, _MM_HINT_NTA);
	//__builtin_prefetch(mbdst + 4, 1, 0);
	for(size_t i=0; i < npix; i+=32, mbdst+=4, mbsrc+=4) {
		_mm_prefetch(mbdst + 4, _MM_HINT_NTA);
		//__builtin_prefetch(mbdst + 4, 1, 0);
		mbdst[0] = _mm_max_epu16(mbdst[0], _mm_stream_load_si128(mbsrc + 0));
		mbdst[1] = _mm_max_epu16(mbdst[1], _mm_stream_load_si128(mbsrc + 1));
		mbdst[2] = _mm_max_epu16(mbdst[2], _mm_stream_load_si128(mbsrc + 2));
		mbdst[3] = _mm_max_epu16(mbdst[3], _mm_stream_load_si128(mbsrc + 3));
	}
#endif
}

#endif
