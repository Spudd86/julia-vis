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

#include "bilin-inlines.h"

#ifndef __EMSCRIPTEN__
//TODO: do something better with the list of clones
// arch=core2,arch=nehalem,arch=westmere,arch=sandybridge,arch=ivybridge,arch=haswell,arch=broadwell,skylake,cannonlake,cooperlake,tigerlake,sapphirerapids,arch=alderlake,rocketlake,k8,k8-sse3,btver1,barcelona,bdver1,znver1,
// #define TASK_PRE_ATTRIB __attribute__((hot,flatten,optimize(3),target_clones("arch=alderlake,avx2,avx,sse3,sse4.1,sse4.2,ssse3,default")))
 #define TASK_PRE_ATTRIB __attribute__((hot,flatten,optimize(3),target_clones("avx2,avx,sse3,sse4.1,sse4.2,ssse3,default")))

// nocona core2 nehalem corei7 westmere sandybridge corei7-avx ivybridge core-avx-i haswell core-avx2 broadwell skylake skylake-avx512 cannonlake icelake-client icelake-server bonnell atom silvermont slm knl knm x86-64 eden-x2 nano nano-1000 nano-2000 nano-3000 nano-x2 eden-x4 nano-x4 k8 k8-sse3 opteron opteron-sse3 athlon64 athlon64-sse3 athlon-fx amdfam10 barcelona bdver1 bdver2 bdver3 bdver4 znver1 btver1 btver2 native
#else
#define TASK_PRE_ATTRIB __attribute__((hot,flatten))
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
TASK_PRE_ATTRIB
static void soft_map_task(size_t work_item_id, size_t span, uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd)
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

TASK_PRE_ATTRIB
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

TASK_PRE_ATTRIB
static void soft_map_butterfly_task(size_t work_item_id, size_t span, uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd)
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

TASK_PRE_ATTRIB
static void soft_map_butterfly_interp_task(size_t work_item_id, size_t span, uint16_t *restrict out, uint16_t *restrict __attribute__ ((aligned (16))) in, int w, int h, const struct point_data *pd)
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

TASK_PRE_ATTRIB
static void soft_map_rational_task(size_t work_item_id, size_t span, uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd)
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

TASK_PRE_ATTRIB
static void soft_map_rational_interp_task(size_t work_item_id, size_t span, uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd)
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
