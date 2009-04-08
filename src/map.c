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
	const float x0 = pd->p[0]*0.25f + 0.5f, y0=pd->p[1]*0.25f + 0.5f;
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
	const float x0 = pd->p[0]*0.25f + 0.5f, y0=pd->p[1]*0.25f + 0.5f;
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

MAP_FUNC_ATTR void soft_map_interp(uint16_t *restrict __attribute__((aligned (16))) out, uint16_t *restrict __attribute__ ((aligned (16))) in, int w, int h, const struct point_data *pd)
{
	const float ustep = BLOCK_SIZE*2.0f/w, vstep = BLOCK_SIZE*2.0f/h;
	const float x0 = pd->p[0]*0.25f + 0.5f, y0=pd->p[1]*0.25f + 0.5f;

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
