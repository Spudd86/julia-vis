#include "common.h"
#include "mymm.h"
#include "map.h"

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

			unsigned int xs = IMIN(IMAX(lrintf(x*w), 0), w-1);
			unsigned int ys = IMIN(IMAX(lrintf(y*h), 0), h-1);
			*out = in[ys*w + xs];
			out++;
		}
	}
}

//TODO: make go fast
//TODO: make version for 4x4 tiles (possibly 8x8 as well)
//TODO: make fast sse/mmx version
MAP_FUNC_ATTR void soft_map_bl(uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd)
{
	const float xstep = 2.0f/w, ystep = 2.0f/h;
	const float x0 = (pd->p[0]-0.5f)*0.25f + 0.5f, y0=pd->p[1]*0.25f + 0.5f;
	for(int yd = 0; yd < h; yd++) {
		float v = yd*ystep - 1.0f;
		for(int xd = 0; xd < w; xd++) {
			float u = xd*xstep - 1.0f;
			float y = 2*u*v + y0;
			float x = u*u - v*v + x0;

			int xs = IMIN(IMAX(lrintf(x*w*256), 0), (w-1)*256);
			int ys = IMIN(IMAX(lrintf(y*h*256), 0), (h-1)*256);
			int x1 = xs>>8, x2 = x1+1, xf = xs&0xFF;
			int y1 = ys>>8, y2 = y1+1, yf = ys&0xFF;

			*(out++) = ((in[y1*w + x1]*(0xff - xf) + in[y1*w + x2]*xf)*(0xff-yf) +
						(in[y2*w + x1]*(0xff - xf) + in[y2*w + x2]*xf)*yf) >> 16;
		}
	}
}

#define BLOCK_SIZE 8

#include "mymm.h"

MAP_FUNC_ATTR void soft_map_line_buff(uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd)
{
	const float ustep = BLOCK_SIZE*2.0f/w, vstep = BLOCK_SIZE*2.0f/h;
	const float x0 = (pd->p[0]-0.5f)*0.25f + 0.5f, y0=pd->p[1]*0.25f + 0.5f;
	const int buf_w = w/BLOCK_SIZE;
#ifndef  __SSE2__
//#if 1
	void fill_line_buff(const float v, int *line) {
		float u = -1.0f;
		for(int x=0; x<buf_w; x++, u+=ustep) {
			float xt, yt;
			xt = u*u - v*v + x0;
			yt = 2*u*v + y0;
			line[x] = IMIN(IMAX(lrintf(xt*w*256), 0), (w-1)*256);
			line[x+buf_w] = IMIN(IMAX(lrintf(yt*h*256), 0), (h-1)*256);
//			*(line++) = IMIN(IMAX(lrintf(xt*w*256), 0), (w-1)*256);
//			*(line++) = IMIN(IMAX(lrintf(yt*h*256), 0), (h-1)*256);
		}
	}
#else
	const __m128 uvecs = _mm_set1_ps(ustep);
	const __m128 x0vec = _mm_set1_ps(x0), y0vec = _mm_set1_ps(y0);
	const __m128 xmul = _mm_set1_ps(w*256), ymul = _mm_set1_ps(h*256);
	const __m128 xmax = _mm_set1_ps((w-1)*256), ymax = _mm_set1_ps((h-1)*256);
	const __m128 zvec = _mm_setzero_ps();
	_MM_SET_ROUNDING_MODE(_MM_ROUND_NEAREST);
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
	void fill_line_buff(float v, int *line) {
		__m128i *ln = line;
//		__m128 uvec = _mm_set_ps(-1.0f, 1*ustep-1.0f, 2*ustep-1.0f, 3*ustep-1.0f);
		for(int x=0; x<buf_w; x+=4) {
			const __m128 vvec = _mm_set1_ps(v);
			const __m128 uvec = _mm_setr_ps(ustep*(x+0)-1.0f, ustep*(x+1)-1.0f, ustep*(x+2)-1.0f, ustep*(x+3)-1.0f);
			__m128 tmp1 = uvec;
			__m128 tmp2 = vvec;
			tmp1 = _mm_mul_ps(tmp1, tmp1);
			tmp2 = _mm_mul_ps(tmp2, tmp2);
			tmp1 = _mm_sub_ps(tmp1, tmp2);
			tmp1 = _mm_add_ps(tmp1, x0vec);

			tmp2 = uvec;
			tmp2 = _mm_mul_ps(tmp2, vvec);
			tmp2 = _mm_add_ps(tmp2, tmp2);
			tmp2 = _mm_add_ps(tmp2, y0vec);

			tmp1 = _mm_mul_ps(tmp1, xmul); tmp1 = _mm_max_ps(tmp1, zvec); tmp1 = _mm_min_ps(tmp1, xmax);
			tmp2 = _mm_mul_ps(tmp2, ymul); tmp2 = _mm_max_ps(tmp2, zvec); tmp2 = _mm_min_ps(tmp2, ymax);

//			float t[4]; _mm_storeu_ps(t, tmp1);
//			line[x+0] = IMIN(IMAX(lrintf(t[0]*w*256), 0), (w-1)*256); line[x+1] = IMIN(IMAX(lrintf(t[1]*w*256), 0), (w-1)*256);
//			line[x+2] = IMIN(IMAX(lrintf(t[2]*w*256), 0), (w-1)*256); line[x+3] = IMIN(IMAX(lrintf(t[3]*w*256), 0), (w-1)*256);
//			_mm_storeu_ps(t, tmp2);
//			line[x+buf_w+0] = IMIN(IMAX(lrintf(t[0]*h*256), 0), (h-1)*256);
//			line[x+buf_w+1] = IMIN(IMAX(lrintf(t[1]*h*256), 0), (h-1)*256);
//			line[x+buf_w+2] = IMIN(IMAX(lrintf(t[2]*h*256), 0), (h-1)*256);
//			line[x+buf_w+3] = IMIN(IMAX(lrintf(t[3]*h*256), 0), (h-1)*256);
			_mm_stream_si128(ln+x/4, _mm_cvtps_epi32(tmp1));
			_mm_stream_si128(ln+(x+buf_w)/4, _mm_cvtps_epi32(tmp2));

//			__m128 st1, st2;
//			st1 = _mm_shuffle_ps(tmp1, tmp2, 0xbb);
//			st1 = _mm_shuffle_ps(st1, st1, 0x72);
//
//			st2 = _mm_shuffle_ps(tmp1, tmp2, 0x11);
//			st2 = _mm_shuffle_ps(st2, st2, 0x72);
//
//			_mm_stream_si128(ln+x*2, _mm_cvtps_epi32(st2));
//			_mm_stream_si128(ln+x*2+1, _mm_cvtps_epi32(st1));
		}
	}
#endif

#if 0
	typedef union { float f[8]; __m128 vec[2]; } vecflt2;
	static const union vecflt2 alph = {7.0f/7, 6.0f/7, 5.0f/7, 4.0f/7, 4.0f/7, 3.0f/7, 2.0f/7, 1.0f/7, 0 };
	const __m128 ones = _mm_set1_ps(1.0f);
	inline void interp_vecs(const __m128 a, const __m128 b, float out[8]) {
		__m128 *res = (__m128 *)out;
		res[0] = _mm_mul_ps(a, alph.vec[0]);
		res[1] = _mm_mul_ps(a, alph.vec[1]);
		__m128 tmp = _mm_sub_ps(ones, alph.vec[0]);
		res[0] = _mm_add_ps(res[0], _mm_mul_ps(b, tmp));
		tmp = _mm_sub_ps(ones, alph.vec[1]);
		res[1] = _mm_add_ps(res[1], _mm_mul_ps(b, tmp));

	}
	void do_interp(const int *restrict line1, const int *restrict line2, int yd) {
		for(int xi = 0; xi < buf_w; xi++) {
			vecflt2 x0, y0;
			const __m128 x00 = _mm_set1_ps(line1[xi]);
			const __m128 y00 = _mm_set1_ps(line1[xi+buf_w]);
			res[0] = _mm_mul_ps(a, alph.vec[0]);
			res[0] = _mm_mul_ps(b, alph.vec[1]);

		}
	}
#endif

	inline __attribute__((always_inline)) void  do_interp(const int *restrict line1, const int *restrict line2, int yd) {
		for(int xi = 1; xi < buf_w; xi++) {
//			const int x00 = line1[(xi-1)*2], y00 = line1[(xi-1)*2+1];
//			const int x01 = line2[(xi-1)*2], y01 = line2[(xi-1)*2+1];
//			const int x10 = line1[xi*2], y10 = line1[xi*2+1];
//			const int x11 = line2[xi*2], y11 = line2[xi*2+1];
			const int x00 = line1[xi-1], y00 = line1[xi-1+buf_w];
			const int x01 = line2[xi-1], y01 = line2[xi-1+buf_w];
			const int x10 = line1[xi],   y10 = line1[xi+buf_w];
			const int x11 = line2[xi],   y11 = line2[xi+buf_w];

			int x0 = x00, y0 = y00;
			int x1 = x10, y1 = y10;
			const int x0s = (x01 - x00)/BLOCK_SIZE;
			const int x1s = (x11 - x10)/BLOCK_SIZE;
			const int y0s = (y01 - y00)/BLOCK_SIZE;
			const int y1s = (y11 - y10)/BLOCK_SIZE;

			int xd = xi*BLOCK_SIZE;
			for(int yt=0; yt<BLOCK_SIZE; yt++, x0+=x0s, y0+=y0s, x1+=x1s, y1+=y1s) {
				int x = x0, y = y0;
				int xst = (x1 - x0)/BLOCK_SIZE;
				int yst = (y1 - y0)/BLOCK_SIZE;
				for(int xt=0; xt<BLOCK_SIZE; xt++, x+=xst, y+=yst) {
					int xs=x/256, ys=y/256;
					int xf=x&0xFF, yf=y&0xFF;

					int xi1 = xs;
					int yi1 = ys*w;
					int xi2 = IMIN(xi1+1,w-1);
					int yi2 = IMIN(yi1+w,(h-1)*w);

					out[(yd+yt)*w+xd+xt] = ((in[yi1 + xi1]*(255 - xf) + in[yi1 + xi2]*xf)*(255-yf) +
								(in[yi2 + xi1]*(255 - xf) + in[yi2 + xi2]*xf)*yf) >> 16;

				}
			}
		}
	}

	__m128i foo;
	int line_buff1[buf_w*2];
	int line_buff2[buf_w*2];

	fill_line_buff(-1.0f, line_buff1);
	float v = -1.0f;
	for(int yd = 0; yd < h; yd+=BLOCK_SIZE) {
		fill_line_buff(vstep*(yd/BLOCK_SIZE)-1.0f, line_buff2);
		do_interp(line_buff1, line_buff2, yd);
		yd+=BLOCK_SIZE;
		v+=vstep; fill_line_buff(vstep*(yd/BLOCK_SIZE)-1.0f, line_buff1);
		do_interp(line_buff2, line_buff1, yd);
	}
}

MAP_FUNC_ATTR void soft_map_interp(uint16_t *restrict __attribute__((aligned (16))) out, uint16_t *restrict __attribute__ ((aligned (16))) in, int w, int h, const struct point_data *pd)
{
	const float ustep = BLOCK_SIZE*2.0f/w, vstep = BLOCK_SIZE*2.0f/h;
	const float x0 = (pd->p[0]-0.5f)*0.25f + 0.5f, y0=pd->p[1]*0.25f + 0.5f;

	float v0 = -1.0f;
	for(int yd = 0; yd < h; yd+=BLOCK_SIZE) {
		float v1 = v0+vstep;

		float y00 = -2.0f*v0 + y0;
		float y10 = -2.0f*v1 + y0;
		float x00 = 1.0f - v0*v0 + x0;
		float x10 = 1.0f - v1*v1 + x0;
		float u1 = -1.0f;
		for(int xd = 0; xd < w; xd+=BLOCK_SIZE) {
			u1 = u1+ustep;

			float y01 = 2*u1*v0 + y0;
			float y11 = 2*u1*v1 + y0;
			float x01 = u1*u1 - v0*v0 + x0;
			float x11 = u1*u1 - v1*v1 + x0;

			int x0 = IMIN(IMAX(lrintf(x00*w*256), 0), (w-1)*256);
			int y0 = IMIN(IMAX(lrintf(y00*h*256), 0), (h-1)*256);

			int x1 = IMIN(IMAX(lrintf(x01*w*256), 0), (w-1)*256);
			int y1 = IMIN(IMAX(lrintf(y01*h*256), 0), (h-1)*256);

			int x0s = (IMIN(IMAX(lrintf(x10*w*256), 0), (w-1)*256) - x0)/BLOCK_SIZE;
			int x1s = (IMIN(IMAX(lrintf(x11*w*256), 0), (w-1)*256) - x1)/BLOCK_SIZE;
			int y0s = (IMIN(IMAX(lrintf(y10*h*256), 0), (h-1)*256) - y0)/BLOCK_SIZE;
			int y1s = (IMIN(IMAX(lrintf(y11*h*256), 0), (h-1)*256) - y1)/BLOCK_SIZE;

			for(int yt=0; yt<BLOCK_SIZE; yt++, x0+=x0s, y0+=y0s, x1+=x1s, y1+=y1s) {
				int x = x0;
				int y = y0;
				int xst = (x1 - x0)/BLOCK_SIZE;
				int yst = (y1 - y0)/BLOCK_SIZE;
				for(int xt=0; xt<BLOCK_SIZE; xt++, x+=xst, y+=yst) {
					int xs=x/256, ys=y/256;
					int xf=x&0xFF, yf=y&0xFF;

					int xi1 = xs;
					int yi1 = ys*w;
					int xi2 = IMIN(xi1+1,w-1);
					int yi2 = IMIN(yi1+w,(h-1)*w);

					out[(yd+yt)*w+xd+xt] = ((in[yi1 + xi1]*(255 - xf) + in[yi1 + xi2]*xf)*(255-yf) +
								(in[yi2 + xi1]*(255 - xf) + in[yi2 + xi2]*xf)*yf) >> 16;

				}
			}
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
	const float cx0 = pd->p[0]*1.5/2.5, cy0 = pd->p[1]*1.5/2.5;

	for(int yd = 0; yd < h; yd++) {
		float v = yd*ystep - 1.0f;
		for(int xd = 0; xd < w; xd++) {
			float u = xd*xstep -1.0f;

			float x = 2.5f*v*v - sqrtf(fabsf(u))*sm + cx0;
			float y = 2.5f*u*u - sqrtf(fabsf(v))*sm + cy0;

			x = (x+1.0f)*0.5f; y = (y+1.0f)*0.5f;

			int xs = IMIN(IMAX(lrintf(x*w*256), 0), (w-1)*256);
			int ys = IMIN(IMAX(lrintf(y*h*256), 0), (h-1)*256);
			int x1 = xs>>8, x2 = x1+1, xf = xs&0xFF;
			int y1 = ys>>8, y2 = y1+1, yf = ys&0xFF;

			*(out++) = ((in[y1*w + x1]*(0xff - xf) + in[y1*w + x2]*xf)*(0xff-yf) +
						(in[y2*w + x1]*(0xff - xf) + in[y2*w + x2]*xf)*yf) >> 16;
		}
	}
}

MAP_FUNC_ATTR void soft_map_rational(uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd)
{
	//cx1*=2; cy1*=2;
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

			int xs = IMIN(IMAX(lrintf(x*w*256), 0), (w-1)*256);
			int ys = IMIN(IMAX(lrintf(y*h*256), 0), (h-1)*256);
			int x1 = xs>>8, x2 = x1+1, xf = xs&0xFF;
			int y1 = ys>>8, y2 = y1+1, yf = ys&0xFF;

			*(out++) = ((in[y1*w + x1]*(0xff - xf) + in[y1*w + x2]*xf)*(0xff-yf) +
						(in[y2*w + x1]*(0xff - xf) + in[y2*w + x2]*xf)*yf) >> 16;
			//~ *(out++) = (fabsf(x) <= 1.0f && fabsf(y) <= 1.0f)
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

			int x0 = IMIN(IMAX(lrintf(x00*w*256), 0), (w-1)*256);
			int y0 = IMIN(IMAX(lrintf(y00*h*256), 0), (h-1)*256);
			int x1 = IMIN(IMAX(lrintf(x01*w*256), 0), (w-1)*256);
			int y1 = IMIN(IMAX(lrintf(y01*h*256), 0), (h-1)*256);

			int x0s = (IMIN(IMAX(lrintf(x10*w*256), 0), (w-1)*256) - x0)/BLOCK_SIZE;
			int x1s = (IMIN(IMAX(lrintf(x11*w*256), 0), (w-1)*256) - x1)/BLOCK_SIZE;
			int y0s = (IMIN(IMAX(lrintf(y10*h*256), 0), (h-1)*256) - y0)/BLOCK_SIZE;
			int y1s = (IMIN(IMAX(lrintf(y11*h*256), 0), (h-1)*256) - y1)/BLOCK_SIZE;

			for(int yt=0; yt<BLOCK_SIZE; yt++, x0+=x0s, y0+=y0s, x1+=x1s, y1+=y1s) {
				int x = x0;
				int y = y0;
				int xst = (x1 - x0)/BLOCK_SIZE;
				int yst = (y1 - y0)/BLOCK_SIZE;
				for(int xt=0; xt<BLOCK_SIZE; xt++, x+=xst, y+=yst) {
					int xs=x/256, ys=y/256;
					int xf=x&0xFF, yf=y&0xFF;

					int xi1 = xs;
					int yi1 = ys*w;
					int xi2 = IMIN(xi1+1,w-1);
					int yi2 = IMIN(yi1+w,(h-1)*w);

					out[(yd+yt)*w+xd+xt] = ((in[yi1 + xi1]*(255 - xf) + in[yi1 + xi2]*xf)*(255-yf) +
								(in[yi2 + xi1]*(255 - xf) + in[yi2 + xi2]*xf)*yf) >> 16;

				}
			}
			y00 = y01; y10 = y11;
			x00 = x01; x10 = x11;
		}
		v0=v1;
	}
}

//~ MAP_FUNC_ATTR void soft_map_rational(uint16_t *restrict out, uint16_t *restrict in, int w, int h, float cx0, float cy0, float cx1, float cy1 )
//~ {
	//~ float complex c1 = cx0 + cy0*I; float complex c2 = cx1 + cy1*I;
	//~ float xstep = 2.0f/w, ystep = 2.0f/h;
	//~ for(int yd = 0; yd < h; yd++) {
		//~ float v = 3*(yd*ystep - 1.0f);
		//~ for(int xd = 0; xd < w; xd++) {
			//~ float complex z = 3*(xd*xstep -1.0f + v*I);
			//~ float complex zsqr = z*z;

			//~ z = ((zsqr*zsqr + c1)/(zsqr + c2))/3.0f;


			//~ unsigned int xs = IMIN(IMAX(lrintf(crealf(z)*w), 0), w-1);
			//~ unsigned int ys = IMIN(IMAX(lrintf(cimagf(z)*h), 0), h-1);
			//~ *out = in[ys*w + xs];
			//~ out++;
		//~ }
	//~ }
//~ }

//~ #ifdef __SSE4_1__
#if 0
// TODO: write this with SSE 4.1 for massive win
MAP_FUNC_ATTR void soft_map_bl_sse4(uint16_t *restrict out, uint16_t *restrict in, int w, int h, float x0, float y0)
{
	float xstep = 2.0f/w, ystep = 2.0f/h;

	__m128 xm, ym;

	x0  = x0*0.25f + 0.5f;
	y0  = y0*0.25f + 0.5f;
	for(int yd = 0; yd < h; yd++) {
		float v = yd*ystep - 1.0f;
		for(int xd = 0; xd < w; xd++) {
			float u = xd*xstep - 1.0f;
			float y = 2*u*v + y0;
			float x = u*u - v*v + x0;

			int xs = IMIN(IMAX(lrintf(x*w*256), 0), (w-1)*256);
			int ys = IMIN(IMAX(lrintf(y*h*256), 0), (h-1)*256);
			int x1 = xs>>8, x2 = x1+1, xf = xs&0xFF;
			int y1 = ys>>8, y2 = y1+1, yf = ys&0xFF;

			*(out++) = ((in[y1*w + x1]*(0xff - xf) + in[y1*w + x2]*xf)*(0xff-yf) +
						(in[y2*w + x1]*(0xff - xf) + in[y2*w + x2]*xf)*yf) >> 16;
		}
	}
}
#endif
