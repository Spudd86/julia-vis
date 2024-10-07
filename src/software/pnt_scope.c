//#pragma GCC optimize "3,ira-hoist-pressure,inline-functions,merge-all-constants,modulo-sched,modulo-sched-allow-regmoves"
#pragma GCC optimize "0"

#include "common.h"
#include "maxsrc.h"
#include "getsamp.h"
#include "scope_render.h"

#include <float.h>
#include <assert.h>

#define DIST_1D_STEPS 256u


typedef struct {
	uint16_t *restrict data;
	uint16_t w,h;
	uint16_t stride;

	uint16_t ddata[DIST_1D_STEPS + 2];
} MxSurf;


struct scope_renderer
{
	int iw;
	int ih;
	int samp;
	MxSurf pnt_src;
};

static void point_init(MxSurf *res, uint16_t w, uint16_t h);
static void draw_points(void *restrict dest, int iw, int ih, const MxSurf *pnt_src, int npnts, const uint32_t *pnts);


struct scope_renderer* scope_renderer_new(int w, int h, int samp)
{
	struct scope_renderer *self = calloc(sizeof(*self), 1);

	self->iw = w; self->ih = h;
	self->samp = samp;
	point_init(&self->pnt_src, (uint16_t)IMAX(w/24, 8), (uint16_t)IMAX(h/24, 8));

	return self;
}

void scope_renderer_delete(struct scope_renderer* self)
{
	aligned_free((void*)self->pnt_src.data);
	free(self);
}

void scope_render(struct scope_renderer *self,
                  void *restrict dest,
                  float tx, float ty, float tz,
                  const float *audio, int audiolen)
{
	float cx=cosf(tx), cy=cosf(ty), cz=cosf(tz);
	float sx=sinf(tx), sy=sinf(ty), sz=sinf(tz);

	const float R[][3] = {
		{cz*cy-sz*sx*sy, -sz*cx, -sy*cz-cy*sz*sx},
		{sz*cy+cz*sx*sy,  cz*cx, -sy*sz+cy*cz*sx},
		{cx*sy         ,    -sx,  cy*cx}
	};

	int iw = self->iw, ih = self->ih;
	int samp = self->samp;
	uint32_t pnts[samp*2]; // TODO: if we do dynamically choose number of points based on spacing move allocating this into context object

	// float thresh = IMIN(iw, ih) * 0.125f/24.0f ;
	int npnts = 0;
	float pxi = INFINITY, pyi = INFINITY;
	for(int i=0; i<samp; i++) {
		//TODO: maybe change the spacing of the points depending on
		// the rate of change so that the distance in final x,y coords is
		// approximately constant?
		// maybe step with samples spaced nicely for for the straight line and
		// let it insert up to n extra between any two?
		// might want initial spacing to be a little tighter, probably need to tweak 
		// that and max number of extra to insert.
		// also should check spacing post transform
		// probably want to shoot for getting about 3 pixels apart at 512x512
		float s = getsamp(audio, audiolen, i*audiolen/(samp-1), audiolen/96);

		s=copysignf(log2f(fabsf(s)*3+1)/2, s);

		// xt ∈ [-0.5, 0.5] ∀∃∄∈∉⊆⊈⊂⊄
		float xt = (float)i/(float)(samp - 1) - 0.5f; // (i - (samp-1)/2.0f)*(1.0f/(samp-1));
		float yt = 0.2f*s;
		float zt = 0.0f;

		float x = R[0][0]*xt + R[1][0]*yt + R[2][0]*zt;
		float y = R[0][1]*xt + R[1][1]*yt + R[2][1]*zt;
		float z = R[0][2]*xt + R[1][2]*yt + R[2][2]*zt;
		float zvd = 0.75f/(z+2);

		float xi = x*zvd*iw+(iw - self->pnt_src.w)/2.0f;
		float yi = y*zvd*ih+(ih - self->pnt_src.h)/2.0f;

		if((pxi-xi)*(pxi-xi) + (pyi-yi)*(pyi-yi) > 3) // if too close to previous point, skip
		{
			pxi = xi, pyi = yi;
			pnts[npnts*2+0] = (uint32_t)(xi*256);
			pnts[npnts*2+1] = (uint32_t)(yi*256);
			npnts++;
		}
	}
	// printf("Num points: % 4i\n", npnts);
	draw_points(dest, self->iw, self->ih, &self->pnt_src, npnts, pnts);
}



static void point_init(MxSurf *res, uint16_t w, uint16_t h)
{
	float minv = expf(-4.5f*0.5f*log2f(1.0f + 1.0f));
	float scale = (UINT16_MAX)/(1.0f - minv);

	res->w = w; res->h = h;
	res->stride = w + 1 + (64 - (w+1)%64);
	uint16_t *buf = aligned_alloc(128, res->stride * (h+1) * sizeof(uint16_t) + 128); // add extra padding for vector instructions to be able to run past the end
	memset(buf, 0, res->stride*(h+1)*sizeof(uint16_t) + 128);
	int stride = res->stride;
	for(int y=0; y < h; y++)  {
		for(int x=0; x < w; x++) {
			float u = (2*x+1.0f)/(w-1) - 1.0f, v = (2*y+1.0f)/(h-1) - 1.0f;
			// buf[y*stride + x] = (uint16_t)(expf(-4.5f*0.5f*log2f((u*u+v*v) + 1.0f))*(UINT16_MAX));
			buf[y*stride + x] = (uint16_t)(fmaxf(expf(-4.5f*0.5f*log2f((u*u+v*v) + 1.0f))-minv, 0.0f)*scale);
		}
	}

	res->data = buf;

	for(uint32_t i=0; i < DIST_1D_STEPS; i++)
	{
		float d = (2.0f * i) / DIST_1D_STEPS; // working in distance squared so need to multiply by 2, since biggest distance is root 2
		res->ddata[i] = (uint16_t)(fmaxf(expf(-4.5f*0.5f*log2f(d + 1.0f))-minv, 0.0f)*scale);
	}
	res->ddata[DIST_1D_STEPS + 1] = res->ddata[DIST_1D_STEPS] = res->ddata[DIST_1D_STEPS - 1];

	printf("running with %dx%d points\n", w, h);
}

#if 1

__attribute__((hot,noinline))
static void draw_points(void *restrict dest, int iw, int ih, const MxSurf *pnt_src, int npnts, const uint32_t *pnts)
{(void)ih;

	int64_t pw = pnt_src->w;
	int64_t ph = pnt_src->h;
	// int64_t ddiv_recip = ( ((int64_t)DIST_1D_STEPS << 32) * 4 ) / (pw*pw + ph*ph);
	// int64_t ddiv_recip = (int64_t)( (INT64_C(1)<<32) * (DIST_1D_STEPS*4.0f) / (pw*pw + ph*ph));
	int64_t ddiv_recip = (int64_t)( (INT64_C(1)<<16) * sqrtf(4.0f / (pw*pw + ph*ph)) );


	for(int i=0; i<npnts; i++) {
		const int64_t ipx = pnts[i*2+0], ipy = pnts[i*2+1];
		const int32_t yf = ipy&0xff, xf = ipx&0xff;
		// const uint32_t yf = 0, xf = 0;
		
		int64_t ymin = ipy - (int64_t)((pnt_src->h) << 7) - 128;
		int64_t ymax = ipy + (int64_t)((pnt_src->h) << 7) + 128;
		int64_t xmin = ipx - (int64_t)((pnt_src->w) << 7) - 128;
		int64_t xmax = ipx + (int64_t)((pnt_src->w) << 7) + 128;

		uint32_t off = (ipy/256u)*(unsigned)iw + ipx/256u;

		uint16_t *dst_line = (uint16_t *restrict)dest + off;
		for(int64_t y=ymin; y < ymax; y+=256, dst_line += iw)
		{
			int64_t py = ipy - y;
			uint16_t *dst_pix = dst_line;
			for(int64_t x=xmin; x < xmax; x+=256, dst_pix++)
			{
				int64_t px = ipx - x;

				uint64_t d = (px*px + py*py);

				d = (d * DIST_1D_STEPS * 4) / (pw*pw + ph*ph);
				uint32_t df = (d >> 8) & 0xFF;
				d = d >> 16;

				// don't have enough bits in the reciprocal...
				// d = d >> 16;
				// d = ( ((d * ddiv_recip) >> 32) * ddiv_recip) >> 48;
				// uint32_t df = (( d * ddiv_recip ) >> 40) & 0xFF;

				uint32_t res = (pnt_src->ddata[d] * (256u - df) + pnt_src->ddata[d+1] * df) >> 8u;
				uint32_t p   = *dst_pix;
				// uint16_t res = pnt_src->ddata[d];
				*dst_pix = MAX(res, p);
			}
		}
	}
}

#else

void list_pnt_blit_ssse3(void * const restrict dest, int iw, const uint16_t *restrict pnt, int pnt_stride, int pw, int ph, int samp, const uint32_t *pnts);
void list_pnt_blit_sse2(void * const restrict dest, int iw, const uint16_t *restrict pnt, int pnt_stride, int pw, int ph, int samp, const uint32_t *pnts);
void list_pnt_blit_sse(void * const restrict dest, int iw, const uint16_t *restrict pnt, int pnt_stride, int pw, int ph, int samp, const uint32_t *pnts);

__attribute__((hot,noinline,optimize("-ffinite-math-only")))
static void draw_points(void *restrict dest, int iw, int ih, const MxSurf *pnt_src, int npnts, const uint32_t *pnts)
{(void)ih;
#if 0 //__SSE__
	list_pnt_blit_sse(dest, iw, pnt_src->data, pnt_src->stride, pnt_src->w, pnt_src->h, npnts, pnts);

	// these two are actually slower than no vectorization
	//list_pnt_blit_sse2(dest, iw, pnt_src->data, pnt_src->stride, pnt_src->w, pnt_src->h, npnts, pnts);
	//list_pnt_blit_ssse3(dest, iw, pnt_src->data, pnt_src->stride, pnt_src->w, pnt_src->h, npnts, pnts);
#else
	const int pnt_stride = pnt_src->stride;

	for(int i=0; i<npnts; i++) {
		const uint32_t ipx = pnts[i*2+0], ipy = pnts[i*2+1];
		const uint32_t yf = ipy&0xff, xf = ipx&0xff;

		uint32_t a00 = (yf*xf);
		uint32_t a01 = (yf*(256-xf));
		uint32_t a10 = ((256-yf)*xf);
		uint32_t a11 = ((256-yf)*(256-xf));

		uint32_t off = (ipy/256u)*(unsigned)iw + ipx/256u;

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
#endif
}
#endif
