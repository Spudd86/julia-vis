
#if (__x86_64__ || __i386__) && !defined(DISABLE_X86_INTRIN)

#if DEBUG
#pragma GCC optimize "3,inline-functions" // Want -fmerge-all-constants but we can't put it in the optimize pragma for some reason
#endif

#include "common.h"
#include "../pixmisc.h"
#include <assert.h>

// all of SSE is always available on x86_64 so we don't have to worry so much about intrinsics being built with the right flags
#ifndef __x86_64__
#pragma GCC target("no-sse2,sse,mmx")
#pragma clang attribute push (__attribute__(( target("no-sse2,sse,mmx") )), apply_to = function)
#endif

#include <mmintrin.h>
#include <xmmintrin.h>
#include <immintrin.h>

#ifndef NDEBUG
#define unreachable() assert(0)
#else
#define unreachable __builtin_unreachable
#endif

// requires w%16 == 0
__attribute__((hot, target("sse,mmx")))
void maxblend_sse(void *restrict dest, const void *restrict src, int w, int h)
{
	//FIXME: use src_stride
	//FIXME: deal with w%16 != 0
	__m64 *mbdst = dest; const __m64 *mbsrc = src;
	const __m64 off = _mm_set1_pi16(0x8000);
	_mm_prefetch(mbdst, _MM_HINT_NTA);
	_mm_prefetch(mbsrc, _MM_HINT_NTA);
	for(unsigned int i=0; i < 2*w*h/sizeof(__m64); i+=4, mbdst+=4, mbsrc+=4) {
		_mm_prefetch(mbdst + 4, _MM_HINT_NTA);
		_mm_prefetch(mbsrc + 4, _MM_HINT_NTA);

		__m64 v1, v2, v3, v4, t1, t2, t3, t4;

		v1 = mbdst[0], t1 = mbsrc[0];
		v1 = _mm_add_pi16(v1, off); t1 = _mm_add_pi16(t1, off);
		v1 = _mm_max_pi16(v1, t1);
		v1 = _mm_sub_pi16(v1, off);
		mbdst[0]=v1;

		v2 = mbdst[1], t2 = mbsrc[1];
		v2 = _mm_add_pi16(v2, off); t2 = _mm_add_pi16(t2, off);
		v2 = _mm_max_pi16(v2, t2);
		v2 = _mm_sub_pi16(v2, off);
		mbdst[1]=v2;

		v3 = mbdst[2], t3 = mbsrc[2];
		v3 = _mm_add_pi16(v3, off); t3 = _mm_add_pi16(t3, off);
		v3 = _mm_max_pi16(v3, t3);
		v3 = _mm_sub_pi16(v3, off);
		mbdst[2]=v3;

		v4 = mbdst[3], t4 = mbsrc[3];
		v4 = _mm_add_pi16(v4, off); t4 = _mm_add_pi16(t4, off);
		v4 = _mm_max_pi16(v4, t4);
		v4 = _mm_sub_pi16(v4, off);
		mbdst[3]=v4;
	}
	_mm_empty();
	_mm_sfence(); // needed because of the non-temporal stores.
}

#define PALBLIT_SSE 1

#include "palblit_mmxsse.h"

#ifndef __x86_64__
#pragma clang attribute pop
#endif

#endif
