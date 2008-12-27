#include <unistd.h>
#include <glib.h>
#include <math.h>

#include <mmintrin.h>
//#include <xmmintrin.h>


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
void soft_map_ref(guint8 *out, guint8 *in, int w, int h, float x0, float y0)
{
	x0 *= 0.5f; y0 *= 0.5f;
	for(int yd = 0; yd < h; yd++) {
		for(int xd = 0; xd < w; xd++) {
			float x = 2.0*(float)xd/w - 1; 
			float y = 2.0*(float)yd/h - 1;
			float y2 = y*y;
			y = 4*x*y + y0;
			x = 2*(x*x - y2) + x0;
			
			//int xs = lrintf(fminf(fmaxf((x+1)/2, 0.0f), 1.0f)*w);
			//int ys = lrintf(fminf(fmaxf((y+1)/2, 0.0f), 1.0f)*h);
			int xs = MIN(MAX(lrintf((x+1)*0.5*w), 0), w);
			int ys = MIN(MAX(lrintf((y+1)*0.5*h), 0), h);
			out[yd*w + xd] = in[ys*w + xs];
		}
	}
}

void soft_map_ref2(guint16 *out, guint16 *in, int w, int h, float x0, float y0)
{
	float xstep = 2.0f/w, ystep = 2.0f/h; 
	
	x0 *= 0.5f; y0 *= 0.5f;
	x0 += 1.0f; y0 += 1.0f;
	float yi = -1.0f;
	for(int yd = 0; yd < h; yd++, yi+=ystep) {
		float xi = - 1.0f;
		for(int xd = 0; xd < w; xd++, xi+=xstep) {
			float y = (4*xi*yi + y0)*0.5;
			float x = (2*(xi*xi - yi*yi) + x0)*0.5;
			
			int xs = MIN(MAX(lrintf(x*w), 0), w);
			int ys = MIN(MAX(lrintf(y*h), 0), h);
			*out = (*out)/2 + in[ys*w + xs]/2;
			out++;
		}
	}
}

//TODO: make go fast
void soft_map_bl(guint16 *out, guint16 *in, int w, int h, float x0, float y0)
{
	float xstep = 2.0f/w, ystep = 2.0f/h; 
	
	x0 *= 0.5f; y0 *= 0.5f;
	x0 += 1.0f; y0 += 1.0f;
	float yi = -1.0f;
	for(int yd = 0; yd < h; yd++, yi+=ystep) {
		float xi = - 1.0f;
		for(int xd = 0; xd < w; xd++, xi+=xstep) {
			float y = (4*xi*yi + y0)*0.5;
			float x = (2*(xi*xi - yi*yi) + x0)*0.5;
			
			int xs = MIN(MAX(lrintf(x*w*256), 0), w*256);
			int ys = MIN(MAX(lrintf(y*h*256), 0), h*256);
			int x1 = xs>>8, x2 = MIN(x1+1,w), xf = xs&0xFF;
			int y1 = ys>>8, y2 = MIN(y1+1,h), yf = ys&0xFF;
			
			guint w1 = (0xff - xf)*(0xff-yf), w2 = xf * (0xff-yf),
			      w3 = (0xff - xf)*yf,        w4 = xf * yf;
			
			*(out++) = (in[y1*w + x1]*w1 + in[y1*w + x2]*w2 +
						in[y2*w + x1]*w3 + in[y2*w + x2]*w4) >> 16;
		}
	}
}