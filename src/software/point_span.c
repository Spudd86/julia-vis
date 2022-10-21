#ifdef NDEBUG
#pragma GCC optimize "3,ira-hoist-pressure,inline-functions,modulo-sched,modulo-sched-allow-regmoves"
#endif


#include "common.h"
#include "maxsrc.h"
#include "getsamp.h"

#include <float.h>
#include <assert.h>

//#define POINT_SPAN_DEBUG_LOG 3
//#define DEBUG_MAX_SPAN_START 1

#if POINT_SPAN_DEBUG_LOG
int point_span_scope_debug_level = 0;
#endif

#if POINT_SPAN_DEBUG_LOG
#define DEBUG_LOG(level, ...) if(point_span_scope_debug_level >= level) printf(__VA_ARGS__)
#else
#define DEBUG_LOG(level, ...)
#endif

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

// NOTES:
//   The geometry of the scope is such that you could get long horizontal spans, but they won't 
//   overlap with very many other generated spans, and would have a very gradual slope

// Acceleration structure notes
//  - When building spans, at all times we are either moving away from the origin or towards it linearly.
//  - can use this (NOTE: this is all in texture space)
//     - can find point of minimum distance to origin and split the spans around that, that way any given span
//       is always either increasing or decreasing it's distance to the origin
//     - can find that point by building equation for distance to origin squared, taking the derivative, and finding
//       where the derivative is zero
//         - Derivative is linear.
//     - then can check new spans that overlap by finding where they interesect
//  - Keep the spans in a tree that has at each node the exteems of the spans inside it.
//  - want to link the leaves in the order that they show up along the line
//    - this helps with two things: when looking up spans that overlap a new one we can look for the right ending spans
//           and then just walk the ones between and when we're actually rendering spans we just start at the left and
//           walk the leaves.
///   - can just store spans in an array and use indicies as pointers, uint16 for compactness
// Need to make sure that when generating the spans for a particular line segment we don't overlap them
//    - this means we need to nudge the endpoints, in both screenspace and texture space
//        - NOTE: current code nudges to put them on integer locations

struct span
{
	uint16_t ix; // output image x position
	uint16_t l; // length of the span
	
	int8_t tx1; // starting texture x co-ord
	int8_t ty1; // starting texture y co-ord
	
	int8_t tx2; // ending texture x co-ord
	int8_t ty2; // ending texture y co-ord
	
	float ftx1;
	float fty1;
	float ftx2;
	float fty2;
};

struct scope_renderer
{
	int iw;
	int ih;
	
	int samp;
	
	int line_width;
	
	int max_lspans; // Maximum number of spans in a line TODO: get rid of the need for this
	
	// texture
	uint16_t *restrict tex_data;
	int tex_stride;
	int tex_w;
	int tex_h;
	
	struct span *span_buf;
	int *span_counts;
};

struct scope_renderer* scope_renderer_new(int w, int h, int samp)
{
	struct scope_renderer *self = calloc(sizeof(*self), 1);

	self->iw = w; self->ih = h;
	self->samp = samp; //MIN(MIN(width/8, height/8), 128); //IMAX(w,h);
	
	// TODO: deal with aspect ratio if screen is not square
	self->line_width = IMAX(IMAX(w/24, 8), IMAX(h/24, 8));
	self->line_width = self->line_width/1.75f;
	int tex_w = self->tex_w = 63;
	int tex_h = self->tex_h = 63;
	int tex_stride = self->tex_stride = tex_w + 1;
	
	self->tex_data = calloc(sizeof(uint16_t), tex_stride*(tex_h+1)); // add extra padding to be able to run past the end
	
	for(int y=0; y < tex_h; y++)  {
		for(int x=0; x < tex_w; x++) {
			//float u = (2.0f*x)/(tex_w-1) - 1, v = (2.0f*y)/(tex_h-1) - 1;
			float u = (2*x+1.0f)/(tex_w-1) - 1;
			float v = (2*y+1.0f)/(tex_h-1) - 1;
#if 1
			float f = fmax(0.0f, expf(-3.0f*0.5f*log2f((u*u+v*v) + 1.0f)) * (1 - sqrt(u*u+v*v)));
			//float f = expf(-4.5f*0.5f*log2f((u*u+v*v) + 1.0f));
			
			self->tex_data[y*tex_stride + x] = (uint16_t)(f*(UINT16_MAX));
#else
			const static uint16_t im[] = {
				0xC*UINT16_MAX/15, 0xD*UINT16_MAX/15, 0xE*UINT16_MAX/15, 0xF*UINT16_MAX/15,
				0x1*UINT16_MAX/15, 0x0*UINT16_MAX/15, 0x2*UINT16_MAX/15, 0x3*UINT16_MAX/15,
				0x4*UINT16_MAX/15, 0x5*UINT16_MAX/15, 0x6*UINT16_MAX/15, 0x7*UINT16_MAX/15,
				0x8*UINT16_MAX/15, 0x9*UINT16_MAX/15, 0xA*UINT16_MAX/15, 0xB*UINT16_MAX/15
			};
			self->tex_data[y*tex_stride + x] = im[((y*4)/tex_h)*4 + (x*4)/tex_w];
#endif
		}
	}

	self->max_lspans = samp*4;
	
	self->span_buf    = calloc(sizeof(self->span_buf[0]), h*self->max_lspans);
	self->span_counts = calloc(sizeof(self->span_counts[0]), h);
	
	
	DEBUG_LOG(0, "Using line width %i\n", self->line_width);
	return self;
}

struct point
{
	float x;
	float y;
};

static void generate_spans(struct scope_renderer *self, int ymin, int ymax, const struct point* pnts);
static void render_spans(struct scope_renderer *self, int ymin, int ymax, uint16_t *restrict dest);

static void render_line(struct scope_renderer * const self, void *restrict dest, int line);
static void generate_spans_for_line(struct scope_renderer *self, const struct point* pnts, int line);

void scope_render(struct scope_renderer *self,
                  void *restrict dest,
                  float tx, float ty, float tz,
                  const float *audio,
                  int audiolen)
{
	int samp = self->samp;
	int iw = self->iw, ih = self->ih;

	float cx=cosf(tx), cy=cosf(ty), cz=cosf(tz);
	float sx=sinf(tx), sy=sinf(ty), sz=sinf(tz);

	const float R[][3] = {
		{cz*cy-sz*sx*sy, -sz*cx, -sy*cz-cy*sz*sx},
		{sz*cy+cz*sx*sy,  cz*cx, -sy*sz+cy*cz*sx},
		{cx*sy         ,    -sx,  cy*cx}
	};

	struct point pnts[samp];
	
	float ymin = ih;
	float ymax = 0;

	for(int i=0; i<samp; i++)
	{
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

		float xi = x*zvd*iw + iw/2.0f;
		float yi = y*zvd*ih + ih/2.0f;

		//pnts[i].x = (uint32_t)(xi*256);
		//pnts[i].y = (uint32_t)(yi*256);
		pnts[i].x = xi;
		pnts[i].y = yi;
		
		ymin = fminf(ymin, yi);
		ymax = fmaxf(ymax, yi);
	}
	
	int first_line = floor(ymin - self->line_width*1.4142135623730950488016887f);
	int last_line = ceil(ymax + self->line_width*1.4142135623730950488016887f);
	
	//for(int line = first_line; line <= last_line; line++)
	//{
		//generate_spans_for_line(self, pnts, line);
		//render_line(self, dest, line);
	//}
	generate_spans(self, first_line, last_line, pnts);
	render_spans(self, first_line, last_line, dest);
}

/*****************************************************************************************************************************************
 * Internal rendering stuff
 */
__attribute__((hot,noinline,optimize("-ffinite-math-only")))
static void render_line(struct scope_renderer * const self, void *restrict dest, int line)
{
	uint16_t *restrict dst_line = (uint16_t *restrict)dest + self->iw * line;

	struct span* span = self->span_buf + line*self->max_lspans;
	int num_spans = self->span_counts[line];
	
	for(int i=0; i < num_spans; ++i, ++span)
	{
		assert(span->l > 0);
		// Make into 8.16 fixed point
		// the co-ords stored in the span structure are effectively 6.1 bit fixed point so
		// we only need to shift by 16
		// only the x co-ord will be negative since the span generation always generates the
		// positive y (due to the geometry of the boxes we're rendering we never need to cross
		// y texture axis in the middle of a span)

#if 0
		const int32_t tx1 = (span->ftx1 + 1.0)*(32768*(self->tex_w - 1));
		const int32_t ty1 = (span->fty1 + 1.0)*(32768*(self->tex_h - 1));
		const int32_t tx2 = (span->ftx2 + 1.0)*(32768*(self->tex_w - 1));
		const int32_t ty2 = (span->fty2 + 1.0)*(32768*(self->tex_h - 1));
		
		const int32_t dx = (tx2 - tx1)/span->l;
		const int32_t dy = (ty2 - ty1)/span->l;
		
		for(int32_t x = span->ix, tx = tx1, ty = ty1; x <= span->ix + span->l; ++x, tx += dx, ty += dy)
		{
			const uint32_t ys =  ty>>16,      xs =  tx>>16;     assert(ys < self->tex_h); assert(xs < self->tex_w);
			const uint32_t yf = (ty>>8)&0xff, xf = (tx>>8)&0xff;
#elif 0
		const int32_t tx1 = (span->ftx1 + 1.0)*(128*(self->tex_w - 1));
		const int32_t ty1 = (span->fty1 + 1.0)*(128*(self->tex_h - 1));
		const int32_t tx2 = (span->ftx2 + 1.0)*(128*(self->tex_w - 1)); 
		const int32_t ty2 = (span->fty2 + 1.0)*(128*(self->tex_h - 1));

		for(uint32_t x = span->ix, i = 0; x <= span->ix + span->l; ++x, ++i)
		{
			const uint32_t t  = (256*i)/span->l;
			const uint32_t tx = tx1 * t + (256 - t) * tx2;
			const uint32_t ty = ty1 * t + (256 - t) * ty2;
			
			const uint32_t ys =  ty>>16,      xs =  tx>>16;     assert(ys < self->tex_h); assert(xs < self->tex_w);
			const uint32_t yf = (ty>>8)&0xff, xf = (tx>>8)&0xff;
#elif 0
		const float linv = 1.0f/span->l;
		// Convert range to 24.8 fixed point ranges are [-1. 1] so add 1 and multiply by 128 to end up
		// with the range being [0, 256*texture_size]
		const float ftx1 = (span->ftx1 + 1.0f)*128*(self->tex_w - 1);
		const float ftx2 = (span->ftx2 + 1.0f)*128*(self->tex_w - 1);
		const float fty1 = (span->fty1 + 1.0f)*128*(self->tex_h - 1);
		const float fty2 = (span->fty2 + 1.0f)*128*(self->tex_h - 1);
		
		for(int32_t i = 0; i <= span->l; ++i)
		{
			const int32_t x = span->ix + i;
			const float t = i/(float)span->l;
			//const int32_t tx = ftx1*t + (1.0f-t)*ftx2;
			//const int32_t ty = fty1*t + (1.0f-t)*fty2;
			const int32_t tx = ftx1*(1.0f - t) + t*ftx2;
			const int32_t ty = fty1*(1.0f - t) + t*fty2;

			const uint32_t ys = ty>>8,   xs = tx>>8;
			const uint32_t yf = ty&0xff, xf = tx&0xff;
#elif 1
		const float dx = (span->ftx2 - span->ftx1)/(span->l);
		const float dy = (span->fty2 - span->fty1)/(span->l);
		
		for(int32_t i = 0; i <= span->l; ++i)
		{
			const int32_t x = span->ix + i;
			const float ftx = span->ftx1 + i*dx;
			const float fty = span->fty1 + i*dy;

			// Convert to 24.8 fixed point ranges are [-1. 1] so add 1 and multiply by 128
			const int32_t tx = (ftx + 1.0f)*(128*(self->tex_w - 1));
			const int32_t ty = (fty + 1.0f)*(128*(self->tex_h - 1));
			
			const uint32_t ys = ty>>8,   xs = tx>>8;     assert(ys < self->tex_h); assert(xs < self->tex_w);
			const uint32_t yf = ty&0xff, xf = tx&0xff;
#else
		const float ftx1 = (span->ftx1 + 1.0f)*128*(self->tex_w - 1);
		const float ftx2 = (span->ftx2 + 1.0f)*128*(self->tex_w - 1);
		const float fty1 = (span->fty1 + 1.0f)*128*(self->tex_h - 1);
		const float fty2 = (span->fty2 + 1.0f)*128*(self->tex_h - 1);
		const float dx = (ftx2 - ftx1)/(span->l);
		const float dy = (fty2 - fty1)/(span->l);
		
		for(int32_t i = 0; i <= span->l; ++i)
		{
			const int32_t x = span->ix + i;
			const int32_t tx = ftx1 + i*dx;
			const int32_t ty = fty1 + i*dy;
			
			const uint32_t ys = ty>>8,   xs = tx>>8;     assert(ys < self->tex_h); assert(xs < self->tex_w);
			const uint32_t yf = ty&0xff, xf = tx&0xff;
#endif

#if 0
			uint32_t a00 = (yf*xf);
			uint32_t a01 = (yf*(256-xf));
			uint32_t a10 = ((256-yf)*xf);
			uint32_t a11 = ((256-yf)*(256-xf)); 
			
			const uint16_t *s0 = self->tex_data + ys*self->tex_stride + xs;
			const uint16_t *s1 = s0 + self->tex_stride;
			
			uint16_t res = (s0[0]*a00 + s0[1]*a01
			              + s1[0]*a10 + s1[1]*a11)>>16;
#else
			uint16_t res = self->tex_data[ys*self->tex_stride + xs];
#endif

			res = IMAX(res, dst_line[x]);
			dst_line[x] = res;
		}
#if DEBUG_MAX_SPAN_START
		dst_line[span->ix] = UINT16_MAX;
#endif
	}
}

/*******************************************************************
 * Interpolate tx/ty to what they should be when the line (x0,y0)->(x1, y1)
 * crosses zero
 */
__attribute__((hot,always_inline,optimize("-ffinite-math-only")))
static inline void calc_span_end(float* xo, float* txo, float* tyo,
                                 float x0, float y0, float x1, float y1, 
                                 float tx0, float ty0, float tx1, float ty1)
{
	float t;
	if(fabsf(x1 - x0) > 0.000001f)
	{
		*xo = x0 - y0 * (x1 - x0) / (y1 - y0);
		t = (*xo  - x0)/(x1 - x0);
		
		// (1 - t) * v0 + t * v1;
		//*xo  = (1 - t) * x0 + t * x1;
		*txo = (1 - t) * tx0 + t * tx1;
		*tyo = (1 - t) * ty0 + t * ty1;
		
		assert((*xo >= x0 && *xo <= x1) || (*xo >= x1 && *xo <= x0));
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

#if defined(POINT_SPAN_DEBUG_LOG) && POINT_SPAN_DEBUG_LOG > 1
#define ADD_SPAN_DEBUG_OUT(x0_, x1_, tx0_, ty0_, tx1_, ty1_) \
	if(point_span_scope_debug_level > 1)                \
		printf("span on line % 4i %4.1f, %4.1f, "       \
		       "t: (%1.4f, %1.4f) -> (%1.4f, %1.4f)\n", \
		       line, x0_, x1_, tx0_, ty0_, tx1_, ty1_);
#else
#define ADD_SPAN_DEBUG_OUT(x0_, x1_, tx0_, ty0_, tx1_, ty1_)
#endif

#define ADD_SPAN(x0_, x1_, tx0_, ty0_, tx1_, ty1_)      \
	do {                                                \
		assert(tx0_ >= -1.0001 && tx0_ <= 1.0001);            \
		assert(tx1_ >= -1.0001 && tx1_ <= 1.0001);            \
		assert(ty0_ >= 0.0 && ty0_ <= 1.0001);             \
		assert(ty1_ >= 0.0 && ty1_ <= 1.0001);             \
		const float x0r_ = ceil(x0_);                       \
		const float x1r_ = floor(x1_);                       \
		const float l_   = x1r_ - x0r_;                       \
		if(l_ > 1) {                                    \
			const float dx_ = (tx1_ - tx0_)/(x1_ - x0_);         \
			const float dy_ = (ty1_ - ty0_)/(x1_ - x0_);         \
			ADD_SPAN_DEBUG_OUT(x0_, x1_, tx0_, ty0_, tx1_, ty1_)\
			spans[span_count].ix  = x0r_;               \
			spans[span_count].l   = l_;                 \
			spans[span_count].tx1 = tx0_*64;            \
			spans[span_count].ty1 = ty0_*64;            \
			spans[span_count].tx2 = tx1_*64;            \
			spans[span_count].ty2 = ty1_*64;            \
			                                            \
			float s0_ = fabsf(x0r_ - x0_);               \
			float s1_ = fabsf(x1r_ - x1_);               \
			spans[span_count].ftx1 = tx0_ + s0_*dx_;     \
			spans[span_count].fty1 = ty0_ + s0_*dy_;     \
			spans[span_count].ftx2 = tx1_ - s1_*dx_;     \
			spans[span_count].fty2 = ty1_ - s1_*dy_;     \
			                                            \
			span_count++;                               \
			if(span_count >= self->max_lspans) break;   \
		}                                               \
	}while(0);

/*
 * Generate all the spans for a specific line in the image
 *
 * Places the spans in self->span_buf starting at self->span_buf[line*samp*2] and returns the number of spans generated
 */
__attribute__((hot,noinline,optimize("-ffinite-math-only")))
static void generate_spans_for_line(struct scope_renderer *self, const struct point* pnts, int line)
{
	// TODO: make the span count an atomic in an array or something so that we can steal spans
	// if we run out, or something like that
	
	DEBUG_LOG(1, "Starting spans for line %i\n", line);

	const int   samp = self->samp;
	const int   width = self->line_width;
	const float width_rt2 = width * 1.4142135623730950488016887f;  // width * root 2
	
	const float normalize_scale = 1.0f/width_rt2;
	
 	struct span* spans = self->span_buf + line*self->max_lspans;

	uint32_t span_count = 0;
	for(int i=0; i<samp-1; i++)
	{
		DEBUG_LOG(1, "cheking segment #% 5i (%f, %f) -> (%f, %f)\n", i, pnts[i].x, pnts[i].y, pnts[i+1].x, pnts[i+1].y);
	
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
		
		// Since we subracted off the height we can flip the line around to always have
		// non-negative slope without affecting our spans, since they now lie on the x-axis
		
		if(y1 > y2)
		{
			y1 = -y1;
			y2 = -y2;
		}
		
		DEBUG_LOG(1, "transformed segment (%f, %f) -> (%f, %f)\n", x1, y1, x2, y2);
		
		// Check which of the various interesting line segments the ray starting at (0, line) intersects
		// too that end, generate all the interesting points.
		const float ddx = x2-x1,       ddy = y2-y1;
		const float  d  = width/hypotf(ddx, ddy);
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

		DEBUG_LOG(3, "\n\nimport matplotlib.pyplot as plt\nfrom matplotlib.path import Path\nfrom matplotlib.patches import PathPatch\n");
		DEBUG_LOG(3, "lx = [%f,%f]\n", x1, x2);
		DEBUG_LOG(3, "ly = [%f,%f]\n", y1, y2);
		DEBUG_LOG(3, "abcd = [(%4.4f, %4.4f), (%4.4f, %4.4f), (%4.4f, %4.4f), (%4.4f, %4.4f), (0.,0.)]\n",
		          ax,ay, bx,by, cx,cy, dx,dy);
		DEBUG_LOG(3, "efgh = [(%4.4f, %4.4f), (%4.4f, %4.4f), (%4.4f, %4.4f), (%4.4f, %4.4f), (0.,0.)]\n",
		          ex,ey, fx,fy, gx,gy, hx,hy);
		DEBUG_LOG(3, "bech = [(%4.4f, %4.4f), (%4.4f, %4.4f), (%4.4f, %4.4f), (%4.4f, %4.4f)]\n",
		          bx,by, ex,ey, cx,cy, hx,hy);
		DEBUG_LOG(3, "box_codes = [Path.MOVETO, Path.LINETO, Path.LINETO, Path.LINETO, Path.CLOSEPOLY]\n");
		DEBUG_LOG(3, "fig, ax = plt.subplots(sharex=True,sharey=True)\n"
		             "ax.plot(lx, ly)\n"
		             //"ax.plot(box, boy)\n"
		             "ax.add_patch(PathPatch(Path(abcd, box_codes)))\n"
		             "ax.add_patch(PathPatch(Path(efgh, box_codes)))\n"
		             "ax.add_patch(PathPatch(Path(bech, [Path.MOVETO, Path.LINETO, Path.MOVETO, Path.LINETO])))\n"
		             "ax.set_xlim(%4.4f, %4.4f)\n"
		             "ax.set_ylim(%4.4f, %4.4f)\n",
		             x1-(width_rt2+2), x2 + width_rt2+2,
		             y1-(width_rt2+2), y2 + width_rt2+2);
		DEBUG_LOG(3, "ax.add_patch(PathPatch(Path([(%4.4f, 0.0), (%4.4f, 0.0)], [Path.MOVETO, Path.LINETO])))\n",
		             x1-(width_rt2+2), x2 + width_rt2+2);
		DEBUG_LOG(3, "ax.text(%4.4f, %4.4f, 'A')\n"
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

		//TODO: should be able to replace all the hypot stuff with linearly interpolating
		// what the co-ords should be along the lines.
		// That is we set what tx,ty should be at each of our 8 line end points
		// and then lerp what they should be at the intersection, also that way it can't overflow.
		
		// already know fy > 0 and dy < 0 from bail out test above
		
		float span_x1, span_x2, span_x3, span_x4;
		float tx1, tx2, tx3, tx4;
		float ty1, ty2, ty3, ty4;
		
		if(ey < 0)
		{ // we are in the p2 end cap, only one span, starting somewhere along the line EF
			DEBUG_LOG(2, "intersect EF\n");
			if(gy < 0)
			{ // other side of span is on line FG
				DEBUG_LOG(2, "intersect FG\n");
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
			DEBUG_LOG(2, "intersect BE\n");
			if(hy < 0)
			{ // Two spans, we switch at a point along line EH
				DEBUG_LOG(2, "intersect EH\n");
				if(gy < 0)
				{ // second span ends somewhere along line FG
					DEBUG_LOG(2, "intersect FG\n");
					calc_span_end(&span_x1, &tx1, &ty1, bx, by, ex, ey, btx, bty, etx, ety);
					calc_span_end(&span_x2, &tx2, &ty2, ex, ey, hx, hy, etx, ety, htx, hty);
					calc_span_end(&span_x3, &tx3, &ty3, fx, fy, gx, gy, ftx, fty, gtx, gty);
					
					ADD_SPAN(span_x1, span_x2, tx1, ty1, tx2, ty2)
					ADD_SPAN(span_x2, span_x3, tx2, ty2, tx3, ty3)
				}
				else
				{ // second span ends somewhere along line GH
					DEBUG_LOG(2, "intersect GH\n");
					calc_span_end(&span_x1, &tx1, &ty1, bx, by, ex, ey, btx, bty, etx, ety);
					calc_span_end(&span_x2, &tx2, &ty2, ex, ey, hx, hy, etx, ety, htx, hty);
					calc_span_end(&span_x3, &tx3, &ty3, gx, gy, hx, hy, gtx, gty, htx, hty);
					
					ADD_SPAN(span_x1, span_x2, tx1, ty1, tx2, ty2)
					ADD_SPAN(span_x2, span_x3, tx2, ty2, tx3, ty3)
				}
			}
			else
			{ // only one span, ends along line CH
				DEBUG_LOG(2, "intersect CH\n");
				calc_span_end(&span_x1, &tx1, &ty1, bx, by, ex, ey, btx, bty, etx, ety);
				calc_span_end(&span_x2, &tx2, &ty2, cx, cy, hx, hy, ctx, cty, htx, hty);
				
				ADD_SPAN(span_x1, span_x2, tx1, ty1, tx2, ty2)
			}
		}
		else if(ay < 0)
		{ // span starts along line AB, already know by >= 0
			DEBUG_LOG(2, "intersect AB\n");
			if(cy < 0)
			{ // at least two spans, we switch at a point along line BC
				DEBUG_LOG(2, "intersect BC\n");
				if(hy < 0)
				{ // three spans, next one starts at a point along line EH
					DEBUG_LOG(2, "intersect EH\n");
					if(gy < 0)
					{ // last span ends at a point along line FG
						DEBUG_LOG(2, "intersect FG\n");
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
						DEBUG_LOG(2, "intersect GH\n");
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
					DEBUG_LOG(2, "intersect CH\n");
					calc_span_end(&span_x1, &tx1, &ty1, ax, ay, bx, by, atx, aty, btx, bty);
					calc_span_end(&span_x2, &tx2, &ty2, bx, by, cx, cy, btx, bty, ctx, cty);
					calc_span_end(&span_x3, &tx3, &ty3, cx, cy, hx, hy, ctx, cty, htx, hty);
					
					ADD_SPAN(span_x1, span_x2, tx1, ty1, tx2, ty2)
					ADD_SPAN(span_x2, span_x3, tx2, ty2, tx3, ty3)
				}
			}
			else
			{ // one span, ends along line CD
				DEBUG_LOG(2, "intersect CD\n");
				calc_span_end(&span_x1, &tx1, &ty1, ax, ay, bx, by, atx, aty, btx, bty);
				calc_span_end(&span_x2, &tx2, &ty2, cx, cy, dx, dy, ctx, cty, dtx, dty);
			
				ADD_SPAN(span_x1, span_x2, tx1, ty1, tx2, ty2)
			}
		}
		else
		{ // span starts along line AD, already know ay >= 0
			DEBUG_LOG(2, "intersect AD\n");
			if(cy < 0)
			{ // at least two spans second starts along BC
				DEBUG_LOG(2, "intersect BC\n");
				if(hy < 0)
				{ // three spans, third starts along EH
					DEBUG_LOG(2, "intersect EH\n");
					if(gy < 0)
					{ // last span ends along FG
						DEBUG_LOG(2, "intersect FG\n");
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
						DEBUG_LOG(2, "intersect GH\n");
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
					DEBUG_LOG(2, "intersect CH\n");
					calc_span_end(&span_x1, &tx1, &ty1, ax, ay, dx, dy, atx, aty, dtx, dty);
					calc_span_end(&span_x2, &tx2, &ty2, bx, by, cx, cy, btx, bty, ctx, cty);
					calc_span_end(&span_x3, &tx3, &ty3, cx, cy, hx, hy, ctx, cty, htx, hty);
					
					ADD_SPAN(span_x1, span_x2, tx1, ty1, tx2, ty2)
					ADD_SPAN(span_x2, span_x3, tx2, ty2, tx3, ty3)
				}
			}
			else
			{ // one span, ends along CD
				DEBUG_LOG(2, "intersect CD\n");
				calc_span_end(&span_x1, &tx1, &ty1, ax, ay, dx, dy, atx, aty, dtx, dty);
				calc_span_end(&span_x2, &tx2, &ty2, cx, cy, dx, dy, ctx, cty, dtx, dty);
				
				ADD_SPAN(span_x1, span_x2, tx1, ty1, tx2, ty2)
			}
		}
	}
	
	// TODO: need to sort and split spans so that they are left to right and don't overlap
	// picking always the segments that are closer to the origin in texture space
	// This should be done via some sort of acceleration structure. For now we can ignore it
	// since we don't need clipping for correct output
	
	// the acceleration structure might be based around the closest point within a span to the origin?
	//   this would mean that going either way away from that point you either are at an end, or move
	//   further from the origin.
	//   Also store the point *furthest from the origin so we can quickly check if they might cross
	
	self->span_counts[line] = span_count;
	DEBUG_LOG(1,"Generated % 4i spans on line % 4i\n", span_count, line);
}


#if NO_PARATASK

__attribute__((hot,noinline,optimize("-ffinite-math-only")))
static void generate_spans(struct scope_renderer *self, int ymin, int ymax, const struct point* pnts)
{
	for(int line = ymin; line <= ymax; line++)
	{
		generate_spans_for_line(self, pnts, line);
	}
}

__attribute__((hot,noinline,optimize("-ffinite-math-only")))
static void render_spans(struct scope_renderer *self, int ymin, int ymax, uint16_t * restrict dest)
{
	for(int line = ymin; line <= ymax; line++)
	{
		render_line(self, dest, line);
	}
}

#else

#include "paratask/paratask.h"

struct generate_spans_args {
	struct scope_renderer *self;
	const struct point* pnts;
	int span;
};

static void generate_spans_paratask_func(size_t work_item_id, void *arg_)
{
	struct generate_spans_args *a = arg_;
	const int ystart = work_item_id * a->span;
	const int yend   = IMIN(ystart + a->span, a->self->iw);
	for(int line = ystart; line < yend; line++)
	{
		generate_spans_for_line(a->self, a->pnts, line);
	}
}

static void generate_spans(struct scope_renderer *self, int ymin, int ymax, const struct point* pnts)
{
	int span = 1;
	struct generate_spans_args args = {
		self,
		pnts,
		span
	};
	paratask_call(paratask_default_instance(), ymin, ymax+1, generate_spans_paratask_func, &args);
}

struct render_spans_args {
	struct scope_renderer *self;
	uint16_t * restrict dest;
	int span;
};

static void render_spans_paratask_func(size_t work_item_id, void *arg_)
{
	struct render_spans_args *a = arg_;
	const int ystart = work_item_id * a->span;
	const int yend   = IMIN(ystart + a->span, a->self->iw);
	for(int line = ystart; line < yend; line++)
	{
		render_line(a->self, a->dest, line);
	}
}

static void render_spans(struct scope_renderer *self, int ymin, int ymax, uint16_t * restrict dest)
{
	int span = 1;
	struct render_spans_args args = {
		self,
		dest,
		span
	};
	paratask_call(paratask_default_instance(), ymin, ymax+1, render_spans_paratask_func, &args);
}

#endif

#if 0
// May eventually want to implement this to see if we get a speed up in single threaded mode
static void generate_spans(struct scope_renderer *self, struct point32* pnts)
{
	int samp = self->samp;

	uint32_t span_count = 0;
	for(int i=0; i<samp-1; i++)
	{
		struct span *sp = NULL;
		// self->span_buf[span_count]
		// TODO: generate spans for line from pnts[i] to pnts[i+1]
		
		// Need to handle different cases, a mostly vertical line or mostly horizontal
		// Also might want to optimize based on angle with previous/next line
		
		float x1 = pnts[i].x, y1 = pnts[i].y, x2 = pnts[i+1].x, y2 = pnts[i+1].y;
		
		if(y1 > y2)
		{ // Swap, always want to step downwards for simplicity
			int32_t xt = x1, yt = y1;
			x1 = x2, y1 = y2;
			x2 = xt, y2 = yt;
		}
		
		// TODO: check for very nearly/exactly horizontal		
		
		float dx = (x2 - x1)/(y2 - y1);
		
		if(dx >= 1.0f)
		{ // mostly horizontal
			
			// walk along edge of our box to fill, the "short" edge that is the line orthogonal to our
			// [p1 -> p2] segment passing through p1 exending exactly the line width on either side
			// we generate spans along that, then we generate spans along the parallel line that is 
			// a line width away
		}
		else
		{ // mostly vertical
			
		}
		
		// simple way to handle end points is just generate spans for the normal square texture.
		
		// we can clip by checking where the two spans texture space lines intersect and using distamce to origin
	}
}
#endif
