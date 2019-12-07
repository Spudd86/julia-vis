#if (__x86_64__ || __i386__) && !defined(DISABLE_X86_INTRIN)
#pragma GCC target("avx2,no-avx512f")

#ifndef DEBUG
#pragma GCC optimize "3,inline-functions" // Want -fmerge-all-constants but we can't put it in the optimize pragma for some reason
// need to shut gcc up about casting away const in normal builds because
// some of the intrinsics that REALLY ought to take const pointer don't
// (mostly _mm_stream_load_si128())
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

#include "common.h"
#include "../pixmisc.h"
#include <mmintrin.h>
#include <xmmintrin.h>
#include <emmintrin.h>
#include <immintrin.h>

// requires w%16 == 0 && h%16 == 0
#if 0
//TODO: which of these is faster?
// for sse4 the one with prefetching is faster, need to figure out why

__attribute__((hot))
void maxblend_avx2(void *restrict dest, const void *restrict src, int w, int h)
{
	__m256i * restrict mbdst = dest; __m256i * restrict mbsrc = (__m256i *)src;
	const size_t npix = (size_t)w*(size_t)h;
	for(size_t i=0; i < npix; i+=128, mbdst+=8, mbsrc+=8) { // can step 128 because w%16 == 0 && h%16 == 0 -> (w*h)%256 == 0 
		_mm256_stream_si256(mbdst + 0, _mm256_max_epu16(_mm256_stream_load_si256(mbdst + 0), _mm256_stream_load_si256(mbsrc + 0)));
		_mm256_stream_si256(mbdst + 1, _mm256_max_epu16(_mm256_stream_load_si256(mbdst + 1), _mm256_stream_load_si256(mbsrc + 1)));
		_mm256_stream_si256(mbdst + 2, _mm256_max_epu16(_mm256_stream_load_si256(mbdst + 2), _mm256_stream_load_si256(mbsrc + 2)));
		_mm256_stream_si256(mbdst + 3, _mm256_max_epu16(_mm256_stream_load_si256(mbdst + 3), _mm256_stream_load_si256(mbsrc + 3)));
		_mm256_stream_si256(mbdst + 4, _mm256_max_epu16(_mm256_stream_load_si256(mbdst + 4), _mm256_stream_load_si256(mbsrc + 4)));
		_mm256_stream_si256(mbdst + 5, _mm256_max_epu16(_mm256_stream_load_si256(mbdst + 5), _mm256_stream_load_si256(mbsrc + 5)));
		_mm256_stream_si256(mbdst + 6, _mm256_max_epu16(_mm256_stream_load_si256(mbdst + 6), _mm256_stream_load_si256(mbsrc + 6)));
		_mm256_stream_si256(mbdst + 7, _mm256_max_epu16(_mm256_stream_load_si256(mbdst + 7), _mm256_stream_load_si256(mbsrc + 7)));
	}
	_mm_mfence(); // needed because of the non-temporal stores.
}
#else
// TODO: since we have 256 vectors can we assume that cache lines are at least 64 bytes and use fewer
// prefetch instructions?
__attribute__((hot))
void maxblend_avx2(void *restrict dest, const void *restrict src, int w, int h)
{
	__m256i * restrict mbdst = dest; const __m256i * restrict mbsrc = src;
	_mm_prefetch(mbdst + 0, _MM_HINT_NTA);
	_mm_prefetch(mbdst + 1, _MM_HINT_NTA);
	_mm_prefetch(mbdst + 2, _MM_HINT_NTA);
	_mm_prefetch(mbdst + 3, _MM_HINT_NTA);
	_mm_prefetch(mbdst + 4, _MM_HINT_NTA);
	_mm_prefetch(mbdst + 5, _MM_HINT_NTA);
	_mm_prefetch(mbdst + 6, _MM_HINT_NTA);
	_mm_prefetch(mbdst + 7, _MM_HINT_NTA);

	const size_t npix = (size_t)w*(size_t)h;
	for(size_t i=0; i < npix; i+=128, mbdst+=8, mbsrc+=8) { // can step 128 because w%16 == 0 && h%16 == 0 -> (w*h)%256 == 0
		__builtin_prefetch(mbdst +  8, 1, 0);
		__builtin_prefetch(mbdst +  9, 1, 0);
		__builtin_prefetch(mbdst + 10, 1, 0);
		__builtin_prefetch(mbdst + 11, 1, 0);
		__builtin_prefetch(mbdst + 12, 1, 0);
		__builtin_prefetch(mbdst + 13, 1, 0);
		__builtin_prefetch(mbdst + 14, 1, 0);
		__builtin_prefetch(mbdst + 15, 1, 0);
		mbdst[0] = _mm256_max_epu16(mbdst[0], _mm256_stream_load_si256(mbsrc + 0));
		mbdst[1] = _mm256_max_epu16(mbdst[1], _mm256_stream_load_si256(mbsrc + 1));
		mbdst[2] = _mm256_max_epu16(mbdst[2], _mm256_stream_load_si256(mbsrc + 2));
		mbdst[3] = _mm256_max_epu16(mbdst[3], _mm256_stream_load_si256(mbsrc + 3));
		mbdst[4] = _mm256_max_epu16(mbdst[4], _mm256_stream_load_si256(mbsrc + 4));
		mbdst[5] = _mm256_max_epu16(mbdst[5], _mm256_stream_load_si256(mbsrc + 5));
		mbdst[6] = _mm256_max_epu16(mbdst[6], _mm256_stream_load_si256(mbsrc + 6));
		mbdst[7] = _mm256_max_epu16(mbdst[7], _mm256_stream_load_si256(mbsrc + 7));
	}
	_mm_mfence(); // needed because of the non-temporal stores.
}
#endif

//_mm256_set_m128i(
__attribute__((hot))
void pallet_blit32_avx2(uint8_t *restrict dest, unsigned int dst_stride,
                        const uint16_t *restrict src, unsigned int src_stride,
                        unsigned int w, unsigned int h,
                        const uint32_t *restrict pal)
{
	_mm256_zeroupper();

	static const __m128i zero  = (const __m128i){0ll, 0ll}; // _mm_setzero_si128();
	const __m256i sub    = _mm256_set1_epi16(256);

	const __m256i vshuf1 = _mm256_setr_epi8( 0, -1,  // s[0] & 0xff
		                                     0, -1,  // s[0] & 0xff
		                                     0, -1,  // s[0] & 0xff
		                                     0, -1,  // s[0] & 0xff
		                                     2, -1,  // s[1] & 0xff
		                                     2, -1,  // s[1] & 0xff
		                                     2, -1,  // s[1] & 0xff
		                                     2, -1,  // s[1] & 0xff

		                                     4, -1,  // s[2] & 0xff
		                                     4, -1,  // s[2] & 0xff
		                                     4, -1,  // s[2] & 0xff
		                                     4, -1,  // s[2] & 0xff
		                                     6, -1,  // s[3] & 0xff
		                                     6, -1,  // s[3] & 0xff
		                                     6, -1,  // s[3] & 0xff
		                                     6, -1); // s[3] & 0xff

	const __m256i vshuf2 = _mm256_setr_epi8( 8, -1,  // s[4] & 0xff
		                                     8, -1,  // s[4] & 0xff
		                                     8, -1,  // s[4] & 0xff
		                                     8, -1,  // s[4] & 0xff
		                                    10, -1,  // s[5] & 0xff
		                                    10, -1,  // s[5] & 0xff
		                                    10, -1,  // s[5] & 0xff
		                                    10, -1,  // s[5] & 0xff

		                                    12, -1,  // s[6] & 0xff
		                                    12, -1,  // s[6] & 0xff
		                                    12, -1,  // s[6] & 0xff
		                                    12, -1,  // s[6] & 0xff
		                                    14, -1,  // s[7] & 0xff
		                                    14, -1,  // s[7] & 0xff
		                                    14, -1,  // s[7] & 0xff
		                                    14, -1); // s[7] & 0xff


	for(size_t y = 0; y < h; y++) {
		const uint16_t *restrict s = src + y*w;
		_mm_prefetch(s, _MM_HINT_NTA);

		//__m128i old_stream;

		uint32_t *restrict d = (uint32_t *restrict)(dest + y*dst_stride);
		size_t x = 0;

		uintptr_t dp = (uintptr_t)d;
		dp = 8 - ((dp%16)/2);
		if(dp) {
			const __m256i move_mask = _mm256_cmpgt_epi32(_mm256_set1_epi32(dp), _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7));
			__m128i col0, col1, col2, col3, ind1, ind2, v;
			__m256i res, c0, c1, c2, c3, t, v1, v2, v1s, v2s;

			// can stream load here because it's our buffer and the start of a line is always aligned
			v = _mm_stream_load_si128((__m128i *)s); // cast away const to suppress warning
			// old_stream = v;

			ind1 = _mm_srli_epi32(_mm_unpacklo_epi16(v, zero), 8); // look into using _mm_cvtepu16_epi32
			ind2 = _mm_srli_epi32(_mm_unpackhi_epi16(v, zero), 8);

			col0 = _mm_i32gather_epi32((const int32_t *)pal, ind1, 4);
			col1 = _mm_i32gather_epi32((const int32_t *)pal + 1, ind1, 4);

			col2 = _mm_i32gather_epi32((const int32_t *)pal, ind2, 4);
			col3 = _mm_i32gather_epi32((const int32_t *)pal + 1, ind2, 4);

			// unpack colours to 16 bit per channel in __m256i
			c0 = _mm256_cvtepu8_epi16(col0); // zero pad bytes up to 16 bit ints
			c1 = _mm256_cvtepu8_epi16(col1);
			c2 = _mm256_cvtepu8_epi16(col2);
			c3 = _mm256_cvtepu8_epi16(col3);

			// shuffle v into v1 v2 so it's properly lined up with colours
			t   = _mm256_broadcastsi128_si256(v);
			v1  = _mm256_shuffle_epi8(t, vshuf1);
			v2  = _mm256_shuffle_epi8(t, vshuf2);
			v1s = _mm256_sub_epi16(sub, v1);
			v2s = _mm256_sub_epi16(sub, v2);

			c0 = _mm256_mullo_epi16(c0, v1);
			c1 = _mm256_mullo_epi16(c1, v1s);
			c2 = _mm256_mullo_epi16(c2, v2);
			c3 = _mm256_mullo_epi16(c3, v2s);

			c0 = _mm256_add_epi16(c0, c1);
			c2 = _mm256_add_epi16(c2, c3);

			// TODO: some sort of shuffle and mix instead of shift and pack?
			c0 = _mm256_srli_epi16(c0, 8);
			c2 = _mm256_srli_epi16(c2, 8);

			res = _mm256_permute4x64_epi64(_mm256_packus_epi16(c0, c2), _MM_SHUFFLE(3,1,2,0));

			_mm256_maskstore_epi32((int *)d, move_mask, res);

			x+=dp;
			d+=dp;
			s+=dp;
		} // else {
			// old_stream = _mm_stream_load_si128((__m128i *)s); // cast away const to suppress warning
		//}
		// old_stream = _mm_shuffle_epi8(old_stream, align_shuff);

		for(; x + 8 < w; x+=8, s+=8, d+=8) {
			_mm_prefetch(s + 8, _MM_HINT_NTA);
			__m128i col0, col1, col2, col3, ind1, ind2, v;
			__m256i res, c0, c1, c2, c3, t, v1, v2, v1s, v2s;

			// TODO: use _mm_alignr_epi8() so we can do use _mm_stream_load_si128 (can't alignr requires the shift to be a compile time constant)
			// something like:
			//   __m128i stemp = _mm_stream_load_si128((__m128i *)s); // cast away const to suppress warning
			//   v = _mm_alignr_epi8(stemp, old_stream, dp*4);
			//
			//   need to rotate stemp so that we can put the right things
			//   in place with the permute, and so that we won't need to mess with
			//   it when it becomes old_stream
			//
			//   stemp = _mm_shuffle_epi8(stemp, align_shuff);
			//   v = _mm_blendv_epi8(old_stream, stemp, align_mask); 
			//   old_stream = stemp
			// and we don't add dp to s inside the if() statement above so it stays aligned
			v = _mm_lddqu_si128((const __m128i *)s);

			ind1 = _mm_unpacklo_epi16(_mm_srli_epi16(v, 8), zero); // look into using _mm_cvtepu16_epi32
			ind2 = _mm_unpackhi_epi16(_mm_srli_epi16(v, 8), zero);

			col0 = _mm_i32gather_epi32((const int32_t *)pal, ind1, 4);
			col1 = _mm_i32gather_epi32((const int32_t *)pal + 1, ind1, 4);

			col2 = _mm_i32gather_epi32((const int32_t *)pal, ind2, 4);
			col3 = _mm_i32gather_epi32((const int32_t *)pal + 1, ind2, 4);

			// unpack colours to 16 bit per channel in __m256i
			c0 = _mm256_cvtepu8_epi16(col0); // zero pad bytes up to 16 bit ints
			c1 = _mm256_cvtepu8_epi16(col1);
			c2 = _mm256_cvtepu8_epi16(col2);
			c3 = _mm256_cvtepu8_epi16(col3);

			// shuffle v into v1 v2 so it's properly lined up with colours
			t   = _mm256_broadcastsi128_si256(v);
			v1  = _mm256_shuffle_epi8(t, vshuf1);
			v2  = _mm256_shuffle_epi8(t, vshuf2);
			v1s = _mm256_sub_epi16(sub, v1);
			v2s = _mm256_sub_epi16(sub, v2);

			c0 = _mm256_mullo_epi16(c0, v1);
			c1 = _mm256_mullo_epi16(c1, v1s);
			c2 = _mm256_mullo_epi16(c2, v2);
			c3 = _mm256_mullo_epi16(c3, v2s);

			c0 = _mm256_add_epi16(c0, c1);
			c2 = _mm256_add_epi16(c2, c3);

			// TODO: some sort of shuffle and mix instead of shift and pack?
			c0 = _mm256_srli_epi16(c0, 8);
			c2 = _mm256_srli_epi16(c2, 8);

			//TODO: _mm256_permute4x64_epi64 is kinda slow (3 clock latency), figure out another way to do this
			res = _mm256_permute4x64_epi64(_mm256_packus_epi16(c0, c2), _MM_SHUFFLE(3,1,2,0));

			_mm256_stream_si256((__m256i *)d, res);
			//_mm256_storeu_si256((__m256i *)d, res);
		}

		if(w-x) { // should be the same as 8-dp
			const __m256i move_mask = _mm256_cmpgt_epi32(_mm256_set1_epi32(w-x), _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7));
			__m128i col0, col1, col2, col3, ind1, ind2, v;
			__m256i res, c0, c1, c2, c3, t, v1, v2, v1s, v2s;

			v = _mm_lddqu_si128((const __m128i *)s);

			ind1 = _mm_srli_epi32(_mm_unpacklo_epi16(v, zero), 8);
			ind2 = _mm_srli_epi32(_mm_unpackhi_epi16(v, zero), 8);

			col0 = _mm_i32gather_epi32((const int32_t *)pal, ind1, 4);
			col1 = _mm_i32gather_epi32((const int32_t *)pal + 1, ind1, 4);

			col2 = _mm_i32gather_epi32((const int32_t *)pal, ind2, 4);
			col3 = _mm_i32gather_epi32((const int32_t *)pal + 1, ind2, 4);

			// unpack colours to 16 bit per channel in __m256i
			c0 = _mm256_cvtepu8_epi16(col0); // zero pad bytes up to 16 bit ints
			c1 = _mm256_cvtepu8_epi16(col1);
			c2 = _mm256_cvtepu8_epi16(col2);
			c3 = _mm256_cvtepu8_epi16(col3);

			// shuffle v into v1 v2 so it's properly lined up with colours
			t   = _mm256_broadcastsi128_si256(v);
			v1  = _mm256_shuffle_epi8(t, vshuf1);
			v2  = _mm256_shuffle_epi8(t, vshuf2);
			v1s = _mm256_sub_epi16(sub, v1);
			v2s = _mm256_sub_epi16(sub, v2);

			c0 = _mm256_mullo_epi16(c0, v1);
			c1 = _mm256_mullo_epi16(c1, v1s);
			c2 = _mm256_mullo_epi16(c2, v2);
			c3 = _mm256_mullo_epi16(c3, v2s);

			c0 = _mm256_add_epi16(c0, c1);
			c2 = _mm256_add_epi16(c2, c3);

			c0 = _mm256_srli_epi16(c0, 8);
			c2 = _mm256_srli_epi16(c2, 8);

			res = _mm256_permute4x64_epi64(_mm256_packus_epi16(c0, c2), _MM_SHUFFLE(3,1,2,0));

			_mm256_maskstore_epi32((int *)d, move_mask, res);
		}
	}
	_mm256_zeroupper();
	_mm_mfence(); // needed because of the non-temporal stores.
}

#endif