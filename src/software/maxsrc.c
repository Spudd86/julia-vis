
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
			float u = (2*x+1.0f)/(w-1) - 1, v = (2*y+1.0f)/(h-1) - 1;
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
static int samp = 0;

void maxsrc_setup(int w, int h)
{
	pnt_w = IMAX(w/24, 8);
	pnt_h = IMAX(h/24, 8);
	iw = w; ih = h;
	samp = IMIN(IMAX(iw,ih)/2, 1023);
	printf("maxsrc using %i points\n", samp);

	point_src = setup_point(pnt_w, pnt_h);

#ifdef HAVE_MMAP
	// use mmap here since it'll give us a nice page aligned chunk (malloc will probably be using it anyway...)
	prev_src = mmap(NULL, 2 * w * h * sizeof(uint16_t), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, 0, 0);
#else
	prev_src = _mm_malloc(2 * w * h * sizeof(uint16_t), 32);
#endif
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
	uint a00 = (yf*xf);
	uint a01 = (yf*(255-xf));
	uint a10 = ((255-yf)*xf);
	uint a11 = ((255-yf)*(255-xf));

	unsigned int off = (ipy/256)*iw + ipx/256;

	uint16_t *restrict dst = dest;
	for(int y=0; y < pnt_h; y++) {
		for(int x=0; x < pnt_w; x++) {
			uint16_t res = (point_src[(pnt_w+1)*y+x]*a00 + point_src[(pnt_w+1)*y+x+1]*a01
					+ point_src[(pnt_w+1)*(y+1)+x]*a10   + point_src[(pnt_w+1)*(y+1)+x+1]*a11)>>16;
			dst[off+iw*y+x] = IMAX(res, dst[off+iw*y+x]);
		}
	}
}

#define BLOCK_SIZE 16

static void zoom(uint16_t * restrict out, uint16_t * restrict in, int w, int h, float R[3][3])
{
	const float ustep = BLOCK_SIZE*2.0f/w, vstep = BLOCK_SIZE*2.0f/h;
	float v0 = -1.0f;
	for(int yd = 0; yd < h; yd+=BLOCK_SIZE) {
		float v1 = v0+vstep;

		float x, y;

		{	const float u = -1.0f, v = v0;
			const float d = 0.95f + 0.05f*sqrtf(u*u + v*v);
			const float p[] = { // first rotate our frame of reference, then do a zoom along 2 of the 3 axis
				(u*R[0][0] + v*R[0][1]),
				(u*R[1][0] + v*R[1][1])*d,
				(u*R[2][0] + v*R[2][1])*d
			};
			x = (p[0]*R[0][0] + p[1]*R[1][0] + p[2]*R[2][0]+1.0f)*0.5f;
			y = (p[0]*R[0][1] + p[1]*R[1][1] + p[2]*R[2][1]+1.0f)*0.5f;
		}
		int x0 = IMIN(IMAX(lrintf(x*w*256), 0), (w-1)*256);
		int y0 = IMIN(IMAX(lrintf(y*h*256), 0), (h-1)*256);

		{	const float u = -1.0f, v = v1;
			const float d = 0.95f + 0.05f*sqrtf(u*u + v*v);
			const float p[] = { // first rotate our frame of reference, then do a zoom along 2 of the 3 axis
				(u*R[0][0] + v*R[0][1]),
				(u*R[1][0] + v*R[1][1])*d,
				(u*R[2][0] + v*R[2][1])*d
			};
			x = (p[0]*R[0][0] + p[1]*R[1][0] + p[2]*R[2][0]+1.0f)*0.5f;
			y = (p[0]*R[0][1] + p[1]*R[1][1] + p[2]*R[2][1]+1.0f)*0.5f;
		}
		int x0s = (IMIN(IMAX(lrintf(x*w*256), 0), (w-1)*256) - x0)/BLOCK_SIZE;
		int y0s = (IMIN(IMAX(lrintf(y*h*256), 0), (h-1)*256) - y0)/BLOCK_SIZE;

		float u1 = -1.0f;
		for(int xd = 0; xd < w; xd+=BLOCK_SIZE) {
			u1 = u1+ustep;

			{	const float u = u1, v = v0;
				const float d = 0.95f + 0.05f*sqrtf(u*u + v*v);
				const float p[] = { // first rotate our frame of reference, then do a zoom along 2 of the 3 axis
					(u*R[0][0] + v*R[0][1]),
					(u*R[1][0] + v*R[1][1])*d,
					(u*R[2][0] + v*R[2][1])*d
				};
				x = (p[0]*R[0][0] + p[1]*R[1][0] + p[2]*R[2][0]+1.0f)*0.5f;
				y = (p[0]*R[0][1] + p[1]*R[1][1] + p[2]*R[2][1]+1.0f)*0.5f;
			}
			const int x1 = IMIN(IMAX(lrintf(x*w*256), 0), (w-1)*256);
			const int y1 = IMIN(IMAX(lrintf(y*h*256), 0), (h-1)*256);

			{	const float u = u1, v = v1;
				const float d = 0.95f + 0.05f*sqrtf(u*u + v*v);
				const float p[] = { // first rotate our frame of reference, then do a zoom along 2 of the 3 axis
					(u*R[0][0] + v*R[0][1]),
					(u*R[1][0] + v*R[1][1])*d,
					(u*R[2][0] + v*R[2][1])*d
				};
				x = (p[0]*R[0][0] + p[1]*R[1][0] + p[2]*R[2][0]+1.0f)*0.5f;
				y = (p[0]*R[0][1] + p[1]*R[1][1] + p[2]*R[2][1]+1.0f)*0.5f;
			}
			const int x1s = (IMIN(IMAX(lrintf(x*w*256), 0), (w-1)*256) - x1)/BLOCK_SIZE;
			const int y1s = (IMIN(IMAX(lrintf(y*h*256), 0), (h-1)*256) - y1)/BLOCK_SIZE;

			const int xsts = (x1s - x0s)/BLOCK_SIZE;
			const int ysts = (y1s - y0s)/BLOCK_SIZE;
			int xst = (x1 - x0)/BLOCK_SIZE;
			int yst = (y1 - y0)/BLOCK_SIZE;

			for(int yt=0; yt<BLOCK_SIZE; yt++, x0+=x0s, y0+=y0s, xst += xsts, yst += ysts) {
				for(int xt=0, x = x0, y = y0; xt<BLOCK_SIZE; xt++, x+=xst, y+=yst) {
					const int xs=x/256, ys=y/256;
					const int xf=x&0xFF, yf=y&0xFF;
					const int xi1 = xs;
					const int yi1 = ys*w;
					const int xi2 = IMIN(xi1+1,w-1);
					const int yi2 = IMIN(yi1+w,(h-1)*w);

					uint32_t tmp = ((in[yi1 + xi1]*(255 - xf) + in[yi1 + xi2]*xf)*(255-yf) +
								(in[yi2 + xi1]*(255 - xf) + in[yi2 + xi2]*xf)*yf);
					out[(yd+yt)*w+xd+xt] = ((255*98/100)*(tmp>>8)) >> 16;
				}
			}
			x0 = x1; y0 = y1;
			x0s = x1s; y0s = y1s;
		}
		v0=v1;
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
//	samp = 4;
	float cx=cosf(tx), cy=cosf(ty), cz=cosf(tz);
	float sx=sinf(tx), sy=sinf(ty), sz=sinf(tz);

	float R[][3] = {
		{cz*cy-sz*sx*sy, -sz*cx, -sy*cz-cy*sz*sx},
		{sz*cy+cz*sx*sy,  cz*cx, -sy*sz+cy*cz*sx},
		{cx*sy         ,    -sx,  cy*cx}
	};

	zoom(dst, prev_src, iw, ih, R);

	audio_data ad;
	audio_get_samples(&ad);
	for(int i=0; i<samp; i++) {
		float s = getsamp(&ad, i*ad.len/(samp-1), ad.len/96);
		s=copysignf(log2f(fabsf(s)*3+1)/2, s);

		float xt = (i - (samp-1)/2.0f)*(1.0f/(samp-1));
		float yt = 0.2f*s;
		float zt = 0.0f;

		float x = R[0][0]*xt + R[1][0]*yt + R[2][0]*zt;
		float y = R[0][1]*xt + R[1][1]*yt + R[2][1]*zt;
		float z = R[0][2]*xt + R[1][2]*yt + R[2][2]*zt;
		float zvd = 0.75f/(z+2);

		float xi = x*zvd*iw+iw/2 - pnt_w/2;
		float yi = y*zvd*ih+ih/2 - pnt_h/2;
		draw_point(dst, xi, yi);
	}
	audio_finish_samples();
	next_src = prev_src;
	prev_src = dst;

	tx+=0.02; ty+=0.01; tz-=0.003;
}


uint16_t *maxsrc_get(void) {
	return prev_src;
}

