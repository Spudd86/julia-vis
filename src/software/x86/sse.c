
#if (__x86_64__ || __i386__)

#pragma GCC target("no-sse2,sse")
#pragma GCC optimize "3,inline-functions,merge-all-constants"

#include "common.h"
#include "../pixmisc.h"
#include <mmintrin.h>
#include <xmmintrin.h>

#if 0 //defined(__3dNOW__)
//TODO: work out how to do this more properly
// available on athlon (3dnow) but not in the header for it (these are the intel intrinsic function names)
#include <mm3dnow.h>
static __inline __m64 __attribute__((__gnu_inline__, __always_inline__, __artificial__))
_mm_max_pi16 (__m64 __A, __m64 __B) { return (__m64) __builtin_ia32_pmaxsw ((__v4hi)__A, (__v4hi)__B); }
extern __inline __m64 __attribute__((__gnu_inline__, __always_inline__, __artificial__))
_mm_shuffle_pi16 (__m64 __A, int const __N) { return (__m64) __builtin_ia32_pshufw ((__v4hi)__A, __N); }
#endif

#include <assert.h>

#ifndef NDEBUG
#define unreachable() assert(0)
#else
#define unreachable __builtin_unreachable
#endif

// requires w%16 == 0
__attribute__((hot))
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
}

#include "palblit_mmxsse.h"

#endif
