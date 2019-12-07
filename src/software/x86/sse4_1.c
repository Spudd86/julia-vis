
#if (__x86_64__ || __i386__) && !defined(DISABLE_X86_INTRIN)
#pragma GCC target("no-sse4.2,sse4.1")
#ifndef DEBUG
#pragma GCC optimize "3,inline-functions" // Want -fmerge-all-constants but we can't put it in the optimize pragma for some reason
// need to shut gcc up about casting away const in normal builds because
// some of the intrinsics that REALLY ought to take const pointer don't
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

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
	__m128i * restrict mbdst = dest;
	__m128i * restrict mbsrc = (__m128i *)src; // cast away const because _mm_stream_load_si128() doesn't take a const pointer for some stupid reason
	const size_t npix = (size_t)w*(size_t)h;
	// just assume cache lines are at least 64 bytes
	_mm_prefetch(mbdst +  0, _MM_HINT_NTA);
	for(size_t i=0; i < npix; i+=32, mbdst+=4, mbsrc+=4) {
		_mm_prefetch(mbdst +  8, _MM_HINT_NTA);
		mbdst[0] = _mm_max_epu16(mbdst[0], _mm_stream_load_si128(mbsrc + 0));
		mbdst[1] = _mm_max_epu16(mbdst[1], _mm_stream_load_si128(mbsrc + 1));
		mbdst[2] = _mm_max_epu16(mbdst[2], _mm_stream_load_si128(mbsrc + 2));
		mbdst[3] = _mm_max_epu16(mbdst[3], _mm_stream_load_si128(mbsrc + 3));
	}
}

#endif
