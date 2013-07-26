
//TODO: test 8/16 bit code paths (32-bit seems to work)

#include "common.h"
#include "pallet.h"

struct pallet_colour {
	uint8_t r, g, b;
	uint8_t pos;
};

#define NUM_PALLETS 10
static const struct pallet_colour static_pallets[NUM_PALLETS][64] = {

#if 0
	{	//  r    g    b  pos
		{   0,   0,   0,   0},
		{   0, 255, 255,   1},
		{   0,   0,   0,   2},
		{   0,   0,  64,  60},
		{ 255, 128,  64, 137},
		{ 255, 255, 255, 240},
		{ 255, 255, 255, 255}
	},
#endif

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

static uint32_t pallets32[NUM_PALLETS][256] __attribute__((aligned(16)));

/*#undef NUM_PALLETS*/
/*#define NUM_PALLETS NUM_PUNKIE_PAL*/
/*#define pallets32 punkie_pals*/
static const int num_pallets = NUM_PALLETS;

static void expand_pallet(const struct pallet_colour *curpal, uint32_t *dest, int bswap)
{
	int j = 0;
	do { j++;
		for(int i = curpal[j-1].pos; i <= curpal[j].pos; i++) {
			int r = (curpal[j-1].r*(curpal[j].pos-i) + curpal[j].r*(i-curpal[j-1].pos))/(curpal[j].pos-curpal[j-1].pos);
			int g = (curpal[j-1].g*(curpal[j].pos-i) + curpal[j].g*(i-curpal[j-1].pos))/(curpal[j].pos-curpal[j-1].pos);
			int b = (curpal[j-1].b*(curpal[j].pos-i) + curpal[j].b*(i-curpal[j-1].pos))/(curpal[j].pos-curpal[j-1].pos);

			if(bswap) dest[i] = (r)|(g<<8)|(b<<16);
			else dest[i] = (r<<16)|(g<<8)|b;
			dest[i] |= 255<<24;
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
	uint32_t active_pal[257] __attribute__((aligned(16)));
};

static void do_pallet_step(int pos, uint32_t * restrict active_pal, const uint8_t *restrict next, const uint8_t *restrict prev);

struct pal_ctx *pal_ctx_new(void)
{
	struct pal_ctx *self = malloc(sizeof(struct pal_ctx));
	self->pallet_changing = 0;
	self->palpos = 0;
	self->nextpal = 0;
	self->curpal = 0;
	memcpy(self->active_pal, pallets32[0], sizeof(uint32_t)*256);
	self->active_pal[256] = self->active_pal[255];
	return self;
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
		memcpy(self->active_pal, pallets32[self->nextpal], 256);
	} else
		do_pallet_step(self->palpos, self->active_pal, (uint8_t *restrict)pallets32[self->nextpal], (uint8_t *restrict)pallets32[self->curpal]);

	self->active_pal[256] = self->active_pal[255];
	return 1;
}

// stuff for global things down here
static struct pal_ctx glbl_ctx;

void pallet_init(int bswap) {
	for(int p=0; p < num_pallets; p++)
		expand_pallet(static_pallets[p], pallets32[p], bswap);
	glbl_ctx.pallet_changing = 0;
	glbl_ctx.palpos = 0;
	glbl_ctx.nextpal = 0;
	glbl_ctx.curpal = 0;
	memcpy(glbl_ctx.active_pal, pallets32[0], sizeof(uint32_t)*256);
	glbl_ctx.active_pal[256] = glbl_ctx.active_pal[255];
}

const uint32_t *get_active_pal(void) { return glbl_ctx.active_pal; }
//const uint32_t *get_active_pal(void) { return pallets32[0]; }
//const uint32_t *get_active_pal(void) { return punkie_pals[FIRE]; }

int get_pallet_changing(void) { return glbl_ctx.pallet_changing; }
void pallet_start_switch(int next) { pal_ctx_start_switch(&glbl_ctx, next); }
int pallet_step(int step) { return pal_ctx_step(&glbl_ctx, step); }

int pallet_num_pal(void) { return num_pallets; }
uint32_t *pallet_get_pal(int pal) { if(pal >= 0 && pal < num_pallets) return pallets32[pal]; else return NULL; }
float pallet_get_pos(void) { return pal_ctx_get_pos(&glbl_ctx); } 
int pallet_get_curpal(void) { return glbl_ctx.curpal; }
int pallet_get_nextpal(void) { return glbl_ctx.nextpal; }

static void do_pallet_step(int pos, uint32_t * restrict active_pal, const uint8_t *restrict next, const uint8_t *restrict prev) {
	uint8_t *restrict d = (uint8_t *restrict)active_pal;
	for(int i=0; i<256*4; i++)
		d[i] = (uint8_t)((next[i]*pos + prev[i]*(255-pos))>>8);
}

