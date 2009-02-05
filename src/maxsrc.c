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
			float d = sqrtf(u*u + v*v)*M_SQRT1_2;
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


static tribuf *max_tb;
static uint16_t *max_src[3];
static uint16_t *prev_src;
static uint16_t *point_src;
static int pnt_w, pnt_h;
static int iw, ih;

void maxsrc_setup(int w, int h)
{
	pnt_w = IMAX(w/32, 8); // need divisiable by 8, at least 8
	pnt_h = IMAX(h/32, 8);
	iw = w; ih = h;
	point_src = setup_point(pnt_w, pnt_h);
	
	for(int i=0; i<3; i++) {
		max_src[i] = valloc(w * h * sizeof(uint16_t));
		memset(max_src[i], 0, w*h*sizeof(uint16_t));
	}
	
	max_tb = tribuf_new((void **)max_src);
	prev_src = tribuf_get_read(max_tb);
}

uint16_t *maxsrc_get(void)
{
	return tribuf_get_read(max_tb);
}

static float sigmoid(float x) {
	float e = expf(x);
	return e/(1+e);
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

void maxsrc_update(void)
{
	uint16_t *dst = tribuf_get_write(max_tb);
	
	fade_pix(dst, prev_src, iw, ih, 255*92/100);
	
	audio_data ad;
	audio_get_samples(&ad);
	int samp = iw/2;
	
	float cx=cosf(tx), cy=cosf(ty), cz=cosf(tz); 
	float sx=sinf(tx), sy=sinf(ty), sz=sinf(tz); 
	float R[][3] = {
		{cy*cz,				cy*sz,				-cy},
		{sx*sy*cz-cx*sz,	sz*sy*sz+cz*cz,		-sx*cy},
		{-cz*sy*cz+sx*sz,	-cx*sy*sz-sx*cz,	cx*cy}
	};
	
	//maxblend_stride(dst + ih*iw/2 + iw/2, iw, point_src, pnt_w, pnt_h);
	
	for(int i=0; i<samp; i++) {
		float s = getsamp(&ad, i*ad.len/samp, ad.len/96);
		s=sigmoid(2*s)*2 - 1;
		//s=copysignf(expm1f(fabsf(s)+1)-expm1(1), s)/(expm1(2)-expm1(1));//)*2.0f - 1.0f; 
		
		float xt = (i - samp/2)*1.0f/samp;
		float yt = 0.1f*s;
		float zt = 0.0f;
		
		float x = R[0][0]*xt + R[0][1]*yt + R[0][2]*zt;
		float y = R[1][0]*xt + R[1][1]*yt + R[1][2]*zt;
		float z = R[2][0]*xt + R[2][1]*yt + R[2][2]*zt;
		float zvd = 1/(z+2);
		
		float xi = x*zvd*iw*3/4+iw/2 - pnt_w/2;
		float yi = y*zvd*ih*3/4+ih/2 - pnt_h/2;
		draw_point(dst, xi, yi);
		//maxblend_stride(dst + yi*iw + xi, iw, point_src, pnt_w, pnt_h);
	}
	
	tribuf_finish_write(max_tb);
	prev_src = dst;
	
	tx+=0.005; ty+=0.01; tx-=0.0025;
}
