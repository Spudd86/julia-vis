//#pragma GCC optimize "inline-functions,unsafe-loop-optimizations,merge-all-constants,fast-math,associative-math,reciprocal-math,no-signed-zeros"
#pragma GCC optimize "3,inline-functions,fast-math,associative-math,reciprocal-math,no-signed-zeros"

#include "common.h"
#include "map.h"

#include <float.h>
#include <assert.h>

#define BLOCK_SIZE 8

// could probably make map fly with AVX2
// NOTE sse4.2 adds these for __m128i types too
// _mm256_mask_i32gather_epi32 to load, will get two adjacent pixels
// _mm256_mulhi_epu16 exists! Shame there's no _mm256_madd_epu16

// below is wrong
// _mm256_hadd_epi16 can be used as part of mullo, mulhi, unpack, hadd sequence though
// _mm256_alignr_epi8 seems tailor made for putting fixed point vectors back together after

// AVX512 has neat stores in it _mm256_mask_cvtepi16_storeu_epi8, _mm_mask_compressstoreu_epi32 and friends
// particularly the cvtepi ones in pallet blit lets us skip the shift+pack, doesn't seem to come with streaming version though
// _mm_mask_expandloadu_epi32 expand is like the compress but other way

// _mm_multishift_epi64_epi8 seems perfect for putting fixed point numbers back together

//TODO: streaming writes?

static inline uint16_t bilin_samp(uint16_t *restrict in, int w, int h, float x, float y)
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
	v = (v*((256u*97u)/100u)) >> 8u; // now that we have fixed bilinear interp need a fade here
#else
	uint_fast64_t v = ((p00*(256u - xf) + p01*xf)*(256u-yf) +
			           (p10*(256u - xf) + p11*xf)*yf);
	v = (v*((256u*97u)/100u)) >> 24u; // now that we have fixed bilinear interp need a fade here
#endif
	return v;
}

#if 0
static inline void block_interp_bilin(uint16_t *restrict out, uint16_t *restrict in, int w, int h, int xd, int yd, float x00, float y00, float x01, float y01, float x10, float y10, float x11, float y11)
{
	const uint_fast32_t clamph = (h-1)*w;

#if 0
	// Conversion to int truncates, which in this case is exactly what we want
	// since we clamp to the largest float smaller than 1.0f when multiply by w*256
	// and convert to int we get an int x ∈ [0, w*256)
	float x0 = fmaxf(fminf(x00, 1.0f-FLT_EPSILON), 0.0f)*(w*256);
	float y0 = fmaxf(fminf(y00, 1.0f-FLT_EPSILON), 0.0f)*(h*256);
	float x1 = fmaxf(fminf(x01, 1.0f-FLT_EPSILON), 0.0f)*(w*256);
	float y1 = fmaxf(fminf(y01, 1.0f-FLT_EPSILON), 0.0f)*(h*256);

	float x0s = (fmaxf(fminf(x10, 1.0f-FLT_EPSILON), 0.0f)*(w*256) - x0)/BLOCK_SIZE;
	float y0s = (fmaxf(fminf(y10, 1.0f-FLT_EPSILON), 0.0f)*(h*256) - y0)/BLOCK_SIZE;
	float x1s = (fmaxf(fminf(x11, 1.0f-FLT_EPSILON), 0.0f)*(w*256) - x1)/BLOCK_SIZE;
	float y1s = (fmaxf(fminf(y11, 1.0f-FLT_EPSILON), 0.0f)*(h*256) - y1)/BLOCK_SIZE;
#else
	int x0 = IMIN(IMAX((int)(x00*w*256), 0), w*256-1);
	int y0 = IMIN(IMAX((int)(y00*h*256), 0), h*256-1);
	int x1 = IMIN(IMAX((int)(x01*w*256), 0), w*256-1);
	int y1 = IMIN(IMAX((int)(y01*h*256), 0), h*256-1);

	int x0s = (IMIN(IMAX((int)(x10*w*256), 0), w*256-1) - x0)/BLOCK_SIZE;
	int x1s = (IMIN(IMAX((int)(x11*w*256), 0), w*256-1) - x1)/BLOCK_SIZE;
	int y0s = (IMIN(IMAX((int)(y10*h*256), 0), h*256-1) - y0)/BLOCK_SIZE;
	int y1s = (IMIN(IMAX((int)(y11*h*256), 0), h*256-1) - y1)/BLOCK_SIZE;
#endif

	// Note: clang breaks this somehow even without -ffast-math, gcc with -ffast-math doesn't...

	uint16_t *restrict out_line = out + yd*w + xd;
	for(int yt=0; yt<BLOCK_SIZE; yt++, x0+=x0s, y0+=y0s, x1+=x1s, y1+=y1s, out_line+=w) {
		int x = x0, y = y0;
		int xst = (x1 - x0)/BLOCK_SIZE;
		int yst = (y1 - y0)/BLOCK_SIZE;

		__builtin_prefetch(out_line, 1, 0); // write prefetch, non-temporal

		#pragma GCC ivdep
		for(uint_fast32_t xt=0; xt<BLOCK_SIZE; xt++, x+=xst, y+=yst) {
			assert(x >= 0); assert((uint_fast32_t)(x)>>8 < w); assert(x < w*256); // in debug builds make sure rounding hasn't messed us up
			assert(y >= 0); assert((uint_fast32_t)(y)>>8 < h); assert(y < h*256);

			uint_fast32_t xs, ys, xf, yf;
			uint_fast32_t xi1, xi2, yi1, yi2;

			xs=(uint_fast32_t)(x) >> 8, ys=(uint_fast32_t)(y) >> 8;
			xf=(uint_fast32_t)(x)&0xFF, yf=(uint_fast32_t)(y)&0xFF;

			xi1 = xs, yi1 = ys*w;
			xi2 = IMIN(xi1+1u, w-1u);
			yi2 = IMIN(yi1+w, clamph);

#if 0 // TODO: use top version if we don't have fast 64bit ints
			uint_fast32_t v = ((in[yi1 + xi1]*(256u - xf) + in[yi1 + xi2]*xf)*(256u-yf) +
			                   (in[yi2 + xi1]*(256u - xf) + in[yi2 + xi2]*xf)*yf) >> 16u;
			v = (v*((256u*97u)/100u)) >> 8u; // now that we have fixed bilinear interp need a fade here
#else
			uint_fast64_t v = ((in[yi1 + xi1]*(256u - xf) + in[yi1 + xi2]*xf)*(256u-yf) +
			                   (in[yi2 + xi1]*(256u - xf) + in[yi2 + xi2]*xf)*yf);
			v = (v*((256u*97u)/100u)) >> 24u; // now that we have fixed bilinear interp need a fade here
#endif
			*(out_line + xt) = v;
		}
	}
}
#else
static inline void block_interp_bilin(uint16_t *restrict out, uint16_t *restrict in, int w, int h, int xd, int yd, float x00, float y00, float x01, float y01, float x10, float y10, float x11, float y11)
{
	//TODO: probably don't want to use this on 32bit x86 because of how expensive
	// float -> int conversion is there
	const uint_fast32_t clamph = (h-1)*w;
	const float u00 = fmaxf(fminf(x00, 1.0f-FLT_EPSILON), 0.0f)*(w*256);
	const float v00 = fmaxf(fminf(y00, 1.0f-FLT_EPSILON), 0.0f)*(h*256);
	const float u01 = fmaxf(fminf(x01, 1.0f-FLT_EPSILON), 0.0f)*(w*256);
	const float v01 = fmaxf(fminf(y01, 1.0f-FLT_EPSILON), 0.0f)*(h*256);
	const float u10 = fmaxf(fminf(x10, 1.0f-FLT_EPSILON), 0.0f)*(w*256);
	const float v10 = fmaxf(fminf(y10, 1.0f-FLT_EPSILON), 0.0f)*(h*256);
	const float u11 = fmaxf(fminf(x11, 1.0f-FLT_EPSILON), 0.0f)*(w*256);
	const float v11 = fmaxf(fminf(y11, 1.0f-FLT_EPSILON), 0.0f)*(h*256);

	float u0     =   u00;
	float v0     =   v00;
	float du0dy  =  (u10 - u00)/BLOCK_SIZE;
	float dv0dy  =  (v10 - v00)/BLOCK_SIZE;
	float dudx   =  (u01 - u00)/BLOCK_SIZE;
	float dvdx   =  (v01 - v00)/BLOCK_SIZE;
	float dudxdy = ((u11 - u10) - (u01 - u00))/(BLOCK_SIZE*BLOCK_SIZE);
	float dvdxdy = ((v11 - v10) - (v01 - v00))/(BLOCK_SIZE*BLOCK_SIZE);

	uint16_t *restrict out_line = out + yd*w + xd;
	for(int yt=0; yt<BLOCK_SIZE; yt++, out_line+=w, u0+=du0dy, v0+=dv0dy, dudx+=dudxdy, dvdx+=dvdxdy) {
		__builtin_prefetch(out_line, 1, 0); // write prefetch, non-temporal
		int_fast32_t u = u0;
		int_fast32_t v = v0;
		int_fast32_t idudx = dudx;
		int_fast32_t idvdx = dvdx;
		#pragma GCC ivdep
		for(uint_fast32_t xt=0; xt<BLOCK_SIZE; xt++, u+=idudx, v+=idvdx) {
			assert(u >= 0); assert((uint_fast32_t)(u)>>8 < w); assert(u < w*256); // in debug builds make sure rounding hasn't messed us up
			assert(v >= 0); assert((uint_fast32_t)(v)>>8 < h); assert(v < h*256);

			uint_fast32_t xs, ys, xf, yf;
			uint_fast32_t xi1, xi2, yi1, yi2;

			xs=((uint_fast32_t)u) >> 8, ys=((uint_fast32_t)v) >> 8;
			xf=((uint_fast32_t)u)&0xFF, yf=((uint_fast32_t)v)&0xFF;

			xi1 = xs, yi1 = ys*w;
			xi2 = IMIN(xi1+1, w-1u);
			yi2 = IMIN(yi1+w, clamph);

#if 0 // TODO: use top version if we don't have fast 64bit ints
			uint_fast32_t o = ((in[yi1 + xi1]*(256u - xf) + in[yi1 + xi2]*xf)*(256u-yf) +
			                   (in[yi2 + xi1]*(256u - xf) + in[yi2 + xi2]*xf)*yf) >> 16u;
			o = (o*((256u*97u)/100u)) >> 8u; // now that we have fixed bilinear interp need a fade here
#else
			uint_fast64_t o = ((in[yi1 + xi1]*(256u - xf) + in[yi1 + xi2]*xf)*(256u-yf) +
			                   (in[yi2 + xi1]*(256u - xf) + in[yi2 + xi2]*xf)*yf);
			o = (o*((256u*97u)/100u)) >> 24u; // now that we have fixed bilinear interp need a fade here
#endif
			*(out_line + xt) = o;
		}
	}
}
#endif

// also for: (use c99 complex support? get's ugly when expanded...)
// z = (z^4 + c1)/(z*z + c2)
//
// where z,c1,c2 are complex numbers. and z starts at x,y
// c1, c2 are beat responsive moving points

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
MAP_FUNC_ATTR void soft_map(uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd)
{
	const float xstep = 2.0f/w, ystep = 2.0f/h;
	const float x0 = (pd->p[0]-0.5f)*0.25f + 0.5f, y0=pd->p[1]*0.25f + 0.5f;
	for(int yd = 0; yd < h; yd++) {
		float v = yd*ystep - 1.0f;
		for(int xd = 0; xd < w; xd++) {
			float u = xd*xstep - 1.0f;
			float y = 2*u*v + y0;
			float x = u*u - v*v + x0;

			*(out++) = bilin_samp(in, w, h, x, y);
		}
	}
}

#if 1
__attribute__((hot))
MAP_FUNC_ATTR void soft_map_interp(uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd)
{
	const float ustep = BLOCK_SIZE*2.0f/w, vstep = BLOCK_SIZE*2.0f/h;
	const float cx0 = (pd->p[0]-0.5f)*0.25f + 0.5f, cy0=pd->p[1]*0.25f + 0.5f;

	float v0 = -1.0f;
	for(int yd = 0; yd < h; yd+=BLOCK_SIZE) {
		const float v1 = v0+vstep;

		float y00 = -2.0f*v0 + cy0;
		float y10 = -2.0f*v1 + cy0;
		float x00 = 1.0f - v0*v0 + cx0;
		float x10 = 1.0f - v1*v1 + cx0;

		float u = ustep - 1.0f;
		for(int xd = 0; xd < w; xd+=BLOCK_SIZE, u+=ustep) {
			const float y01 = 2.0f*u*v0 + cy0;
			const float y11 = 2.0f*u*v1 + cy0;
			const float x01 = u*u - v0*v0 + cx0;
			const float x11 = u*u - v1*v1 + cx0;

			block_interp_bilin(out, in, w, h, xd, yd, x00, y00, x01, y01, x10, y10, x11, y11);

			y00 = y01; y10 = y11;
			x00 = x01; x10 = x11;
		}
		v0=v1;
	}
}
#endif

MAP_FUNC_ATTR void soft_map_butterfly(uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd)
{
	const float xstep = 2.0f/w, ystep = 2.0f/h;
	const float sm = sqrtf(2.5f)/2.5f;
	const float cx0 = pd->p[0]*1.5f/2.5f, cy0 = pd->p[1]*1.5f/2.5f;

	for(int yd = 0; yd < h; yd++) {
		float v = yd*ystep - 1.0f;
		for(int xd = 0; xd < w; xd++) {
			float u = xd*xstep -1.0f;

			float x = 2.5f*v*v - sqrtf(fabsf(u))*sm + cx0;
			float y = 2.5f*u*u - sqrtf(fabsf(v))*sm + cy0;

			x = (x+1.0f)*0.5f; y = (y+1.0f)*0.5f;

			*(out++) = bilin_samp(in, w, h, x, y);
		}
	}
}

MAP_FUNC_ATTR void soft_map_butterfly_interp(uint16_t *restrict out, uint16_t *restrict __attribute__ ((aligned (16))) in, int w, int h, const struct point_data *pd)
{
	const float ustep = 2.0f/w, vstep = 2.0f/h;
	const float sm = sqrtf(2.5f)/2.5f;
	const float cx0 = pd->p[0]*1.5f/2.5f, cy0 = pd->p[1]*1.5f/2.5f;

	for(int yd = 0; yd < h; yd+=BLOCK_SIZE) {
		float v0 = yd*vstep - 1.0f;
		float v1 = v0+vstep;

		float y00 = 2.5f - sqrtf(fabsf(v0))*sm + cy0;
		float y10 = 2.5f - sqrtf(fabsf(v1))*sm + cy0;
		float x00 = 2.5f*v0*v0 - sm + cx0;
		float x10 = 2.5f*v1*v1 - sm + cx0;
		y00 = (y00+1.0f)*0.5f; y10 = (y10+1.0f)*0.5f;
		x00 = (x00+1.0f)*0.5f; x10 = (x10+1.0f)*0.5f;

		for(int xd = 0; xd < w; xd+=BLOCK_SIZE) {
			float u1 = (xd+1)*ustep -1.0f;

			float y01 = 2.5f*u1*u1 - sqrtf(fabsf(v0))*sm + cy0;
			float y11 = 2.5f*u1*u1 - sqrtf(fabsf(v1))*sm + cy0;
			float x01 = 2.5f*v0*v0 - sqrtf(fabsf(u1))*sm + cx0;
			float x11 = 2.5f*v1*v1 - sqrtf(fabsf(u1))*sm + cx0;
			y01 = (y01+1.0f)*0.5f; y11 = (y11+1.0f)*0.5f;
			x01 = (x01+1.0f)*0.5f; x11 = (x11+1.0f)*0.5f;

			block_interp_bilin(out, in, w, h, xd, yd, x00, y00, x01, y01, x10, y10, x11, y11);

			y00 = y01; y10 = y11;
			x00 = x01; x10 = x11;
		}
		v0=v1;
	}
}

MAP_FUNC_ATTR void soft_map_rational(uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd)
{
	const float xoom = 3.0f, moox = 1.0f/xoom;
	float xstep = 2.0f/w, ystep = 2.0f/h;
	const float cx0 = pd->p[0], cy0 = pd->p[1], cx1 = pd->p[2]*2, cy1 = pd->p[3]*2;

	for(int yd = 0; yd < h; yd++) {
		float v = yd*ystep - 1.0f;
		for(int xd = 0; xd < w; xd++) {
			float u = xd*xstep -1.0f;
			float a,b,c,d,sa,sb, cdivt, x, y;

			a=u*xoom; b=v*xoom; sa=a*a; sb=b*b;
			c=sa-sb + cx1; d=2*a*b+cy1;
			b=4*(sa*a*b - a*b*sb) + cy0;  a=sa*sa -6*sa*sb + sb*sb + cx0;
			cdivt = moox/(c*c + d*d);
			x= (a*c + b*d)*cdivt;  y= (a*d + c*b)*cdivt;

			x = (x+1.0f)*0.5f; y = (y+1.0f)*0.5f;

			*(out++) = bilin_samp(in, w, h, x, y);
		}
	}
}

#define infs(a, b) ((isfinite(a))?(a):(b))

MAP_FUNC_ATTR void soft_map_rational_interp(uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd)
{
	const float cx0 = pd->p[0], cy0 = pd->p[1], cx1 = pd->p[2]*2, cy1 = pd->p[3]*2;
	const float xoom = 3.0f, moox = 1.0f/xoom;
	const float ustep = BLOCK_SIZE*2.0f/w, vstep = BLOCK_SIZE*2.0f/h;
	float v0 = -1.0f;
	for(int yd = 0; yd < h; yd+=BLOCK_SIZE) {
		float v1 = v0+vstep;

		float a,b,c,d,sa,sb, cdivt, x, y;

		a=-xoom; b=v0*xoom; sa=a*a; sb=b*b;
		c=sa-sb + cx1; d=2*a*b+cy1;
		b=4*(sa*a*b - a*b*sb) + cy0;  a=sa*sa -6*sa*sb + sb*sb + cx0;
		cdivt = moox/(c*c + d*d);
		x= (a*c + b*d)*cdivt;  y= (a*d + c*b)*cdivt;
		float y00 = (y+1.0f)*0.5f;
		float x00 = (x+1.0f)*0.5f;

		a=-xoom; b=v1*xoom; sa=a*a; sb=b*b;
		c=sa-sb + cx1; d=2*a*b+cy1;
		b=4*(sa*a*b - a*b*sb) + cy0;  a=sa*sa -6*sa*sb + sb*sb + cx0;
		cdivt = moox/(c*c + d*d);
		x= (a*c + b*d)*cdivt;  y= (a*d + c*b)*cdivt;
		float y10 = (y+1.0f)*0.5f;
		float x10 = (x+1.0f)*0.5f;
		float u1 = -1.0f;
		for(int xd = 0; xd < w; xd+=BLOCK_SIZE) {
			u1 = u1+ustep;

			a=u1*xoom; b=v0*xoom; sa=a*a; sb=b*b;
			c=sa-sb + cx1; d=2*a*b+cy1;
			b=4*(sa*a*b - a*b*sb) + cy0;  a=sa*sa -6*sa*sb + sb*sb + cx0;
			cdivt = moox/(c*c + d*d);
			x= (a*c + b*d)*cdivt;  y= (a*d + c*b)*cdivt;
			float y01 = (y+1.0f)*0.5f;
			float x01 = (x+1.0f)*0.5f;

			a=u1*xoom; b=v1*xoom; sa=a*a; sb=b*b;
			c=sa-sb + cx1; d=2*a*b+cy1;
			b=4*(sa*a*b - a*b*sb) + cy0;  a=sa*sa -6*sa*sb + sb*sb + cx0;
			cdivt = moox/(c*c + d*d);
			x= (a*c + b*d)*cdivt;  y= (a*d + c*b)*cdivt;
			float y11 = (y+1.0f)*0.5f;
			float x11 = (x+1.0f)*0.5f;

			block_interp_bilin(out, in, w, h, xd, yd, x00, y00, x01, y01, x10, y10, x11, y11);

			y00 = y01; y10 = y11;
			x00 = x01; x10 = x11;
		}
		v0=v1;
	}
}

#if 0
typedef struct vec2f {
	float x, y;
} vec2f_t;

__attribute__((hot))
void span_interp(uint16_t *restrict out, uint16_t *restrict in, int w, int h, const vec2f_t *restrict grid, int gw, int gh)
{
	struct {
		int_fast32_t lastx;
		float u0, v0;

		float dudx, dvdx;
		float du0dy, dv0dy;
		float dudxdy, dvdxdy;
		//bool is_edge_span; // if set this span was created for clipping purposes and just fills the output with 1 pixel
	} active_spans[gw-1];

	const uint_fast32_t clamph = (h-1)*w;

	// TODO: if a span end is outside of the source split it into a span that
	// has u0, v0 set to the edge pixel and dudx, dvdx, du0dy, du0dy, dudxdy, dudxdy all 0
	// so that for the entire run it just uses the value of the edge pixel
	// have to do this for both ends of the span, so potentially every span could end up
	// split in 3
	// Want to do this because it doesn't distort the mapping for spans that need clipping
	// like the current code does. (current code clips before computing dudx and friends
	// so the map function is distorted across the entire block)

	// maybe just include a flag in the span that it's an edge span so we can just totally
	// skip the bilinear interpolation and increments
	// the if should wind up being fairly cheap since we're effectively just picking between
	// two possible loops, basically it's just one extra branch per span


	const vec2f_t *grid_line = grid;
	for(int gx = 0; gx < gw-1; gx++) {
		float x00 = grid_line[gx].x;
		float y00 = grid_line[gx].y;
		float x01 = grid_line[gx + 1].x;
		float y01 = grid_line[gx + 1].y;
		float x10 = grid_line[gw + gx].x;
		float y10 = grid_line[gw + gx].y;
		float x11 = grid_line[gw + gx + 1].x;
		float y11 = grid_line[gw + gx + 1].y;

#if 1
		float u00 = fmaxf(fminf(x00, 1.0f-FLT_EPSILON), 0.0f)*(w*256);
		float v00 = fmaxf(fminf(y00, 1.0f-FLT_EPSILON), 0.0f)*(h*256);
		float u01 = fmaxf(fminf(x01, 1.0f-FLT_EPSILON), 0.0f)*(w*256);
		float v01 = fmaxf(fminf(y01, 1.0f-FLT_EPSILON), 0.0f)*(h*256);
		float u10 = fmaxf(fminf(x10, 1.0f-FLT_EPSILON), 0.0f)*(w*256);
		float v10 = fmaxf(fminf(y10, 1.0f-FLT_EPSILON), 0.0f)*(h*256);
		float u11 = fmaxf(fminf(x11, 1.0f-FLT_EPSILON), 0.0f)*(w*256);
		float v11 = fmaxf(fminf(y11, 1.0f-FLT_EPSILON), 0.0f)*(h*256);

		float du0dy = ((u10 - u00)*(gh-1))/h;
		float dv0dy = ((v10 - v00)*(gh-1))/h;
		float dudxdy = ((((u11 - u10)*(gw-1))/w - ((u01 - u00)*(gw-1))/w)*(gh-1))/h;
		float dvdxdy = ((((v11 - v10)*(gw-1))/w - ((v01 - v00)*(gw-1))/w)*(gh-1))/h;
#else
		// somehow need to force truncation to be round towards 0
		// even for negative numbers
		// otherwise we step too far when stepping towards 0 and end up with
		// either u or v < 0
		int u00 = IMIN(IMAX((int)(x00*w*256), 0), w*256-1);
		int v00 = IMIN(IMAX((int)(y00*h*256), 0), h*256-1);
		int u01 = IMIN(IMAX((int)(x01*w*256), 0), w*256-1);
		int v01 = IMIN(IMAX((int)(y01*h*256), 0), h*256-1);
		int u10 = IMIN(IMAX((int)(x10*w*256), 0), w*256-1);
		int v10 = IMIN(IMAX((int)(y10*h*256), 0), h*256-1);
		int u11 = IMIN(IMAX((int)(x11*w*256), 0), w*256-1);
		int v11 = IMIN(IMAX((int)(y11*h*256), 0), h*256-1);

		int du0dy = ((u10 - u00)*(gh-1))/h;
		int dv0dy = ((v10 - v00)*(gh-1))/h;
		int du1dy = ((u11 - u10)*(gh-1))/h;
		int dv1dy = ((v11 - v10)*(gh-1))/h;
		int dudxdy = ((((u11 - u10)*(gw-1))/w - ((u01 - u00)*(gw-1))/w)*(gh-1))/h;
		int dvdxdy = ((((v11 - v10)*(gw-1))/w - ((v01 - v00)*(gw-1))/w)*(gh-1))/h;
#endif

			active_spans[gx].lastx = ((gx+1)*w)/(gw-1);
			active_spans[gx].u0    = u00;
			active_spans[gx].v0    = v00;
			//active_spans[gx].u1    = u01;
			//active_spans[gx].v1    = v01;
			active_spans[gx].dudx  = (u01 - u00)*(gw-1)/w;
			active_spans[gx].dvdx  = (v01 - v00)*(gw-1)/w;
			active_spans[gx].du0dy = du0dy;
			active_spans[gx].dv0dy = dv0dy;
			//active_spans[gx].du1dy = du1dy;
			//active_spans[gx].dv1dy = dv1dy;
			active_spans[gx].dudxdy = dudxdy;
			active_spans[gx].dvdxdy = dvdxdy;
	}

	for(int y = 0; y < h/(gh-1); y++) {
		uint16_t *restrict out_line = __builtin_assume_aligned(out + y*w, 32); // w%16 == 0, 2 bytes per pixel

		for(int x = 0, curspan = 0; x < w; curspan++) {
			assert(curspan < gw-1);

			uint_fast32_t u   = active_spans[curspan].u0;
			uint_fast32_t v   = active_spans[curspan].v0;
			int_fast32_t dudx = active_spans[curspan].dudx;
			int_fast32_t dvdx = active_spans[curspan].dvdx;
			//int_fast32_t dudx = ((active_spans[curspan].u1 - active_spans[curspan].u0)*(gw-1))/w;
			//int_fast32_t dvdx = ((active_spans[curspan].v1 - active_spans[curspan].v0)*(gw-1))/w;

			// assume a span is smaller than a cache line
			// and prefetch just the start of it.
			// this should work just fine because most cachelines these days
			// are at least 32 bytes or 16 of our pixels
			__builtin_prefetch(out_line, 1, 0); // write prefetch, non-temporal
			// TODO: maybe add more prefetching if we have really big spans

			//if(__builtin_expect(!active_spans[curspan].is_edge_span, 1)) {
				for(; x < active_spans[curspan].lastx; x++, u+=dudx, v+=dvdx) {
					assert(u >= 0); assert((uint_fast32_t)(u)>>8 < w); assert(u < w*256); // in debug builds make sure rounding hasn't messed us up
					assert(v >= 0); assert((uint_fast32_t)(v)>>8 < h); assert(v < h*256);
					assert(x < w);

					uint_fast32_t xs, ys, xf, yf;
					uint_fast32_t xi1, xi2, yi1, yi2;

					xs=(uint_fast32_t)(u) >> 8, ys=(uint_fast32_t)(v) >> 8;
					xf=(uint_fast32_t)(u)&0xFF, yf=(uint_fast32_t)(v)&0xFF;

					xi1 = xs, yi1 = ys*w;
					xi2 = IMIN(xi1+1, w-1u);
					yi2 = IMIN(yi1+w, clamph);

#if 0 // TODO: use top version if we don't have fast 64bit ints
					uint_fast32_t o = ((in[yi1 + xi1]*(256u - xf) + in[yi1 + xi2]*xf)*(256u-yf) +
					                   (in[yi2 + xi1]*(256u - xf) + in[yi2 + xi2]*xf)*yf) >> 16u;
					o = (o*((256u*97u)/100u)) >> 8u; // now that we have fixed bilinear interp need a fade here
#else
					uint_fast64_t o = ((in[yi1 + xi1]*(256u - xf) + in[yi1 + xi2]*xf)*(256u-yf) +
					                   (in[yi2 + xi1]*(256u - xf) + in[yi2 + xi2]*xf)*yf);
					o = (o*((256u*97u)/100u)) >> 24u; // now that we have fixed bilinear interp need a fade here
#endif
					*(out_line++) = o;
				}
#if 0
			} else {
				uint_fast32_t xs, ys;
				xs=(uint_fast32_t)(u) >> 8, ys=(uint_fast32_t)(v) >> 8;
				uint16_t o = in[ys*w + xs];
				for(; x < active_spans[curspan].lastx; x++) {
					*(out_line++) = o;
				}
			}
#endif

			// step so that we are ready for the next line in the block
			active_spans[curspan].u0   += active_spans[curspan].du0dy;
			active_spans[curspan].v0   += active_spans[curspan].dv0dy;
			//active_spans[curspan].u1   += active_spans[curspan].du1dy;
			//active_spans[curspan].v1   += active_spans[curspan].dv1dy;
			active_spans[curspan].dudx += active_spans[curspan].dudxdy;
			active_spans[curspan].dvdx += active_spans[curspan].dvdxdy;
		}
	}
}

__attribute__((hot))
MAP_FUNC_ATTR void soft_map_interp(uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd)
{
	const int gh = h/BLOCK_SIZE + 1, gw = w/BLOCK_SIZE + 1;
	vec2f_t grid[gh*gw];

	const float ustep = BLOCK_SIZE*2.0f/w, vstep = BLOCK_SIZE*2.0f/h;
	const float cx0 = (pd->p[0]-0.5f)*0.25f + 0.5f, cy0=pd->p[1]*0.25f + 0.5f;

	vec2f_t *grid_line = grid;
	float u = -1.0f;
	for(int xd = 0; xd < gw; xd++, u+=ustep) {
		const float y = -2*u + cy0;
		const float x = u*u - 1 + cx0;
		grid_line[xd].x = x;
		grid_line[xd].y = y;
	}

	float v = vstep - 1.0f;
	for(int yd = 1; yd < gh; yd++, v+=vstep) {
		u = -1.0f;
		grid_line = grid + yd*gw;
		for(int xd = 0; xd < gw; xd++, u+=ustep) {
			const float y = 2*u*v + cy0;
			const float x = u*u - v*v + cx0;
			grid_line[xd].x = x;
			grid_line[xd].y = y;
		}
		span_interp(out + (yd-1)*w*BLOCK_SIZE, in, w, h, grid_line - gw, gw, gh);
	}
}
#endif
