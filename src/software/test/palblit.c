
#include "common.h"
#include "software/pixmisc.h"
#include "pallet.h"
#include "software/pixformat.h"




void pallet_blit32_fallback(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal);
void pallet_blit565_fallback(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal);
void pallet_blit555_fallback(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal);
void pallet_blit8_fallback(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h);

#if (__x86_64__ || __i386__)

#include "../x86/x86_features.h"

int main()
{
	uint64_t feat = x86feat_get_features();

	uint16_t *src = malloc(sizeof(*src)*UINT16_MAX);
	uint8_t *ref  = malloc(sizeof(uint32_t)*UINT16_MAX);
	uint8_t *test = malloc(sizeof(uint32_t)*UINT16_MAX);
	uint32_t *pal = malloc(sizeof(uint32_t)*257);

	for(long i=0; i<UINT16_MAX; i++) {
		src[i] = i;
	}

	for(int i=0; i<256; i++) {
		pal[i] = i + (((i%2)*128)<<8) + (i<<16);
	}
	pal[256] = pal[255];

	//TODO: add tests against a reference image so we have something to do even
	// if there's no additional implementations

	// test 32 bit blits
	printf("Testing 32 (rgbx8888) bit output\n");
	pallet_blit32_fallback(ref, UINT16_MAX, src, UINT16_MAX, UINT16_MAX, 1, pal);

#if !defined(__x86_64__)
	if(feat & X86FEAT_MMX) {
		pallet_blit32_mmx(test, UINT16_MAX, src, UINT16_MAX, UINT16_MAX, 1, pal);
		if(memcmp(ref, test, sizeof(uint32_t)*UINT16_MAX) != 0) {
			printf("MMX 32 blit failed!\n");
		}
	}
	if(feat & X86FEAT_MMXEXT) {
		pallet_blit32_3dnow(test, UINT16_MAX, src, UINT16_MAX, UINT16_MAX, 1, pal);
		if(memcmp(ref, test, sizeof(uint32_t)*UINT16_MAX) != 0) {
			printf("3dNOW 32 blit failed!\n");
		}
	}
#endif
	if(feat & X86FEAT_SSE) {
		pallet_blit32_sse(test, UINT16_MAX, src, UINT16_MAX, UINT16_MAX, 1, pal);
		if(memcmp(ref, test, sizeof(uint32_t)*UINT16_MAX) != 0) {
			printf("SSE 32 blit failed!\n");
		}
	}
	if(feat & X86FEAT_SSE2) {
		pallet_blit32_sse2(test, UINT16_MAX, src, UINT16_MAX, UINT16_MAX, 1, pal);
		if(memcmp(ref, test, sizeof(uint32_t)*UINT16_MAX) != 0) {
			printf("SSE2 32 blit failed!\n");
		}
	}
	if(feat & X86FEAT_SSSE3) {
		pallet_blit32_ssse3(test, UINT16_MAX, src, UINT16_MAX, UINT16_MAX, 1, pal);
		if(memcmp(ref, test, sizeof(uint32_t)*UINT16_MAX) != 0) {
			printf("SSSE2 32 blit failed!\n");
		}
	}
	if(feat & X86FEAT_AVX2) {
		pallet_blit32_avx2(test, UINT16_MAX, src, UINT16_MAX, UINT16_MAX, 1, pal);
		if(memcmp(ref, test, sizeof(uint32_t)*UINT16_MAX) != 0) {
			printf("AVX2 32 blit failed!\n");
		}
	}

	printf("32 bit tests complete\n");

	printf("Testing 16 (rgb565) bit output\n");
	pallet_blit565_fallback(ref, UINT16_MAX, src, UINT16_MAX, UINT16_MAX, 1, pal);

#if !defined(__x86_64__)
	if(feat & X86FEAT_MMX) {
		pallet_blit565_mmx(test, UINT16_MAX, src, UINT16_MAX, UINT16_MAX, 1, pal);
		if(memcmp(ref, test, sizeof(uint16_t)*UINT16_MAX) != 0) {
			printf("MMX 16 blit failed!\n");
		}
	}
	if(feat & X86FEAT_MMXEXT) {
		pallet_blit565_3dnow(test, UINT16_MAX, src, UINT16_MAX, UINT16_MAX, 1, pal);
		if(memcmp(ref, test, sizeof(uint16_t)*UINT16_MAX) != 0) {
			printf("3dNOW 16 blit failed!\n");
		}
	}
#endif
	if(feat & X86FEAT_SSE) {
		pallet_blit565_sse(test, UINT16_MAX, src, UINT16_MAX, UINT16_MAX, 1, pal);
		if(memcmp(ref, test, sizeof(uint16_t)*UINT16_MAX) != 0) {
			printf("SSE 16 blit failed!\n");
		}
	}

		printf("Testing 15 (rgb555) bit output\n");
	pallet_blit555_fallback(ref, UINT16_MAX, src, UINT16_MAX, UINT16_MAX, 1, pal);

#if !defined(__x86_64__)
	if(feat & X86FEAT_MMX) {
		pallet_blit555_mmx(test, UINT16_MAX, src, UINT16_MAX, UINT16_MAX, 1, pal);
		if(memcmp(ref, test, sizeof(uint16_t)*UINT16_MAX) != 0) {
			printf("MMX 15 blit failed!\n");
		}
	}
	if(feat & X86FEAT_MMXEXT) {
		pallet_blit555_3dnow(test, UINT16_MAX, src, UINT16_MAX, UINT16_MAX, 1, pal);
		if(memcmp(ref, test, sizeof(uint16_t)*UINT16_MAX) != 0) {
			printf("3dNOW 15 blit failed!\n");
		}
	}
#endif
	if(feat & X86FEAT_SSE) {
		pallet_blit555_sse(test, UINT16_MAX, src, UINT16_MAX, UINT16_MAX, 1, pal);
		if(memcmp(ref, test, sizeof(uint16_t)*UINT16_MAX) != 0) {
			printf("SSE 15 blit failed!\n");
		}
	}

}

#else
int main()
{
	return 0;
}
#endif