
#if (__i386__)

#pragma GCC target("mmx,no-sse")
#pragma GCC optimize "3,inline-functions,merge-all-constants"

#include "common.h"
#include "../pixmisc.h"
#include <mmintrin.h>

#include <assert.h>

#ifndef NDEBUG
#define unreachable() assert(0)
#else
#define unreachable __builtin_unreachable
#endif

__attribute__((hot))
void maxblend_mmx(void *restrict dest, const void *restrict src, int w, int h)
{
	__m64 *mbdst = dest; const __m64 *mbsrc = src;
	const __m64 off = _mm_set1_pi16(0x8000);
	for(unsigned int i=0; i < 2*w*h/sizeof(__m64); i+=4, mbdst+=4, mbsrc+=4) {

		__m64 v1, v2, v3, v4, t1, t2, t3, t4;
		__m64 m1, m2, m3, m4;

		v1 = mbdst[0], t1 = mbsrc[0];
		v1 = _mm_add_pi16(v1, off); t1 = _mm_add_pi16(t1, off);
		m1 = _mm_cmpgt_pi16(v1, t1);
		v1 = _mm_and_si64(v1, m1); // mask out parts of v that are less than t
		t1 = _mm_andnot_si64(m1, t1); // mask out parts of t that are less than v
		v1 = _mm_or_si64(v1, t1); // combine
		v1 = _mm_sub_pi16(v1, off);
		mbdst[0]=v1;

		v2 = mbdst[1], t2 = mbsrc[1];
		v2 = _mm_add_pi16(v2, off); t2 = _mm_add_pi16(t2, off);
		m2 = _mm_cmpgt_pi16(v2, t2);
		v2 = _mm_and_si64(v2, m2); // mask out parts of v that are less than t
		t2 = _mm_andnot_si64(m2, t2); // mask out parts of t that are less than v
		v2 = _mm_or_si64(v2, t2); // combine
		v2 = _mm_sub_pi16(v2, off);
		mbdst[1]=v2;

		v3 = mbdst[2], t3 = mbsrc[2];
		v3 = _mm_add_pi16(v3, off); t3 = _mm_add_pi16(t3, off);
		m3 = _mm_cmpgt_pi16(v3, t3);
		v3 = _mm_and_si64(v3, m3); // mask out parts of v that are less than t
		t3 = _mm_andnot_si64(m3, t3); // mask out parts of t that are less than v
		v3 = _mm_or_si64(v3, t3); // combine
		v3 = _mm_sub_pi16(v3, off);
		mbdst[2]=v3;

		v4 = mbdst[3], t4 = mbsrc[3];
		v4 = _mm_add_pi16(v4, off); t4 = _mm_add_pi16(t4, off);
		m4 = _mm_cmpgt_pi16(v4, t4);
		v4 = _mm_and_si64(v4, m4); // mask out parts of v that are less than t
		t4 = _mm_andnot_si64(m4, t4); // mask out parts of t that are less than v
		v4 = _mm_or_si64(v4, t4); // combine
		v4 = _mm_sub_pi16(v4, off);
		mbdst[3]=v4;
	}
	_mm_empty();
}

#include "palblit_mmxsse.h"

#endif
