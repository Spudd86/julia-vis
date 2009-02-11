#include <unistd.h>
#include <stdint.h>
#include <malloc.h>
#include <math.h>

#include "common.h"
#include "audio/audio.h"
#include "tribuf.h"
#include "pixmisc.h"

static void initbuf(uint16_t *max_src, int stride, int w, int h) 
{
	for(int y=0; y < h; y++)  {
		for(int x=0; x < w; x++) {
			float u = 2*(x-w*0.5f+0.5f)/w; float v = 2*(y-w*0.5f+0.5f)/h;
			float d = log2f(sqrtf(u*u + v*v)*M_SQRT1_2 + 1.0f);
			max_src[y*stride + x] = (uint16_t)(fmaxf(1.0f - d, 0.0f)*UINT16_MAX);
		}
	}
}

static uint16_t *setup_point(int w, int h) 
{
	uint16_t *max_src = malloc((w+1) * (h+1) * sizeof(uint16_t));
	memset(max_src, 0, (w+1)*(h+1)*sizeof(uint16_t));
	initbuf(max_src, w+1, w, h);
	
	return max_src;
}


//~ static tribuf *max_tb;
//~ static uint16_t *max_src[3];
static uint16_t *prev_src;
static uint16_t *next_src;
static uint16_t *point_src;
static int pnt_w, pnt_h;
static int iw, ih;

void maxsrc_setup(int w, int h)
{
	pnt_w = IMAX(w/24, 8); // need divisiable by 8, at least 8
	pnt_h = IMAX(h/24, 8);
	iw = w; ih = h;
	point_src = setup_point(pnt_w, pnt_h);
	
	prev_src = valloc(2 * w * h * sizeof(uint16_t));
	memset(prev_src, 0, 2*w*h*sizeof(uint16_t));
	next_src = prev_src + w*h;
	
	//~ max_src[0] = valloc(3 * w * h * sizeof(uint16_t));
	//~ memset(max_src[0], 0, 3*w*h*sizeof(uint16_t));
	//~ for(int i=1; i<3; i++)
		//~ max_src[i] = max_src[0] + i * w * h;
	
	//~ for(int i=0; i<3; i++) {
		//~ max_src[i] = valloc(w * h * sizeof(uint16_t));
		//~ memset(max_src[i], 0, w*h*sizeof(uint16_t));
	//~ }
	
	//~ next_src = max_src[0];
	//~ prev_src = max_src[1];
	//~ max_tb = tribuf_new((void **)max_src);
	//~ prev_src = tribuf_get_read(max_tb);
}

uint16_t *maxsrc_get(void)
{
	//return tribuf_get_read(max_tb);
	return prev_src;
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

static void draw_point(void *restrict dest, float px, float py)
{
	float yfrac = fmodf(py, 1.0f);
	float xfrac = fmodf(px, 1.0f);
	int a00 = yfrac*xfrac*256;
	int a01 = yfrac*(1.0f-xfrac)*256;
	int a10 = (1.0f-yfrac)*xfrac*256;
	int a11 = (1.0f-yfrac)*(1.0f-xfrac)*256;
	
	unsigned int off = ((int)py)*iw + (int)px;
	
	uint16_t *restrict dst = dest;
	for(int y=0; y < pnt_h; y++) {
		for(int x=0; x < pnt_w; x++) {
			int res = point_src[(pnt_w+1)*y+x] * a00     + point_src[(pnt_w+1)*y+x+1] * a01 
					+ point_src[(pnt_w+1)*(y+1)+x] * a10 + point_src[(pnt_w+1)*(y+1)+x+1] * a11;
			
			dst[off+iw*y+x] = IMAX(dst[off+iw*y+x], res>>8);
		}
	}
}

static void zoom(uint16_t *out, uint16_t *in, int w, int h, float xzf, float yzf)
{
	float s = sinf(-0.001f), c = cosf(-0.001f);
	
	float xstep = 2.0f/w, ystep = 2.0f/h;
	for(int yd = 0; yd < h; yd++) {
		float v = yd*ystep - 1.0f;// + 0.5f/h; 
		for(int xd = 0; xd < w; xd++) {
			float u = xd*xstep - 1.0f + 0.5f/w;
			float d = 1.0f - sqrtf(u*u + v*v)*0.01*M_SQRT1_2;
			//float xz = 1.0f - xzf*0.01f;
			//float yz = 1.0f - yzf*0.01f;
			float x = u*c - v*s;
			float y = u*s + v*c;
			x = (x*d+1.0f)*0.5f;
			y = (y*d+1.0f)*0.5f;
					
			int xs = IMIN(IMAX(lrintf(x*w*256), 0), (w-1)*256);
			int ys = IMIN(IMAX(lrintf(y*h*256), 0), (h-1)*256);
			int x1 = xs>>8, x2 = x1+1, xf = xs&0xFF;
			int y1 = ys>>8, y2 = y1+1, yf = ys&0xFF;
			
			*(out++) = (((in[y1*w + x1]*(0xff - xf) + in[y1*w + x2]*xf)*(0xff-yf) +
						(in[y2*w + x1]*(0xff - xf) + in[y2*w + x2]*xf)*yf) >> 16);
		}
	}
}


// MUST NOT be called < frame of consumer apart (only uses double buffering)
// if it's called too soon consumer may be using the frame we are modifying
//   Note: this is also true of the triple buffering but for that to break we'd
//       need to be calling this at least 3 times faster than the consumer is running
//       which would be just wastful and stupid
void maxsrc_update(void)
{
	//~ uint16_t *dst = tribuf_get_write(max_tb);
	uint16_t *dst = next_src;
	
	audio_data ad;
	audio_get_samples(&ad);
	int samp = IMAX(iw,ih)*2/5;
	
	float cx=cosf(tx), cy=cosf(ty), cz=cosf(tz); 
	float sx=sinf(tx), sy=sinf(ty), sz=sinf(tz); 
	float R[][3] = {
		{cy*cz,				cy*sz,				-cy},
		{sx*sy*cz-cx*sz,	sz*sy*sz+cz*cz,		-sx*cy},
		{-cz*sy*cz+sx*sz,	-cx*sy*sz-sx*cz,	cx*cy}
	};
	
	float xzf = fabsf(R[0][0]), yzf = fabsf(R[1][1]);
	
	// want to zoom x by 1.0 - (1-d)*xzf*0.01
	// want to zoom y by 1.0 - (1-d)*yzf*0.01
	
	zoom(dst, prev_src, iw, ih, xzf, yzf);
	fade_pix(dst, iw, ih, 255*98/100);
	
	for(int i=0; i<samp; i++) {
		float s = getsamp(&ad, i*ad.len/samp, ad.len/96);
		s=copysignf(logf(fabsf(s)*3+1)/2, s);
		
		float xt = (i - samp/2)*1.0f/samp;
		float yt = 0.2f*s;
		float zt = 0.0f;
		
		float x = R[0][0]*xt + R[0][1]*yt + R[0][2]*zt;
		float y = R[1][0]*xt + R[1][1]*yt + R[1][2]*zt;
		float z = R[2][0]*xt + R[2][1]*yt + R[2][2]*zt;
		float zvd = 1/(z+2);
		
		float xi = x*zvd*iw*3/4+iw/2 - pnt_w/2;
		float yi = y*zvd*ih*3/4+ih/2 - pnt_h/2;
		draw_point(dst, xi, yi);
	}
	
	//~ tribuf_finish_write(max_tb);
	next_src = prev_src;
	prev_src = dst;
	
	tx+=0.03; ty+=0.01; tx-=0.025;
}
