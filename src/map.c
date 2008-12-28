#include <unistd.h>
#include <stdint.h>
#include <math.h>

#include <mmintrin.h>
#include <xmmintrin.h>

#include "map.h"

#include "common.h"

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
void soft_map(uint16_t *out, uint16_t *in, int w, int h, float x0, float y0)
{
	float xstep = 2.0f/w, ystep = 2.0f/h;
	x0  = x0*0.25 + 0.5;
	y0  = y0*0.25 + 0.5;
	for(int yd = 0; yd < h; yd++) {
		float v = yd*ystep - 1.0f; 
		for(int xd = 0; xd < w; xd++) {
			float u = xd*xstep - 1.0f;
			float y = 2*u*v + y0;
			float x = u*u - v*v + x0;
					
			unsigned int xs = IMIN(IMAX(lrintf(x*w), 0), w);
			unsigned int ys = IMIN(IMAX(lrintf(y*h), 0), h);
			*out = in[ys*w + xs];
			out++;
		}
	}
}


//TODO: make go fast
//TODO: make version for 4x4 tiles (possibly 8x8 as well)
//TODO: make fast sse/mmx version
void soft_map_bl(uint16_t *out, uint16_t *in, int w, int h, float x0, float y0)
{
	float xstep = 2.0f/w, ystep = 2.0f/h; 
	x0  = x0*0.25 + 0.5;
	y0  = y0*0.25 + 0.5;
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


void soft_map8x8(uint16_t *out, uint16_t *in, int w, int h, float x0, float y0)
{
	float xstep = 2.0f/w, ystep = 2.0f/h;
	x0  = x0*0.25 + 0.5;
	y0  = y0*0.25 + 0.5;
	for(int yd = 0; yd < h/8; yd++) {
		for(int xd = 0; xd < w/8; xd++) {
			float v = yd*8*ystep - 1.0f;
			for(int yt=0; yt<8; yt++, v+=ystep) {
				float u = xd*8*xstep - 1.0f;
				for(int xt=0; xt<8; xt++, u+=xstep) {
					float y = 2*u*v + y0;
					float x = u*u - v*v + x0;
					
					unsigned int xs = IMIN(IMAX(lrintf(x*w), 0), w);
					unsigned int ys = IMIN(IMAX(lrintf(y*h), 0), h);
					
					*(out++) = in[(ys-ys%8)*w + (ys%8)*8+ (xs-xs%8)*8 + (xs%8)];
				}
			}
		}
	}
}

void soft_map_bl8x8(uint16_t *out, uint16_t *in, int w, int h, float x0, float y0)
{
	float xstep = 2.0f/w, ystep = 2.0f/h;
	x0  = x0*0.25 + 0.5;
	y0  = y0*0.25 + 0.5;
	for(int yd = 0; yd < h/8; yd++) {
		for(int xd = 0; xd < w/8; xd++) {
			float v = yd*8*ystep - 1.0f;
			for(int yt=0; yt<8; yt++, v+=ystep) {
				float u = xd*8*xstep - 1.0f;
				for(int xt=0; xt<8; xt++, u+=xstep) {
					float y = 2*u*v + y0;
					float x = u*u - v*v + x0;
					
					unsigned int xf = IMIN(IMAX(lrintf(x*w*256), 0), w*256);
					unsigned int yf = IMIN(IMAX(lrintf(y*h*256), 0), h*256);
					unsigned int xs = xf/256;
					unsigned int ys = yf/256;
					xf = xf%256;
					yf = yf%256;
					
					int xi1 = (xs-xs%8)*8 + (xs%8); 
					int yi1 = (ys-ys%8)*w + (ys%8)*8;
					xs=IMIN(xs+1,w); ys=IMIN(ys+1,h);
					int xi2 = (xs-xs%8)*8 + (xs%8);
					int yi2 = (ys-ys%8)*w + (ys%8)*8;

					//*(out++) = in[yi1 + xi1];
					*(out++) = ((in[yi1 + xi1]*(0xff - xf) + in[yi1 + xi2]*xf)*(0xff-yf) +
								(in[yi2 + xi1]*(0xff - xf) + in[yi2 + xi2]*xf)*yf) >> 16;
				}
			}
		}
	}
}

// FIXME: this doesn't work
void soft_map_interp8x8(uint16_t *out, uint16_t *in, int w, int h, float x0, float y0)
{
	const float xstep = 16.0f/w, ystep = 16.0f/h; 
	
	x0  = x0*0.25 + 0.5;
	y0  = y0*0.25 + 0.5;
	
	for(int yd = 0; yd < h/8; yd++) 
	{
		__v4sf v = { -1.0f + ystep*yd, -1.0f + ystep*yd, -1.0f + ystep*(yd+1), -1.0f + ystep*(yd+1)};
		__v4sf u = {-1.0f, -1.0f + xstep, -1.0f, -1.0f + xstep};
		for(int xd = 0; xd < w/8; xd++, u+=(__v4sf)_mm_load1_ps(&xstep)) 
		{
			__v4sf xv = u*u - v*v + (__v4sf)_mm_load1_ps(&x0);
			__v4sf yv = u*v + u*v + (__v4sf)_mm_load1_ps(&y0);
			float *x = (float *)&xv;
			float *y = (float *)&yv;
			
			
			for(int i=1; i<4; i++) { x[i]=x[i]-x[0]; y[i]=y[i]-y[0]; }
			
			int xo = lrintf((x[0])*w/8); int xto = lrintf((x[0])*w);
			int yo = lrintf((y[0])*h/8); int yto = lrintf((y[0])*h);

			for(float ty=0; ty<1.0f; ty+=1.0f/8.0f) 
			{
				for(float tx=0; tx<1.0f; tx+=1.0f/8.0f) 
				{
					float xs = x[1]*tx*(1-ty) + x[2]*(1-tx)*ty + x[3]*tx*ty;
					float ys = y[1]*tx*(1-ty) + y[2]*(1-tx)*ty + y[3]*tx*ty;
					int	x1 = IMIN(IMAX(xo+lrintf(xs*w/8), 0), w/8);
					int	y1 = IMIN(IMAX(yo+lrintf(ys*h/8), 0), h/8);
					int xt = IMIN(IMAX(xto+lrintf(xs*w), 0), w)%8;
					int yt = IMIN(IMAX(yto+lrintf(ys*h), 0), h)%8;
					//int x1 = xt/8; int y1 = yt/8; xt=xt%8; yt=yt%8;
					
					*(out++) = in[y1*w*64/8 + x1*64 + yt*8+xt];
				}
			}
		}
	}
}