#ifndef DEBUG
#pragma GCC optimize "3,ira-hoist-pressure,inline-functions,modulo-sched,modulo-sched-allow-regmoves"
#endif

//#define NEW_SCOPE 1

// Absolutely have to parallelize scope rendering

// IDEA 1:
// Generate spans from line segments and end points.
// Keep a list of spans on each line sorted by leftmost x co-ord
// Need to accelerate clipping spans to each other.

// Split spans at generation time around the line/point, so each side is it's own span, probably simplifies math

// Two types of spans, line segment distance based, and point distance based
// Once clipped... can probably just have a rate to step through an image for the renderer
//     - Can probably be a 1/4 image and just flip the slope around.

// Once we have clipped spans can render each line in parallel.


// Maybe try some sort of tile based renderer instead of what we do below.
//   - do parallel check for line segment/tile intersection
//       - need to preallocate list for each tile, for lock free run.
//   - Want tiles to be cache line width within lines, need to add stride to lines so they are cache alinged?

#include "common.h"
#include "maxsrc.h"
#include "getsamp.h"

#include <float.h>
#include <assert.h>

static void zoom(uint16_t * restrict out, uint16_t * restrict in, int w, int h, const float R[3][3]);

#if NEW_SCOPE
#include "scope_render.h"
#else
typedef struct {
	uint16_t *restrict data;
	uint16_t w,h;
	uint16_t stride;
} MxSurf;

static void point_init(MxSurf *res, uint16_t w, uint16_t h);
static void draw_points(void *restrict dest, int iw, int ih, const MxSurf *pnt_src, int npnts, const uint32_t *pnts);
#endif

struct maxsrc {
	uint16_t *prev_src;
	uint16_t *next_src;
	int iw, ih;
	int samp;
	float tx, ty, tz;
#if NEW_SCOPE
	struct scope_renderer* scope;
#else
	MxSurf pnt_src;
#endif
};

struct maxsrc *maxsrc_new(int w, int h)
{
	//TODO: support non-square images, need to scale for aspect
	struct maxsrc *self = calloc(sizeof(*self), 1);

	self->iw = w; self->ih = h;

#if NEW_SCOPE
	self->samp = MIN(MAX(w/12, h/12), 128);
	self->scope = scope_renderer_new(w, h, self->samp);
#else
	self->samp = IMAX(w,h); //IMIN(IMAX(w,h), 1023);
	point_init(&self->pnt_src, (uint16_t)IMAX(w/24, 8), (uint16_t)IMAX(h/24, 8));
	printf("maxsrc using %i %dx%d points\n", self->samp, self->pnt_src.w, self->pnt_src.h);
#endif

	// add extra padding for vector instructions to be able to run past the end
	size_t bufsize = (size_t)w * (size_t)h * sizeof(uint16_t) + 256;
	self->prev_src = aligned_alloc(256, bufsize);
	memset(self->prev_src, 0, bufsize);
	self->next_src = aligned_alloc(256, bufsize);
	memset(self->next_src, 0, bufsize);

	return self;
}

void maxsrc_delete(struct maxsrc *self)
{
	aligned_free(self->prev_src);
	aligned_free(self->next_src);
#if NEW_SCOPE
	scope_renderer_delete(self->scope);
	self->scope = NULL;
#else
	free((void*)self->pnt_src.data);
#endif
	self->next_src = self->prev_src = NULL;
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
// we take up less cache and fewer pages and that means fewer faults, and since almost
// everything we do either runs fast enough or bottlenecks on memory double
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
#if NEW_SCOPE
	scope_render(self->scope, dst, self->tx, self->ty, self->tz, audio, audiolen);
#else
	uint32_t pnts[samp*2]; // TODO: if we do dynamically choose number of points based on spacing move allocating this into context object

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

		// shape the waveform a bit, more slope near zero so queit stuff still makes it wiggle
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

		if((pxi-xi)*(pxi-xi) + (pyi-yi)*(pyi-yi) > 6) // if too close to previous point, skip
		{
			pxi = xi, pyi = yi;
			pnts[npnts*2+0] = (uint32_t)(xi*256);
			pnts[npnts*2+1] = (uint32_t)(yi*256);
			npnts++;
		}
	}
	draw_points(dst, self->iw, self->ih, &self->pnt_src, npnts, pnts);
#endif
	self->next_src = self->prev_src;
	self->prev_src = dst;

	self->tx+=0.02f; self->ty+=0.01f; self->tz-=0.003f;
}

#if !NEW_SCOPE
static void point_init(MxSurf *res, uint16_t w, uint16_t h)
{
	res->w = w; res->h = h;
	res->stride = w + 1 + (64 - (w+1)%64);
	uint16_t *buf = aligned_alloc(128, res->stride * (h+1) * sizeof(uint16_t) + 128); // add extra padding for vector instructions to be able to run past the end
	memset(buf, 0, res->stride*(h+1)*sizeof(uint16_t) + 128);
	int stride = res->stride;
	for(int y=0; y < h; y++)  {
		for(int x=0; x < w; x++) {
			float u = 1.0f*((2*x+1.0f)/(w-1) - 1), v = 1.0f*((2*y+1.0f)/(h-1) - 1);
			buf[y*stride + x] = (uint16_t)(expf(-4.5f*0.5f*log2f((u*u+v*v) + 1.0f))*(UINT16_MAX));
		}
	}

	res->data = buf;
}

void list_pnt_blit_ssse3(void * const restrict dest, int iw, const uint16_t *restrict pnt, int pnt_stride, int pw, int ph, int samp, const uint32_t *pnts);
void list_pnt_blit_sse2(void * const restrict dest, int iw, const uint16_t *restrict pnt, int pnt_stride, int pw, int ph, int samp, const uint32_t *pnts);
void list_pnt_blit_sse(void * const restrict dest, int iw, const uint16_t *restrict pnt, int pnt_stride, int pw, int ph, int samp, const uint32_t *pnts);

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
#if 0
				uint16_t res = (s0[x]*a00 + s0[x+1]*a01
				              + s1[x]*a10 + s1[x+1]*a11)>>16;
#else
				uint16_t res = s0[x];
#endif
				res = IMAX(res, dst_line[x]);
				dst_line[x] = res;
			}
			s0 += pnt_stride;
			s1 += pnt_stride;
		}
	}
#endif
}
#endif // Old scope code

#ifdef DEBUG
#define map_assert(a) assert(a)
#else
#define map_assert(a)
#endif

#define BLOCK_SIZE 8

static inline void zoom_func(float *x, float *y, float u, float v, const float R[3][3])
{
	const float d = 0.95f + 0.053f*hypotf(u,v);
	const float p[] = { // first rotate our frame of reference, then do a zoom along 2 of the 3 axis
		(u*R[0][0] + v*R[0][1]),
		(u*R[1][0] + v*R[1][1])*d,
		(u*R[2][0] + v*R[2][1])*d
	};
	*x = (p[0]*R[0][0] + p[1]*R[1][0] + p[2]*R[2][0]+1.0f)*0.5f;
	*y = (p[0]*R[0][1] + p[1]*R[1][1] + p[2]*R[2][1]+1.0f)*0.5f;
}

__attribute__((hot))
static void zoom_task(size_t work_item_id, size_t span, uint16_t * restrict out, uint16_t * restrict in, int w, int h, const float R[3][3])
{
	const int ystart = work_item_id * span * BLOCK_SIZE;
	const int yend   = IMIN(ystart + span * BLOCK_SIZE, (unsigned int)h);

	const float ustep = 2.0f/w, vstep = 2.0f/h;
	float y0 = ystart*vstep - 1.0f;
	for(int yd = ystart; yd < yend; yd+=BLOCK_SIZE) {
		const float y1 = (yd+BLOCK_SIZE)*vstep - 1.0f;

		float x, y;

		zoom_func(&x, &y, -1.0f, y0, R);
		uint32_t u00 = IMIN(IMAX((int32_t)(x*w*256), 0), w*256-1);
		uint32_t v00 = IMIN(IMAX((int32_t)(y*h*256), 0), h*256-1);

		zoom_func(&x, &y, -1.0f, y1, R);
		uint32_t u10 = IMIN(IMAX((int32_t)(x*w*256), 0), w*256-1);
		uint32_t v10 = IMIN(IMAX((int32_t)(y*h*256), 0), h*256-1);

		for(int xd = 0; xd < w; xd+=BLOCK_SIZE) {
			const float x1 = (xd+BLOCK_SIZE)*ustep - 1.0f;

			zoom_func(&x, &y, x1, y0, R);
			const uint32_t u01 = IMIN(IMAX((int32_t)(x*w*256), 0), w*256-1);
			const uint32_t v01 = IMIN(IMAX((int32_t)(y*h*256), 0), h*256-1);

			zoom_func(&x, &y, x1, y1, R);
			const uint32_t u11 = IMIN(IMAX((int32_t)(x*w*256), 0), w*256-1);
			const uint32_t v11 = IMIN(IMAX((int32_t)(y*h*256), 0), h*256-1);

			uint32_t u0 = u00*BLOCK_SIZE;
			uint32_t u1 = u01*BLOCK_SIZE;
			uint32_t v0 = v00*BLOCK_SIZE;
			uint32_t v1 = v01*BLOCK_SIZE;

			for(uint32_t yt=0; yt<BLOCK_SIZE; yt++) {
				uint16_t *restrict out_line = out + (yd+yt)*w + xd;

				uint32_t u = u0*BLOCK_SIZE;
				uint32_t v = v0*BLOCK_SIZE;
				for(uint32_t xt=0; xt<BLOCK_SIZE; xt++) {
					const uint32_t ut = u/(BLOCK_SIZE*BLOCK_SIZE), vt = v/(BLOCK_SIZE*BLOCK_SIZE);
					const uint32_t xs=ut>>8,  ys=vt>>8;
					const uint32_t xf=ut&0xFF, yf=vt&0xFF;
					const uint32_t xi1 = xs;
					const uint32_t yi1 = ys*w;
					const uint32_t xi2 = IMIN(xi1+1,(uint32_t)(w-1));
					const uint32_t yi2 = IMIN(yi1+w, (uint32_t)((h-1)*w));

					// it is critical that this entire calculation be done as uint32s to avoid overflow
					uint32_t tmp = ((in[yi1 + xi1]*(256u - xf) + in[yi1 + xi2]*xf)*(256u-yf) +
								    (in[yi2 + xi1]*(256u - xf) + in[yi2 + xi2]*xf)*yf) >> 16u;
					tmp = (tmp*((256u*97u)/100u)) >> 8u;

					out_line[xt] = (uint16_t)tmp;

					u = u + u1 - u0;
					v = v + v1 - v0;
				}
				u0 = u0 - u00 + u10;
				u1 = u1 - u01 + u11;
				v0 = v0 - v00 + v10;
				v1 = v1 - v01 + v11;

			}
			v00 = v01; v10 = v11;
			u00 = u01; u10 = u11;
		}
		y0=y1;
	}
}

#if NO_PARATASK

__attribute__((hot))
static void zoom(uint16_t * restrict out, uint16_t * restrict in, int w, int h, const float R[3][3]) {
	zoom_task(0, h/BLOCK_SIZE, out, in, w, h, R);
}

#else

#include "paratask/paratask.h"
struct softmap_work_args {
	void (*task_fn)(size_t work_item_id, size_t span, uint16_t *restrict out, uint16_t *restrict in, int w, int h, const float R[3][3]);
	uint16_t *restrict out;
	uint16_t *restrict in;
	const float (*R)[3];
	size_t span;
	int w, h;
};
static void paratask_func(size_t work_item_id, void *arg_)
{
	struct softmap_work_args *a = arg_;
	a->task_fn(work_item_id, a->span, a->out, a->in, a->w, a->h, a->R);
}

static void zoom(uint16_t * restrict out, uint16_t * restrict in, int w, int h, const float R[3][3])
{
	int span = 2;
	struct softmap_work_args args = {
		zoom_task,
		out, in,
		R,
		span,
		w, h
	};
	paratask_call(paratask_default_instance(), 0, h/(span*BLOCK_SIZE), paratask_func, &args);
}
#endif
