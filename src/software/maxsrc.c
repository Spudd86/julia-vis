//#pragma GCC optimize "3,ira-hoist-pressure,inline-functions,merge-all-constants,modulo-sched,modulo-sched-allow-regmoves"

#include "common.h"
#include "audio/audio.h"
#include "tribuf.h"
#include "maxsrc.h"
#include "getsamp.h"

#include <float.h>
#include <assert.h>

#if defined(__SSE__) //TODO: runtime detect
#include <mmintrin.h>
#include <xmmintrin.h>
#endif

typedef struct {
	uint16_t *restrict data;
	uint16_t w,h;
} MxSurf;

static void point_init(MxSurf *res, int w, int h);
static void zoom(uint16_t * restrict out, uint16_t * restrict in, int w, int h, const float R[3][3]);
static void draw_point(void *restrict dest, int iw, int ih, const MxSurf *pnt_src, float px, float py);

struct maxsrc {
	void *buf;
	uint16_t *prev_src;
	uint16_t *next_src;
	int iw, ih;
	int samp;
	float tx, ty, tz;

	MxSurf pnt_src;
};

struct maxsrc *maxsrc_new(int w, int h)
{
	struct maxsrc *self = calloc(sizeof(*self), 1);

	self->iw = w; self->ih = h;
	self->samp = IMIN(IMAX(w,h), 1023);

	point_init(&self->pnt_src, IMAX(w/24, 8), IMAX(h/24, 8));
	printf("maxsrc using %i %dx%d points\n", self->samp, self->pnt_src.w, self->pnt_src.h);

	self->prev_src = self->buf = aligned_alloc(64, 2 * w * h * sizeof(uint16_t));
	memset(self->prev_src, 0, 2*w*h*sizeof(uint16_t));
	self->next_src = self->prev_src + w*h;

	return self;
}

void maxsrc_delete(struct maxsrc *self)
{
	aligned_free(self->buf);
	free((void*)self->pnt_src.data);
	self->next_src = self->prev_src = self->buf = NULL;
	self->iw = self->ih = self->samp = 0;
	free(self);
}

const uint16_t *maxsrc_get(struct maxsrc *self) {
	return self->prev_src;
}

// MUST NOT be called < frame of consumer apart (only uses double buffering)
// if it's called too soon consumer may be using the frame we are modifying
// we don't use triple buffering because this really doesn't need to run very
// often, even below 12 times a second still looks ok, and double buffering means
// we take up less cache and fewer pages and that means fewer falts, and since almost
// everything we do either runs fast enough or bottleneeks on memory double
// buffering here seems like a good idea
void maxsrc_update(struct maxsrc *self, const float *audio, int audiolen)
{
	uint16_t *dst = self->next_src;
	int samp = self->samp;
	int iw = self->iw, ih = self->ih;

	float cx=cosf(self->tx), cy=cosf(self->ty), cz=cosf(self->tz);
	float sx=sinf(self->tx), sy=sinf(self->ty), sz=sinf(self->tz);

	const float R[][3] = {
		{cz*cy-sz*sx*sy, -sz*cx, -sy*cz-cy*sz*sx},
		{sz*cy+cz*sx*sy,  cz*cx, -sy*sz+cy*cz*sx},
		{cx*sy         ,    -sx,  cy*cx}
	};

	zoom(dst, self->prev_src, iw, ih, R);

	for(int i=0; i<samp; i++) {
		float s = getsamp(audio, audiolen, i*audiolen/(samp-1), audiolen/96);

		s=copysignf(log2f(fabsf(s)*3+1)/2, s);

		float xt = (float)i/(float)(samp - 1) - 0.5f; // (i - (samp-1)/2.0f)*(1.0f/(samp-1));
		float yt = 0.2f*s;
		float zt = 0.0f;

		float x = R[0][0]*xt + R[1][0]*yt + R[2][0]*zt;
		float y = R[0][1]*xt + R[1][1]*yt + R[2][1]*zt;
		float z = R[0][2]*xt + R[1][2]*yt + R[2][2]*zt;
		float zvd = 0.75f/(z+2);

		float xi = x*zvd*iw+(iw - self->pnt_src.w)/2.0f;
		float yi = y*zvd*ih+(ih - self->pnt_src.h)/2.0f;
		draw_point(dst, self->iw, self->ih, &self->pnt_src, xi, yi);
	}
	self->next_src = self->prev_src;
	self->prev_src = dst;

	self->tx+=0.02f; self->ty+=0.01f; self->tz-=0.003f;
}

static void point_init(MxSurf *res, int w, int h)
{
	res->w = w; res->h = h;
	uint16_t *buf = xmalloc((w+1) * (h+1) * sizeof(uint16_t));
	memset(buf, 0, (w+1)*(h+1)*sizeof(uint16_t));
	int stride = w+1;
	for(int y=0; y < h; y++)  {
		for(int x=0; x < w; x++) {
			float u = 1.25f*((2*x+1.0f)/(w-1) - 1), v = 1.25f*((2*y+1.0f)/(h-1) - 1);
			buf[y*stride + x] = (uint16_t)(expf(-4.5f*0.5f*log2f((u*u+v*v) + 1.0f))*(UINT16_MAX));
		}
	}

	res->data = buf;
}

static void draw_point(void *restrict dest, int iw, int ih, const MxSurf *pnt_src, float px, float py)
{(void)ih;
	const uint32_t ipx = px*256, ipy = py*256;
	uint32_t yf = ipy&0xff, xf = ipx&0xff;
	uint32_t a00 = (yf*xf);
	uint32_t a01 = (yf*(256-xf));
	uint32_t a10 = ((256-yf)*xf);
	uint32_t a11 = ((256-yf)*(256-xf));

	uint32_t off = (ipy/256u)*iw + ipx/256u;

	const int pnt_stride = pnt_src->w+1;

#if defined(__SSE__) //TODO: runtime detect
	if(pnt_src->w % 4 == 0) {
		const __m64 w00 = _mm_set1_pi16(a00);
		const __m64 w01 = _mm_set1_pi16(a01);
		const __m64 w10 = _mm_set1_pi16(a10);
		const __m64 w11 = _mm_set1_pi16(a11);

		const __m64 max_off = _mm_set1_pi16(0x8000);
		for(int y=0; y < pnt_src->h; y++) {
			uint16_t *restrict dst_line = (uint16_t *restrict)dest + off + iw*y;
			const uint16_t *s0 = pnt_src->data + y*pnt_stride;
			const uint16_t *s1 = s0 + pnt_stride;
			_mm_prefetch(dst_line, _MM_HINT_NTA);
			for(int x=0; x < pnt_src->w; x+=4, dst_line+=4, s0+=4, s1+=4) {
				_mm_prefetch(dst_line + 4, _MM_HINT_NTA);
				__m64 p00 = *(const __m64 *)(s0);
				__m64 p01 = *(const __m64 *)(s0 + 1);
				__m64 p10 = *(const __m64 *)(s1);
				__m64 p11 = *(const __m64 *)(s1 + 1);

				p00 = _mm_mulhi_pu16(p00, w00);
				p01 = _mm_mulhi_pu16(p01, w01);
				p10 = _mm_mulhi_pu16(p10, w10);
				p11 = _mm_mulhi_pu16(p11, w11);

				__m64 oa = _mm_add_pi16(p00, p01);
				__m64 ob = _mm_add_pi16(p10, p11);
				__m64 res = _mm_add_pi16(oa, ob);
				// no need to shift because when we did the multiplication we
				// only got the high 16 bits

				__m64 dp = *(__m64 *)(dst_line);
				dp  = _mm_add_pi16(dp,  max_off);
				res = _mm_add_pi16(res, max_off);
				res = _mm_max_pi16(res, dp);
				res = _mm_sub_pi16(res, max_off);

				*(__m64 *)(dst_line) = res;
			}
		}
		_mm_empty();
		return;
	}
#endif
	const uint16_t *s0 = pnt_src->data;
	const uint16_t *s1 = pnt_src->data + pnt_stride;
	for(int y=0; y < pnt_src->h; y++) {
		uint16_t *restrict dst_line = (uint16_t *restrict)dest + off + iw*y;
		for(int x=0; x < pnt_src->w; x++) {
			uint16_t res = (s0[x]*a00 + s0[x+1]*a01
			              + s1[x]*a10 + s1[x+1]*a11)>>16;
			res = IMAX(res, dst_line[x]);
			dst_line[x] = res;
		}
		s0 += pnt_stride;
		s1 += pnt_stride;
	}
}

#define BLOCK_SIZE 8

static void zoom(uint16_t * restrict out, uint16_t * restrict in, int w, int h, const float R[3][3])
{
	const float ustep = BLOCK_SIZE*2.0f/w, vstep = BLOCK_SIZE*2.0f/h;
	float v0 = -1.0f;
	for(int yd = 0; yd < h; yd+=BLOCK_SIZE) {
		float v1 = v0+vstep;

		float x, y;

		{	const float u = -1.0f, v = v0;
			const float d = 0.95f + 0.053f*hypotf(u,v);
			const float p[] = { // first rotate our frame of reference, then do a zoom along 2 of the 3 axis
				(u*R[0][0] + v*R[0][1]),
				(u*R[1][0] + v*R[1][1])*d,
				(u*R[2][0] + v*R[2][1])*d
			};
			x = (p[0]*R[0][0] + p[1]*R[1][0] + p[2]*R[2][0]+1.0f)*0.5f;
			y = (p[0]*R[0][1] + p[1]*R[1][1] + p[2]*R[2][1]+1.0f)*0.5f;
		}
		float x0 = fmaxf(fminf(x, 1.0f-FLT_EPSILON), 0.0f)*w*256;
		float y0 = fmaxf(fminf(y, 1.0f-FLT_EPSILON), 0.0f)*h*256;

		{	const float u = -1.0f, v = v1;
			const float d = 0.95f + 0.053f*hypotf(u,v);
			const float p[] = { // first rotate our frame of reference, then do a zoom along 2 of the 3 axis
				(u*R[0][0] + v*R[0][1]),
				(u*R[1][0] + v*R[1][1])*d,
				(u*R[2][0] + v*R[2][1])*d
			};
			x = (p[0]*R[0][0] + p[1]*R[1][0] + p[2]*R[2][0]+1.0f)*0.5f;
			y = (p[0]*R[0][1] + p[1]*R[1][1] + p[2]*R[2][1]+1.0f)*0.5f;
		}
		float x0s = (fmaxf(fminf(x, 1.0f-FLT_EPSILON), 0.0f)*w*256 - x0)/BLOCK_SIZE;
		float y0s = (fmaxf(fminf(y, 1.0f-FLT_EPSILON), 0.0f)*h*256 - y0)/BLOCK_SIZE;

		float u1 = -1.0f;
		for(int xd = 0; xd < w; xd+=BLOCK_SIZE) {
			u1 = u1+ustep;

			{	const float u = u1, v = v0;
				const float d = 0.95f + 0.053f*hypotf(u,v);
				const float p[] = { // first rotate our frame of reference, then do a zoom along 2 of the 3 axis
					(u*R[0][0] + v*R[0][1]),
					(u*R[1][0] + v*R[1][1])*d,
					(u*R[2][0] + v*R[2][1])*d
				};
				x = (p[0]*R[0][0] + p[1]*R[1][0] + p[2]*R[2][0]+1.0f)*0.5f;
				y = (p[0]*R[0][1] + p[1]*R[1][1] + p[2]*R[2][1]+1.0f)*0.5f;
			}
			const float x1 = fmaxf(fminf(x, 1.0f-FLT_EPSILON), 0.0f)*w*256;
			const float y1 = fmaxf(fminf(y, 1.0f-FLT_EPSILON), 0.0f)*h*256;


			{	const float u = u1, v = v1;
				const float d = 0.95f + 0.053f*hypotf(u,v);
				const float p[] = { // first rotate our frame of reference, then do a zoom along 2 of the 3 axis
					(u*R[0][0] + v*R[0][1]),
					(u*R[1][0] + v*R[1][1])*d,
					(u*R[2][0] + v*R[2][1])*d
				};
				x = (p[0]*R[0][0] + p[1]*R[1][0] + p[2]*R[2][0]+1.0f)*0.5f;
				y = (p[0]*R[0][1] + p[1]*R[1][1] + p[2]*R[2][1]+1.0f)*0.5f;
			}
			const float x1s = (fmaxf(fminf(x, 1.0f-FLT_EPSILON), 0.0f)*w*256 - x1)/BLOCK_SIZE;
			const float y1s = (fmaxf(fminf(y, 1.0f-FLT_EPSILON), 0.0f)*h*256 - y1)/BLOCK_SIZE;

			const float xsts = (x1s - x0s)/BLOCK_SIZE;
			const float ysts = (y1s - y0s)/BLOCK_SIZE;
			float fxst = (x1 - x0)/BLOCK_SIZE;
			float fyst = (y1 - y0)/BLOCK_SIZE;

			for(uint32_t yt=0; yt<BLOCK_SIZE; yt++, x0+=x0s, y0+=y0s, fxst+=xsts, fyst+=ysts) {
				uint16_t *restrict out_line = out + (yd+yt)*w + xd;
				int32_t xst = fxst, yst = fyst;
				for(uint32_t xt=0, u = x0, v = y0; xt<BLOCK_SIZE; xt++, u+=xst, v+=yst) {
					assert(u >= 0); assert(u < w*256); // in debug builds make sure rounding hasn't messed us up
					assert(v >= 0); assert(v < h*256);

					const uint32_t xs=u>>8,  ys=v>>8;
					const uint32_t xf=u&0xFF, yf=v&0xFF;
					const uint32_t xi1 = xs;
					const uint32_t yi1 = ys*w;
					const uint32_t xi2 = IMIN(xi1+1,(uint32_t)(w-1));
					const uint32_t yi2 = IMIN(yi1+w, (uint32_t)((h-1)*w));

#if 0 // TODO: use top version if we don't have fast 64bit ints
					// it is critical that this entire calculation be done as uint32s to avoid overflow
					uint32_t tmp = ((in[yi1 + xi1]*(256u - xf) + in[yi1 + xi2]*xf)*(256u-yf) +
								    (in[yi2 + xi1]*(256u - xf) + in[yi2 + xi2]*xf)*yf) >> 16u;
					tmp = (tmp*((256u*97u)/100u)) >> 8u;
#else
					uint64_t tmp = ((in[yi1 + xi1]*(256u - xf) + in[yi1 + xi2]*xf)*(256u-yf) +
								    (in[yi2 + xi1]*(256u - xf) + in[yi2 + xi2]*xf)*yf);
					tmp = (tmp*((256u*97u)/100u)) >> 24u;
#endif

					out_line[xt] = (uint16_t)tmp;
				}
			}
			x0 = x1; y0 = y1;
			x0s = x1s; y0s = y1s;
		}
		v0=v1;
	}
}

