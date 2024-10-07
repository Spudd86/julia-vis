#include "common.h"

#define BLOCK_SIZE 8

#include "bilin-inlines.h"


__attribute__((__hot__, access (write_only, 4,5)))
inline void map_func(float u, float v, const float *restrict p, float *restrict x, float *restrict y) ;


__attribute__((hot,flatten))
void soft_map_dynamic_task(size_t work_item_id, size_t span, uint16_t *restrict out, uint16_t *restrict in, int w, int h, const float *restrict p)
{
	const int ystart = work_item_id * span * BLOCK_SIZE;
	const int yend   = IMIN(ystart + span * BLOCK_SIZE, (unsigned int)h);
	out += ystart * w;

	const float xstep = 2.0f/w, ystep = 2.0f/h;

	for(int yd = ystart; yd < yend; yd++) {
		float v = yd*ystep - 1.0f;
		for(int xd = 0; xd < w; xd++) {
			float u = xd*xstep -1.0f;

			float x, y;
			map_func(u, v, p, &x, &y);

			x = (x+1.0f)*0.5f; y = (y+1.0f)*0.5f;

			*(out++) = bilin_samp(in, w, h, x, y);
		}
	}
}

__attribute__((hot,flatten))
void soft_map_dynamic_interp_task(size_t work_item_id, size_t span, uint16_t *restrict out, uint16_t *restrict __attribute__ ((aligned (16))) in, int w, int h, const float *restrict p)
{
	const int ystart = work_item_id * span * BLOCK_SIZE;
	const int yend   = IMIN(ystart + span * BLOCK_SIZE, (unsigned int)h);

	const float ustep = 2.0f/w, vstep = 2.0f/h;

	for(int yd = ystart; yd < yend; yd+=BLOCK_SIZE) {
		float v0 = yd*vstep - 1.0f;
		float v1 = v0+vstep;

		float x00, y00;
		float x10, y10;

		map_func(-1.0f, v0, p, &x00, &y00);
		map_func(-1.0f, v1, p, &x10, &y10);

		y00 = (y00+1.0f)*0.5f; y10 = (y10+1.0f)*0.5f;
		x00 = (x00+1.0f)*0.5f; x10 = (x10+1.0f)*0.5f;

		for(int xd = 0; xd < w; xd+=BLOCK_SIZE) {
			float u1 = (xd+1)*ustep - 1.0f;

			float x01, y01;
			float x11, y11;

			map_func(u1, v0, p, &x01, &y01);
			map_func(u1, v1, p, &x11, &y11);

			y01 = (y01+1.0f)*0.5f; y11 = (y11+1.0f)*0.5f;
			x01 = (x01+1.0f)*0.5f; x11 = (x11+1.0f)*0.5f;

			block_interp_bilin(out, in, w, h, xd, yd, x00, y00, x01, y01, x10, y10, x11, y11);

			y00 = y01; y10 = y11;
			x00 = x01; x10 = x11;
		}
	}
}


void map_func(float u, float v, const float *restrict p, float *restrict x, float *restrict y)
{
	const float x0 = p[0]*0.25f -0.5f*0.25f + 0.5f, y0=p[1]*0.25f + 0.5f;
	*y = 2*u*v + y0;
	*x = u*u - v*v + x0;
}