//#pragma GCC optimize "inline-functions,unsafe-loop-optimizations,merge-all-constants,fast-math,associative-math,reciprocal-math,no-signed-zeros"
#ifndef DEBUG
#pragma GCC optimize "3,inline-functions,fast-math,associative-math,reciprocal-math,no-signed-zeros"
#endif

#include "common.h"
#include "map.h"

#include <float.h>
#include <assert.h>

// thee asserts in this file really are slow because they are on the inner loop
#ifdef DEBUG
#define map_assert(a) assert(a)
#else
#define map_assert(a)
#endif

#define BLOCK_SIZE 8

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

__attribute__((hot))
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
	v = (v*((256u*97u)/100u)) >> 8u; // now that we have correct bilinear interp need a fade here
#else
	uint_fast64_t v = ((p00*(256u - xf) + p01*xf)*(256u-yf) +
			           (p10*(256u - xf) + p11*xf)*yf);
	v = (v*((256u*97u)/100u)) >> 24u; // now that we have correct bilinear interp need a fade here
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
__attribute__((hot))
static inline void block_interp_bilin(uint16_t *restrict out, uint16_t *restrict in, int w, int h, int xd, int yd, float x00, float y00, float x01, float y01, float x10, float y10, float x11, float y11)
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

#if 1 // TODO: use top version if we don't have fast 64bit ints
			uint_fast32_t o = ((in[yi1 + xi1]*(256u - xf) + in[yi1 + xi2]*xf)*(256u-yf) +
			                   (in[yi2 + xi1]*(256u - xf) + in[yi2 + xi2]*xf)*yf) >> 16u;
			o = (o*((256u*97u)/100u)) >> 8u; // now that we have fixed bilinear interp need a fade here
#else
			uint_fast64_t o = ((in[yi1 + xi1]*(256u - xf) + in[yi1 + xi2]*xf)*(256u-yf) +
			                   (in[yi2 + xi1]*(256u - xf) + in[yi2 + xi2]*xf)*yf);
			o = (o*((256u*97u)/100u)) >> 24u; // now that we have fixed bilinear interp need a fade here
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
void soft_map_task(size_t work_item_id, size_t span, uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd)
{
	const int ystart = work_item_id * span * BLOCK_SIZE;
	const int yend   = IMIN(ystart + span * BLOCK_SIZE, (unsigned int)h);
	out += ystart * w;

	const float xstep = 2.0f/w, ystep = 2.0f/h;
	const float x0 = pd->p[0]*0.25f -0.5f*0.25f + 0.5f, y0=pd->p[1]*0.25f + 0.5f;
	for(int yd = ystart; yd < yend; yd++) {
		float v = yd*ystep - 1.0f;
		for(int xd = 0; xd < w; xd++) {
			float u = xd*xstep - 1.0f;
			float y = 2*u*v + y0;
			float x = u*u - v*v + x0;
			*(out++) = bilin_samp(in, w, h, x, y);
		}
	}
}

__attribute__((hot))
static void soft_map_interp_task(size_t work_item_id, size_t span, uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd)
{
	const int ystart = work_item_id * span * BLOCK_SIZE;
	const int yend   = IMIN(ystart + span * BLOCK_SIZE, (unsigned int)h);

	const float ustep = 2.0f/w, vstep = 2.0f/h;
	const float cx0 = pd->p[0]*0.25f + (-0.5f*0.25f + 0.5f), cy0=pd->p[1]*0.25f + 0.5f;

	float v0 = ystart*vstep - 1.0f;
	for(int yd = ystart; yd < yend; yd+=BLOCK_SIZE) {
		const float v1 = (yd+BLOCK_SIZE)*vstep - 1.0f;
		float y00 = -2.0f*v0 + cy0;
		float y10 = -2.0f*v1 + cy0;
		float x00 = 1.0f - v0*v0 + cx0;
		float x10 = 1.0f - v1*v1 + cx0;

		for(int xd = 0; xd < w; xd+=BLOCK_SIZE) {
			const float u = (xd+BLOCK_SIZE)*ustep - 1.0f;
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

__attribute__((hot))
void soft_map_butterfly_task(size_t work_item_id, size_t span, uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd)
{
	const int ystart = work_item_id * span * BLOCK_SIZE;
	const int yend   = IMIN(ystart + span * BLOCK_SIZE, (unsigned int)h);
	out += ystart * w;

	const float xstep = 2.0f/w, ystep = 2.0f/h;
	const float sm = sqrtf(2.5f)/2.5f;
	const float cx0 = pd->p[0]*1.5f/2.5f, cy0 = pd->p[1]*1.5f/2.5f;

	for(int yd = ystart; yd < yend; yd++) {
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

__attribute__((hot))
void soft_map_butterfly_interp_task(size_t work_item_id, size_t span, uint16_t *restrict out, uint16_t *restrict __attribute__ ((aligned (16))) in, int w, int h, const struct point_data *pd)
{
	const int ystart = work_item_id * span * BLOCK_SIZE;
	const int yend   = IMIN(ystart + span * BLOCK_SIZE, (unsigned int)h);

	const float ustep = 2.0f/w, vstep = 2.0f/h;
	const float sm = sqrtf(2.5f)/2.5f;
	const float cx0 = pd->p[0]*1.5f/2.5f, cy0 = pd->p[1]*1.5f/2.5f;

	for(int yd = ystart; yd < yend; yd+=BLOCK_SIZE) {
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
	}
}

void soft_map_rational_task(size_t work_item_id, size_t span, uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd)
{
	const int ystart = work_item_id * span * BLOCK_SIZE;
	const int yend   = IMIN(ystart + span * BLOCK_SIZE, (unsigned int)h);
	out += ystart * w;

	const float xoom = 3.0f, moox = 1.0f/xoom;
	float xstep = 2.0f/w, ystep = 2.0f/h;
	const float cx0 = pd->p[0], cy0 = pd->p[1], cx1 = pd->p[2]*2, cy1 = pd->p[3]*2;

	for(int yd = ystart; yd < yend; yd++) {
		float v = yd*ystep - 1.0f;
		for(int xd = 0; xd < w; xd++) {
			float u = xd*xstep - 1.0f;
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

void soft_map_rational_interp_task(size_t work_item_id, size_t span, uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd)
{
	const int ystart = work_item_id * span * BLOCK_SIZE;
	const int yend   = IMIN(ystart + span * BLOCK_SIZE, (unsigned int)h);

	const float cx0 = pd->p[0], cy0 = pd->p[1], cx1 = pd->p[2]*2, cy1 = pd->p[3]*2;
	const float xoom = 3.0f, moox = 1.0f/xoom;
	const float ustep = 2.0f/w, vstep = 2.0f/h;
	float v0 = ystart * vstep - 1.0f;
	for(int yd = ystart; yd < yend; yd+=BLOCK_SIZE) {
		float v1 = (yd + BLOCK_SIZE) * vstep - 1.0f;

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
		for(int xd = 0; xd < w; xd+=BLOCK_SIZE) {
			float u1 = (xd + BLOCK_SIZE) * ustep - 1.0f;

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

#if NO_PARATASK

#define GEN_TASK_FN_NAME_(name) name##_task
#define GEN_TASK_FN_NAME(name) GEN_TASK_FN_NAME_(name)
#define DO_TASK_WRAP(name) \
	void name(uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd) { \
		GEN_TASK_FN_NAME(name)(0, h/BLOCK_SIZE, out, in, w, h, pd); \
	}

#else

#define GEN_TASK_FN_NAME_(name) name##_task
#define GEN_TASK_FN_NAME(name) GEN_TASK_FN_NAME_(name)
#define DO_TASK_WRAP(name) \
	void name(uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd) { \
		int span = 2; \
		struct softmap_work_args args = { \
			GEN_TASK_FN_NAME(name), \
			out, in, \
			pd, \
			span, \
			w, h \
		}; \
		paratask_call(paratask_default_instance(), 0, h/(span*BLOCK_SIZE), paratask_func, &args); \
	}

#include "paratask/paratask.h"
struct softmap_work_args {
	void (*soft_map_task_fn)(size_t work_item_id, size_t span, uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd);
	uint16_t *restrict out;
	uint16_t *restrict in;
	const struct point_data *pd;
	size_t span;
	int w, h;
};
static void paratask_func(size_t work_item_id, void *arg_)
{
	struct softmap_work_args *a = arg_;
	a->soft_map_task_fn(work_item_id, a->span, a->out, a->in, a->w, a->h, a->pd);
}

#endif // NO_PARATASK

DO_TASK_WRAP(soft_map_interp)
DO_TASK_WRAP(soft_map)
DO_TASK_WRAP(soft_map_butterfly)
DO_TASK_WRAP(soft_map_butterfly_interp)
DO_TASK_WRAP(soft_map_rational)
DO_TASK_WRAP(soft_map_rational_interp)
