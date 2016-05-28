//#pragma GCC optimize "3,ira-hoist-pressure,inline-functions,merge-all-constants,modulo-sched,modulo-sched-allow-regmoves,aggressive-loop-optimizations,unsafe-loop-optimizations"

#include "common.h"
#include "mymm.h"
#include "map.h"

#include "swizzle.h"

#define BLOCK_SIZE 8

static inline uint16_t bilin_samp(uint16_t *restrict in, int w, int h, float x, float y)
{
	int32_t xs = IMIN(IMAX(lrintf(x*w*256), 0), (w-1)*256);
	int32_t ys = IMIN(IMAX(lrintf(y*h*256), 0), (h-1)*256);
	int32_t x1 = xs>>8, x2 = x1+1;
	int32_t y1 = ys>>8, y2 = y1+1;
	uint_fast32_t xf = xs&0xFF, yf = ys&0xFF;

	uint_fast32_t p00 = in[y1*w + x1];
	uint_fast32_t p01 = in[y1*w + x2];
	uint_fast32_t p10 = in[y2*w + x1];
	uint_fast32_t p11 = in[y2*w + x2];

	// it is critical that this entire calculation be done as at least uint32s
	uint_fast32_t v = ((p00*(256u - xf) + p01*xf)*(256u-yf) +
			           (p10*(256u - xf) + p11*xf)*yf) >> 16u;
	v = (v*((256u*97u)/100u)) >> 8u; // now that we have fixed bilinear interp need a fade here
	return v;
}

static inline void block_interp_bilin(uint16_t *restrict out, uint16_t *restrict in, int w, int h, int xd, int yd, float x00, float y00, float x01, float y01, float x10, float y10, float x11, float y11)
{
	int x0 = IMIN(IMAX(lrintf(x00*w*256), 0), (w-1)*256);
	int y0 = IMIN(IMAX(lrintf(y00*h*256), 0), (h-1)*256);
	int x1 = IMIN(IMAX(lrintf(x01*w*256), 0), (w-1)*256);
	int y1 = IMIN(IMAX(lrintf(y01*h*256), 0), (h-1)*256);

	int x0s = (IMIN(IMAX(lrintf(x10*w*256), 0), (w-1)*256) - x0)/BLOCK_SIZE;
	int x1s = (IMIN(IMAX(lrintf(x11*w*256), 0), (w-1)*256) - x1)/BLOCK_SIZE;
	int y0s = (IMIN(IMAX(lrintf(y10*h*256), 0), (h-1)*256) - y0)/BLOCK_SIZE;
	int y1s = (IMIN(IMAX(lrintf(y11*h*256), 0), (h-1)*256) - y1)/BLOCK_SIZE;

	int y_off = yd * w;
	for(int yt=0; yt<BLOCK_SIZE; y_off+=w, yt++, x0+=x0s, y0+=y0s, x1+=x1s, y1+=y1s) {
		int x = x0;
		int y = y0;
		int xst = (x1 - x0)/BLOCK_SIZE;
		int yst = (y1 - y0)/BLOCK_SIZE;
		#pragma GCC ivdep
		for(int xt=0; xt<BLOCK_SIZE; xt++, x+=xst, y+=yst) {
			uint_fast32_t xs=x/256, ys=y/256;
			uint_fast32_t xf=x&0xFF, yf=y&0xFF;

			uint_fast32_t xi1 = xs;
			uint_fast32_t yi1 = ys;
			uint_fast32_t xi2 = IMIN(xi1+1,(uint_fast32_t)w-1);
			uint_fast32_t yi2 = IMIN(yi1+1,(uint_fast32_t)h-1);

			uint_fast32_t p00 = in[yi1*w + xi1];
			uint_fast32_t p01 = in[yi1*w + xi2];
			uint_fast32_t p10 = in[yi2*w + xi1];
			uint_fast32_t p11 = in[yi2*w + xi2];

			uint_fast32_t v = ((p00*(256u - xf) + p01*xf)*(256u-yf) +
			                   (p10*(256u - xf) + p11*xf)*yf) >> 16u;
			v = (v*((256u*97u)/100u)) >> 8u; // now that we have fixed bilinear interp need a fade here
			out[y_off+xd+xt] = v;
		}
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
			const float y01 = 2*u*v0 + cy0;
			const float y11 = 2*u*v1 + cy0;
			const float x01 = u*u - v0*v0 + cx0;
			const float x11 = u*u - v1*v1 + cx0;

			block_interp_bilin(out, in, w, h, xd, yd, x00, y00, x01, y01, x10, y10, x11, y11);

			y00 = y01; y10 = y11;
			x00 = x01; x10 = x11;
		}
		v0=v1;
	}
}

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
