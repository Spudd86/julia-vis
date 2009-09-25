
#include "common.h"
#include "audio/audio.h"
#include "tribuf.h"
#include "pixmisc.h"
#include <mm_malloc.h>

static uint16_t *setup_point(int w, int h)
{
	uint16_t *buf = malloc((w+1) * (h+1) * sizeof(uint16_t));
	memset(buf, 0, (w+1)*(h+1)*sizeof(uint16_t));
	int stride = w+1;
	for(int y=0; y < h; y++)  {
		for(int x=0; x < w; x++) {
			float u = (2*x+1.0f)/w - 1, v = (2*y+1.0f)/h - 1;
			buf[y*stride + x] = (uint16_t)(expf(-4.5f*(u*u+v*v))*(UINT16_MAX));
		}
	}
	return buf;
}

static uint16_t *prev_src;
static uint16_t *next_src;
static uint16_t *point_src;
static int pnt_w, pnt_h;
static int iw, ih;

void maxsrc_setup(int w, int h)
{
	pnt_w = IMAX(w/24, 8);
	pnt_h = IMAX(h/24, 8);
	iw = w; ih = h;
	point_src = setup_point(pnt_w, pnt_h);

	prev_src = _mm_malloc(2 * w * h * sizeof(uint16_t), 32);
	memset(prev_src, 0, 2*w*h*sizeof(uint16_t));
	next_src = prev_src + w*h;
}

#define COORD_S (8)
#define COORD_R (256)
#define COORD_M (COORD_R-1)

static void draw_point(void *restrict dest, float px, float py)
{
	int ipx = lrintf(px*256), ipy = lrintf(py*256);
	uint yf = ipy&0xff, xf = ipx&0xff;
	uint a00 = (yf*xf)/2;
	uint a01 = (yf*(255-xf))/2;
	uint a10 = ((255-yf)*xf)/2;
	uint a11 = ((255-yf)*(255-xf))/2;

	unsigned int off = (ipy/256)*iw + ipx/256;

	uint16_t *restrict dst = dest;
	for(int y=0; y < pnt_h; y++) {
		for(int x=0; x < pnt_w; x++) {
			uint32_t res = (point_src[(pnt_w+1)*y+x]*a00 + point_src[(pnt_w+1)*y+x+1]*a01
					+ point_src[(pnt_w+1)*(y+1)+x]*a10   + point_src[(pnt_w+1)*(y+1)+x+1]*a11)>>15;
			dst[off+iw*y+x] = IMAX(res, dst[off+iw*y+x]);
		}
	}
}

static void zoom(uint16_t * restrict out, uint16_t * restrict in, int w, int h, float R[3][3])
{
	float xstep = 2.0f/w, ystep = 2.0f/h;
	for(int yd = 0; yd < h; yd++) {
		float v = yd*ystep - 1.0f;
		for(int xd = 0; xd < w; xd++) {
			float u = xd*xstep - 1.0f;

			float d = 0.95f + 0.05f*sqrtf(u*u + v*v);
			float p[] = { // first rotate our frame of reference, then do a zoom along 2 of the 3 axis
				(u*R[0][0] + v*R[0][1]),
				(u*R[1][0] + v*R[1][1])*d,
				(u*R[2][0] + v*R[2][1])*d
			};

			// rotate back and shift/scale to [0, 1]
			float x = (p[0]*R[0][0] + p[1]*R[1][0] + p[2]*R[2][0]+1.0f)*0.5f;
			float y = (p[0]*R[0][1] + p[1]*R[1][1] + p[2]*R[2][1]+1.0f)*0.5f;

			int xs = IMIN(IMAX(lrintf(x*w*256), 0), (w-1)*256);
			int ys = IMIN(IMAX(lrintf(y*h*256), 0), (h-1)*256);
			int x1 = xs>>8, x2 = IMIN(x1+1,w-1), xf = xs&0xFF;
			int y1 = ys>>8, y2 = IMIN(y1+1,h-1), yf = ys&0xFF;

			uint16_t r = ((in[y1*w + x1]*(0xff - xf) + in[y1*w + x2]*xf)*(0xff-yf) +
						  (in[y2*w + x1]*(0xff - xf) + in[y2*w + x2]*xf)*yf) >> 16;
			*(out++) = r;
		}
	}
}

static float tx=0, ty=0, tz=0;

static inline float getsamp(audio_data *d, int i, int w) {
	float res = 0;
	int l = IMAX(i-w, 0);
	int u = IMIN(i+w, d->len);
	for(int i = l; i < u; i++) {
		res += d->data[i];
	}
	return res / (2*w);
}


// MUST NOT be called < frame of consumer apart (only uses double buffering)
// if it's called too soon consumer may be using the frame we are modifying
// we don't use triple buffering because this really doesn't need to run very
// often, even below 12 times a second still looks ok, and double buffering means
// we take up less cache and fewer pages and that means fewer falts, and since almost
// everything we do either runs fast enough or bottleneeks on memory double
// buffering here seems like a good idea
void maxsrc_update(void)
{
	uint16_t *dst = next_src;

	audio_data ad;
	audio_get_samples(&ad);
	int samp = IMAX(IMAX(iw,ih)/4, 1023);
	//samp = 4;
	float cx=cosf(tx), cy=cosf(ty), cz=cosf(tz);
	float sx=sinf(tx), sy=sinf(ty), sz=sinf(tz);

	float R[][3] = {
		{cz*cy-sz*sx*sy, -sz*cx, -sy*cz-cy*sz*sx},
		{sz*cy+cz*sx*sy,  cz*cx, -sy*sz+cy*cz*sx},
		{cx*sy         ,    -sx,  cy*cx}
	};

	zoom(dst, prev_src, iw, ih, R);
	fade_pix(dst, iw, ih, 255*98/100);

	for(int i=0; i<samp; i++) {
		float s = getsamp(&ad, i*ad.len/samp, ad.len/96);
		s=copysignf(log2f(fabsf(s)*3+1)/2, s);

		float xt = (i - samp/2)*1.0f/samp;
		float yt = 0.2f*s;
		float zt = 0.0f;

		float x = R[0][0]*xt + R[1][0]*yt + R[2][0]*zt;
		float y = R[0][1]*xt + R[1][1]*yt + R[2][1]*zt;
		float z = R[0][2]*xt + R[1][2]*yt + R[2][2]*zt;
		float zvd = 1/(z+2);

		float xi = x*zvd*iw*3/4+iw/2 - pnt_w/2;
		float yi = y*zvd*ih*3/4+ih/2 - pnt_h/2;
		draw_point(dst, xi, yi);
	}
	next_src = prev_src;
	prev_src = dst;

	tx+=0.02; ty+=0.01; tz-=0.003;
}


uint16_t *maxsrc_get(void) {
	return prev_src;
}

