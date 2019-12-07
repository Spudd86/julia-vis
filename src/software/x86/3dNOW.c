#if (__i386__) && !defined(DISABLE_X86_INTRIN)

#pragma GCC target("no-sse,athlon,3dnow")
#pragma GCC optimize "3,inline-functions"
// Want -fmerge-all-constants but we can't put it in the optimize pragma for some reason

#include "common.h"
#include "../pixmisc.h"
#include <mmintrin.h>
#include <mm3dnow.h>

static inline __attribute__((__always_inline__, __artificial__))
__m64 _mm_max_pi16(__m64 a, __m64 b) {
	return (__m64)__builtin_ia32_pmaxsw((v4hi)a, (v4hi)b);
}

// should be the same as the sse version but split out just to be sure gcc doesn't generate any sse only instructions
// plus lets us use femms which is faster on some AMD CPUs (also tunes for athalons instead of generic)
__attribute__((hot))
void maxblend_3dnow(void *restrict dest, const void *restrict src, int w, int h)
{
	__m64 *mbdst = dest; const __m64 *mbsrc = src;
	const __m64 off = _mm_set1_pi16(0x8000);
	__builtin_prefetch(mbdst, 0, 0);
	__builtin_prefetch(mbsrc, 0, 0);
	for(unsigned int i=0; i < 2*w*h/sizeof(__m64); i+=4, mbdst+=4, mbsrc+=4) {
		__builtin_prefetch(mbdst + 4, 0, 0);
		__builtin_prefetch(mbsrc + 4, 0, 0);

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
	__builtin_ia32_sfence();
	_m_femms();
}

#include "palblit_mmxsse.h"

#endif