
#include "common.h"
#include "pixmisc.h"

#ifdef HAVE_ORC
#include <orc/orc.h>
void maxblend(void *restrict dest, const void *restrict src, int w, int h)
{
	static OrcProgram *p = NULL;
	if (p == NULL) {
		p = orc_program_new_ds(2,2);
		orc_program_append_str(p, "maxuw", "d1", "d1", "s1");
		orc_program_compile (p);  //TODO: check return value here
	}

	OrcExecutor _ex;
	OrcExecutor *ex = &_ex;
	orc_executor_set_program (ex, p);
	orc_executor_set_n (ex, w*h);
	orc_executor_set_array (ex, ORC_VAR_S1, src);
	orc_executor_set_array (ex, ORC_VAR_D1, dest);
	orc_executor_run (ex);
}

#elif defined(__SSE2__)
#include "mymm.h"
#include <emmintrin.h>
#warning Doing sse2 Compiled program will NOT work on system without it!

// requires w%16 == 0
void maxblend(void *restrict dest, const void *restrict src, int w, int h)
{
	__m128i * restrict mbdst = dest; const __m128i * restrict mbsrc = src;
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
#elif defined(__SSE__) || defined(__3dNOW__)
#include "mymm.h"
void maxblend(void *restrict dest, const void *restrict src, int w, int h)
{
	__m64 *mbdst = dest; const __m64 *mbsrc = src;
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
void maxblend(void *restrict dest, const void *restrict src, int w, int h)
{
	const int n = w*h;
	uint16_t *restrict d=dest; const uint16_t *restrict s=src;
	for(int i=0; i<n; i++)
		d[i] = MAX(d[i], s[i]);
}
#endif
