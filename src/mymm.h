#ifndef MYMM_H
#define MYMM_H

#ifdef __MMX__
#	include <mmintrin.h>
#	ifdef __SSE__
#		include <xmmintrin.h>
#	elif defined(__3dNOW__)
// available on athlon (3dnow) but not in the header for it (these are the intel intrinsic function names)
#		include <mm3dnow.h>
static __inline __m64 __attribute__((__gnu_inline__, __always_inline__, __artificial__))
_mm_max_pi16 (__m64 __A, __m64 __B) { return (__m64) __builtin_ia32_pmaxsw ((__v4hi)__A, (__v4hi)__B); }
extern __inline __m64 __attribute__((__gnu_inline__, __always_inline__, __artificial__))
_mm_shuffle_pi16 (__m64 __A, int const __N) { return (__m64) __builtin_ia32_pshufw ((__v4hi)__A, __N); }
#	endif
#endif

#endif
