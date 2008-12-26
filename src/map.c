#include <unistd.h>
#include <glib.h>
#include <math.h>

#include <mmintrin.h>
#include <xmmintrin.h>


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
void soft_map_c(guint16 *out, guint16 *in, int w, int h, float x0, float y0)
{
	float xstep = 4.0f/w, ystep = 4.0f/h; // need to run from -2 to +2
	
	x0 += 2.0f; y0 += 2.0f;
	x0 *= 0.25f; y0 *= 0.25f;
	float yi = -2.0f;
	for(int yd = 0; yd < h; yd++, yi+=ystep) {
		float xi = - 2.0f
		for(int xd = 0; xd < w; xd++, xi+=xstep) {
			int x = lrintf(minf(maxf(0.25f*(xi*xi - yi*yi) + x0, 0.0f), 1.0f)*w);
			int y = lrintf(minf(maxf(0.5f*xi*yi + y0, 0.0f), 1.0f)*h);
			out[yd*w + xd] = in[y*w + x];
		}
	}
}
