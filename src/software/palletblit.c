
#pragma GCC optimize "3,inline-functions,merge-all-constants"

#include "common.h"
#include "pixmisc.h"
#include "pallet.h"
#include "mymm.h"

//#undef NDEBUG

#include <assert.h>

#ifndef NDEBUG
#define unreachable() assert(0)
#else
#define unreachable __builtin_unreachable
#endif


// pallet must have 257 entries (for easier interpolation on 16 bit indices)
// output pix = (pallet[in/256]*(in%256) + pallet[in/256+1]*(255-in%256])/256
// for each colour component

// the above is not done in 16 bit modes they just do out = pallet[in/256] (and convert the pallet)

//TODO: load pallets from files of some sort

//TODO: optimize 16 bit modes
//  - move y*w calculations outside of the loop
//  - non-temporal store version if sse is available

//TODO: dispatch based on availability of instructions
//   except maybe on x86_64 since sse2 is always available there


#if defined(__SSE2__)
//TODO: if we have sse4.1 and dest and dst_stride are aligned properly
// use non-temporal loads?

//TODO: test if going up to a full 16 pixel at a time loop is faster, could be due to
// that being a cache line size chuck
static void pallet_blit32(uint32_t *restrict dest, unsigned int dst_stride, const uint16_t *restrict src, unsigned int w, unsigned int h, const uint32_t *restrict pal)
{
	// TODO: _mm_loadl_epi64 is dumb, it takes a pointer to __m128i rather than a uint64_t or __m64, figure out a way around using it.
	// TODO: set the constants with a literal rather than a function (except maybe zero)
	const __m128i zero = _mm_setzero_si128();
	const __m128i mask = _mm_set1_epi16(0xff);
	const __m128i sub = _mm_set1_epi16(256);

	for(size_t y = 0; y < h; y++) {
		const uint16_t *restrict s = src + y*w;
		uint32_t *restrict d = (uint32_t *restrict)((char *restrict)dest + y*dst_stride);
		size_t x = 0;

		// need to align our destination address so we can use non-temporal writes
		// assume that the pixels are at least aligned to 4 bytes
		uintptr_t dp = (uintptr_t)d;
		switch(4 - ((dp%16)/4)) {
			__m128i col1, col2, v1, v1s;
			case 2:
				col1 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[0]/256u)));
				col2 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[1]/256u)));
				col1 = _mm_unpacklo_epi32(col1, col2);
				col2 = col1;
				col1 = _mm_unpacklo_epi8(col1, zero);
				col2 = _mm_unpackhi_epi8(col2, zero);

				v1   = _mm_set1_epi32(*(const uint32_t *)(s + 0));
				v1   = _mm_and_si128(v1, mask);
				col2 = _mm_mullo_epi16(col2, v1);
				v1s  = _mm_sub_epi16(sub, v1);
				col1 = _mm_mullo_epi16(col1, v1s);
				col1 = _mm_add_epi16(col1, col2);
				col1 = _mm_srli_epi16(col1, 8);

				col1 = _mm_packus_epi16(col1, zero);
				_mm_stream_si64((int64_t *restrict)d, _mm_cvtsi128_si64(col1));
				s+=2;
				d+=2;
				x+=2;
			break;
			case 3:
				col1 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[0]/256u)));
				col2 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[1]/256u)));
				col1 = _mm_unpacklo_epi32(col1, col2);
				col2 = col1;
				col1 = _mm_unpacklo_epi8(col1, zero);
				col2 = _mm_unpackhi_epi8(col2, zero);

				v1   = _mm_set1_epi32(*(const uint32_t *)(s + 0));
				v1   = _mm_and_si128(v1, mask);
				col2 = _mm_mullo_epi16(col2, v1);
				v1s  = _mm_sub_epi16(sub, v1);
				col1 = _mm_mullo_epi16(col1, v1s);
				col1 = _mm_add_epi16(col1, col2);
				col1 = _mm_srli_epi16(col1, 8);

				col1 = _mm_packus_epi16(col1, zero);
				_mm_stream_si64((int64_t *restrict)d, _mm_cvtsi128_si64(col1));

				s+=2;
				d+=2;
				x+=2;
			//fall through
			case 1:
				col1 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[0]/256u)));
				col1 = _mm_unpacklo_epi32(col1, zero);
				col2 = col1;
				col1 = _mm_unpacklo_epi8(col1, zero);
				col2 = _mm_unpackhi_epi8(col2, zero);

				v1   = _mm_set1_epi32(*(const uint32_t *)(s + 0));
				v1   = _mm_and_si128(v1, mask);
				col2 = _mm_mullo_epi16(col2, v1);
				v1s  = _mm_sub_epi16(sub, v1);
				col1 = _mm_mullo_epi16(col1, v1s);
				col1 = _mm_add_epi16(col1, col2);
				col1 = _mm_srli_epi16(col1, 8);

				col1 = _mm_packus_epi16(col1, zero);
				_mm_stream_si32((int *)d, _mm_cvtsi128_si32(col1));
				s++;
				d++;
				x++;
			case 0:
			case 4:
				break;
			default:
				unreachable();// can't happen
		}

		for(; x + 8 < w; x+=8, s+=8, d+=8) {
			__m128i col1, col2, col3, col4, v1, v2, v1s, v2s;

			col1 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[0]/256u)));
			col2 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[1]/256u)));
			col1 = _mm_unpacklo_epi32(col1, col2);
			col2 = col1;
			col1 = _mm_unpacklo_epi8(col1, zero);
			col2 = _mm_unpackhi_epi8(col2, zero);

			v1   = _mm_set1_epi32(*(const uint32_t *)(s + 0));
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

			v2   = _mm_set1_epi32(*(const uint32_t *)(s + 2));//_mm_set1_epi32(s[2] | (s[3] << 16));
			v2   = _mm_and_si128(v2, mask);
			col4 = _mm_mullo_epi16(col4, v2);
			v2s  = _mm_sub_epi16(sub, v2);
			col3 = _mm_mullo_epi16(col3, v2s);
			col3 = _mm_add_epi16(col3, col4);
			col3 = _mm_srli_epi16(col3, 8);

			col3 = _mm_packus_epi16(col3, zero);

			_mm_stream_si128((__m128i *restrict)(d + 0), _mm_unpacklo_epi64(col1, col3));
			//_mm_storeu_si128((__m128i *restrict)(d + 0), _mm_unpacklo_epi64(col1, col3));

			col1 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[4]/256u)));
			col2 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[5]/256u)));
			col1 = _mm_unpacklo_epi32(col1, col2);
			col2 = col1;
			col1 = _mm_unpacklo_epi8(col1, zero);
			col2 = _mm_unpackhi_epi8(col2, zero);

			v1   = _mm_set1_epi32(*(const uint32_t *)(s + 4));
			v1   = _mm_and_si128(v1, mask);
			col2 = _mm_mullo_epi16(col2, v1);
			v1s  = _mm_sub_epi16(sub, v1);
			col1 = _mm_mullo_epi16(col1, v1s);
			col1 = _mm_add_epi16(col1, col2);
			col1 = _mm_srli_epi16(col1, 8);

			col1 = _mm_packus_epi16(col1, zero);


			col3 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[6]/256u)));
			col4 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[7]/256u)));

			col3 = _mm_unpacklo_epi32(col3, col4);
			col4 = col3;
			col3 = _mm_unpacklo_epi8(col3, zero);
			col4 = _mm_unpackhi_epi8(col4, zero);

			v2   = _mm_set1_epi32(*(const uint32_t *)(s + 6));//_mm_set1_epi32(s[2] | (s[3] << 16));
			v2   = _mm_and_si128(v2, mask);
			col4 = _mm_mullo_epi16(col4, v2);
			v2s  = _mm_sub_epi16(sub, v2);
			col3 = _mm_mullo_epi16(col3, v2s);
			col3 = _mm_add_epi16(col3, col4);
			col3 = _mm_srli_epi16(col3, 8);

			col3 = _mm_packus_epi16(col3, zero);

			_mm_stream_si128((__m128i *restrict)(d + 4), _mm_unpacklo_epi64(col1, col3));
			//_mm_storeu_si128((__m128i *restrict)(d + 4), _mm_unpacklo_epi64(col1, col3));

		}

		// we'll never have less than 4 pixels to do at the end because w is
		// always divisible by 16 (and therefore 8, so since we only did 0 - 3
		// pixels to force alignment and the loop does 8 pixels per iteration
		// we end up with 4 - 8 pixels that need doing at loop exit)
		__m128i col1, col2, col3, col4, v1, v2, v1s, v2s;
		col1 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[0]/256u)));
		col2 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[1]/256u)));
		col1 = _mm_unpacklo_epi32(col1, col2);
		col2 = col1;
		col1 = _mm_unpacklo_epi8(col1, zero);
		col2 = _mm_unpackhi_epi8(col2, zero);

		v1   = _mm_set1_epi32(*(const uint32_t *)(s + 0));
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

		v2   = _mm_set1_epi32(*(const uint32_t *)(s + 2));//_mm_set1_epi32(s[2] | (s[3] << 16));
		v2   = _mm_and_si128(v2, mask);
		col4 = _mm_mullo_epi16(col4, v2);
		v2s  = _mm_sub_epi16(sub, v2);
		col3 = _mm_mullo_epi16(col3, v2s);
		col3 = _mm_add_epi16(col3, col4);
		col3 = _mm_srli_epi16(col3, 8);

		col3 = _mm_packus_epi16(col3, zero);
		_mm_stream_si128((__m128i *restrict)(d + 0), _mm_unpacklo_epi64(col1, col3));

		x+=4;
		s+=4;
		d+=4;

		switch(w-x) {
			case 4:
				col1 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[0]/256u)));
				col2 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[1]/256u)));
				col1 = _mm_unpacklo_epi32(col1, col2);
				col2 = col1;
				col1 = _mm_unpacklo_epi8(col1, zero);
				col2 = _mm_unpackhi_epi8(col2, zero);

				v1   = _mm_set1_epi32(*(const uint32_t *)(s + 0));
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

				v2   = _mm_set1_epi32(*(const uint32_t *)(s + 2));//_mm_set1_epi32(s[2] | (s[3] << 16));
				v2   = _mm_and_si128(v2, mask);
				col4 = _mm_mullo_epi16(col4, v2);
				v2s  = _mm_sub_epi16(sub, v2);
				col3 = _mm_mullo_epi16(col3, v2s);
				col3 = _mm_add_epi16(col3, col4);
				col3 = _mm_srli_epi16(col3, 8);

				col3 = _mm_packus_epi16(col3, zero);

				_mm_stream_si128((__m128i *restrict)(d + 0), _mm_unpacklo_epi64(col1, col3));
			break;
			case 2:
				col1 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[0]/256u)));
				col2 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[1]/256u)));
				col1 = _mm_unpacklo_epi32(col1, col2);
				col2 = col1;
				col1 = _mm_unpacklo_epi8(col1, zero);
				col2 = _mm_unpackhi_epi8(col2, zero);

				v1   = _mm_set1_epi32(*(const uint32_t *)(s + 0));
				v1   = _mm_and_si128(v1, mask);
				col2 = _mm_mullo_epi16(col2, v1);
				v1s  = _mm_sub_epi16(sub, v1);
				col1 = _mm_mullo_epi16(col1, v1s);
				col1 = _mm_add_epi16(col1, col2);
				col1 = _mm_srli_epi16(col1, 8);

				col1 = _mm_packus_epi16(col1, zero);
				_mm_stream_si64((int64_t *restrict)d, _mm_cvtsi128_si64(col1));
			break;
			case 3:
				col1 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[0]/256u)));
				col2 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[1]/256u)));
				col1 = _mm_unpacklo_epi32(col1, col2);
				col2 = col1;
				col1 = _mm_unpacklo_epi8(col1, zero);
				col2 = _mm_unpackhi_epi8(col2, zero);

				v1   = _mm_set1_epi32(*(const uint32_t *)(s + 0));
				v1   = _mm_and_si128(v1, mask);
				col2 = _mm_mullo_epi16(col2, v1);
				v1s  = _mm_sub_epi16(sub, v1);
				col1 = _mm_mullo_epi16(col1, v1s);
				col1 = _mm_add_epi16(col1, col2);
				col1 = _mm_srli_epi16(col1, 8);

				col1 = _mm_packus_epi16(col1, zero);
				_mm_stream_si64((int64_t *restrict)d, _mm_cvtsi128_si64(col1));

				s+=2;
				d+=2;
			//fall through
			case 1:
				col1 = _mm_loadl_epi64((const __m128i *restrict)(pal+(s[0]/256u)));
				col1 = _mm_unpacklo_epi32(col1, zero);
				col2 = col1;
				col1 = _mm_unpacklo_epi8(col1, zero);
				col2 = _mm_unpackhi_epi8(col2, zero);

				v1   = _mm_set1_epi32(*(const uint32_t *)(s + 0));
				v1   = _mm_and_si128(v1, mask);
				col2 = _mm_mullo_epi16(col2, v1);
				v1s  = _mm_sub_epi16(sub, v1);
				col1 = _mm_mullo_epi16(col1, v1s);
				col1 = _mm_add_epi16(col1, col2);
				col1 = _mm_srli_epi16(col1, 8);

				col1 = _mm_packus_epi16(col1, zero);
				_mm_stream_si32((int *)d, _mm_cvtsi128_si32(col1));
			case 0:
				break;
			default:
				unreachable(); // can't happen
		}
	}

	_mm_sfence(); // needed because of the non-temporal stores.
}
#elif defined(__MMX__)
//TODO: split out an SSE1 version?
static void pallet_blit32(uint32_t *restrict dest, unsigned int dst_stride, const uint16_t *restrict src, unsigned int w, unsigned int h, const uint32_t *restrict pal)
{
	const __m64 zero = (__m64)(0);
	const __m64 mask = (__m64)(0x00ff00ff00ff);
	const __m64 sub = (__m64)(0x010001000100);

	for(size_t y = 0; y < h; y++) {
		const uint16_t *restrict s = src + y*w;
		uint32_t *restrict d = (uint32_t *restrict)((char *restrict)dest + y*dst_stride);
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
#if defined(__SSE__)
			_mm_stream_pi((__m64 *)(d+0), tmp);
#else
			*(__m64 *)(d + 0) = tmp;
#endif

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
#if defined(__SSE__)
			_mm_stream_pi((__m64 *)(d+2), tmp);
#else
			*(__m64 *)(d + 2) = tmp;
#endif
		}
	}

	_mm_empty();
#if defined(__SSE__)
	_mm_sfence(); // needed because of the non-temporal stores.
#endif
}
#else
static void pallet_blit32(uint8_t *restrict dest, unsigned int dst_stride, const uint16_t *restrict src, unsigned int w, unsigned int h, const uint32_t *restrict pal)
{
	for(unsigned int y = 0; y < h; y++) {
		uint32_t * restrict d = (uint32_t *restrict)(dest + y*dst_stride);
		const uint16_t *restrict s = src + y*w;
		for(unsigned int x = 0; x < w; x+=16) {
			*(d++) = pal[*(s++)>>8];
			*(d++) = pal[*(s++)>>8];
			*(d++) = pal[*(s++)>>8];
			*(d++) = pal[*(s++)>>8];

			*(d++) = pal[*(s++)>>8];
			*(d++) = pal[*(s++)>>8];
			*(d++) = pal[*(s++)>>8];
			*(d++) = pal[*(s++)>>8];

			*(d++) = pal[*(s++)>>8];
			*(d++) = pal[*(s++)>>8];
			*(d++) = pal[*(s++)>>8];
			*(d++) = pal[*(s++)>>8];

			*(d++) = pal[*(s++)>>8];
			*(d++) = pal[*(s++)>>8];
			*(d++) = pal[*(s++)>>8];
			*(d++) = pal[*(s++)>>8];
		}
	}
}
#endif

// needs _mm_shuffle_pi16 no other sse/3dnow stuff used
#if defined(__SSE__) || defined(__3dNOW__)
static void pallet_blit565(uint8_t *restrict dest, unsigned int dst_stride, const uint16_t *restrict src, unsigned int src_stride, unsigned int w, unsigned int h, const uint32_t *restrict pal)
{
	for(unsigned int y = 0; y < h; y++) {
		for(unsigned int x = 0; x < w; x+=4) {
			__m64 r1, r2, g1, g2, b1, b2, c;
			int v = src[y*src_stride + x];
			c  = _mm_cvtsi32_si64(pal[v/256u]);
			r1 = _mm_shuffle_pi16(c, 0xfd);
			g1 = _mm_shuffle_pi16(c, 0xfc);
			c  = _mm_slli_pi32(c, 8);
			b1 = _mm_shuffle_pi16(c, 0xfc);

			v=src[y*src_stride + x+1];
			c  = _mm_cvtsi32_si64(pal[v/256u]);
			r2 = _mm_shuffle_pi16(c, 0xf7);
			g2 = _mm_shuffle_pi16(c, 0xf3);
			c  = _mm_slli_pi32(c, 8);
			b2 = _mm_shuffle_pi16(c, 0xf3);
			r1 = _mm_or_si64(r1, r2);
			g1 = _mm_or_si64(g1, g2);
			b1 = _mm_or_si64(b1, b2);

			v=src[y*src_stride + x+2];
			c  = _mm_cvtsi32_si64(pal[v/256u]);
			r2 = _mm_shuffle_pi16(c, 0xdf);
			g2 = _mm_shuffle_pi16(c, 0xcf);
			c  = _mm_slli_pi32(c, 8);
			b2 = _mm_shuffle_pi16(c, 0xcf);
			r1 = _mm_or_si64(r1, r2);
			g1 = _mm_or_si64(g1, g2);
			b1 = _mm_or_si64(b1, b2);

			v=src[y*src_stride + x+3];
			c  = _mm_cvtsi32_si64(pal[v/256u]);
			r2 = _mm_shuffle_pi16(c, 0x7f);
			g2 = _mm_shuffle_pi16(c, 0x3f);
			c  = _mm_slli_pi32(c, 8);
			b2 = _mm_shuffle_pi16(c, 0x3f);
			r1 = _mm_or_si64(r1, r2);
			g1 = _mm_or_si64(g1, g2);
			b1 = _mm_or_si64(b1, b2);

			r1 = _mm_srli_pi16(r1, 3);
			g1 = _mm_srli_pi16(g1, 10);
			b1 = _mm_srli_pi16(b1, 11);

			r1 = _mm_slli_pi16(r1, 11);
			g1 = _mm_slli_pi16(g1, 5);

			r1 = _mm_or_si64(r1, g1);
			r1 = _mm_or_si64(r1, b1);

			*(__m64 *)(dest + y*dst_stride + x*2) = r1;
		}
	}

	_mm_empty();
}

static void pallet_blit555(uint8_t *restrict dest, unsigned int dst_stride, const uint16_t *restrict src, unsigned int src_stride, unsigned int w, unsigned int h, const uint32_t *restrict pal)
{
	for(unsigned int y = 0; y < h; y++) {
		for(unsigned int x = 0; x < w; x+=4) {
			int v;
			__m64 r1, r2, g1, g2, b1, b2, c;

			v  = src[y*src_stride + x];
			c  = _mm_cvtsi32_si64(pal[v/256u]);
			r1 = _mm_shuffle_pi16(c, 0xfd);
			g1 = _mm_shuffle_pi16(c, 0xfc);
			c  = _mm_slli_pi32(c, 8);
			b1 = _mm_shuffle_pi16(c, 0xfc);

			v  = src[y*src_stride + x+1];
			c  = _mm_cvtsi32_si64(pal[v/256u]);
			r2 = _mm_shuffle_pi16(c, 0xf7);
			g2 = _mm_shuffle_pi16(c, 0xf3);
			c  = _mm_slli_pi32(c, 8);
			b2 = _mm_shuffle_pi16(c, 0xf3);
			r1 = _mm_or_si64(r1, r2);
			g1 = _mm_or_si64(g1, g2);
			b1 = _mm_or_si64(b1, b2);

			v  = src[y*src_stride + x+2];
			c  = _mm_cvtsi32_si64(pal[v/256u]);
			r2 = _mm_shuffle_pi16(c, 0xdf);
			g2 = _mm_shuffle_pi16(c, 0xcf);
			c  = _mm_slli_pi32(c, 8);
			b2 = _mm_shuffle_pi16(c, 0xcf);
			r1 = _mm_or_si64(r1, r2);
			g1 = _mm_or_si64(g1, g2);
			b1 = _mm_or_si64(b1, b2);

			v  = src[y*src_stride + x+3];
			c  = _mm_cvtsi32_si64(pal[v/256u]);
			r2 = _mm_shuffle_pi16(c, 0x7f);
			g2 = _mm_shuffle_pi16(c, 0x3f);
			c  = _mm_slli_pi32(c, 8);
			b2 = _mm_shuffle_pi16(c, 0x3f);
			r1 = _mm_or_si64(r1, r2);
			g1 = _mm_or_si64(g1, g2);
			b1 = _mm_or_si64(b1, b2);

			r1 = _mm_srli_pi16(r1, 3);
			g1 = _mm_srli_pi16(g1, 11);
			b1 = _mm_srli_pi16(b1, 11);

			r1 = _mm_slli_pi16(r1, 10);
			g1 = _mm_slli_pi16(g1, 5);

			r1 = _mm_or_si64(r1, g1);
			r1 = _mm_or_si64(r1, b1);

			*(__m64 *)(dest + y*dst_stride + x*2) = r1;
		}
	}

	_mm_empty();
}
#else //TODO: test these
#ifdef __MMX__
#warning no mmx for 16-bit modes (needs extras added in 3dnow or SSE)!
#endif
static void pallet_blit565(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal)
{
	for(unsigned int y = 0; y < h; y++) {
		for(unsigned int x = 0; x < w; x++) {
			uint32_t cl = pal[src[y*src_stride + x]>>8];
			uint16_t px = (cl>>3)&0x1f;
			px = px | (((cl>>10)&0x3f)<<5);
			px = px | (((cl>>19)&0x1f)<<11);
			*(uint16_t *)(dest + y*dst_stride + x*4) = px;
		}
	}
}
static void pallet_blit555(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal)
{
	for(unsigned int y = 0; y < h; y++) {
		for(unsigned int x = 0; x < w; x++) {
			uint32_t cl = pal[src[y*src_stride + x]>>8];
			uint16_t px = (cl>>3)&0x1f;
			px = px | (((cl>>11)&0x1f)<<5);
			px = px | (((cl>>19)&0x1f)<<10);
			*(uint16_t *)(dest + y*dst_stride + x*4) = px;
		}
	}
}
#endif

#if defined(__MMX__)
static void pallet_blit8(uint8_t* restrict dest, unsigned int dst_stride,
		const uint16_t *restrict src, unsigned int src_stride, unsigned int w, unsigned int h)
{
	for(unsigned int y = 0; y < h; y++) {
		for(unsigned int x = 0; x < w; x+=16) {
			__m64 p1, p2;

			p1 = *(const __m64 *)(src + y*src_stride + x);
			p1  = _mm_srli_pi16(p1, 8);
			p2 = *(const __m64 *)(src + y*src_stride + x+4);
			p2  = _mm_srli_pi16(p2, 8);

			p1 = _mm_packs_pu16(p1, p2);
			*(__m64 *)(dest + y*dst_stride + x) = p1;

			p1 = *(const __m64 *)(src + y*src_stride + x+8);
			p1  = _mm_srli_pi16(p1, 8);
			p2 = *(const __m64 *)(src + y*src_stride + x+12);
			p2  = _mm_srli_pi16(p2, 8);

			p1 = _mm_packs_pu16(p1, p2);
			*(__m64 *)(dest + y*dst_stride + x+ 8) = p1;
		}
	}

	_mm_empty();
}
#else
static void pallet_blit8(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h)
{
	for(unsigned int y = 0; y < h; y++)
		for(unsigned int x = 0; x < w; x++)
			*(dest + y*dst_stride + x*4) = src[y*src_stride + x]>>8;
}
#endif

void pallet_blit_Pixbuf(Pixbuf* dst, const uint16_t* restrict src, int w, int h, const uint32_t *restrict pal)
{
	const unsigned  int src_stride = w;
	w = IMIN(w, dst->w);
	h = IMIN(h, dst->h);

	if(dst->bpp == 32) pallet_blit32(dst->data, dst->pitch, src, w, h, pal);
	else if(dst->bpp == 16) pallet_blit565(dst->data, dst->pitch, src, src_stride, w, h, pal);
	else if(dst->bpp == 15) pallet_blit555(dst->data, dst->pitch, src, src_stride, w, h, pal);
	else if(dst->bpp == 8) { // need to set surface's pallet
		pallet_blit8(dst->data, dst->pitch, src, src_stride, w, h);
	}
}

#ifdef USE_SDL
#include <SDL.h>
void pallet_blit_SDL(SDL_Surface *dst, const uint16_t* restrict src, int w, int h, const uint32_t *restrict pal)
{
	const unsigned int src_stride = w;
	w = IMIN(w, dst->w);
	h = IMIN(h, dst->h);

	if((SDL_MUSTLOCK(dst) && SDL_LockSurface(dst) < 0) || w < 0 || h < 0) return;
	if(dst->format->BitsPerPixel == 32) pallet_blit32(dst->pixels, dst->pitch, src, w, h, pal);
	else if(dst->format->BitsPerPixel == 16) pallet_blit565(dst->pixels, dst->pitch, src, src_stride, w, h, pal);
	else if(dst->format->BitsPerPixel == 15) pallet_blit555(dst->pixels, dst->pitch, src, src_stride, w, h, pal);
	else if(dst->format->BitsPerPixel == 8) { // need to set surface's pallet
		pallet_blit8(dst->pixels, dst->pitch, src, src_stride, w, h);
		SDL_SetColors(dst, (void *)pal, 0, 256);
	}
	if(SDL_MUSTLOCK(dst)) SDL_UnlockSurface(dst);
}
#endif

