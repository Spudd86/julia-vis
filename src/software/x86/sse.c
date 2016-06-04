
#if (__x86_64__ || __i386__)

#pragma GCC target("no-sse2")

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

__attribute__((hot))
void pallet_blit32_sse(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal)
{
	const __m64 zero = (__m64)(0ll);
	const __m64 mask = (__m64)(0x00ff00ff00ffll);
	const __m64 sub = (__m64)(0x010001000100ll);

	//FIXME: add src_stride back in
	for(size_t y = 0; y < h; y++) {
		const uint16_t *restrict s = src + y*w;
		uint32_t *restrict d = (uint32_t *restrict)(dest + y*dst_stride);
		for(size_t x = 0; x < w; x+=4, s+=4, d+=4) {
			uint16_t v = s[0];

			__m64 col1 = *(const __m64 *)(pal+(v/256u));
			__m64 col2 = col1;
			col1 = _mm_unpacklo_pi8(col1, zero);
			col2 = _mm_unpackhi_pi8(col2, zero);

			//col1 = (col2*v + col1*(256-v))/256;
			__m64 vt = _mm_set1_pi16(v);
			vt = _mm_and_si64(vt, mask);
			col2 = _mm_mullo_pi16(col2, vt);
			vt = _mm_subs_pu16(sub, vt);
			col1 = _mm_mullo_pi16(col1, vt);
			col1 = _mm_add_pi16(col1, col2);
			col1 = _mm_srli_pi16(col1, 8);

			__m64 tmp = col1;

			v = s[1];
			col1 = *(const __m64 *)(pal+(v/256u));
			col2 = col1;
			col1 = _mm_unpacklo_pi8(col1, zero);
			col2 = _mm_unpackhi_pi8(col2, zero);

			vt = _mm_set1_pi16(v);
			vt = _mm_and_si64(vt, mask);
			col2 = _mm_mullo_pi16(col2, vt);
			vt = _mm_subs_pu16(sub, vt);
			col1 = _mm_mullo_pi16(col1, vt);
			col1 = _mm_add_pi16(col1, col2);
			col1 = _mm_srli_pi16(col1, 8);

			tmp = _mm_packs_pu16(tmp, col1);
			_mm_stream_pi((__m64 *)(d+0), tmp); // sse1

			v = s[2];
			col1 = *(const __m64 *)(pal+(v/256u));
			col2 = col1;
			col1 = _mm_unpacklo_pi8(col1, zero);
			col2 = _mm_unpackhi_pi8(col2, zero);

			vt = _mm_set1_pi16(v);
			vt = _mm_and_si64(vt, mask);
			col2 = _mm_mullo_pi16(col2, vt);
			vt = _mm_subs_pu16(sub, vt);
			col1 = _mm_mullo_pi16(col1, vt);
			col1 = _mm_add_pi16(col1, col2);
			col1 = _mm_srli_pi16(col1, 8);

			tmp = col1;

			v = s[3];
			col1 = *(const __m64 *)(pal+(v/256u));
			col2 = col1;
			col1 = _mm_unpacklo_pi8(col1, zero);
			col2 = _mm_unpackhi_pi8(col2, zero);

			vt = _mm_set1_pi16(v);
			vt = _mm_and_si64(vt, mask);
			col2 = _mm_mullo_pi16(col2, vt);
			vt = _mm_subs_pu16(sub, vt);
			col1 = _mm_mullo_pi16(col1, vt);
			col1 = _mm_add_pi16(col1, col2);
			col1 = _mm_srli_pi16(col1, 8);

			tmp = _mm_packs_pu16(tmp, col1);
			_mm_stream_pi((__m64 *)(d+2), tmp); // sse1
		}
	}

	_mm_empty();
	//_mm_sfence(); // needed because of the non-temporal stores. (sse/mmxext)
}

__attribute__((hot))
void pallet_blit565_sse(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal)
{
	// dither tables from // http://stackoverflow.com/a/17438757
	static const uint8_t dither_thresh_r[] = {
		1, 7, 3, 5, 0, 8, 2, 6,
		7, 1, 5, 3, 8, 0, 6, 2,
		3, 5, 0, 8, 2, 6, 1, 7,
		5, 3, 8, 0, 6, 2, 7, 1,
		0, 8, 2, 6, 1, 7, 3, 5,
		8, 0, 6, 2, 7, 1, 5, 3,
		2, 6, 1, 7, 3, 5, 0, 8,
		6, 2, 7, 1, 5, 3, 8, 0
	};

	static const uint8_t dither_thresh_g[] = {
		1, 3, 2, 2, 3, 1, 2, 2,
		2, 2, 0, 4, 2, 2, 4, 0,
		3, 1, 2, 2, 1, 3, 2, 2,
		2, 2, 4, 0, 2, 2, 0, 4,
		1, 3, 2, 2, 3, 1, 2, 2,
		2, 2, 0, 4, 2, 2, 4, 0,
		3, 1, 2, 2, 1, 3, 2, 2,
		2, 2, 4, 0, 2, 2, 0, 4
	};

	static const uint8_t dither_thresh_b[] = {
		5, 3, 8, 0, 6, 2, 7, 1,
		3, 5, 0, 8, 2, 6, 1, 7,
		8, 0, 6, 2, 7, 1, 5, 3,
		0, 8, 2, 6, 1, 7, 3, 5,
		6, 2, 7, 1, 5, 3, 8, 0,
		2, 6, 1, 7, 3, 5, 0, 8,
		7, 1, 5, 3, 8, 0, 6, 2,
		1, 7, 3, 5, 0, 8, 2, 6
	};

	// TODO: could skip the second mask if red was in the low byte in the
	// pallet since we don't need to mask blue because we're going to shift
	// it right to get it in the correct bit position

	//TODO: interopolate colours and dither?

	const __m64 mask = (__m64)(0xfcfcfcfcf8f8f8f8ll);
	const __m64 zero = (__m64)(0ll);
	for(unsigned int y = 0; y < h; y++) {
		const uint16_t *restrict s = src + y*src_stride;
		__m64 *restrict d = (__m64 *restrict)(dest + y*dst_stride);

		const __m64 *dith_r = (const __m64 *)(dither_thresh_r + ((y & 7) << 3));
		const __m64 *dith_g = (const __m64 *)(dither_thresh_g + ((y & 7) << 3));
		const __m64 *dith_b = (const __m64 *)(dither_thresh_b + ((y & 7) << 3));

		for(unsigned int x = 0; x < w; x+=8, s+=8, d+=2) {
			__m64 c0, c1, c2, c3, bg, r, g, b;

			c0 = _mm_cvtsi32_si64(pal[s[0]/256u]);
			c1 = _mm_cvtsi32_si64(pal[s[1]/256u]);
			c2 = _mm_cvtsi32_si64(pal[s[2]/256u]);
			c3 = _mm_cvtsi32_si64(pal[s[3]/256u]);

			c0 = _mm_unpacklo_pi8(c0, c1);  // c0 = bbggrrxx
			c1 = _mm_unpacklo_pi8(c2, c3);  // c1 = bbggrrxx
			bg = _mm_unpacklo_pi16(c0, c1); // rg = bbbbgggg
			r  = _mm_unpackhi_pi16(c0, c1); // r  = rrrrxxxx

			// TODO: merge dither tables in an optimised way
			// for this lookup
			bg = _mm_adds_pu8(bg, _mm_unpacklo_pi32(*dith_b, *dith_g));
			r  = _mm_adds_pu8(r,  *dith_r);

			bg = _mm_and_si64(bg, mask); // mask of low bits we don't want
			r  = _mm_and_si64(r,  mask);

			b = _mm_unpacklo_pi8(bg, zero);
			g = _mm_unpackhi_pi8(bg, zero);
			r = _mm_unpacklo_pi8(r,  zero);

			r = _mm_slli_pi16(r, 8);
			g = _mm_slli_pi16(g, 3);
			b = _mm_srli_pi16(b, 3);

			r = _mm_or_si64(r, g);
			r = _mm_or_si64(r, b);
			_mm_stream_pi(d + 0, r); // sse/mmxext

			c0 = _mm_cvtsi32_si64(pal[s[4]/256u]);
			c1 = _mm_cvtsi32_si64(pal[s[5]/256u]);
			c2 = _mm_cvtsi32_si64(pal[s[6]/256u]);
			c3 = _mm_cvtsi32_si64(pal[s[7]/256u]);

			c0 = _mm_unpacklo_pi8(c0, c1);  // c0 = bbggrrxx
			c1 = _mm_unpacklo_pi8(c2, c3);  // c1 = bbggrrxx
			bg = _mm_unpacklo_pi16(c0, c1); // rg = bbbbgggg
			r  = _mm_unpackhi_pi16(c0, c1); // r  = rrrrxxxx

			// TODO: merge dither tables in an optimised way
			// for this lookup
			bg = _mm_adds_pu8(bg, _mm_unpackhi_pi32(*dith_b, *dith_g));
			r  = _mm_adds_pu8(r,  _mm_srli_si64(*dith_r, 32));

			bg = _mm_and_si64(bg, mask); // mask of low bits we don't want
			r  = _mm_and_si64(r,  mask);

			b = _mm_unpacklo_pi8(bg, zero);
			g = _mm_unpackhi_pi8(bg, zero);
			r = _mm_unpacklo_pi8(r,  zero);

			r = _mm_slli_pi16(r, 8);
			g = _mm_slli_pi16(g, 3);
			b = _mm_srli_pi16(b, 3);

			r = _mm_or_si64(r, g);
			r = _mm_or_si64(r, b);
			_mm_stream_pi(d + 1, r); // sse/mmxext
		}
	}

	//_mm_sfence(); // needed because of the non-temporal stores. (sse/mmxext)
	_mm_empty();
}

__attribute__((hot))
void pallet_blit555_sse(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal)
{
	static const uint8_t dither_thresh_r[] = {
		0, 5, 1, 7, 0, 6, 2, 7,
		2, 4, 3, 5, 2, 4, 3, 5,
		0, 6, 1, 6, 1, 6, 1, 6,
		2, 4, 3, 4, 3, 4, 3, 5,
		0, 5, 1, 7, 0, 5, 2, 7,
		2, 4, 3, 5, 2, 4, 3, 5,
		1, 6, 1, 6, 1, 6, 1, 6,
		2, 4, 3, 4, 2, 4, 3, 5
	};

	// TODO: find a better table for this green
	static const uint8_t dither_thresh_g[] = {
		2, 4, 3, 4, 2, 4, 3, 5,
		6, 1, 6, 1, 6, 1, 6, 1,
		3, 5, 2, 4, 3, 5, 2, 4,
		7, 1, 5, 0, 7, 2, 5, 0,
		3, 4, 3, 5, 2, 4, 3, 4,
		6, 1, 6, 1, 6, 0, 6, 1,
		3, 5, 2, 4, 3, 5, 2, 4,
		7, 2, 6, 0, 7, 1, 5, 0
	};

	static const uint8_t dither_thresh_b[] = {
		5, 1, 4, 0, 4, 1, 4, 0,
		3, 6, 2, 5, 3, 6, 2, 5,
		4, 1, 5, 2, 4, 0, 5, 1,
		2, 6, 3, 7, 2, 6, 3, 7,
		4, 1, 4, 0, 5, 1, 4, 0,
		3, 6, 2, 5, 3, 6, 2, 6,
		4, 1, 5, 1, 4, 1, 5, 2,
		2, 6, 3, 7, 3, 6, 3, 7
	};

	// TODO: could skip the second mask if red was in the low byte in the
	// pallet since we don't need to mask blue because we're going to shift
	// it right to get it in the correct bit position

	//TODO: interopolate colours and dither?

	const __m64 mask = _mm_set1_pi8(0xf8);
	const __m64 zero = (__m64)(0ll);
	for(unsigned int y = 0; y < h; y++) {

		const __m64 *dith_r = (const __m64 *)(dither_thresh_r + ((y & 7) << 3));
		const __m64 *dith_g = (const __m64 *)(dither_thresh_g + ((y & 7) << 3));
		const __m64 *dith_b = (const __m64 *)(dither_thresh_b + ((y & 7) << 3));

		const uint16_t *restrict s = src + y*src_stride;
		__m64 *restrict d = (__m64 *restrict)(dest + y*dst_stride);

		// TODO: deal with w%16 != 0
		_mm_prefetch((const __m64 *restrict)s, _MM_HINT_NTA);
		for(unsigned int x = 0; x < w; x+=8, s+=8, d+=2) {
			_mm_prefetch((const __m64 *restrict)(s+8), _MM_HINT_NTA);
			__m64 c0, c1, c2, c3, bg, r, g, b;

			c0 = _mm_cvtsi32_si64(pal[s[0]/256u]);
			c1 = _mm_cvtsi32_si64(pal[s[1]/256u]);
			c2 = _mm_cvtsi32_si64(pal[s[2]/256u]);
			c3 = _mm_cvtsi32_si64(pal[s[3]/256u]);

			c0 = _mm_unpacklo_pi8(c0, c1);  // c0 = bbggrrxx
			c1 = _mm_unpacklo_pi8(c2, c3);  // c1 = bbggrrxx
			bg = _mm_unpacklo_pi16(c0, c1); // rg = bbbbgggg
			r  = _mm_unpackhi_pi16(c0, c1); // r  = rrrrxxxx

			// TODO: merge dither tables in an optimised way
			// for this lookup
			bg = _mm_adds_pu8(bg, _mm_unpacklo_pi32(*dith_b, *dith_g));
			r  = _mm_adds_pu8(r,  *dith_r);

			bg = _mm_and_si64(bg, mask); // mask of low bits we don't want
			r  = _mm_and_si64(r,  mask);

			b = _mm_unpacklo_pi8(bg, zero);
			g = _mm_unpackhi_pi8(bg, zero);
			r = _mm_unpacklo_pi8(r,  zero);

			r = _mm_slli_pi16(r, 7);
			g = _mm_slli_pi16(g, 2);
			b = _mm_srli_pi16(b, 3);

			r = _mm_or_si64(r, g);
			r = _mm_or_si64(r, b);
			_mm_stream_pi(d+0, r); // sse/mmxext

			c0 = _mm_cvtsi32_si64(pal[s[4]/256u]);
			c1 = _mm_cvtsi32_si64(pal[s[5]/256u]);
			c2 = _mm_cvtsi32_si64(pal[s[6]/256u]);
			c3 = _mm_cvtsi32_si64(pal[s[7]/256u]);

			c0 = _mm_unpacklo_pi8(c0, c1);  // c0 = bbggrrxx
			c1 = _mm_unpacklo_pi8(c2, c3);  // c1 = bbggrrxx
			bg = _mm_unpacklo_pi16(c0, c1); // rg = bbbbgggg
			r  = _mm_unpackhi_pi16(c0, c1); // r  = rrrrxxxx

			// TODO: merge dither tables in an optimised way
			// for this lookup
			bg = _mm_adds_pu8(bg, _mm_unpackhi_pi32(*dith_b, *dith_g));
			r  = _mm_adds_pu8(r,  _mm_srli_si64(*dith_r, 32));

			bg = _mm_and_si64(bg, mask); // mask of low bits we don't want
			r  = _mm_and_si64(r,  mask);

			b = _mm_unpacklo_pi8(bg, zero);
			g = _mm_unpackhi_pi8(bg, zero);
			r = _mm_unpacklo_pi8(r,  zero);

			r = _mm_slli_pi16(r, 7);
			g = _mm_slli_pi16(g, 2);
			b = _mm_srli_pi16(b, 3);

			r = _mm_or_si64(r, g);
			r = _mm_or_si64(r, b);
			_mm_stream_pi(d+1, r); // sse/mmxext
		}
	}

	//_mm_sfence(); // needed because of the non-temporal stores. (sse/mmxext)
	_mm_empty();
}

__attribute__((hot)) //TODO: work out how to make it stop using the wrong intrinsics
void pallet_blit8_sse(uint8_t* restrict dest, unsigned int dst_stride,
		const uint16_t *restrict src, unsigned int src_stride,
		unsigned int w, unsigned int h)
{
	for(unsigned int y = 0; y < h; y++) {
		const uint16_t *restrict s = src + y*src_stride;
		__m64 *restrict d = (__m64 *restrict)(dest + y*dst_stride);

		// TODO: deal with w%16 != 0
		_mm_prefetch((const __m64 *restrict)s, _MM_HINT_NTA);
		for(unsigned int x = 0; x < w; x+=16, s+=16, d+=2) {
			_mm_prefetch((const __m64 *restrict)(s+16), _MM_HINT_NTA);

			__m64 p1, p2;

			p1 = *(const __m64 *)(s + 0);
			p1  = _mm_srli_pi16(p1, 8);
			p2 = *(const __m64 *)(s + 4);
			p2  = _mm_srli_pi16(p2, 8);

			p1 = _mm_packs_pu16(p1, p2);
			_mm_stream_pi(d + 0, p1);

			p1 = *(const __m64 *)(s + 8);
			p1  = _mm_srli_pi16(p1, 8);
			p2 = *(const __m64 *)(s + 12);
			p2  = _mm_srli_pi16(p2, 8);

			p1 = _mm_packs_pu16(p1, p2);
			_mm_stream_pi(d + 1, p1);
		}
	}

	_mm_empty();
}

#endif
