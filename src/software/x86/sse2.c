
#if (__x86_64__ || __i386__)
#pragma GCC target("no-sse3")

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

		v1 = mbdst[0], t1 = mbsrc[0];
		v1 = _mm_add_epi16(v1, off); t1 = _mm_add_epi16(t1, off);
		v1 = _mm_max_epi16(v1, t1);
		v1 = _mm_sub_epi16(v1, off);
		mbdst[0]=v1;

		v2 = mbdst[1], t2 = mbsrc[1];
		v2 = _mm_add_epi16(v2, off); t2 = _mm_add_epi16(t2, off);
		v2 = _mm_max_epi16(v2, t2);
		v2 = _mm_sub_epi16(v2, off);
		mbdst[1]=v2;

		v3 = mbdst[2], t3 = mbsrc[2];
		v3 = _mm_add_epi16(v3, off); t3 = _mm_add_epi16(t3, off);
		v3 = _mm_max_epi16(v3, t3);
		v3 = _mm_sub_epi16(v3, off);
		mbdst[2]=v3;

		v4 = mbdst[3], t4 = mbsrc[3];
		v4 = _mm_add_epi16(v4, off); t4 = _mm_add_epi16(t4, off);
		v4 = _mm_max_epi16(v4, t4);
		v4 = _mm_sub_epi16(v4, off);
		mbdst[3]=v4;
	}
}

//TODO: test if going up to a full 16 pixel at a time loop is faster, could be due to
// that being a cache line size chuck
__attribute__((hot))
void pallet_blit32_sse2(uint8_t *restrict dest, unsigned int dst_stride, const uint16_t *restrict src, unsigned int src_stride, unsigned int w, unsigned int h, const uint32_t *restrict pal)
{
	// TODO: _mm_loadl_epi64 is dumb, it takes a pointer to __m128i rather than a uint64_t or __m64, figure out a way around using it.
	// TODO: set the constants with a literal rather than a function (except maybe zero)
	//FIXME: add src_stride back in
	const __m128i zero = _mm_setzero_si128();
	const __m128i mask = _mm_set1_epi16(0xff);
	const __m128i sub = _mm_set1_epi16(256);

	for(size_t y = 0; y < h; y++) {
		const uint16_t *restrict s = src + y*w;
		uint32_t *restrict d = (uint32_t *restrict)(dest + y*dst_stride);
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
#if __x86_64__  //TODO: test which of these is fastest on x86_64
				_mm_stream_si64((int64_t *)d, _mm_cvtsi128_si64(col1)); // x86_64 only
#else
				_mm_stream_pi((__m64 *)d, _mm_movepi64_pi64(col1));
#endif
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
#if __x86_64__  //TODO: test which of these is fastest on x86_64
				_mm_stream_si64((int64_t *)d, _mm_cvtsi128_si64(col1)); // x86_64 only
#else
				_mm_stream_pi((__m64 *)d, _mm_movepi64_pi64(col1));
#endif
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

			_mm_stream_si128((__m128i *)(d + 0), _mm_unpacklo_epi64(col1, col3));
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

			_mm_stream_si128((__m128i *)(d + 4), _mm_unpacklo_epi64(col1, col3));
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
		_mm_stream_si128((__m128i *)(d + 0), _mm_unpacklo_epi64(col1, col3));

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

				_mm_stream_si128((__m128i *)(d + 0), _mm_unpacklo_epi64(col1, col3));
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
#if __x86_64__  //TODO: test which of these is fastest on x86_64
				_mm_stream_si64((int64_t *)d, _mm_cvtsi128_si64(col1)); // x86_64 only
#else
				_mm_stream_pi((__m64 *)d, _mm_movepi64_pi64(col1));
#endif
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
#if __x86_64__  //TODO: test which of these is fastest on x86_64
				_mm_stream_si64((int64_t *)d, _mm_cvtsi128_si64(col1)); // x86_64 only
#else
				_mm_stream_pi((__m64 *)d, _mm_movepi64_pi64(col1));
#endif
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
#if !__x86_64__
	_mm_empty();
#endif
	_mm_sfence(); // needed because of the non-temporal stores.
}

#endif
