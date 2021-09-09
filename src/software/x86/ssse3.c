#if (__x86_64__ || __i386__)  && !defined(DISABLE_X86_INTRIN)
#pragma GCC target("no-sse4.1,ssse3,sse3")
#ifndef DEBUG
#pragma GCC optimize "3,inline-functions" // Want -fmerge-all-constants but we can't put it in the optimize pragma for some reason
#endif

#include "common.h"
#include "../pixmisc.h"
#include <mmintrin.h>
#include <xmmintrin.h>
#include <emmintrin.h>
#include <tmmintrin.h>

#if 0
__attribute__((hot, target("no-sse4.1,ssse3,sse3")))
void list_pnt_blit_ssse3(void * const restrict dest, int iw, const uint16_t *restrict pnt, int pnt_stride, int pw, int ph, int samp, const uint32_t *pnts)
{
	const __m128i max_off = _mm_set1_epi16(0x8000);
	const __m128i move_mask = _mm_cmpgt_epi16(_mm_set1_epi16(pw%8), _mm_setr_epi16(0, 1, 2, 3, 4, 5, 6, 7));

	for(int i=0; i<samp; i++) {
		const uint32_t ipx = pnts[i*2+0], ipy = pnts[i*2+1];
		const uint32_t yf = ipy&0xff, xf = ipx&0xff;

		uint32_t off = (ipy/256u)*iw + ipx/256u;

		const __m128i w00 = _mm_set1_epi16((     yf)*(     xf));
		const __m128i w01 = _mm_set1_epi16((     yf)*(256u-xf));
		const __m128i w10 = _mm_set1_epi16((256u-yf)*(     xf));
		const __m128i w11 = _mm_set1_epi16((256u-yf)*(256u-xf));

		for(int y=0; y < ph; y++) {
			uint16_t *restrict dst_line = (uint16_t *restrict)dest + off + iw*y;
			_mm_prefetch(dst_line, _MM_HINT_NTA);

			const uint16_t *s0 = pnt + y*pnt_stride;
			const uint16_t *s1 = s0 + pnt_stride;

			__m128i p00_cur = _mm_load_si128((const __m128i *)(s0));
			__m128i p10_cur = _mm_load_si128((const __m128i *)(s1));
			for(int x = 0; x + 8 < pw; x+=8, dst_line+=8, s0+=8, s1+=8) {
				_mm_prefetch(dst_line + 8, _MM_HINT_NTA);
				
				__m128i p00_next = _mm_load_si128((const __m128i *)(s0) + 1);
				__m128i p10_next = _mm_load_si128((const __m128i *)(s1) + 1);

				__m128i p00 = p00_cur;
				__m128i p01 = _mm_alignr_epi8(p00_next, p00_cur, 2);
				__m128i p10 = p10_cur;
				__m128i p11 = _mm_alignr_epi8(p10_next, p10_cur, 2);

				p00 = _mm_mulhi_epu16(p00, w00);
				p01 = _mm_mulhi_epu16(p01, w01);
				p10 = _mm_mulhi_epu16(p10, w10);
				p11 = _mm_mulhi_epu16(p11, w11);

				__m128i oa = _mm_add_epi16(p00, p01);
				__m128i ob = _mm_add_epi16(p10, p11);
				__m128i res = _mm_add_epi16(oa, ob);
				// no need to shift because when we did the multiplication we
				// only got the high 16 bits
				__m128i dp = _mm_lddqu_si128((__m128i *)dst_line);
				dp  = _mm_add_epi16(dp,  max_off);
				res = _mm_add_epi16(res, max_off);
				res = _mm_max_epi16(res, dp);
				res = _mm_sub_epi16(res, max_off);
				_mm_storeu_si128((__m128i *)(dst_line), res);

				p00_cur = p00_next;
				p10_cur = p10_next;
			}

			__m128i p00_next = _mm_load_si128((const __m128i *)(s0) + 1);
			__m128i p10_next = _mm_load_si128((const __m128i *)(s1) + 1);

			__m128i p00 = p00_cur;
			__m128i p01 = _mm_alignr_epi8(p00_next, p00_cur, 2);
			__m128i p10 = p10_cur;
			__m128i p11 = _mm_alignr_epi8(p10_next, p10_cur, 2);

			p00 = _mm_mulhi_epu16(p00, w00);
			p01 = _mm_mulhi_epu16(p01, w01);
			p10 = _mm_mulhi_epu16(p10, w10);
			p11 = _mm_mulhi_epu16(p11, w11);

			__m128i oa = _mm_add_epi16(p00, p01);
			__m128i ob = _mm_add_epi16(p10, p11);
			__m128i res = _mm_add_epi16(oa, ob);
			// no need to shift because when we did the multiplication we
			// only got the high 16 bits

			__m128i dp = _mm_lddqu_si128((__m128i *)dst_line);
			dp  = _mm_add_epi16(dp,  max_off);
			res = _mm_add_epi16(res, max_off);
			res = _mm_max_epi16(res, dp);
			res = _mm_sub_epi16(res, max_off);
			_mm_maskmoveu_si128(res, move_mask, (char *)(dst_line));
		}
	}
	//_mm_mfence(); // needed because of the non-temporal stores.
}
#endif

#if 1
//FIXME: interpolation is wrong way around
__attribute__((hot, target("no-sse4.1,ssse3,sse3")))
void pallet_blit32_ssse3(uint8_t *restrict dest, unsigned int dst_stride,
                        const uint16_t *restrict src, unsigned int src_stride,
                        unsigned int w, unsigned int h,
                        const uint32_t *restrict pal)
{
	const __m128i mask = _mm_setr_epi16(0xff, 0xff, 0, 0, 0, 0, 0, 0);
	const __m128i sub  = _mm_setr_epi16( 256,  256, 0, 0, 0, 0, 0, 0);

	const __m128i col_shuffle = _mm_setr_epi8(0,  8, // pal[s+0].r
	                                          4,  9, // pal[s+1].r
	                                          1, 10, // pal[s+0].g
	                                          5, 11, // pal[s+1].g
	                                          2, 12, // pal[s+0].b
	                                          6, 13, // pal[s+1].b
	                                          3, 14, // pal[s+0].x
	                                          7, 15);// pal[s+1].x

	for(size_t y = 0; y < h; y++) {
		const uint16_t *restrict s = src + y*w;
		_mm_prefetch(s, _MM_HINT_NTA);
		uint32_t *restrict d = (uint32_t *restrict)(dest + y*dst_stride);
		size_t x = 0;

		// need to align our destination address so we can use non-temporal writes
		// assume that the pixels are at least aligned to 4 bytes
		uintptr_t dp = (uintptr_t)d;
		dp = 4 - ((dp%16)/4);
		if(dp) {
			const __m128i move_mask = _mm_cmplt_epi32(_mm_setr_epi32(0, 1, 2, 3), _mm_set1_epi32(dp));
			__m128i col1, col2, col3, col4, vt, vs, v1, v2;

			col1 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[0]/256u)));
			col2 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[1]/256u)));

			col1 = _mm_shuffle_epi8(col1, col_shuffle);
			col2 = _mm_shuffle_epi8(col2, col_shuffle);

			vt = _mm_set_epi32(0,0,0,*(const uint32_t *)(s)); // compiler WILL actually fold this to down a single movd instruction
			vt = _mm_and_si128(vt, mask);
			vs = _mm_sub_epi16(sub, vt);
			vt = _mm_unpacklo_epi16(vt, vs); // vt = v1, v1s, v2, v2s, 0, 0, 0, 0
			v1 = _mm_shuffle_epi8(vt, _mm_set1_epi32(0x03020100)); // broadcast (v1, v1s)
			v2 = _mm_shuffle_epi8(vt, _mm_set1_epi32(0x07060504)); // broadcast (v2, v2s)

			col1 = _mm_madd_epi16(col1, v1);
			col2 = _mm_madd_epi16(col2, v2);
			col1 = _mm_srli_epi32(col1, 8);
			col2 = _mm_srli_epi32(col2, 8);
			col1 = _mm_packs_epi32(col1, col2);
			

			col3 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[2]/256u)));
			col4 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[3]/256u)));

			col3 = _mm_shuffle_epi8(col3, col_shuffle);
			col4 = _mm_shuffle_epi8(col4, col_shuffle);

			vt = _mm_set_epi32(0,0,0,*(const uint32_t *)(s)); // compiler WILL actually fold this to down a single movd instruction
			vt = _mm_and_si128(vt, mask);
			vs = _mm_sub_epi16(sub, vt);
			vt = _mm_unpacklo_epi16(vt, vs); // vt = v1, v1s, v2, v2s, 0, 0, 0, 0
			v1 = _mm_shuffle_epi8(vt, _mm_set1_epi32(0x03020100)); // broadcast (v1, v1s)
			v2 = _mm_shuffle_epi8(vt, _mm_set1_epi32(0x07060504)); // broadcast (v2, v2s)

			col3 = _mm_madd_epi16(col3, v1);
			col4 = _mm_madd_epi16(col4, v2);
			col3 = _mm_srli_epi32(col3, 8);
			col4 = _mm_srli_epi32(col4, 8);
			col3 = _mm_packs_epi32(col3, col4);


			col1 = _mm_packus_epi16(col1, col3);
			_mm_maskmoveu_si128(col1, move_mask, (char *restrict)(d + 0));
			x+=dp;
			d+=dp;
			s+=dp;
		}

		for(; x + 8 < w; x+=8, s+=8, d+=8) {
			_mm_prefetch(s+8, _MM_HINT_NTA);
			__m128i col1, col2, col3, col4, vt, vs, v1, v2;

			col1 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[0]/256u)));
			col2 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[1]/256u)));

			col1 = _mm_shuffle_epi8(col1, col_shuffle);
			col2 = _mm_shuffle_epi8(col2, col_shuffle);

			vt = _mm_set_epi32(0,0,0,*(const uint32_t *)(s)); // compiler WILL actually fold this to down a single movd instruction
			vt = _mm_and_si128(vt, mask);
			vs = _mm_sub_epi16(sub, vt);
			vt = _mm_unpacklo_epi16(vt, vs); // vt = v1, v1s, v2, v2s, 0, 0, 0, 0
			v1 = _mm_shuffle_epi8(vt, _mm_set1_epi32(0x03020100)); // broadcast (v1, v1s)
			v2 = _mm_shuffle_epi8(vt, _mm_set1_epi32(0x07060504)); // broadcast (v2, v2s)

			col1 = _mm_madd_epi16(col1, v1);
			col2 = _mm_madd_epi16(col2, v2);
			col1 = _mm_srli_epi32(col1, 8);
			col2 = _mm_srli_epi32(col2, 8);
			col1 = _mm_packs_epi32(col1, col2);
			// on sse4.1 could do
			// col1 = _mm_packus_epi32(col1, col2);
			// col1 = _mm_srli_epi32(col1, 8);
			

			col3 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[2]/256u)));
			col4 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[3]/256u)));

			col3 = _mm_shuffle_epi8(col3, col_shuffle);
			col4 = _mm_shuffle_epi8(col4, col_shuffle);

			vt = _mm_set_epi32(0,0,0,*(const uint32_t *)(s)); // compiler WILL actually fold this to down a single movd instruction
			vt = _mm_and_si128(vt, mask);
			vs = _mm_sub_epi16(sub, vt);
			vt = _mm_unpacklo_epi16(vt, vs); // vt = v1, v1s, v2, v2s, 0, 0, 0, 0
			v1 = _mm_shuffle_epi8(vt, _mm_set1_epi32(0x03020100)); // broadcast (v1, v1s)
			v2 = _mm_shuffle_epi8(vt, _mm_set1_epi32(0x07060504)); // broadcast (v2, v2s)

			col3 = _mm_madd_epi16(col3, v1);
			col4 = _mm_madd_epi16(col4, v2);
			col3 = _mm_srli_epi32(col3, 8);
			col4 = _mm_srli_epi32(col4, 8);
			col3 = _mm_packs_epi32(col3, col4);


			col1 = _mm_packus_epi16(col1, col3);
			_mm_stream_si128((__m128i *)(d + 0), col1);


			col1 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[4]/256u)));
			col2 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[5]/256u)));

			col1 = _mm_shuffle_epi8(col1, col_shuffle);
			col2 = _mm_shuffle_epi8(col2, col_shuffle);

			vt = _mm_set_epi32(0,0,0,*(const uint32_t *)(s)); // compiler WILL actually fold this to down a single movd instruction
			vt = _mm_and_si128(vt, mask);
			vs = _mm_sub_epi16(sub, vt);
			vt = _mm_unpacklo_epi16(vt, vs); // vt = v1, v1s, v2, v2s, 0, 0, 0, 0
			v1 = _mm_shuffle_epi8(vt, _mm_set1_epi32(0x03020100)); // broadcast (v1, v1s)
			v2 = _mm_shuffle_epi8(vt, _mm_set1_epi32(0x07060504)); // broadcast (v2, v2s)

			col1 = _mm_madd_epi16(col1, v1);
			col2 = _mm_madd_epi16(col2, v2);
			col1 = _mm_srli_epi32(col1, 8);
			col2 = _mm_srli_epi32(col2, 8);
			col1 = _mm_packs_epi32(col1, col2);


			col3 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[6]/256u)));
			col4 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[7]/256u)));

			col3 = _mm_shuffle_epi8(col3, col_shuffle);
			col4 = _mm_shuffle_epi8(col4, col_shuffle);

			vt = _mm_set_epi32(0,0,0,*(const uint32_t *)(s)); // compiler WILL actually fold this to down a single movd instruction
			vt = _mm_and_si128(vt, mask);
			vs = _mm_sub_epi16(sub, vt);
			vt = _mm_unpacklo_epi16(vt, vs); // vt = v1, v1s, v2, v2s, 0, 0, 0, 0
			v1 = _mm_shuffle_epi8(vt, _mm_set1_epi32(0x03020100)); // broadcast (v1, v1s)
			v2 = _mm_shuffle_epi8(vt, _mm_set1_epi32(0x07060504)); // broadcast (v2, v2s)

			col3 = _mm_madd_epi16(col3, v1);
			col4 = _mm_madd_epi16(col4, v2);
			col3 = _mm_srli_epi32(col3, 8);
			col4 = _mm_srli_epi32(col4, 8);
			col3 = _mm_packs_epi32(col3, col4);


			col1 = _mm_packus_epi16(col1, col3);
			_mm_stream_si128((__m128i *)(d + 4), col1);
		}

		__m128i col1, col2, col3, col4, vt, vs, v1, v2;

		col1 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[0]/256u)));
		col2 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[1]/256u)));

		col1 = _mm_shuffle_epi8(col1, col_shuffle);
		col2 = _mm_shuffle_epi8(col2, col_shuffle);

		vt = _mm_set_epi32(0,0,0,*(const uint32_t *)(s)); // compiler WILL actually fold this to down a single movd instruction
		vt = _mm_and_si128(vt, mask);
		vs = _mm_sub_epi16(sub, vt);
		vt = _mm_unpacklo_epi16(vt, vs); // vt = v1, v1s, v2, v2s, 0, 0, 0, 0
		v1 = _mm_shuffle_epi8(vt, _mm_set1_epi32(0x03020100)); // broadcast (v1, v1s)
		v2 = _mm_shuffle_epi8(vt, _mm_set1_epi32(0x07060504)); // broadcast (v2, v2s)

		col1 = _mm_madd_epi16(col1, v1);
		col2 = _mm_madd_epi16(col2, v2);
		col1 = _mm_srli_epi32(col1, 8);
		col2 = _mm_srli_epi32(col2, 8);
		col1 = _mm_packs_epi32(col1, col2);
		

		col3 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[2]/256u)));
		col4 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[3]/256u)));

		col3 = _mm_shuffle_epi8(col3, col_shuffle);
		col4 = _mm_shuffle_epi8(col4, col_shuffle);

		vt = _mm_set_epi32(0,0,0,*(const uint32_t *)(s)); // compiler WILL actually fold this to down a single movd instruction
		vt = _mm_and_si128(vt, mask);
		vs = _mm_sub_epi16(sub, vt);
		vt = _mm_unpacklo_epi16(vt, vs); // vt = v1, v1s, v2, v2s, 0, 0, 0, 0
		v1 = _mm_shuffle_epi8(vt, _mm_set1_epi32(0x03020100)); // broadcast (v1, v1s)
		v2 = _mm_shuffle_epi8(vt, _mm_set1_epi32(0x07060504)); // broadcast (v2, v2s)

		col3 = _mm_madd_epi16(col3, v1);
		col4 = _mm_madd_epi16(col4, v2);
		col3 = _mm_srli_epi32(col3, 8);
		col4 = _mm_srli_epi32(col4, 8);
		col3 = _mm_packs_epi32(col3, col4);


		col1 = _mm_packus_epi16(col1, col3);
		_mm_stream_si128((__m128i *)(d + 0), col1);

		if(w-(x+4)) { // should be the same as 4-dp
			const __m128i move_mask = _mm_cmplt_epi32(_mm_setr_epi32(0, 1, 2, 3), _mm_set1_epi32(w-(x+4)));

			col1 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[4]/256u)));
			col2 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[5]/256u)));

			col1 = _mm_shuffle_epi8(col1, col_shuffle);
			col2 = _mm_shuffle_epi8(col2, col_shuffle);

			vt = _mm_set_epi32(0,0,0,*(const uint32_t *)(s)); // compiler WILL actually fold this to down a single movd instruction
			vt = _mm_and_si128(vt, mask);
			vs = _mm_sub_epi16(sub, vt);
			vt = _mm_unpacklo_epi16(vt, vs); // vt = v1, v1s, v2, v2s, 0, 0, 0, 0
			v1 = _mm_shuffle_epi8(vt, _mm_set1_epi32(0x03020100)); // broadcast (v1, v1s)
			v2 = _mm_shuffle_epi8(vt, _mm_set1_epi32(0x07060504)); // broadcast (v2, v2s)

			col1 = _mm_madd_epi16(col1, v1);
			col2 = _mm_madd_epi16(col2, v2);
			col1 = _mm_srli_epi32(col1, 8);
			col2 = _mm_srli_epi32(col2, 8);
			col1 = _mm_packs_epi32(col1, col2);


			col3 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[6]/256u)));
			col4 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[7]/256u)));

			col3 = _mm_shuffle_epi8(col3, col_shuffle);
			col4 = _mm_shuffle_epi8(col4, col_shuffle);

			vt = _mm_set_epi32(0,0,0,*(const uint32_t *)(s)); // compiler WILL actually fold this to down a single movd instruction
			vt = _mm_and_si128(vt, mask);
			vs = _mm_sub_epi16(sub, vt);
			vt = _mm_unpacklo_epi16(vt, vs); // vt = v1, v1s, v2, v2s, 0, 0, 0, 0
			v1 = _mm_shuffle_epi8(vt, _mm_set1_epi32(0x03020100)); // broadcast (v1, v1s)
			v2 = _mm_shuffle_epi8(vt, _mm_set1_epi32(0x07060504)); // broadcast (v2, v2s)

			col3 = _mm_madd_epi16(col3, v1);
			col4 = _mm_madd_epi16(col4, v2);
			col3 = _mm_srli_epi32(col3, 8);
			col4 = _mm_srli_epi32(col4, 8);
			col3 = _mm_packs_epi32(col3, col4);


			col1 = _mm_packus_epi16(col1, col3);
			_mm_maskmoveu_si128(col1, move_mask, (char *restrict)(d + 4));
		}
	}
	//_mm_mfence(); // needed because of the non-temporal stores.
}

#else

__attribute__((hot, target("no-sse4.1,ssse3,sse3")))
void pallet_blit32_ssse3(uint8_t *restrict dest, unsigned int dst_stride,
                        const uint16_t *restrict src, unsigned int src_stride,
                        unsigned int w, unsigned int h,
                        const uint32_t *restrict pal)
{
	const __m128i mask = _mm_setr_epi16(0xff, 0xff, 0, 0, 0, 0, 0, 0);
	const __m128i sub  = _mm_setr_epi16( 256,  256, 0, 0, 0, 0, 0, 0);
	const __m128i col_shuffle = _mm_setr_epi8(
		 0, // pal[s[0]+0].r
		 4, // pal[s[0]+1].r
		 1, // pal[s[0]+0].g
		 5, // pal[s[0]+1].g
		 2, // pal[s[0]+0].b
		 6, // pal[s[0]+1].b
		 3, // pal[s[0]+0].x
		 7, // pal[s[0]+1].x
		 8, // pal[s[1]+0].r
		12, // pal[s[1]+1].r
		 9, // pal[s[1]+0].g
		13, // pal[s[1]+1].g
		10, // pal[s[1]+0].b
		14, // pal[s[1]+1].b
		11, // pal[s[1]+0].x
		15);// pal[s[1]+1].x
	const __m128i v_shuffle = _mm_setr_epi8(
		0, 2,
		0, 2,
		0, 2,
		0, 2,
		4, 6,
		4, 6,
		4, 6,
		4, 6);

	for(size_t y = 0; y < h; y++) {
		const uint16_t *restrict s = src + y*w;
		_mm_prefetch(s, _MM_HINT_NTA);
		uint32_t *restrict d = (uint32_t *restrict)(dest + y*dst_stride);
		size_t x = 0;

		// need to align our destination address so we can use non-temporal writes
		// assume that the pixels are at least aligned to 4 bytes
		uintptr_t dp = (uintptr_t)d;
		dp = 4 - ((dp%16)/4);
		if(dp) {
			const __m128i move_mask = _mm_cmplt_epi32(_mm_setr_epi32(0, 1, 2, 3), _mm_set1_epi32(dp));
			__m128i col1, col2, vt, vs;


			col1 = _mm_setr_epi64(*(const __m64 *restrict)(pal+(s[0]/256u)),
			                      *(const __m64 *restrict)(pal+(s[1]/256u)));
			col1 = _mm_shuffle_epi8(col1, col_shuffle);

			vt = _mm_set_epi32(0,0,0,*(const uint32_t *)(s)); // compiler WILL actually fold this to down a single movd instruction
			vt = _mm_and_si128(vt, mask);
			vs = _mm_sub_epi16(sub, vt);
			vt = _mm_unpacklo_epi16(vt, vs); // vt = v1, v1s, v2, v2s, 0, 0, 0, 0
			vt = _mm_srli_epi16(vt, 2);
			vt = _mm_shuffle_epi8(vt, v_shuffle);

			col1 = _mm_maddubs_epi16(col1, vt);
			col1 = _mm_srli_epi16(col1, 6);


			col2 = _mm_setr_epi64(*(const __m64 *restrict)(pal+(s[2]/256u)),
			                      *(const __m64 *restrict)(pal+(s[3]/256u)));
			col2 = _mm_shuffle_epi8(col2, col_shuffle);

			vt = _mm_set_epi32(0,0,0,*(const uint32_t *)(s)); // compiler WILL actually fold this to down a single movd instruction
			vt = _mm_and_si128(vt, mask);
			vs = _mm_sub_epi16(sub, vt);
			vt = _mm_unpacklo_epi16(vt, vs); // vt = v1, v1s, v2, v2s, 0, 0, 0, 0
			vt = _mm_srli_epi16(vt, 2);
			vt = _mm_shuffle_epi8(vt, v_shuffle);

			col2 = _mm_maddubs_epi16(col2, vt);
			col2 = _mm_srli_epi16(col2, 6);


			col1 = _mm_packus_epi16(col1, col2);
			_mm_maskmoveu_si128(col1, move_mask, (char *restrict)(d + 0));
			x+=dp;
			d+=dp;
			s+=dp;
		}

		for(; x + 8 < w; x+=8, s+=8, d+=8) {
			_mm_prefetch(s+8, _MM_HINT_NTA);
			__m128i col1, col2, vt, vs;


			col1 = _mm_setr_epi64(*(const __m64 *restrict)(pal+(s[0]/256u)),
			                      *(const __m64 *restrict)(pal+(s[1]/256u)));
			col1 = _mm_shuffle_epi8(col1, col_shuffle);

			vt = _mm_set_epi32(0,0,0,*(const uint32_t *)(s)); // compiler WILL actually fold this to down a single movd instruction
			vt = _mm_and_si128(vt, mask);
			vs = _mm_sub_epi16(sub, vt);
			vt = _mm_unpacklo_epi16(vt, vs); // vt = v1, v1s, v2, v2s, 0, 0, 0, 0
			vt = _mm_srli_epi16(vt, 2);
			vt = _mm_shuffle_epi8(vt, v_shuffle);

			col1 = _mm_maddubs_epi16(col1, vt);
			col1 = _mm_srli_epi16(col1, 6);


			col2 = _mm_setr_epi64(*(const __m64 *restrict)(pal+(s[2]/256u)),
			                      *(const __m64 *restrict)(pal+(s[3]/256u)));
			col2 = _mm_shuffle_epi8(col2, col_shuffle);

			vt = _mm_set_epi32(0,0,0,*(const uint32_t *)(s)); // compiler WILL actually fold this to down a single movd instruction
			vt = _mm_and_si128(vt, mask);
			vs = _mm_sub_epi16(sub, vt);
			vt = _mm_unpacklo_epi16(vt, vs); // vt = v1, v1s, v2, v2s, 0, 0, 0, 0
			vt = _mm_srli_epi16(vt, 2);
			vt = _mm_shuffle_epi8(vt, v_shuffle);

			col2 = _mm_maddubs_epi16(col2, vt);
			col2 = _mm_srli_epi16(col2, 6);


			col1 = _mm_packus_epi16(col1, col2);
			_mm_stream_si128((__m128i *)(d + 0), col1);


			col1 = _mm_setr_epi64(*(const __m64 *restrict)(pal+(s[4]/256u)),
			                      *(const __m64 *restrict)(pal+(s[5]/256u)));
			col1 = _mm_shuffle_epi8(col1, col_shuffle);

			vt = _mm_set_epi32(0,0,0,*(const uint32_t *)(s)); // compiler WILL actually fold this to down a single movd instruction
			vt = _mm_and_si128(vt, mask);
			vs = _mm_sub_epi16(sub, vt);
			vt = _mm_unpacklo_epi16(vt, vs); // vt = v1, v1s, v2, v2s, 0, 0, 0, 0
			vt = _mm_srli_epi16(vt, 2);
			vt = _mm_shuffle_epi8(vt, v_shuffle);

			col1 = _mm_maddubs_epi16(col1, vt);
			col1 = _mm_srli_epi16(col1, 6);


			col2 = _mm_setr_epi64(*(const __m64 *restrict)(pal+(s[6]/256u)),
			                      *(const __m64 *restrict)(pal+(s[7]/256u)));
			col2 = _mm_shuffle_epi8(col2, col_shuffle);

			vt = _mm_set_epi32(0,0,0,*(const uint32_t *)(s)); // compiler WILL actually fold this to down a single movd instruction
			vt = _mm_and_si128(vt, mask);
			vs = _mm_sub_epi16(sub, vt);
			vt = _mm_unpacklo_epi16(vt, vs); // vt = v1, v1s, v2, v2s, 0, 0, 0, 0
			vt = _mm_srli_epi16(vt, 2);
			vt = _mm_shuffle_epi8(vt, v_shuffle);

			col2 = _mm_maddubs_epi16(col2, vt);
			col2 = _mm_srli_epi16(col2, 6);


			col1 = _mm_packus_epi16(col1, col2);
			_mm_stream_si128((__m128i *)(d + 4), col1);
		}

		__m128i col1, col2, vt, vs;

		col1 = _mm_setr_epi64(*(const __m64 *restrict)(pal+(s[0]/256u)),
			                  *(const __m64 *restrict)(pal+(s[1]/256u)));
		col1 = _mm_shuffle_epi8(col1, col_shuffle);

		vt = _mm_set_epi32(0,0,0,*(const uint32_t *)(s)); // compiler WILL actually fold this to down a single movd instruction
		vt = _mm_and_si128(vt, mask);
		vs = _mm_sub_epi16(sub, vt);
		vt = _mm_unpacklo_epi16(vt, vs); // vt = v1, v1s, v2, v2s, 0, 0, 0, 0
		vt = _mm_srli_epi16(vt, 2);
		vt = _mm_shuffle_epi8(vt, v_shuffle);

		col1 = _mm_maddubs_epi16(col1, vt);
		col1 = _mm_srli_epi16(col1, 6);


		col2 = _mm_setr_epi64(*(const __m64 *restrict)(pal+(s[2]/256u)),
		                      *(const __m64 *restrict)(pal+(s[3]/256u)));
		col2 = _mm_shuffle_epi8(col2, col_shuffle);

		vt = _mm_set_epi32(0,0,0,*(const uint32_t *)(s)); // compiler WILL actually fold this to down a single movd instruction
		vt = _mm_and_si128(vt, mask);
		vs = _mm_sub_epi16(sub, vt);
		vt = _mm_unpacklo_epi16(vt, vs); // vt = v1, v1s, v2, v2s, 0, 0, 0, 0
		vt = _mm_srli_epi16(vt, 2);
		vt = _mm_shuffle_epi8(vt, v_shuffle);

		col2 = _mm_maddubs_epi16(col2, vt);
		col2 = _mm_srli_epi16(col2, 6);


		col1 = _mm_packus_epi16(col1, col2);
		_mm_stream_si128((__m128i *)(d + 0), col1);

		if(w-(x+4)) { // should be the same as 4-dp
			const __m128i move_mask = _mm_cmplt_epi32(_mm_setr_epi32(0, 1, 2, 3), _mm_set1_epi32(w-(x+4)));

			col1 = _mm_setr_epi64(*(const __m64 *restrict)(pal+(s[4]/256u)),
			                      *(const __m64 *restrict)(pal+(s[5]/256u)));
			col1 = _mm_shuffle_epi8(col1, col_shuffle);

			vt = _mm_set_epi32(0,0,0,*(const uint32_t *)(s)); // compiler WILL actually fold this to down a single movd instruction
			vt = _mm_and_si128(vt, mask);
			vs = _mm_sub_epi16(sub, vt);
			vt = _mm_unpacklo_epi16(vt, vs); // vt = v1, v1s, v2, v2s, 0, 0, 0, 0
			vt = _mm_srli_epi16(vt, 2);
			vt = _mm_shuffle_epi8(vt, v_shuffle);

			col1 = _mm_maddubs_epi16(col1, vt);
			col1 = _mm_srli_epi16(col1, 6);


			col2 = _mm_setr_epi64(*(const __m64 *restrict)(pal+(s[6]/256u)),
			                      *(const __m64 *restrict)(pal+(s[7]/256u)));
			col2 = _mm_shuffle_epi8(col2, col_shuffle);

			vt = _mm_set_epi32(0,0,0,*(const uint32_t *)(s)); // compiler WILL actually fold this to down a single movd instruction
			vt = _mm_and_si128(vt, mask);
			vs = _mm_sub_epi16(sub, vt);
			vt = _mm_unpacklo_epi16(vt, vs); // vt = v1, v1s, v2, v2s, 0, 0, 0, 0
			vt = _mm_srli_epi16(vt, 2);
			vt = _mm_shuffle_epi8(vt, v_shuffle);

			col2 = _mm_maddubs_epi16(col2, vt);
			col2 = _mm_srli_epi16(col2, 6);


			col1 = _mm_packus_epi16(col1, col2);
			_mm_maskmoveu_si128(col1, move_mask, (char *restrict)(d + 4));
		}
	}
	//_mm_mfence(); // needed because of the non-temporal stores.
}
#endif

#endif