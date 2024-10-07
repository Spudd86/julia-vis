#include "common.h"

#include <float.h>

//TODO: pick a pixel location convention and fix all the code in here to use it
// probably want to use pixels have their co-ords at the centre and are a box around it
// because it'll probably mean that we can stop worrying about FLT_EPISILON in the 
// clamp step since 1.0 would be at the edge of the image so we run from [0+<pixel width>/2. 1-<pixel width>/2]
// or we pick pixel location is upper left in hopes that that is less calculation


// could probably make map fly with AVX2
// NOTE sse4.2 adds these for __m128i types too
// _mm256_mask_i32gather_epi32 to load, will get two adjacent pixels
// _mm256_mulhi_epu16 exists! Shame there's no _mm256_madd_epu16

// AVX512 has neat stores in it _mm256_mask_cvtepi16_storeu_epi8, _mm_mask_compressstoreu_epi32 and friends
// particularly the cvtepi ones in pallet blit lets us skip the shift+pack, doesn't seem to come with streaming version though
// _mm_mask_expandloadu_epi32 expand is like the compress but other way

// _mm_multishift_epi64_epi8 seems perfect for putting fixed point numbers back together

//TODO: streaming writes?

__attribute__((hot,flatten,always_inline,nonnull))
static inline uint16_t bilin_samp(const uint16_t *restrict in, int w, int h, float x, float y)
{
	// Conversion to int truncates, which in this case is exactly what we want
	// since we clamp to the largest float smaller than 1.0f when multiply by w*256
	// and convert to int we get an int x ∈ [0, w*256)
	int32_t xs = fmaxf(fminf(x, 1.0f-FLT_EPSILON), 0.0f)*w*256;
	int32_t ys = fmaxf(fminf(y, 1.0f-FLT_EPSILON), 0.0f)*h*256;
	int32_t x1 = xs>>8, x2 = IMIN(x1+1, w-1);
	int32_t y1 = ys>>8, y2 = IMIN(y1+1, h-1);
	uint_fast32_t xf = xs&0xFF, yf = ys&0xFF;

	uint_fast32_t p00 = in[y1*w + x1];
	uint_fast32_t p01 = in[y1*w + x2];
	uint_fast32_t p10 = in[y2*w + x1];
	uint_fast32_t p11 = in[y2*w + x2];

#if 0 // TODO: use top version if we don't have fast 64bit ints
	// it is critical that this entire calculation be done as at least uint32s
	// otherwise it overflows
	uint_fast32_t v = ((p00*(256u - xf) + p01*xf)*(256u-yf) +
			           (p10*(256u - xf) + p11*xf)*yf) >> 16u;
	v = (v*((256u*97u)/100u)) >> 8u; // fade
#else
	uint_fast64_t v = ((p00*(256u - xf) + p01*xf)*(256u-yf) +
			           (p10*(256u - xf) + p11*xf)*yf);
	v = (v*((256u*97u)/100u)) >> 24u; // fade
#endif
	return v;
}

/*
 * We are interpolating uv across an 8x8 square so in the inner loop we want
 *   u =  ((u00*(BLOCK_SIZE - yt) + u10*yt)*(BLOCK_SIZE - xt)
 *       + (u01*(BLOCK_SIZE - yt) + u11*yt)*xt)/(BLOCK_SIZE*BLOCK_SIZE);
 * which is the straight forward bi-linear interpolation.
 *
 * (u00*(BLOCK_SIZE - yt) + u10*yt) and (u01*(BLOCK_SIZE - yt) + u11*yt)
 * are invariant inside the loop so hoist them out as u0 and u1
 *   u = (u0*(BLOCK_SIZE - xt) + u1*xt)/(BLOCK_SIZE*BLOCK_SIZE);
 * re-arrange that to get
 *   u = (u0*BLOCK_SIZE - u0*xt + u1*xt)/(BLOCK_SIZE*BLOCK_SIZE);
 *   u = (u0*BLOCK_SIZE + (u1 - u0)*xt)/(BLOCK_SIZE*BLOCK_SIZE);
 * so we can convert to 
 *   ui = u0*BLOCK_SIZE 
 *   for(...) {
 *      u = ui/(BLOCK_SIZE*BLOCK_SIZE);
 *      // loop body
 *      ui = ui + u1 - u0;
 *   }
 * We can do the same optimisation on the outer loop for u0 and u1 too
 */
__attribute__((hot,flatten,always_inline,nonnull))
static inline void block_interp_bilin(uint16_t *restrict out, const uint16_t *restrict in, int w, int h, int xd, int yd, float x00, float y00, float x01, float y01, float x10, float y10, float x11, float y11)
{
	const uint_fast32_t clamph = (h-1)*w;

	const uint_fast32_t u00 = IMIN(IMAX((int_fast32_t)(x00*(w*256)), 0), w*256-1);
	const uint_fast32_t v00 = IMIN(IMAX((int_fast32_t)(y00*(h*256)), 0), h*256-1);
	const uint_fast32_t u01 = IMIN(IMAX((int_fast32_t)(x01*(w*256)), 0), w*256-1);
	const uint_fast32_t v01 = IMIN(IMAX((int_fast32_t)(y01*(h*256)), 0), h*256-1);
	const uint_fast32_t u10 = IMIN(IMAX((int_fast32_t)(x10*(w*256)), 0), w*256-1);
	const uint_fast32_t v10 = IMIN(IMAX((int_fast32_t)(y10*(h*256)), 0), h*256-1);
	const uint_fast32_t u11 = IMIN(IMAX((int_fast32_t)(x11*(w*256)), 0), w*256-1);
	const uint_fast32_t v11 = IMIN(IMAX((int_fast32_t)(y11*(h*256)), 0), h*256-1);

	uint_fast32_t u0 = u00*BLOCK_SIZE;
	uint_fast32_t u1 = u01*BLOCK_SIZE;
	uint_fast32_t v0 = v00*BLOCK_SIZE;
	uint_fast32_t v1 = v01*BLOCK_SIZE;

	uint16_t *restrict out_line = out + yd*w + xd;
	for(int yt=0; yt<BLOCK_SIZE; yt++, out_line+=w) {
		uint_fast32_t ui = u0*BLOCK_SIZE;
		uint_fast32_t vi = v0*BLOCK_SIZE;
		for(uint_fast32_t xt=0; xt<BLOCK_SIZE; xt++) {
			const uint_fast32_t u = ui/(BLOCK_SIZE*BLOCK_SIZE);
			const uint_fast32_t v = vi/(BLOCK_SIZE*BLOCK_SIZE);

			const uint_fast32_t xs=u >> 8, ys=v >> 8;
			const uint_fast32_t xf=u&0xFF, yf=v&0xFF;

			const uint_fast32_t xi1 = xs, yi1 = ys*w;
			const uint_fast32_t xi2 = IMIN(xi1+1, w-1u);
			const uint_fast32_t yi2 = IMIN(yi1+w, clamph);

#if 0 // TODO: use top version if we don't have fast 64bit ints
			uint_fast32_t o = ((in[yi1 + xi1]*(256u - xf) + in[yi1 + xi2]*xf)*(256u-yf) +
			                   (in[yi2 + xi1]*(256u - xf) + in[yi2 + xi2]*xf)*yf) >> 16u;
			o = (o*((256u*97u)/100u)) >> 8u; // fade
#else
			uint_fast64_t o = ((in[yi1 + xi1]*(256u - xf) + in[yi1 + xi2]*xf)*(256u-yf) +
			                   (in[yi2 + xi1]*(256u - xf) + in[yi2 + xi2]*xf)*yf);
			o = (o*((256u*97u)/100u)) >> 24u; // fade
#endif
			*(out_line + xt) = o;

			ui = ui + u1 - u0;
			vi = vi + v1 - v0;
		}
		u0 = u0 - u00 + u10;
		u1 = u1 - u01 + u11;
		v0 = v0 - v00 + v10;
		v1 = v1 - v01 + v11;
	}
}
