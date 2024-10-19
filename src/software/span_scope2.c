// #undef NDEBUG
#ifdef NDEBUG
#pragma GCC optimize "3,ira-hoist-pressure,inline-functions,modulo-sched,modulo-sched-allow-regmoves"
#endif
#pragma GCC optimize "0"
// #define NO_PARATASK 1


#include "common.h"
#include "maxsrc.h"
#include "getsamp.h"
#include "scope_render.h"

#include <tgmath.h>
#include <float.h>
#include <assert.h>

#define TEXTURE_SIZE   31
#define TEXTURE_STRIDE (TEXTURE_SIZE+1)

// #define SCOPE_DEBUG_LOG 2
// #define SCOPE_DEBUG_LOG 0
// #define SCOPE_SPAN_STATS 1

#ifdef SCOPE_TEST
//Always enable full logging in test
#	ifdef SCOPE_DEBUG_LOG
#		undef SCOPE_DEBUG_LOG
#	endif
#	define SCOPE_DEBUG_LOG 5
#endif

#ifdef SCOPE_DEBUG_LOG
int point_span_scope_debug_level = SCOPE_DEBUG_LOG;
#endif

#ifdef SCOPE_DEBUG_LOG
#define DEBUG_LOG(level, ...) do { if(SCOPE_DEBUG_LOG >= level && point_span_scope_debug_level >= level) printf(__VA_ARGS__); } while(0)
#else
#define DEBUG_LOG(level, ...)
#endif

#ifdef SCOPE_DEBUG_LOG
#define SCOPE_SPAN_STATS 1
#endif


// Idea: same as existing span rendering, but we just render rather than storing spans.

// Rationale: we should have dramatically less overdraw to begin with since compared to the point
//            based scope we don't count on densely packed, overlapping squares to make the scope
//            contigious.
//            Since we also do the polyline decimation that will further reduce overdraw.
//            So we probably won't get any speed up from an acceleration structure that culls
//            span segments.


struct scope_renderer
{
	int iw;
	int ih;

	int samp;

	int line_width;

	// texture
	uint8_t tex_data[(TEXTURE_SIZE+1) * TEXTURE_STRIDE];
#ifdef SCOPE_SPAN_STATS
	int span_counts[];
#endif
};

struct scope_renderer* scope_renderer_new(int w, int h, int samp)
{
#ifdef SCOPE_SPAN_STATS
	struct scope_renderer *self = calloc(sizeof(*self) + sizeof(self->span_counts[0])*h, 1);
#else
	struct scope_renderer *self = calloc(sizeof(*self), 1);
#endif

	self->iw = w; self->ih = h;
	self->samp = MAX(MIN(w/8, h/8), 53);
	self->line_width = IMAX(IMAX(w/24, 8), IMAX(h/24, 8));

	for(int y=0; y < TEXTURE_SIZE; y++)  {
		for(int x=0; x < TEXTURE_SIZE; x++) {
			float u = ((float)x)/(TEXTURE_SIZE-1), v = ((float)y)/(TEXTURE_SIZE-1);
			// float u = (2*x+1.0f)/(TEXTURE_SIZE-1) - 1,  v = (2*y+1.0f)/(TEXTURE_SIZE-1) - 1;
			float f = fmaxf(0.0f, expf(-3.0f*0.5f*log2f((u*u+v*v) + 1.0f)) * (1 - hypotf(u,v)));
			// float f = fmaxf(0.0f, expf(-4.5f*0.5f*log2f((u*u+v*v) + 1.0f)) * (1 - hypotf(u,v)));

			self->tex_data[y*TEXTURE_STRIDE + x] = (uint8_t)(f*(UINT8_MAX));
		}
	}
	self->tex_data[0] = UINT8_MAX; // force middle to full brightness

	printf("Scope using line width %i with %i samples\n", self->line_width, self->samp);

	return self;
}

void scope_renderer_delete(struct scope_renderer* self)
{
	free(self);
}

struct point
{
	float x;
	float y;
};

static void render(struct scope_renderer *self, int ymin, int ymax, int npnts, const struct point* pnts, uint16_t *restrict dest);
static void render_line(struct scope_renderer * const self, int npnts, const struct point* pnts, void *restrict dest, int line);

#define DO_POLYLINE_DECIMATION 1

static inline __attribute__((always_inline, const))
float point_line_dist_sqr(float x1, float y1, float x2, float y2, float x0, float y0)
{
	float tp = (x2-x1)*(y1-y0) - (x1-x0)*(y2-y1);
	float bt = (x2-x1)*(x2-x1) + (y2-y1)*(y2-y1);
	return (tp*tp) / bt;
}


static inline __attribute__((always_inline, flatten, const))
struct point transform(float s, float t, const float R[3][3], int iw, int ih)
{
	// shape the waveform a bit, more slope near zero so quiet stuff still makes it wiggle
	s=copysignf(log2f(fabsf(s)*3+1)/2, s);

	// xt ∈ [-0.5, 0.5] ∀∃∄∈∉⊆⊈⊂⊄
	float xt = t - 0.5f;
	float yt = 0.2f*s;
	float zt = 0.0f;

	float x = R[0][0]*xt + R[1][0]*yt + R[2][0]*zt;
	float y = R[0][1]*xt + R[1][1]*yt + R[2][1]*zt;
	float z = R[0][2]*xt + R[1][2]*yt + R[2][2]*zt;
	float zvd = 0.75f/(z+2);

	struct point r = {
		x*zvd*iw + iw/2.0f,
		y*zvd*ih + ih/2.0f
	};
	return r;
}

void scope_render(struct scope_renderer *self,
                  void *restrict dest,
                  float tx, float ty, float tz,
                  const float *audio,
                  int audiolen)
{
	int iw = self->iw, ih = self->ih;

	float cx=cosf(tx), cy=cosf(ty), cz=cosf(tz);
	float sx=sinf(tx), sy=sinf(ty), sz=sinf(tz);

	const float R[][3] = {
		{cz*cy-sz*sx*sy, -sz*cx, -sy*cz-cy*sz*sx},
		{sz*cy+cz*sx*sy,  cz*cx, -sy*sz+cy*cz*sx},
		{cx*sy         ,    -sx,  cy*cx}
	};


	struct point start = transform(getsamp(audio, audiolen, 0, audiolen/96), 0, R, iw, ih);
	struct point end   = transform(getsamp(audio, audiolen, audiolen-1, audiolen/96), 1, R, iw, ih);

	float length = hypotf((start.x - end.x), (start.y-end.y));
	int samp = fminf(self->samp, ceilf(length/2)); // check a point every 3 pixels along the sampling line

	struct point pnts[samp];

	float ymin = ih;
	float ymax = 0;

	int npnts = 1;
	float thresh = self->line_width * 0.25f; //TODO: tune this
	struct point p = start;

	pnts[0] = start;
#ifndef DO_POLYLINE_DECIMATION
	ymin = fminf(ymin, start.y);
	ymax = fmaxf(ymax, start.y);
#endif

	struct samp_state samp_state;
	getsamp_step_init(&samp_state, audio, audiolen, audiolen/96);
	for(int i=1; i<samp-1; i++)
	{
		float s = getsamp_step(&samp_state, i*audiolen/(samp-1));

		struct point c = transform(s, (float)i/(float)(samp - 1), R, iw, ih);

		if((p.x-c.x)*(p.x-c.x) + (p.y-c.y)*(p.y-c.y) > thresh) // skip short segments
		{
			pnts[npnts] = p = c;
			npnts++;
#ifndef DO_POLYLINE_DECIMATION
			ymin = fminf(ymin, c.y);
			ymax = fmaxf(ymax, c.y);
#endif
		}
	}

	// Make sure we ALWAYS have at least a line!
	pnts[npnts] = end;
	npnts++;
#ifndef DO_POLYLINE_DECIMATION
	ymin = fminf(ymin, end.y);
	ymax = fmaxf(ymax, end.y);
#endif


#ifdef DO_POLYLINE_DECIMATION
	// do polyline decimation with Reumann-Witkam algorithm
	{
		const float tolerance = 1.25f; //IMAX(self->iw, self->ih)/(24.0f*24.0f);
		int p0 = 0, p1 = 1;
		int pj = p1;

		ymin = fminf(ymin, pnts[0].y);
		ymax = fmaxf(ymax, pnts[0].y);

		int out = 1;

		for( int j=2; j < npnts-1 ; ++j )
		{
			int pi = pj;
			pj++;

			// printf("%f\n", point_line_dist(pnts[p0].x, pnts[p0].y, pnts[p1].x, pnts[p1].y, pnts[pj].x, pnts[pj].y));

			if(point_line_dist_sqr(pnts[p0].x, pnts[p0].y, pnts[p1].x, pnts[p1].y, pnts[pj].x, pnts[pj].y) < tolerance*tolerance)
			// if(point_line_dist(pnts[p0].x, pnts[p0].y, pnts[p1].x, pnts[p1].y, pnts[pj].x, pnts[pj].y) < tolerance)
			{
				DEBUG_LOG(4, "skip point %d\n", pj);
				continue;
			}
			pnts[out] = pnts[pi];
			p0 = pi;
			p1 = pj;
			out++;

			ymin = fminf(ymin, pnts[pi].y);
			ymax = fmaxf(ymax, pnts[pi].y);
		}
		pnts[out] = end;
		out++;

		ymin = fminf(ymin, end.y);
		ymax = fmaxf(ymax, end.y);

		DEBUG_LOG(0, "Started with % 4i, skipped % 4i, removed % 4i points of % 4d, leaving % 4d\n", samp, samp - npnts, npnts - out, npnts, out );
		npnts = out;
	}
#endif
	// printf("Num points: % 4i ", npnts);
	// int first_line = floorf(ymin - self->line_width*1.4142135623730950488016887f);
	// int last_lin e =  ceilf(ymax + self->line_width*1.4142135623730950488016887f);
	int first_line = floorf(ymin - self->line_width);
	int last_line  =  ceilf(ymax + self->line_width);

	DEBUG_LOG(0, "y-range: % 4.4f % 4.4f, lines: % 4d to % 4d\n", ymin, ymax, first_line, last_line);

	render(self, first_line, last_line, npnts, pnts, dest);
#ifdef SCOPE_SPAN_STATS
	int total_spans = 0;
	for(int line = first_line; line <= last_line; line++)
	{
		total_spans += self->span_counts[line];
	}
	printf("Generated %d spans total over y-range: % 4.4f % 4.4f, lines: % 4d to % 4d\n", total_spans, ymin, ymax, first_line, last_line);
	if(0 == total_spans)
	{
		// Dump points
		for(int i=0; i<npnts; i++)
		{
			printf("% 1.5f, % 1.5f\n", pnts[i].x, pnts[i].y);
		}
		abort();
	}
#endif
}


struct span
{
	uint16_t ix; // output image x position
	uint16_t l; // length of the span
#ifdef FIXED_POINT_SPAN_TEX_COORD
	uint16_t tx1; // starting texture x co-ord
	uint16_t ty1; // starting texture y co-ord
	uint16_t tx2; // ending texture x co-ord
	uint16_t ty2; // ending texture y co-ord
#else
	float ftx1;
	float fty1;
	float ftx2;
	float fty2;
#endif
};

__attribute__((always_inline, hot, flatten))
static inline float dist(float x, float y)
{
	x = fabsf(x);
	y = fabsf(y);
	float abig   = fmaxf(x, y);
	float asmall = fminf(x, y);

	if (asmall == 0.0f) /* Trivial case.  */
		return abig;

	/* Scale the numbers as much as possible by using its ratio.
	For example, if both ABIG and ASMALL are VERY small, then
	X^2 + Y^2 might be VERY inaccurate due to loss of
	significant digits.  Dividing ASMALL by ABIG scales them
	to a certain degree, so that accuracy is better.  */
	float ratio = asmall / abig ;
	return abig * sqrtf(1.0 + ratio*ratio);
}


#define SPAN_BUF_SIZE 9

#if defined(POINT_SPAN_DEBUG_LOG) && POINT_SPAN_DEBUG_LOG > 4
#define ADD_SPAN_DEBUG_OUT(x0_, x1_, tx0_, ty0_, tx1_, ty1_) \
	{ if(point_span_scope_debug_level > 4)                \
		printf("span on line % 4i %4.1f, %4.1f, "       \
		       "t: (%1.4f, %1.4f) -> (%1.4f, %1.4f)\n", \
		       line, x0_, x1_, tx0_, ty0_, tx1_, ty1_); }
#else
#define ADD_SPAN_DEBUG_OUT(x0_, x1_, tx0_, ty0_, tx1_, ty1_)
#endif

#	define ADD_SPAN(x0_, x1_, tx0_, ty0_, tx1_, ty1_) {\
		ADD_SPAN_DEBUG_OUT(x0_, x1_, tx0_, ty0_, tx1_, ty1_) \
		if(add_span(spans, &span_count, x0_, x1_, tx0_, ty0_, tx1_, ty1_)) break; \
	}


__attribute__((hot,always_inline,optimize("-ffinite-math-only")))
static inline bool add_span(struct span* spans, uint32_t *span_count_, float x0, float x1, float tx0, float ty0, float tx1, float ty1)
{
	uint32_t span_count = *span_count_;
	assert(tx0 >= -1.0001 && tx0 <= 1.0001);
	assert(tx1 >= -1.0001 && tx1 <= 1.0001);
	assert(ty0 >= 0.0 && ty0 <= 1.0001);
	assert(ty1 >= 0.0 && ty1 <= 1.0001);
	const float x0r = ceilf(x0);
	const float x1r = floorf(x1);
	const float l   = x1r - x0r;
	if(l > 1)
	{
		// slopes of map from screen space to texture coords
		const float dx = (tx1 - tx0)/(x1 - x0);
		const float dy = (ty1 - ty0)/(x1 - x0);

		float s0 = x0r - x0; // we moved line end to line up with a pixel, this is that difference
		float s1 = x1 - x1r;

#if 1
		if(tx0 * tx1 < 0) // texure co-ords have different sign, need to split span at origin in texture space
		{
			float t = tx0/(tx0-tx1); // (tx0 - 0.0f)/(tx0-tx1), how far along segment [tx0, tx1] is zero
			assert(t >= 0); assert(t <= 1);

			float xm = x0*(1-t) + x1*t; assert(xm >= x0); assert(xm <= x1);
			float txm = 0;
			float tym = ty0*(1-t) + ty1*t;

			float xmr0 = floorf(xm);
			float xmr1 = xmr0 + 1;

			if(x0r <= xmr0)
			{
				float sm0 = xm - xmr0; // we moved line end to line up with a pixel, this is that difference
				// correct texture space line segment ends for the adjustment to line up span ends with pixels
				float tx0a = tx0 + s0*dx;
				float ty0a = ty0 + s0*dy;
				float txm0 = txm - sm0*dx;
				float tym0 = tym - sm0*dy;

				assert(txm0*tx0a >= 0);
				assert(tym0 >= -0.00001);
				tym0 = fmaxf(tym0, 0);
				spans[span_count].ix  = x0r;
				spans[span_count].l   = fmaxf(xmr0 - x0r, 1);

#ifdef FIXED_POINT_SPAN_TEX_COORD
				spans[span_count].tx1 = (1<<15)*fabsf(tx0a);
				spans[span_count].ty1 = (1<<15)*(ty0a);
				spans[span_count].tx2 = (1<<15)*fabsf(txm0);
				spans[span_count].ty2 = (1<<15)*(tym0);
#else
				spans[span_count].ftx1 = fabsf(tx0a);
				spans[span_count].fty1 =       ty0a;
				spans[span_count].ftx2 = fabsf(txm0);
				spans[span_count].fty2 =       tym0;
#endif
				if(spans[span_count].l >= 0) (*span_count_)++;
				if(*span_count_ >= SPAN_BUF_SIZE) return true;
				span_count = (*span_count_);
			}

			if(xmr1 <= x1r)
			{
				float sm1 = xmr1 - xm; // we moved line end to line up with a pixel, this is that differenc
				// correct texture space line segment end for the adjustment to line up span ends with pixels
				float txm1 = txm + sm1*dx;
				float tym1 = tym + sm1*dy;
				float tx1a = tx1 - s1*dx;
				float ty1a = ty1 - s1*dy;

				assert(txm1*tx1a >= 0);
				assert(tym1 >= -0.00001);
				tym1 = fmaxf(tym1, 0);
				spans[span_count].ix  = xmr1;
				spans[span_count].l   = fmaxf(x1r - xmr1, 0);

#ifdef FIXED_POINT_SPAN_TEX_COORD
				spans[span_count].tx1 = (1<<15)*fabsf(txm1);
				spans[span_count].ty1 = (1<<15)*(tym1);
				spans[span_count].tx2 = (1<<15)*fabsf(tx1a);
				spans[span_count].ty2 = (1<<15)*(ty1a);
#else
				spans[span_count].ftx1 = fabsf(txm1);
				spans[span_count].fty1 =       tym1;
				spans[span_count].ftx2 = fabsf(tx1a);
				spans[span_count].fty2 =       ty1a;
#endif
				if(spans[span_count].l >= 0) (*span_count_)++;
				if(*span_count_ >= SPAN_BUF_SIZE) return true;
			}
			return false;
		}
#endif

		// correct texture space line segment ends for the adjustment to line up span ends with pixels
		tx0 = tx0 + s0*dx;
		ty0 = ty0 + s0*dy;
		tx1 = tx1 - s1*dx;
		ty1 = ty1 - s1*dy;

		assert(ty0 >= 0);
		assert(ty1 >= 0);

		spans[span_count].ix  = x0r;
		spans[span_count].l   = l;

#ifdef FIXED_POINT_SPAN_TEX_COORD
		spans[span_count].tx1 = (1<<15)*fabsf(tx0);
		spans[span_count].ty1 = (1<<15)*(ty0);
		spans[span_count].tx2 = (1<<15)*fabsf(tx1);
		spans[span_count].ty2 = (1<<15)*(ty1);
#else
		spans[span_count].ftx1 = fabsf(tx0);
		spans[span_count].fty1 =      (ty0);
		spans[span_count].ftx2 = fabsf(tx1);
		spans[span_count].fty2 =      (ty1);
#endif
		if(spans[span_count].l > 0) (*span_count_)++;
		if(*span_count_ >= SPAN_BUF_SIZE) return true;
	}
	return false;
}

/*******************************************************************
 * Interpolate tx/ty to what they should be when the line (x0,y0)->(x1, y1)
 * crosses zero
 */
__attribute__((hot,always_inline,flatten,optimize("-ffinite-math-only")))
static inline void calc_span_end(float* xo, float* txo, float* tyo,
                                 float x0, float y0, float x1, float y1,
                                 float tx0, float ty0, float tx1, float ty1)
{
	float t;
	if(fabsf(x1 - x0) > 0.00001f)
	{
		*xo = x0 - y0 * (x1 - x0) / (y1 - y0);
		t = (*xo  - x0)/(x1 - x0);

		// (1 - t) * v0 + t * v1;
		//*xo  = (1 - t) * x0 + t * x1;
		*txo = (1 - t) * tx0 + t * tx1;
		*tyo = (1 - t) * ty0 + t * ty1;

		assert(fminf(x0, x1) <= *xo && *xo <= fmaxf(x0, x1));
		assert(*txo >= -1.0f && *txo <= 1.0f);
		assert(*tyo >=  0.0f && *tyo <= 1.0f);
	}
	else
	{
		*xo = x0;
		*txo = (tx0 + tx1)*0.5f;
		*tyo = (ty0 + ty1)*0.5f;
	}
}

__attribute__((hot,noinline,flatten,optimize("-ffinite-math-only")))
static void render_spans(struct scope_renderer * const self, void *restrict dest, struct span* span, int num_spans, int line)
{
	uint16_t *restrict dst_line = (uint16_t *restrict)dest + self->iw * line;

	for(int i=0; i < num_spans; ++i, ++span)
	{
		uint16_t div = IMAX(1u, span->l);
		// Make into 8.16 fixed point
		// the co-ords stored in the span structure are effectively 6.1 bit fixed point so
		// we only need to shift by 16
		// only the x co-ord will be negative since the span generation always generates the
		// positive y (due to the geometry of the boxes we're rendering we never need to cross
		// y texture axis in the middle of a span)

#ifndef FIXED_POINT_SPAN_TEX_COORD
		// const float ftx1 = (span->ftx1 + 1.0f)*128*(TEXTURE_SIZE - 1);
		// const float ftx2 = (span->ftx2 + 1.0f)*128*(TEXTURE_SIZE - 1);
		// const float fty1 = (span->fty1 + 1.0f)*128*(TEXTURE_SIZE - 1);
		// const float fty2 = (span->fty2 + 1.0f)*128*(TEXTURE_SIZE - 1);
		const float ftx1 = (span->ftx1)*256*(TEXTURE_SIZE - 1);
		const float ftx2 = (span->ftx2)*256*(TEXTURE_SIZE - 1);
		const float fty1 = (span->fty1)*256*(TEXTURE_SIZE - 1);
		const float fty2 = (span->fty2)*256*(TEXTURE_SIZE - 1);
		const float dx = (ftx2 - ftx1)/div;
		const float dy = (fty2 - fty1)/div;

		for(int32_t i = 0; i <= span->l; ++i)
		{
			const int32_t x = span->ix + i;
			const int32_t tx = ftx1 + i*dx;
			const int32_t ty = fty1 + i*dy;

			const uint32_t ys = ty>>8,   xs = tx>>8;     assert(ys < TEXTURE_SIZE); assert(xs < TEXTURE_SIZE);
			const uint32_t yf = ty&0xff, xf = tx&0xff;
#else
		// const int32_t tx1 = (span->tx1 + (1<<14))*(32768ll*(TEXTURE_SIZE - 1)) >> 14;
		// const int32_t ty1 = (span->ty1 + (1<<14))*(32768ll*(TEXTURE_SIZE - 1)) >> 14;
		// const int32_t tx2 = (span->tx2 + (1<<14))*(32768ll*(TEXTURE_SIZE - 1)) >> 14;
		// const int32_t ty2 = (span->ty2 + (1<<14))*(32768ll*(TEXTURE_SIZE - 1)) >> 14;
		const int32_t tx1 = (span->tx1)*((uint32_t)(TEXTURE_SIZE - 1)) << 1;
		const int32_t ty1 = (span->ty1)*((uint32_t)(TEXTURE_SIZE - 1)) << 1;
		const int32_t tx2 = (span->tx2)*((uint32_t)(TEXTURE_SIZE - 1)) << 1;
		const int32_t ty2 = (span->ty2)*((uint32_t)(TEXTURE_SIZE - 1)) << 1;

		const int32_t dx = (tx2 - tx1)/div;
		const int32_t dy = (ty2 - ty1)/div;

		for(int32_t x = span->ix, tx = tx1, ty = ty1; x <= span->ix + span->l; ++x, tx += dx, ty += dy)
		{
			const uint32_t ys =  ty>>16,      xs =  tx>>16;     assert(ys < TEXTURE_SIZE); assert(xs < TEXTURE_SIZE);
			const uint32_t yf = (ty>>8)&0xff, xf = (tx>>8)&0xff;
#endif

			uint32_t a11 = (yf*xf);
			uint32_t a10 = (yf*(256-xf));
			uint32_t a01 = ((256-yf)*xf);
			uint32_t a00 = ((256-yf)*(256-xf));

			const uint8_t *s0 = self->tex_data + ys*TEXTURE_STRIDE + xs;
			const uint8_t *s1 = s0 + TEXTURE_STRIDE;

			uint16_t res = (s0[0]*a00 + s0[1]*a01
			              + s1[0]*a10 + s1[1]*a11)>>8;

			res = IMAX(res, dst_line[x]);
			dst_line[x] = res;
		}
#if DEBUG_MAX_SPAN_START
		dst_line[span->ix] = UINT16_MAX;
#endif
	}
}

/*
 * Generate all the spans for a specific line in the image
 *
 * Places the spans in self->span_buf starting at self->span_buf[line*samp*2] and returns the number of spans generated
 */
__attribute__((hot,noinline,optimize("-ffinite-math-only")))
static void render_line(struct scope_renderer *self, int npnts, const struct point* pnts, void *restrict dest, int line)
{
//#pragma clang fp finite-math-only(on)
#pragma clang fp contract(fast) exceptions(ignore)
	// TODO: make the span count an atomic in an array or something so that we can steal spans
	// if we run out, or something like that

	DEBUG_LOG(1, "Starting spans for line %i over %i points\n", line, npnts);

	// const int   samp = self->samp;
	const int   width = self->line_width;
	const float width_rt2 = width * 1.4142135623730950488016887f;  // width * root 2

	uint32_t total_span_count = 0;
	for(int i = 0; i < npnts-1; i++)
	{
		uint32_t span_count = 0;
		struct span spans[SPAN_BUF_SIZE];

		DEBUG_LOG(2, "cheking segment #% 5i (%f, %f) -> (%f, %f)\n", i, pnts[i].x, pnts[i].y, pnts[i+1].x, pnts[i+1].y);

		// We subtract the line we are at from the y co-ords to make things easy so
		// we just care about what is going on at the x axis
		float x1 = pnts[i].x, y1 = pnts[i].y - line, x2 = pnts[i+1].x, y2 = pnts[i+1].y - line;


		// Fast test to reject segments that can't possibly intersect with our raster
		if( (y1 < -width_rt2  && y2 < -width_rt2) || (y1 > width_rt2 && y2 > width_rt2) )
			continue;

		if(x1 > x2)
		{ // Always work with lines pointing right for simplicity
			float xt = x1, yt = y1;
			x1 = x2, y1 = y2;
			x2 = xt, y2 = yt;
		}

		// Since we subtracted off the height we can flip the line around to always have
		// non-negative slope without affecting our spans, since they now lie on the x-axis

		if(y1 > y2)
		{
			y1 = -y1;
			y2 = -y2;
		}

		DEBUG_LOG(2, "transformed segment (%f, %f) -> (%f, %f)\n", x1, y1, x2, y2);

		// Check which of the various interesting line segments the ray starting at (0, line) intersects
		// to that end, generate all the interesting points.
		const float ddx = x2-x1,       ddy = y2-y1;
		// const float  d  = width/sqrtf(ddx*ddx + ddy*ddy); // hypot is slower, and we'll only get close to over flow in case of a bug and aren't sensitive to error near 0
		const float  d  = width/dist(ddx, ddy);
		// const float  d  = width/hypotf(ddx, ddy);
		const float  tx =  ddx*d, ty = ddy*d;
		const float  nx = -ddy*d, ny = ddx*d;

		// (tx, ty) is a line width length vector that points along our line segment
		// (nx, ny) is a line width length vector that points normal our line segment

		if(y2+ny+ty <= 0) continue; // everything is below the current raster
		if(y1-ny-ty >= 0) continue; // everything is above the current raster

		// Rectangle ABCD is the bottom/left end cap
		// Rectangle EFGH is the top/right end cap
		// Rectangle BEHC is the box around the line segment

		const float ax = x1+nx-tx, ay = y1+ny-ty, atx = -1.0f, aty = 1.0f;
		const float bx = x1+nx,    by = y1+ny,    btx = -1.0f, bty = 0.0f;
		const float cx = x1-nx,    cy = y1-ny,    ctx =  1.0f, cty = 0.0f;
		const float dx = x1-nx-tx, dy = y1-ny-ty, dtx =  1.0f, dty = 1.0f;

		const float ex = x2+nx,    ey = y2+ny,    etx = -1.0f, ety = 0.0f;
		const float fx = x2+nx+tx, fy = y2+ny+ty, ftx = -1.0f, fty = 1.0f;
		const float gx = x2-nx+tx, gy = y2-ny+ty, gtx =  1.0f, gty = 1.0f;
		const float hx = x2-nx,    hy = y2-ny,    htx =  1.0f, hty = 0.0f;

#if defined(POINT_SPAN_DEBUG_LOG) && POINT_SPAN_DEBUG_LOG > 2
		if(point_span_scope_debug_level > 3) {

		DEBUG_LOG(4, "\n\nimport matplotlib.pyplot as plt\nfrom matplotlib.path import Path\nfrom matplotlib.patches import PathPatch\n");
		DEBUG_LOG(4, "lx = [%f,%f]\n", x1, x2);
		DEBUG_LOG(4, "ly = [%f,%f]\n", y1, y2);
		DEBUG_LOG(4, "abcd = [(%4.4f, %4.4f), (%4.4f, %4.4f), (%4.4f, %4.4f), (%4.4f, %4.4f), (0.,0.)]\n",
		          ax,ay, bx,by, cx,cy, dx,dy);
		DEBUG_LOG(4, "efgh = [(%4.4f, %4.4f), (%4.4f, %4.4f), (%4.4f, %4.4f), (%4.4f, %4.4f), (0.,0.)]\n",
		          ex,ey, fx,fy, gx,gy, hx,hy);
		DEBUG_LOG(4, "bech = [(%4.4f, %4.4f), (%4.4f, %4.4f), (%4.4f, %4.4f), (%4.4f, %4.4f)]\n",
		          bx,by, ex,ey, cx,cy, hx,hy);
		DEBUG_LOG(4, "box_codes = [Path.MOVETO, Path.LINETO, Path.LINETO, Path.LINETO, Path.CLOSEPOLY]\n");
		DEBUG_LOG(4, "fig, ax = plt.subplots(sharex=True,sharey=True)\n"
		             "ax.plot(lx, ly)\n"
		             //"ax.plot(box, boy)\n"
		             "ax.add_patch(PathPatch(Path(abcd, box_codes)))\n"
		             "ax.add_patch(PathPatch(Path(efgh, box_codes)))\n"
		             "ax.add_patch(PathPatch(Path(bech, [Path.MOVETO, Path.LINETO, Path.MOVETO, Path.LINETO])))\n"
		             "ax.set_xlim(%4.4f, %4.4f)\n"
		             "ax.set_ylim(%4.4f, %4.4f)\n",
		             x1-(width_rt2+2), x2 + width_rt2+2,
		             y1-(width_rt2+2), y2 + width_rt2+2);
		DEBUG_LOG(4, "ax.add_patch(PathPatch(Path([(%4.4f, 0.0), (%4.4f, 0.0)], [Path.MOVETO, Path.LINETO])))\n",
		             x1-(width_rt2+2), x2 + width_rt2+2);
		DEBUG_LOG(4, "ax.text(%4.4f, %4.4f, 'A')\n"
		             "ax.text(%4.4f, %4.4f, 'B')\n"
		             "ax.text(%4.4f, %4.4f, 'C')\n"
		             "ax.text(%4.4f, %4.4f, 'D')\n"
		             "ax.text(%4.4f, %4.4f, 'E')\n"
		             "ax.text(%4.4f, %4.4f, 'F')\n"
		             "ax.text(%4.4f, %4.4f, 'G')\n"
		             "ax.text(%4.4f, %4.4f, 'H')\n"
		             "plt.show()\n\n",
		             ax,ay, bx,by, cx,cy, dx,dy,
		             ex,ey, fx,fy, gx,gy, hx,hy);
		}
#endif

		// already know fy > 0 and dy < 0 from bail out test above

		//TODO: simplify by using a vertical line for endcap endpoints

		// need to find "big" angle between segments then project a line width length vector that bisects that,
		// then the quad formed by the tip of the bisecting vector and the line box ends is what we need to fill
		// need to find a fast way to generate spans for that here. Maybe just clip against next line segment?

		// TODO: only need to do end cap on one end, except for either start or end segment.
		//      maybe handle start segment special and skip spans from "left" end cap?
		//      that is explicitly check start point and generate a span for it, then only do far end caps
		//      If we handle end cap spans by checking against quad described above we could split them out
		//      on their own so we handle only line segments, then one endcap and a special check for endcaps
		//      on start/end of scope

		float span_x1, span_x2, span_x3, span_x4;
		float tx1, tx2, tx3, tx4;
		float ty1, ty2, ty3, ty4;

		if(ey < 0)
		{ // we are in the p2 end cap, only one span, starting somewhere along the line EF
			DEBUG_LOG(3, "intersect EF\n");
			if(gy < 0)
			{ // other side of span is on line FG
				DEBUG_LOG(3, "intersect FG\n");
				calc_span_end(&span_x1, &tx1, &ty1, ex, ey, fx, fy, etx, ety, ftx, fty);
				calc_span_end(&span_x2, &tx2, &ty2, fx, fy, gx, gy, ftx, fty, gtx, gty);

				ADD_SPAN(span_x1, span_x2, tx1, ty1, tx2, ty2)
			}
			else
			{ // other side of the span is on line GH
				DEBUG_LOG(2, "intersect GH\n");
				calc_span_end(&span_x1, &tx1, &ty1, ex, ey, fx, fy, etx, ety, ftx, fty);
				calc_span_end(&span_x2, &tx2, &ty2, gx, gy, hx, hy, gtx, gty, htx, hty);

				ADD_SPAN(span_x1, span_x2, tx1, ty1, tx2, ty2)
			}
		}
		else if(by < 0)
		{ // first span starts on line BE, already know ey > 0
			DEBUG_LOG(3, "intersect BE\n");
			if(hy < 0)
			{ // Two spans, we switch at a point along line EH
				DEBUG_LOG(3, "intersect EH\n");
				if(gy < 0)
				{ // second span ends somewhere along line FG
					DEBUG_LOG(3, "intersect FG\n");
					calc_span_end(&span_x1, &tx1, &ty1, bx, by, ex, ey, btx, bty, etx, ety);
					calc_span_end(&span_x2, &tx2, &ty2, ex, ey, hx, hy, etx, ety, htx, hty);
					calc_span_end(&span_x3, &tx3, &ty3, fx, fy, gx, gy, ftx, fty, gtx, gty);

					ADD_SPAN(span_x1, span_x2, tx1, ty1, tx2, ty2)
					ADD_SPAN(span_x2, span_x3, tx2, ty2, tx3, ty3)
				}
				else
				{ // second span ends somewhere along line GH
					DEBUG_LOG(3, "intersect GH\n");
					calc_span_end(&span_x1, &tx1, &ty1, bx, by, ex, ey, btx, bty, etx, ety);
					calc_span_end(&span_x2, &tx2, &ty2, ex, ey, hx, hy, etx, ety, htx, hty);
					calc_span_end(&span_x3, &tx3, &ty3, gx, gy, hx, hy, gtx, gty, htx, hty);

					ADD_SPAN(span_x1, span_x2, tx1, ty1, tx2, ty2)
					ADD_SPAN(span_x2, span_x3, tx2, ty2, tx3, ty3)
				}
			}
			else
			{ // only one span, ends along line CH
				DEBUG_LOG(3, "intersect CH\n");
				calc_span_end(&span_x1, &tx1, &ty1, bx, by, ex, ey, btx, bty, etx, ety);
				calc_span_end(&span_x2, &tx2, &ty2, cx, cy, hx, hy, ctx, cty, htx, hty);

				ADD_SPAN(span_x1, span_x2, tx1, ty1, tx2, ty2)
			}
		}
		else if(ay < 0)
		{ // span starts along line AB, already know by >= 0
			DEBUG_LOG(3, "intersect AB\n");
			if(cy < 0)
			{ // at least two spans, we switch at a point along line BC
				DEBUG_LOG(3, "intersect BC\n");
				if(hy < 0)
				{ // three spans, next one starts at a point along line EH
					DEBUG_LOG(3, "intersect EH\n");
					if(gy < 0)
					{ // last span ends at a point along line FG
						DEBUG_LOG(3, "intersect FG\n");
						calc_span_end(&span_x1, &tx1, &ty1, ax, ay, bx, by, atx, aty, btx, bty);
						calc_span_end(&span_x2, &tx2, &ty2, bx, by, cx, cy, btx, bty, ctx, cty);
						calc_span_end(&span_x3, &tx3, &ty3, ex, ey, hx, hy, etx, ety, htx, hty);
						calc_span_end(&span_x4, &tx4, &ty4, fx, fy, gx, gy, ftx, fty, gtx, gty);

						ADD_SPAN(span_x1, span_x2, tx1, ty1, tx2, ty2)
						ADD_SPAN(span_x2, span_x3, tx2, ty2, tx3, ty3)
						ADD_SPAN(span_x3, span_x4, tx3, ty3, tx4, ty4)
					}
					else
					{ // last span ends at a point along line GH
						DEBUG_LOG(3, "intersect GH\n");
						calc_span_end(&span_x1, &tx1, &ty1, ax, ay, bx, by, atx, aty, btx, bty);
						calc_span_end(&span_x2, &tx2, &ty2, bx, by, cx, cy, btx, bty, ctx, cty);
						calc_span_end(&span_x3, &tx3, &ty3, ex, ey, hx, hy, etx, ety, htx, hty);
						calc_span_end(&span_x4, &tx4, &ty4, gx, gy, hx, hy, gtx, gty, htx, hty);

						ADD_SPAN(span_x1, span_x2, tx1, ty1, tx2, ty2)
						ADD_SPAN(span_x2, span_x3, tx2, ty2, tx3, ty3)
						ADD_SPAN(span_x3, span_x4, tx3, ty3, tx4, ty4)
					}
				}
				else
				{ // two spans, second one ends along line CH
					DEBUG_LOG(3, "intersect CH\n");
					calc_span_end(&span_x1, &tx1, &ty1, ax, ay, bx, by, atx, aty, btx, bty);
					calc_span_end(&span_x2, &tx2, &ty2, bx, by, cx, cy, btx, bty, ctx, cty);
					calc_span_end(&span_x3, &tx3, &ty3, cx, cy, hx, hy, ctx, cty, htx, hty);

					ADD_SPAN(span_x1, span_x2, tx1, ty1, tx2, ty2)
					ADD_SPAN(span_x2, span_x3, tx2, ty2, tx3, ty3)
				}
			}
			else
			{ // one span, ends along line CD
				DEBUG_LOG(3, "intersect CD\n");
				calc_span_end(&span_x1, &tx1, &ty1, ax, ay, bx, by, atx, aty, btx, bty);
				calc_span_end(&span_x2, &tx2, &ty2, cx, cy, dx, dy, ctx, cty, dtx, dty);

				ADD_SPAN(span_x1, span_x2, tx1, ty1, tx2, ty2)
			}
		}
		else
		{ // span starts along line AD, already know ay >= 0
			DEBUG_LOG(3, "intersect AD\n");
			if(cy < 0)
			{ // at least two spans second starts along BC
				DEBUG_LOG(3, "intersect BC\n");
				if(hy < 0)
				{ // three spans, third starts along EH
					DEBUG_LOG(3, "intersect EH\n");
					if(gy < 0)
					{ // last span ends along FG
						DEBUG_LOG(3, "intersect FG\n");
						calc_span_end(&span_x1, &tx1, &ty1, ax, ay, dx, dy, atx, aty, dtx, dty);
						calc_span_end(&span_x2, &tx2, &ty2, bx, by, cx, cy, btx, bty, ctx, cty);
						calc_span_end(&span_x3, &tx3, &ty3, ex, ey, hx, hy, etx, ety, htx, hty);
						calc_span_end(&span_x4, &tx4, &ty4, fx, fy, gx, gy, ftx, fty, gtx, gty);

						ADD_SPAN(span_x1, span_x2, tx1, ty1, tx2, ty2)
						ADD_SPAN(span_x2, span_x3, tx2, ty2, tx3, ty3)
						ADD_SPAN(span_x3, span_x4, tx3, ty3, tx4, ty4)
					}
					else
					{ // last span ends along GH
						DEBUG_LOG(3, "intersect GH\n");
						calc_span_end(&span_x1, &tx1, &ty1, ax, ay, dx, dy, atx, aty, dtx, dty);
						calc_span_end(&span_x2, &tx2, &ty2, bx, by, cx, cy, btx, bty, ctx, cty);
						calc_span_end(&span_x3, &tx3, &ty3, ex, ey, hx, hy, etx, ety, htx, hty);
						calc_span_end(&span_x4, &tx4, &ty4, gx, gy, hx, hy, gtx, gty, htx, hty);

						ADD_SPAN(span_x1, span_x2, tx1, ty1, tx2, ty2)
						ADD_SPAN(span_x2, span_x3, tx2, ty2, tx3, ty3)
						ADD_SPAN(span_x3, span_x4, tx3, ty3, tx4, ty4)
					}
				}
				else
				{ // two spans second ends along CH
					DEBUG_LOG(3, "intersect CH\n");
					calc_span_end(&span_x1, &tx1, &ty1, ax, ay, dx, dy, atx, aty, dtx, dty);
					calc_span_end(&span_x2, &tx2, &ty2, bx, by, cx, cy, btx, bty, ctx, cty);
					calc_span_end(&span_x3, &tx3, &ty3, cx, cy, hx, hy, ctx, cty, htx, hty);

					ADD_SPAN(span_x1, span_x2, tx1, ty1, tx2, ty2)
					ADD_SPAN(span_x2, span_x3, tx2, ty2, tx3, ty3)
				}
			}
			else
			{ // one span, ends along CD
				DEBUG_LOG(3, "intersect CD\n");
				calc_span_end(&span_x1, &tx1, &ty1, ax, ay, dx, dy, atx, aty, dtx, dty);
				calc_span_end(&span_x2, &tx2, &ty2, cx, cy, dx, dy, ctx, cty, dtx, dty);

				ADD_SPAN(span_x1, span_x2, tx1, ty1, tx2, ty2)
			}
		}
		render_spans(self, dest, spans, span_count, line);
		total_span_count += span_count;
	}
#ifdef SCOPE_SPAN_STATS
	self->span_counts[line] = total_span_count;
#endif
	DEBUG_LOG(1, "Generated % 4i spans on line % 4i\n", total_span_count, line);
}

#if NO_PARATASK
__attribute__((hot,noinline,optimize("-ffinite-math-only")))
static void render(struct scope_renderer *self, int ymin, int ymax, int npnts, const struct point* pnts, uint16_t * restrict dest) {
	for(int line = ymin; line <= ymax; line++) {
		render_line(self, npnts, pnts, dest, line);
	}
}
#else
struct render_args {
	struct scope_renderer *self;
	int npnts ;
	const struct point* pnts;
	uint16_t * restrict dest;
	int span;
};
static void render_paratask_func(size_t work_item_id, void *arg_) {
	struct render_args *a = arg_;
	const int ystart = work_item_id * a->span;
	const int yend   = IMIN(ystart + a->span, a->self->iw);
	for(int line = ystart; line < yend; line++) {
		render_line(a->self, a->npnts, a->pnts, a->dest, line);
	}
}

#include "paratask/paratask.h"
static void render(struct scope_renderer *self, int ymin, int ymax, int npnts, const struct point* pnts, uint16_t * restrict dest)
{
	int span = 1;
	struct render_args args = {
		self,
		npnts,
		pnts,
		dest,
		span
	};
	paratask_call(paratask_default_instance(), ymin, ymax-ymin+1, render_paratask_func, &args);
}
#endif
