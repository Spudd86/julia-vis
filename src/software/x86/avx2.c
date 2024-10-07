#if (__x86_64__) && !defined(DISABLE_X86_INTRIN)
#pragma GCC target("avx2,no-avx512f")

#ifndef DEBUG
#pragma GCC optimize "3,inline-functions" // Want -fmerge-all-constants but we can't put it in the optimize pragma for some reason
// need to shut gcc up about casting away const in normal builds because
// some of the intrinsics that REALLY ought to take const pointer don't
// (mostly _mm_stream_load_si128())
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

/*
 * Streaming reads and modern Intel CPUs
 * For the most part the streaming load instruction doesn't really do anything
 * on modern CPUs unless it's reading from Write-Combining memory We aren't using
 * WC memory so we should ALWAYS prioritize streaming writes. We DO however want
 * to use the non-temporal pre-fetch hint for reads as it *does* have an effect.
 *
 * So, prefer pre-fetch hints for reads, streaming writes
 *
 * See https://stackoverflow.com/questions/40096894/do-current-x86-architectures-support-non-temporal-loads-from-normal-memory
 */

#include "common.h"
#include "software/pixmisc.h"
#include "points.h"
#include <mmintrin.h>
#include <xmmintrin.h>
#include <emmintrin.h>
#include <immintrin.h>

#include <float.h>

// requires w%16 == 0 && h%16 == 0
__attribute__((hot, flatten, target("avx2,no-avx512f")))
void maxblend_avx2(void *restrict dest, const void *restrict src, size_t npix)
{
	_mm256_zeroupper();
	__m256i * restrict mbdst = dest; const __m256i * restrict mbsrc = src;
	// _mm_prefetch(mbdst + 0, _MM_HINT_NTA);
	// _mm_prefetch(mbdst + 2, _MM_HINT_NTA);
	// _mm_prefetch(mbdst + 4, _MM_HINT_NTA);
	// _mm_prefetch(mbdst + 6, _MM_HINT_NTA);

	_mm_prefetch(mbsrc + 0, _MM_HINT_NTA);
	_mm_prefetch(mbsrc + 2, _MM_HINT_NTA);
	_mm_prefetch(mbsrc + 4, _MM_HINT_NTA);
	_mm_prefetch(mbsrc + 6, _MM_HINT_NTA);

	for(size_t i=0; i < npix; i+=64, mbdst+=4, mbsrc+=4) { // can step up to 128 because w%16 == 0 && h%16 == 0 -> (w*h)%256 == 0
		// _mm_prefetch(mbdst + 8 , _MM_HINT_NTA);
		// _mm_prefetch(mbdst + 10, _MM_HINT_NTA);
		_mm_prefetch(mbsrc + 8 , _MM_HINT_NTA);
		_mm_prefetch(mbsrc + 10, _MM_HINT_NTA);
		_mm256_stream_si256(mbdst + 0, _mm256_max_epu16(_mm256_load_si256(mbdst + 0), _mm256_load_si256(mbsrc + 0)));
		_mm256_stream_si256(mbdst + 1, _mm256_max_epu16(_mm256_load_si256(mbdst + 1), _mm256_load_si256(mbsrc + 1)));
		_mm256_stream_si256(mbdst + 2, _mm256_max_epu16(_mm256_load_si256(mbdst + 2), _mm256_load_si256(mbsrc + 2)));
		_mm256_stream_si256(mbdst + 3, _mm256_max_epu16(_mm256_load_si256(mbdst + 3), _mm256_load_si256(mbsrc + 3)));
	}
	_mm_mfence(); // needed because of the non-temporal stores.
}

/**
 * do the map for backwards iteration of julia set
 * (x0, y0) is the parameter
 *
 * needs properly aligned pointers
 *
 * @param out output surface
 * @param in input surface
 * @param w width of image (needs power of 2)
 * @param h height of image (needs divisable by ?)
 */
__attribute__((hot, flatten, target("avx2,no-avx512f")))
void soft_map_avx2_task(size_t work_item_id, size_t span, uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd)
{
	const int ystart = work_item_id * span * 8;
	const int yend   = IMIN(ystart + span * 8, (unsigned int)h);
	out += ystart * w;

	const float xstep = 2.0f/w, ystep = 2.0f/h;
	const float x0 = pd->p[0]*0.25f -0.5f*0.25f + 0.5f, y0=pd->p[1]*0.25f + 0.5f;

	_mm256_zeroupper();

	const __m256 my0 = _mm256_set1_ps(y0);
	const __m256 mx0 = _mm256_set1_ps(x0);

	const __m256i mask_fraction = _mm256_set1_epi32(0xFF);

	for(int yd = ystart; yd < yend; yd++)
	{
		float v = yd*ystep - 1.0f;

		__m256 mv = _mm256_set1_ps(v);

		for(int xd = 0; xd < w; xd += 8, out += 8)
		{
			// compute sampling source (x, y) for 8 pixels
			__m256 mxd = _mm256_add_ps(_mm256_set1_ps(xd), _mm256_setr_ps(0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f));
			__m256 u = _mm256_sub_ps(_mm256_mul_ps(mxd, _mm256_set1_ps(xstep)), _mm256_set1_ps(1.0f));
			__m256 tmp = _mm256_mul_ps(u, mv);
			__m256 fy = _mm256_add_ps(_mm256_add_ps(tmp, tmp), my0);
			__m256 fx = _mm256_add_ps(_mm256_sub_ps(_mm256_mul_ps(u, u), _mm256_mul_ps(mv, mv)), mx0);

			// Sample from source pixels
			const __m256i xs = _mm256_max_epi32(_mm256_min_epi32(_mm256_cvtps_epi32(_mm256_mul_ps(fx, _mm256_set1_ps(w*256))), _mm256_set1_epi32((w-1)*256)), _mm256_setzero_si256());
			const __m256i ys = _mm256_max_epi32(_mm256_min_epi32(_mm256_cvtps_epi32(_mm256_mul_ps(fy, _mm256_set1_ps(h*256))), _mm256_set1_epi32((h-1)*256)), _mm256_setzero_si256());

			__m256i x1 = _mm256_srli_epi32(xs, 8);
			__m256i y1 = _mm256_srli_epi32(ys, 8);

			__m256i y2 = _mm256_min_epi32(_mm256_add_epi32(y1, _mm256_set1_epi32(1)), _mm256_set1_epi32(w-1));
			__m256i x2 = _mm256_add_epi32(x1, _mm256_set1_epi32(1));

			__m256i idx1 = _mm256_add_epi32(_mm256_mullo_epi32(y1, _mm256_set1_epi32(w)), x1);
			__m256i idx2 = _mm256_add_epi32(_mm256_mullo_epi32(y2, _mm256_set1_epi32(w)), x1);

			__m256i p00_01 = _mm256_i32gather_epi32((int *)in, idx1, 2);
			__m256i p10_11 = _mm256_i32gather_epi32((int *)in, idx2, 2);
#if 0
			p00_01 = _mm256_shufflehi_epi16(_mm256_shufflelo_epi16 (p00_01, _MM_SHUFFLE(3, 1, 2, 0)), _MM_SHUFFLE(3, 1, 2, 0));
			p00_01 = _mm256_permutevar8x32_epi32(p00_01, _mm256_setr_epi32(0, 2, 4, 6, 1, 3, 5, 7));
			__m128i r = _mm256_extracti128_si256(p00_01, 0);
#elif 0
			// TODO: do mask to clamp
			const __m256i permute_8x32_idx = _mm256_setr_epi32(0, 2, 4, 6, 1, 3, 5, 7);

			__m256i wxf = _mm256_and_si256(xs, mask_fraction);
			__m256i wyf = _mm256_and_si256(ys, mask_fraction);

			wxf = _mm256_shufflehi_epi16(_mm256_shufflelo_epi16(wxf, _MM_SHUFFLE(3, 1, 2, 0)), _MM_SHUFFLE(3, 1, 2, 0));
			wyf = _mm256_shufflehi_epi16(_mm256_shufflelo_epi16(wyf, _MM_SHUFFLE(3, 1, 2, 0)), _MM_SHUFFLE(3, 1, 2, 0));

			wxf = _mm256_permutevar8x32_epi32(wxf, permute_8x32_idx);
			wyf = _mm256_permutevar8x32_epi32(wyf, permute_8x32_idx);

			__m128i xf = _mm256_extracti128_si256(wxf, 0);
			__m128i yf = _mm256_extracti128_si256(wyf, 0);

			p00_01 = _mm256_shufflehi_epi16(_mm256_shufflelo_epi16 (p00_01, _MM_SHUFFLE(3, 1, 2, 0)), _MM_SHUFFLE(3, 1, 2, 0));
			p10_11 = _mm256_shufflehi_epi16(_mm256_shufflelo_epi16 (p10_11, _MM_SHUFFLE(3, 1, 2, 0)), _MM_SHUFFLE(3, 1, 2, 0));

			p00_01 = _mm256_permutevar8x32_epi32(p00_01, permute_8x32_idx);
			p10_11 = _mm256_permutevar8x32_epi32(p10_11, permute_8x32_idx);

			__m128i p00 = _mm256_extracti128_si256(p00_01, 0);
			__m128i p01 = _mm256_extracti128_si256(p00_01, 1);
			__m128i p10 = _mm256_extracti128_si256(p10_11, 0);
			__m128i p11 = _mm256_extracti128_si256(p10_11, 1);

			p00 = _mm_mulhi_epu16(p00, _mm_mullo_epi16(_mm_sub_epi16(_mm_set1_epi16(256), xf), _mm_sub_epi16(_mm_set1_epi16(256), yf)));
			p01 = _mm_mulhi_epu16(p01, _mm_mullo_epi16(                                   xf , _mm_sub_epi16(_mm_set1_epi16(256), yf)));
			p10 = _mm_mulhi_epu16(p10, _mm_mullo_epi16(_mm_sub_epi16(_mm_set1_epi16(256), xf),                                    yf ));
			p11 = _mm_mulhi_epu16(p11, _mm_mullo_epi16(                                   xf ,                                    yf ));

			__m128i r = _mm_add_epi16(_mm_add_epi16(p00, p01),_mm_add_epi16(p10, p11));
#else
			// compute fractional stuff to multiply by
			__m256i xf = _mm256_and_si256(xs, mask_fraction); // mask to get only bottom byte of co-ord, fractional part.
			xf = _mm256_add_epi32(_mm256_slli_epi32(xf, 16), _mm256_sub_epi32(_mm256_set1_epi32(256), xf));

			__m256i yf = _mm256_and_si256(ys, mask_fraction); // mask to get only bottom byte of co-ord, fractional part.
			yf = _mm256_add_epi32(yf, _mm256_slli_epi32(yf, 16));

			__m256i f1 = _mm256_mullo_epi16(xf, _mm256_sub_epi16(_mm256_set1_epi16(256), yf));
			__m256i f2 = _mm256_mullo_epi16(xf, yf);

			// Copy the lower 16 bits in each pair up, so that we have all in bounds values
			// then use the mask to blend so that only the high halfs that are also in bounds stay
			__m256i mask = _mm256_and_si256(_mm256_cmpgt_epi32(_mm256_set1_epi32(w-1), x2), _mm256_set1_epi32(0xFFFF0000));

			__m256i p00_00 = _mm256_shufflehi_epi16(_mm256_shufflelo_epi16 (p00_01, _MM_SHUFFLE(2, 2, 0, 0)), _MM_SHUFFLE(2, 2, 0, 0));
			__m256i p10_10 = _mm256_shufflehi_epi16(_mm256_shufflelo_epi16 (p10_11, _MM_SHUFFLE(2, 2, 0, 0)), _MM_SHUFFLE(2, 2, 0, 0));

			p00_01 = _mm256_blendv_epi8(p00_01, p00_00, mask);
			p10_11 = _mm256_blendv_epi8(p10_11, p10_10, mask);

			// Do the bilinear interpolation multiply
			p00_01 = _mm256_mulhi_epu16(p00_01, f1);
			p10_11 = _mm256_mulhi_epu16(p10_11, f2);

			// Horizontally add to mix the pixels along the x direction
	        __m256i res = _mm256_hadd_epi16(_mm256_add_epi16(p00_01, p10_11), _mm256_setzero_si256());

			// then we need to shuffle and extract the result __mm128i
			// luckily the way the horizontal add arranges things mean we don't need to do any 16 bit shuffles and go
			// directly to the "cross lane" 32 bit shuffles
			res = _mm256_permutevar8x32_epi32(res, _mm256_setr_epi32(0, 1, 4, 5, 0, 0, 0, 0));
			__m128i r = _mm256_extracti128_si256(res, 0);
#endif
			// fade out the sample a bit
			r = _mm_mulhi_epu16(r, _mm_set1_epi16(63569u /*(uint32_t)((65536uLL*97uLL)/100uLL)*/ ));

			// Store and prep for next iteration
			_mm_stream_si128((__m128i*)out, r);
		}
	}
	_mm_mfence(); // needed because of the non-temporal stores.
}

#if 0
__attribute__((hot, target("avx2,no-avx512f"), gnu_inline, always_inline,__artificial__))
static void do16_pix(uint32_t *restrict d, const uint16_t *restrict s, const uint32_t *restrict pal)
{
	__m128i ind1, ind2, v, vs, vt, v_1;
	__m256i res, c0, c1, c2, c3, t, v0, v1;

	//  first 8 pixels
	v = _mm_lddqu_si128((const __m128i *)s);

	v_1 = _mm_lddqu_si128((const __m128i *)s + 1);

	ind1 = _mm_srli_epi16(_mm_unpacklo_epi16(v, (__m128i){0ll, 0ll}), 8); // look into using _mm_cvtepu16_epi32
	ind2 = _mm_srli_epi16(_mm_unpackhi_epi16(v, (__m128i){0ll, 0ll}), 8);

	c0 = _mm256_i32gather_epi64((const int64_t *)pal, ind1, 4);
	c1 = _mm256_i32gather_epi64((const int64_t *)pal, ind2, 4);

	ind1 = _mm_srli_epi16(_mm_unpacklo_epi16(v_1, (__m128i){0ll, 0ll}), 8); // look into using _mm_cvtepu16_epi32
	ind2 = _mm_srli_epi16(_mm_unpackhi_epi16(v_1, (__m128i){0ll, 0ll}), 8);

	c0 = _mm256_shuffle_epi8(c0, _mm256_setr_epi8( 0,  4,  1,  5,  2,  6,  3,  7,
	                                                8, 12,  9, 13, 10, 14, 11, 15,
	                                                16, 20, 17, 21, 18, 22, 19, 23,
	                                                24, 28, 25, 29, 26, 30, 27, 31));
	c1 = _mm256_shuffle_epi8(c1, _mm256_setr_epi8( 0,  4,  1,  5,  2,  6,  3,  7,
	                                                8, 12,  9, 13, 10, 14, 11, 15,
	                                                16, 20, 17, 21, 18, 22, 19, 23,
	                                                24, 28, 25, 29, 26, 30, 27, 31));

	c2 = _mm256_i32gather_epi64((const int64_t *)pal, ind1, 4);
	c3 = _mm256_i32gather_epi64((const int64_t *)pal, ind2, 4);

	c2 = _mm256_shuffle_epi8(c2, _mm256_setr_epi8( 0,  4,  1,  5,  2,  6,  3,  7,
	                                                8, 12,  9, 13, 10, 14, 11, 15,
	                                                16, 20, 17, 21, 18, 22, 19, 23,
	                                                24, 28, 25, 29, 26, 30, 27, 31));
	c3 = _mm256_shuffle_epi8(c3, _mm256_setr_epi8( 0,  4,  1,  5,  2,  6,  3,  7,
	                                                8, 12,  9, 13, 10, 14, 11, 15,
	                                                16, 20, 17, 21, 18, 22, 19, 23,
	                                                24, 28, 25, 29, 26, 30, 27, 31));

	v  = _mm_and_si128(v, _mm_set1_epi16(0xff));
	vs = _mm_sub_epi16(_mm_set1_epi16(256), v);

	v  = _mm_packs_epi16(v, v);
	vs = _mm_packs_epi16(vs, vs);

	v = _mm_unpacklo_epi8(vs, v); // vs0, v0, vs1, v1 ...

	t = _mm256_set_m128i(_mm_unpackhi_epi16(v, v), _mm_unpacklo_epi16(v, v)); // vs0, v0, vs0, v0, vs1, v1, vs1, v1, ...

	v0 = _mm256_unpacklo_epi32(t, t);
	t  = _mm256_unpackhi_epi32(t, t);

	v1 = _mm256_permute2x128_si256(v0, t, 013); // Top halves of v0, t go in v1
	v0 = _mm256_permute2x128_si256(v0, t, 020); // bottom halves of v0, t go in v0

	c0 = _mm256_maddubs_epi16(c0, v0);
	c1 = _mm256_maddubs_epi16(c1, v1);

	c0 = _mm256_srli_epi16(c0, 8);
	c1 = _mm256_srli_epi16(c1, 8);

	res = _mm256_permute4x64_epi64(_mm256_packus_epi16(c0, c1), _MM_SHUFFLE(3,1,2,0));

	_mm256_stream_si256((__m256i *)d, res);

	v  = _mm_and_si128(v, _mm_set1_epi16(0xff));
	vs = _mm_sub_epi16(_mm_set1_epi16(256), v);

	v  = _mm_packs_epi16(v, v);
	vs = _mm_packs_epi16(vs, vs);

	v = _mm_unpacklo_epi8(vs, v); // vs0, v0, vs1, v1 ...

	t = _mm256_set_m128i(_mm_unpackhi_epi16(v, v), _mm_unpacklo_epi16(v, v)); // vs0, v0, vs0, v0, vs1, v1, vs1, v1, ...

	v0 = _mm256_unpacklo_epi32(t, t);
	t  = _mm256_unpackhi_epi32(t, t);

	v1 = _mm256_permute2x128_si256(v0, t, 013); // Top halves of v0, t go in v1
	v0 = _mm256_permute2x128_si256(v0, t, 020); // bottom halves of v0, t go in v0

	c2 = _mm256_maddubs_epi16(c2, v0);
	c3 = _mm256_maddubs_epi16(c3, v1);

	c2 = _mm256_srli_epi16(c2, 8);
	c3 = _mm256_srli_epi16(c3, 8);

	res = _mm256_permute4x64_epi64(_mm256_packus_epi16(c2, c3), _MM_SHUFFLE(3,1,2,0));
	_mm256_stream_si256((__m256i *)d + 1, res);
}
#endif

__attribute__((hot, flatten, target("avx2,no-avx512f")))
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

		uint32_t *restrict d = (uint32_t *restrict)(dest + y*dst_stride);
		size_t x = 0;

		if( ((uintptr_t)d) % sizeof(__m256i) ) {
			uintptr_t dp = 8u - ( ((uintptr_t)d) % sizeof(__m256i) )/4u;
			const __m256i move_mask = _mm256_cmpgt_epi32(_mm256_set1_epi32(dp), _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7));
			__m128i col0, col1, col2, col3, ind1, ind2, v;
			__m256i res, c0, c1, c2, c3, t, v1, v2, v1s, v2s;

			v = _mm_lddqu_si128((const __m128i *)s);

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

			c0 = _mm256_mullo_epi16(c0, v1s);
			c1 = _mm256_mullo_epi16(c1, v1);
			c2 = _mm256_mullo_epi16(c2, v2s);
			c3 = _mm256_mullo_epi16(c3, v2);

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
		}

		// Number of bytes to get to the next cache line so we can mark with the proper non-temporal pre-fetch hint
		size_t next_cache_offset = (int8_t)(sizeof(__m128i) - ((uintptr_t)s)%sizeof(__m128i));

		for(; x + 8 < w; x+=8, s+=8, d+=8) {
			_mm_prefetch((const uint8_t *)s + sizeof(__m128i) + next_cache_offset, _MM_HINT_NTA); // load next cache line
#if 1
			__m128i col0, col1, col2, col3, ind1, ind2, v;
			__m256i res, c0, c1, c2, c3, t, v1, v2, v1s, v2s;

			//  first 8 pixels
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

			c0 = _mm256_mullo_epi16(c0, v1s);
			c1 = _mm256_mullo_epi16(c1, v1);
			c2 = _mm256_mullo_epi16(c2, v2s);
			c3 = _mm256_mullo_epi16(c3, v2);

			c0 = _mm256_add_epi16(c0, c1);
			c2 = _mm256_add_epi16(c2, c3);

			// TODO: some sort of shuffle and mix instead of shift and pack?
			c0 = _mm256_srli_epi16(c0, 8);
			c2 = _mm256_srli_epi16(c2, 8);

			//TODO: _mm256_permute4x64_epi64 is kinda slow (3 clock latency), figure out another way to do this
			res = _mm256_permute4x64_epi64(_mm256_packus_epi16(c0, c2), _MM_SHUFFLE(3,1,2,0));
#else

#endif
			_mm256_stream_si256((__m256i *)d, res);
		}

		if(w-x) {
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

			c0 = _mm256_mullo_epi16(c0, v1s);
			c1 = _mm256_mullo_epi16(c1, v1);
			c2 = _mm256_mullo_epi16(c2, v2s);
			c3 = _mm256_mullo_epi16(c3, v2);

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
