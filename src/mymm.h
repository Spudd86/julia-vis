#ifndef MYMM_H
#define MYMM_H

#include <mmintrin.h>
#ifdef __SSE__
#	include <xmmintrin.h>
#else
static __inline __m64 __attribute__((__always_inline__))
_mm_max_pi16 (__m64 __A, __m64 __B) { return (__m64) __builtin_ia32_pmaxsw ((__v4hi)__A, (__v4hi)__B); }
static __inline void __attribute__((__always_inline__))
_mm_stream_pi (__m64 *__P, __m64 __A) { __builtin_ia32_movntq ((unsigned long long *)__P, (unsigned long long)__A); }
#endif

#endif
