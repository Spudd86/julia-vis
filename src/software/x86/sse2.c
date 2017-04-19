
#if (__x86_64__ || __i386__) && !defined(DISABLE_X86_INTRIN)
#pragma GCC target("no-sse3,sse2")
#ifndef DEBUG
#pragma GCC optimize "3,inline-functions,merge-all-constants"
#endif

#include "common.h"
#include "../pixmisc.h"
#include <mmintrin.h>
#include <xmmintrin.h>
#include <emmintrin.h>

#include <assert.h>

#ifndef NDEBUG
#define unreachable() assert(0)
#else
#define unreachable __builtin_unreachable
#endif

// requires w%16 == 0 && h%16 == 0
__attribute__((hot))
void maxblend_sse2(void *restrict dest, const void *restrict src, int w, int h)
{
	__m128i *restrict mbdst = dest; const __m128i *restrict mbsrc = src;
	const __m128i off = _mm_set1_epi16(0x8000);
	const size_t npix = (size_t)w*(size_t)h;

	_mm_prefetch(mbdst, _MM_HINT_NTA);
	_mm_prefetch(mbsrc, _MM_HINT_NTA);
	for(size_t i=0; i < npix; i+=32, mbdst+=4, mbsrc+=4) {
		_mm_prefetch(mbdst + 4, _MM_HINT_NTA);
		_mm_prefetch(mbsrc + 4, _MM_HINT_NTA);

		__m128i v1, v2, t1, t2, v3, t3, v4, t4;

		v1 = _mm_load_si128(mbdst+0), t1 = _mm_load_si128(mbsrc+0);
		v1 = _mm_add_epi16(v1, off); t1 = _mm_add_epi16(t1, off);
		v1 = _mm_max_epi16(v1, t1);
		v1 = _mm_sub_epi16(v1, off);
		_mm_store_si128(mbdst+0, v1);

		v2 = _mm_load_si128(mbdst+1), t2 = _mm_load_si128(mbsrc+1);
		v2 = _mm_add_epi16(v2, off); t2 = _mm_add_epi16(t2, off);
		v2 = _mm_max_epi16(v2, t2);
		v2 = _mm_sub_epi16(v2, off);
		_mm_store_si128(mbdst+1, v2);

		v3 = _mm_load_si128(mbdst+2), t3 = _mm_load_si128(mbsrc+2);
		v3 = _mm_add_epi16(v3, off); t3 = _mm_add_epi16(t3, off);
		v3 = _mm_max_epi16(v3, t3);
		v3 = _mm_sub_epi16(v3, off);
		_mm_store_si128(mbdst+2, v3);

		v4 = _mm_load_si128(mbdst+3), t4 = _mm_load_si128(mbsrc+3);
		v4 = _mm_add_epi16(v4, off); t4 = _mm_add_epi16(t4, off);
		v4 = _mm_max_epi16(v4, t4);
		v4 = _mm_sub_epi16(v4, off);
		_mm_store_si128(mbdst+3, v4);
	}
	_mm_mfence(); // needed because of the non-temporal stores.
}

//#define pb32_load_v(s) (_mm_setr_epi16(*((s)+0), *((s)+0), *((s)+0), *((s)+0), *((s)+1), *((s)+1), *((s)+1), *((s)+1)))
static inline __attribute__((__always_inline__, __artificial__))
__m128i pb32_load_v(const uint16_t *restrict s) {
	// yes this REALLY is faster than calling _mm_setr_epi16
	__m128i r = _mm_set_epi32(0,0,0,*(const uint32_t *)(s)); // compiler WILL actually fold this to down a single movd instruction
	r = _mm_shuffle_epi32(r, _MM_SHUFFLE(3,0,1,0)); // pass through values we don't care about, clang changed our mask to this when we used 0 so probably an optimisation
	r = _mm_shufflelo_epi16(r, 0);
	r = _mm_shufflehi_epi16(r, _MM_SHUFFLE(1,1,1,1));
	return r;
}

#if 1
__attribute__((hot))
void pallet_blit32_sse2(uint8_t *restrict dest, unsigned int dst_stride,
                        const uint16_t *restrict src, unsigned int src_stride,
                        unsigned int w, unsigned int h,
                        const uint32_t *restrict pal)
{
	//FIXME: add src_stride back in
	static const __m128i zero  = (const __m128i){0ll, 0ll}; // _mm_setzero_si128();
	static const __m128i mask  = (const __m128i){0x00ff00ff00ff00ffll, 0x00ff00ff00ff00ffll}; // _mm_set1_epi16(0xff);
	static const __m128i sub   = (const __m128i){0x0100010001000100ll, 0x0100010001000100ll}; // _mm_set1_epi16(256);

	for(size_t y = 0; y < h; y++) {
		const uint16_t *restrict s = src + y*w; // for some reason actually using src_sride is a lot slower (like 10FPS) maybe because it's doing a transformation of some kind...
		_mm_prefetch(s, _MM_HINT_NTA);

		uint32_t *restrict d = (uint32_t *restrict)(dest + y*dst_stride);
		size_t x = 0;

		// need to align our destination address so we can use non-temporal writes
		// assume that the pixels are at least aligned to 4 bytes
		uintptr_t dp = (uintptr_t)d;
		dp = 4 - ((dp%16)/4);
		if(dp) {
			const __m128i move_mask = _mm_cmplt_epi32(_mm_setr_epi32(0, 1, 2, 3), _mm_set1_epi32(dp));
			__m128i col1, col2, col3, col4, v1, v2, v1s, v2s;

			col1 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[0]/256u)));
			col2 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[1]/256u)));
			v1   = pb32_load_v(s+0);
			v1   = _mm_and_si128(v1, mask);

			col1 = _mm_unpacklo_epi32(col1, col2);
			col2 = col1;
			col1 = _mm_unpacklo_epi8(col1, zero);
			col2 = _mm_unpackhi_epi8(col2, zero);

			col2 = _mm_mullo_epi16(col2, v1);
			v1s  = _mm_sub_epi16(sub, v1);
			col1 = _mm_mullo_epi16(col1, v1s);
			col1 = _mm_add_epi16(col1, col2);
			col1 = _mm_srli_epi16(col1, 8);


			col3 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[2]/256u)));
			col4 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[3]/256u)));
			v2   = pb32_load_v(s+2);
			v2   = _mm_and_si128(v2, mask);

			col3 = _mm_unpacklo_epi32(col3, col4);
			col4 = col3;
			col3 = _mm_unpacklo_epi8(col3, zero);
			col4 = _mm_unpackhi_epi8(col4, zero);

			col4 = _mm_mullo_epi16(col4, v2);
			v2s  = _mm_sub_epi16(sub, v2);
			col3 = _mm_mullo_epi16(col3, v2s);
			col3 = _mm_add_epi16(col3, col4);
			col3 = _mm_srli_epi16(col3, 8);


			col1 = _mm_packus_epi16(col1, col3);
			_mm_maskmoveu_si128(col1, move_mask, (char *restrict)(d + 0));
			x+=dp;
			d+=dp;
			s+=dp;
		}

		// want to go 8 pixels at a time because that way our
		// prefetches line up nicely, the prefetch is guaranteed to do 32 bytes
		// but might do more. We'll just assume the minimum that we get decent
		// performance even on old systems with smaller cache lines
		for(; x + 8 < w; x+=8, s+=8, d+=8) {
			_mm_prefetch(s + 8, _MM_HINT_NTA);
			__m128i col1, col2, col3, col4, v1, v2, v1s, v2s;

			col1 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[0]/256u)));
			col2 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[1]/256u)));
			v1   = pb32_load_v(s+0);
			v1   = _mm_and_si128(v1, mask);

			col1 = _mm_unpacklo_epi32(col1, col2);
			col2 = col1;
			col1 = _mm_unpacklo_epi8(col1, zero);
			col2 = _mm_unpackhi_epi8(col2, zero);

			col2 = _mm_mullo_epi16(col2, v1);
			v1s  = _mm_sub_epi16(sub, v1);
			col1 = _mm_mullo_epi16(col1, v1s);
			col1 = _mm_add_epi16(col1, col2);
			col1 = _mm_srli_epi16(col1, 8);


			col3 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[2]/256u)));
			col4 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[3]/256u)));
			v2   = pb32_load_v(s+2);
			v2   = _mm_and_si128(v2, mask);

			col3 = _mm_unpacklo_epi32(col3, col4);
			col4 = col3;
			col3 = _mm_unpacklo_epi8(col3, zero);
			col4 = _mm_unpackhi_epi8(col4, zero);

			col4 = _mm_mullo_epi16(col4, v2);
			v2s  = _mm_sub_epi16(sub, v2);
			col3 = _mm_mullo_epi16(col3, v2s);
			col3 = _mm_add_epi16(col3, col4);
			col3 = _mm_srli_epi16(col3, 8);


			col1 = _mm_packus_epi16(col1, col3);
			_mm_stream_si128((__m128i *)(d + 0), col1);
			//_mm_storeu_si128((__m128i *restrict)(d + 0), col1);

			col1 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[4]/256u)));
			col2 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[5]/256u)));
			v1   = pb32_load_v(s+4);
			v1   = _mm_and_si128(v1, mask);

			col1 = _mm_unpacklo_epi32(col1, col2);
			col2 = col1;
			col1 = _mm_unpacklo_epi8(col1, zero);
			col2 = _mm_unpackhi_epi8(col2, zero);

			col2 = _mm_mullo_epi16(col2, v1);
			v1s  = _mm_sub_epi16(sub, v1);
			col1 = _mm_mullo_epi16(col1, v1s);
			col1 = _mm_add_epi16(col1, col2);
			col1 = _mm_srli_epi16(col1, 8);


			col3 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[6]/256u)));
			col4 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[7]/256u)));
			v2   = pb32_load_v(s+6);
			v2   = _mm_and_si128(v2, mask);

			col3 = _mm_unpacklo_epi32(col3, col4);
			col4 = col3;
			col3 = _mm_unpacklo_epi8(col3, zero);
			col4 = _mm_unpackhi_epi8(col4, zero);

			col4 = _mm_mullo_epi16(col4, v2);
			v2s  = _mm_sub_epi16(sub, v2);
			col3 = _mm_mullo_epi16(col3, v2s);
			col3 = _mm_add_epi16(col3, col4);
			col3 = _mm_srli_epi16(col3, 8);


			col1 = _mm_packus_epi16(col1, col3);
			_mm_stream_si128((__m128i *)(d + 4), col1);
			//_mm_storeu_si128((__m128i *restrict)(d + 4), col1);

			// would it be faster to just do the maskmove here and not bother with
			// the stuff outside the loop?
			// It'd be a lot less code but we'd be adding
			// at least two extra instructions inside the loop to do the add and compare
			// plus extra register use, which on 32 bit means we need to spill even more
			// than we already do.
		}

		// we'll never have less than 4 pixels to do at the end because w is
		// always divisible by 16 (and therefore 8, so since we only did 0 - 3
		// pixels to force alignment and the loop does 8 pixels per iteration
		// we end up with 4 - 8 pixels that need doing at loop exit)
		// so only the second set of four needs to use a maskmove

		__m128i col1, col2, col3, col4, v1, v2, v1s, v2s;

		col1 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[0]/256u)));
		col2 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[1]/256u)));
		v1   = pb32_load_v(s+0);
		v1   = _mm_and_si128(v1, mask);

		col1 = _mm_unpacklo_epi32(col1, col2);
		col2 = col1;
		col1 = _mm_unpacklo_epi8(col1, zero);
		col2 = _mm_unpackhi_epi8(col2, zero);

		col2 = _mm_mullo_epi16(col2, v1);
		v1s  = _mm_sub_epi16(sub, v1);
		col1 = _mm_mullo_epi16(col1, v1s);
		col1 = _mm_add_epi16(col1, col2);
		col1 = _mm_srli_epi16(col1, 8);


		col3 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[2]/256u)));
		col4 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[3]/256u)));
		v2   = pb32_load_v(s+2);
		v2   = _mm_and_si128(v2, mask);

		col3 = _mm_unpacklo_epi32(col3, col4);
		col4 = col3;
		col3 = _mm_unpacklo_epi8(col3, zero);
		col4 = _mm_unpackhi_epi8(col4, zero);

		col4 = _mm_mullo_epi16(col4, v2);
		v2s  = _mm_sub_epi16(sub, v2);
		col3 = _mm_mullo_epi16(col3, v2s);
		col3 = _mm_add_epi16(col3, col4);
		col3 = _mm_srli_epi16(col3, 8);


		col1 = _mm_packus_epi16(col1, col3);
		_mm_stream_si128((__m128i *)(d + 0), col1);

		if(w-(x+4)) { // should be the same as 4-dp
		//if(4-dp) {
			const __m128i move_mask = _mm_cmplt_epi32(_mm_setr_epi32(0, 1, 2, 3), _mm_set1_epi32(w-(x+4)));
			//const __m128i move_mask = _mm_cmplt_epi32(_mm_setr_epi32(0, 1, 2, 3), _mm_set1_epi32(4-dp));

			col1 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[4]/256u)));
			col2 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[5]/256u)));
			v1   = pb32_load_v(s+4);
			v1   = _mm_and_si128(v1, mask);

			col1 = _mm_unpacklo_epi32(col1, col2);
			col2 = col1;
			col1 = _mm_unpacklo_epi8(col1, zero);
			col2 = _mm_unpackhi_epi8(col2, zero);

			col2 = _mm_mullo_epi16(col2, v1);
			v1s  = _mm_sub_epi16(sub, v1);
			col1 = _mm_mullo_epi16(col1, v1s);
			col1 = _mm_add_epi16(col1, col2);
			col1 = _mm_srli_epi16(col1, 8);


			col3 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[6]/256u)));
			col4 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[7]/256u)));
			v2   = pb32_load_v(s+6);
			v2   = _mm_and_si128(v2, mask);

			col3 = _mm_unpacklo_epi32(col3, col4);
			col4 = col3;
			col3 = _mm_unpacklo_epi8(col3, zero);
			col4 = _mm_unpackhi_epi8(col4, zero);

			col4 = _mm_mullo_epi16(col4, v2);
			v2s  = _mm_sub_epi16(sub, v2);
			col3 = _mm_mullo_epi16(col3, v2s);
			col3 = _mm_add_epi16(col3, col4);
			col3 = _mm_srli_epi16(col3, 8);


			col1 = _mm_packus_epi16(col1, col3);
			_mm_maskmoveu_si128(col1, move_mask, (char *restrict)(d + 4));
		}
	}
#if !__x86_64__
	_mm_empty();
#endif
	_mm_mfence(); // needed because of the non-temporal stores.
}
#else

// *******************************************************************************************************************
// **************************************** Simple unaligned accesses version ****************************************
// mostly here for reference and comparison.
__attribute__((hot))
void pallet_blit32_sse2(uint8_t *restrict dest, unsigned int dst_stride,
                        const uint16_t *restrict src, unsigned int src_stride,
                        unsigned int w, unsigned int h,
                        const uint32_t *restrict pal)
{
	static const __m128i zero = (const __m128i){0ll, 0ll}; // _mm_setzero_si128();
	static const __m128i mask = (const __m128i){0x00ff00ff00ff00ffll, 0x00ff00ff00ff00ffll}; // _mm_set1_epi16(0xff);
	static const __m128i sub  = (const __m128i){0x0100010001000100ll, 0x0100010001000100ll}; // _mm_set1_epi16(256);

	for(size_t y = 0; y < h; y++) {
		const uint16_t *restrict s = src + y*w;
		_mm_prefetch(s, _MM_HINT_NTA);
		uint32_t *restrict d = (uint32_t *restrict)(dest + y*dst_stride);
		size_t x = 0;

		for(; x < w; x+=4, s+=4, d+=4) {
			_mm_prefetch(s+4, _MM_HINT_NTA);
			__m128i col1, col2, col3, col4, v1, v2, v1s, v2s;

			col1 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[0]/256u)));
			col2 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[1]/256u)));
			col1 = _mm_unpacklo_epi32(col1, col2);
			col2 = col1;
			col1 = _mm_unpacklo_epi8(col1, zero);
			col2 = _mm_unpackhi_epi8(col2, zero);

			v1   = pb32_load_v(s+0);
			v1   = _mm_and_si128(v1, mask);
			col2 = _mm_mullo_epi16(col2, v1);
			v1s  = _mm_sub_epi16(sub, v1);
			col1 = _mm_mullo_epi16(col1, v1s);
			col1 = _mm_add_epi16(col1, col2);
			col1 = _mm_srli_epi16(col1, 8);

			col1 = _mm_packus_epi16(col1, zero);


			col3 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[2]/256u)));
			col4 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[3]/256u)));

			col3 = _mm_unpacklo_epi32(col3, col4);
			col4 = col3;
			col3 = _mm_unpacklo_epi8(col3, zero);
			col4 = _mm_unpackhi_epi8(col4, zero);

			v2   = pb32_load_v(s+2);
			v2   = _mm_and_si128(v2, mask);
			col4 = _mm_mullo_epi16(col4, v2);
			v2s  = _mm_sub_epi16(sub, v2);
			col3 = _mm_mullo_epi16(col3, v2s);
			col3 = _mm_add_epi16(col3, col4);
			col3 = _mm_srli_epi16(col3, 8);

			col3 = _mm_packus_epi16(col3, zero);

			_mm_stream_si128((__m128i *)(d + 0), _mm_unpacklo_epi64(col1, col3));
			//_mm_storeu_si128((__m128i *restrict)(d + 0), _mm_unpacklo_epi64(col1, col3));
		}
	}
	_mm_empty();
	_mm_mfence(); // needed because of the non-temporal stores.
}
#endif

#endif
