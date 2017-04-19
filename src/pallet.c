
//TODO: test 8/16 bit code paths (32-bit seems to work)

#include "common.h"
#include "pallet.h"
#include <float.h>

#define NO_GAMMA_CORRECT_PALLET_EXPAND 1

struct pallet_colour {
	uint8_t r, g, b;
	uint8_t pos;
};

struct col_shifts {
	uint8_t r, g, b, x;
};

#define NUM_PALLETS 10
static const struct pallet_colour static_pallets[NUM_PALLETS][64] = {

	{	//  r    g    b  pos
		{   0,   0,   0,   0},
		{   0,   0,  64,  60},
		{ 255, 128,  64, 137},
		{ 255, 255, 255, 240},
		{ 255, 255, 255, 255}
	},

	{	//  r    g    b  pos
		{   0,  11, 138,   0},
		{ 255, 111, 255, 104},
		{ 111, 111, 255, 125},
		{ 111, 230, 255, 152},
		{ 255, 216, 111, 185},
		{ 255, 111, 111, 240},
		{ 255, 255, 255, 255}
	},

	{	//  r    g    b  pos
		{   0,   0,   0,   0},
		{ 255,   0, 128,  52},
		{ 196,   0,   0,  86},
		{   0,   0,   0, 175},
		{ 255,   0, 128, 240},
		{ 255, 255, 255, 255}
	},

	{	//  r    g    b  pos
		{   0,   0,   0,   0},
		{   0,   0,   0,  29},
		{ 115, 230,   0, 240},
		{  45, 255, 255, 255}
	},

	{	//  r    g    b  pos
		{   0,   0,   0,   0}, // TODO tweak this
		{ 196,  23,   0,  60},
		{ 255, 255, 255, 150},
		{  32,  96,   0, 200},
		{ 115, 230,   0, 255},
	},

	/*//punkie25 BLUEMETA, tweaked
	{	//  r    g    b  pos
		{   0,   0,   0,   0},
		{ 255, 255, 255, 103},
		{   0,   0,   0, 135},
		{   0,  64, 128, 200},
		{   0,   0,   0, 255},
	}, */

	//punkie21 STRANGE
	{	//  r    g    b  pos
		{   0,   0,   0,   0},
		{ 119,   0,   0,  26},
		{ 128,   0,  64, 110},
		{ 133,   9,  62, 111},
		{ 255, 255,   0, 189},
		{ 255, 255, 255, 255},
	},

	//punkie22 GORTOX
	{	//  r    g    b  pos
		{   0,   0,   0,   0},
		{  96,   8,  72,  28},
		{   0,   0, 252,  44},
		{   0, 252, 252,  60},
		{   0,   0, 252,  92},
		{  96,   8,  72, 124},
		{   0,   0,   0, 133},
		{  60,   4,  44, 144},
		{  96,   8,  76, 156},
		{ 252,   0,   0, 172},
		{ 252, 252,   0, 197},
		{ 252,   0,   0, 220},
		{  96,   8,  76, 236},
		{   0,   0,   0, 255},
	},

	//punkie20 OUTERDUST
	{	//  r    g    b  pos
		{   0,   0,   0,   0},
		{ 228, 231, 152, 172},
		{   0,   0,   0, 255},
	},

	//punkie18 GOTSOME
	{	//  r    g    b  pos
		{   0,   0,   0,   0},
		{ 255, 255,   0,  96},
		{   0,   0,   0, 191},
		{   0, 255,   0, 255},
	},

	//punkie16 VULCANO
	{	//  r    g    b  pos
		{   0,   0,   0,   0},
		{ 100,  48,  48,   8},
		{ 252,   0,   0,  32},
		{ 252, 252,   0,  64},
		{ 252, 252, 252,  96},
		{ 252, 120,   0, 160},
		{ 252,   0,   0, 192},
		{ 160,   0,   0, 224},
		{  60,  60,  60, 255},
	},

/*
	{	//  r    g    b  pos
		{   0,   0,   0,   0},
		{ 255, 255, 255, 255},
	}
*/
	// END
	//{   0,   0,   0,   0},
	//{   0,   0,   0,   0}
};


//#include "data/punkie_pal.h"

/*#undef NUM_PALLETS*/
/*#define NUM_PALLETS NUM_PUNKIE_PAL*/
/*#define pallets32 punkie_pals*/

#include "colourspace.h"

static void expand_pallet(const struct pallet_colour *curpal, uint32_t *dest, struct col_shifts s)
{
	//TODO: try out gamma correct interpolation here
	int j = 0;
	do { j++;
		for(int i = curpal[j-1].pos; i <= curpal[j].pos; i++) {
#if NO_GAMMA_CORRECT_PALLET_EXPAND
			uint32_t r = (curpal[j-1].r*(curpal[j].pos-i) + curpal[j].r*(i-curpal[j-1].pos))/(curpal[j].pos-curpal[j-1].pos);
			uint32_t g = (curpal[j-1].g*(curpal[j].pos-i) + curpal[j].g*(i-curpal[j-1].pos))/(curpal[j].pos-curpal[j-1].pos);
			uint32_t b = (curpal[j-1].b*(curpal[j].pos-i) + curpal[j].b*(i-curpal[j-1].pos))/(curpal[j].pos-curpal[j-1].pos);
#else
			colourf c1 = make_linear(curpal[j-1].r, curpal[j-1].g, curpal[j-1].b);
			colourf c2 = make_linear(curpal[j].r,   curpal[j].g,   curpal[j].b);
			// interpolate in L*u*v* colour space because it's perceptually uniform
			// not using L*a*b* because during fades to black it makes colours out
			// of nowhere and generally doesn't look as good.
			// Not using Lch because we don't want to move around the colour wheel
			// as we interpolate.
			c1 = rgb2luv(c1);
			c2 = rgb2luv(c2);
			for(int k=0; k < 3; k++)
				c1.v[k] = (c1.v[k]*(curpal[j].pos-i) + c2.v[k]*(i-curpal[j-1].pos))/(curpal[j].pos-curpal[j-1].pos);
			c1 = luv2rgb(c1);

			//TODO: cope better with out of gamut colours
			uint32_t r = fmaxf(0, fminf(255, gamma_curve(c1.r)));
			uint32_t g = fmaxf(0, fminf(255, gamma_curve(c1.g)));
			uint32_t b = fmaxf(0, fminf(255, gamma_curve(c1.b)));
#endif

			dest[i] = (r << s.r) | (g << s.g) | (b << s.b) | (255u << s.x);
		}
	} while(curpal[j].pos < 255);
}

struct pal_ctx {
	int pallet_changing;
	int palpos;
	int nextpal;
	int curpal;
	/**
	 * holds the current pallet (with interpolation between changes done already).
	 * extra element makes pallet blit much simpler (Don't have to check for last element)
	 */
	uint32_t active_pal[256 + 64] __attribute__((aligned(16))); // the 64 is padding for mmx/sse/etc code

	uint32_t pallets32[NUM_PALLETS][256];
};

static void do_pallet_step(int pos, uint32_t * restrict active_pal, const uint8_t *restrict next, const uint8_t *restrict prev);

struct pal_lst *pallet_get_palettes(void)
{
	struct pal_lst *res = malloc(sizeof(*res) + sizeof(uint32_t)*256*NUM_PALLETS);

	res->numpals = NUM_PALLETS;

	struct col_shifts shifts;
	if(0) {
		shifts.r =  0;
		shifts.g =  8;
		shifts.b = 16;
		shifts.x = 24;
	} else {
		shifts.r = 16;
		shifts.g =  8;
		shifts.b =  0;
		shifts.x = 24;
	}
	for(int p=0; p < NUM_PALLETS; p++)
		expand_pallet(static_pallets[p], res->pallets[p], shifts);

	return res;
}

struct pal_ctx *pal_ctx_new(int bswap)
{
	struct col_shifts shifts;
	if(bswap) {
		shifts.r =  0;
		shifts.g =  8;
		shifts.b = 16;
		shifts.x = 24;
	} else {
		shifts.r = 16;
		shifts.g =  8;
		shifts.b =  0;
		shifts.x = 24;
	}
	struct pal_ctx *self = malloc(sizeof(struct pal_ctx));
	for(int p=0; p < NUM_PALLETS; p++)
		expand_pallet(static_pallets[p], self->pallets32[p], shifts);
	self->pallet_changing = 0;
	self->palpos = 0;
	self->nextpal = 0;
	self->curpal = 0;
	memcpy(self->active_pal, self->pallets32[0], sizeof(uint32_t)*256);
	self->active_pal[256] = self->active_pal[255];
	return self;
}

//TODO: endianness?
static const struct col_shifts format_shifts[] = {
//	{  r,  g,  b,  x}
#if ALLOW_101010_PAL
	{  0, 10, 20, 30}, // SOFT_PIX_FMT_RGBx101010,
	{ 20, 10,  0, 30}, // SOFT_PIX_FMT_BGRx101010,
#else
	{  0,  8, 16, 24}, // SOFT_PIX_FMT_RGBx101010,
	{ 24,  8,  0, 24}, // SOFT_PIX_FMT_BGRx101010,
#endif
	{  0,  8, 16, 24}, // SOFT_PIX_FMT_RGBx8888,
	{ 16,  8,  0, 24}, // SOFT_PIX_FMT_BGRx8888,
	{  8, 16, 24,  0}, // SOFT_PIX_FMT_xRGB8888,
	{ 24, 16,  8,  0}, // SOFT_PIX_FMT_xBGR8888,
	{ 16,  8,  0, 24}, // SOFT_PIX_FMT_RGB565,
	{  0,  8, 16, 24}, // SOFT_PIX_FMT_BGR565,
	{ 16,  8,  0, 24}, // SOFT_PIX_FMT_RGB555,
	{  0,  8, 16, 24}, // SOFT_PIX_FMT_BGR555,
	{  8, 16, 24,  0}, // SOFT_PIX_FMT_8_xRGB_PAL,
	{ 24, 16,  8,  0}, // SOFT_PIX_FMT_8_xBGR_PAL,
	{  0,  8, 16, 24}, // SOFT_PIX_FMT_8_RGBx_PAL,
	{ 16,  8,  0, 24}, // SOFT_PIX_FMT_8_BGRx_PAL,
};

struct pal_ctx *pal_ctx_pix_format_new(julia_vis_pixel_format format)
{
	struct pal_ctx *self = malloc(sizeof(struct pal_ctx));
	for(int p=0; p < NUM_PALLETS; p++)
		expand_pallet(static_pallets[p], self->pallets32[p], format_shifts[format]);
	self->pallet_changing = 0;
	self->palpos = 0;
	self->nextpal = 0;
	self->curpal = 0;
	memcpy(self->active_pal, self->pallets32[0], sizeof(uint32_t)*256);
	self->active_pal[256] = self->active_pal[255];
	return self;
}

void pal_ctx_delete(struct pal_ctx *self)
{
	memset(self, 0, sizeof(*self));
	free(self);
}

const uint32_t *pal_ctx_get_active(struct pal_ctx *self) { return self->active_pal; }
int pal_ctx_changing(struct pal_ctx *self) { return self->pallet_changing; }
float pal_ctx_get_pos(struct pal_ctx *self) { return self->palpos*(1.0f/256); }

void pal_ctx_start_switch(struct pal_ctx *self, int next) {
	if(next<0) return;
	if(self->pallet_changing) return; // haven't finished the last one
	next = next % NUM_PALLETS;
	if(next == self->curpal) next = (next+1) % NUM_PALLETS;
	self->nextpal = next;
	self->pallet_changing = 1;
}

int pal_ctx_step(struct pal_ctx *self, uint8_t step) {
	if(!self->pallet_changing) return 0;
	self->palpos += step;
	if(self->palpos >=256) {
		self->pallet_changing = self->palpos = 0;
		self->curpal = self->nextpal;
		memcpy(self->active_pal, self->pallets32[self->nextpal], 256);
	} else {
		do_pallet_step(self->palpos,
		               self->active_pal,
		               (uint8_t *restrict)self->pallets32[self->nextpal],
		               (uint8_t *restrict)self->pallets32[self->curpal]);
	}

	self->active_pal[256] = self->active_pal[255];
	return 1;
}

static void do_pallet_step(int pos, uint32_t * restrict active_pal, const uint8_t *restrict next, const uint8_t *restrict prev)
{
	uint8_t *restrict d = (uint8_t *restrict)active_pal;
	const float p1 = pos/256.0f, p2 = (256-pos)/256.0f;
	for(int i=0; i<256*4; i++) {
		//d[i] = (uint8_t)((next[i]*pos + prev[i]*(256-pos))>>8);
		float out = linearize(next[i])*p1 + linearize(prev[i])*p2;
		d[i] = (uint8_t)fmaxf(0, fminf(255, gamma_curve(out)));
	}
}


