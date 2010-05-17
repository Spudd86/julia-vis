
//TODO: test w/o mmx/sse
//TODO: test 8/16 bit code paths (32-bit seems to work)

#include "common.h"
#include "pallet.h"

struct pallet_colour {
	uint8_t r, g, b;
	uint8_t pos;
};

#define NUM_PALLETS 5
static const int num_pallets = NUM_PALLETS;
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
//		{ 115, 230,   0, 255},
		{  45, 255, 255, 255}
	}

	// END
	//{   0,   0,   0,   0},
	//{   0,   0,   0,   0}
};


static uint32_t pallets32[NUM_PALLETS][256] __attribute__((aligned(16)));

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
int get_pallet_changing(void) { return glbl_ctx.pallet_changing; }
void pallet_start_switch(int next) { pal_ctx_start_switch(&glbl_ctx, next); }
int pallet_step(int step) { return pal_ctx_step(&glbl_ctx, step); }


#if HAVE_ORC
//#if 0
#include <orc/orc.h>
static void do_pallet_step(int pos, uint32_t * restrict active_pal, const uint8_t *restrict next, const uint8_t *restrict prev) {
	static OrcProgram *p = NULL;
	OrcExecutor _ex;
	OrcExecutor *ex = &_ex;
	static int palp, vt;

	if (p == NULL) {
		p = orc_program_new_dss(1,1,1);
#ifndef __MMX__ // some of the orc stuff used below doesn't work off x86
		palp = orc_program_add_parameter(p, 1, "palpos");
		vt = orc_program_add_parameter(p, 1, "vt");

		orc_program_add_temporary(p, 1, "nx");
		orc_program_add_temporary(p, 1, "pr");
		orc_program_append_str(p, "mulhub", "nx", "s1", "palpos");
		orc_program_append_str(p, "mulhub", "pr", "s2", "vt");
		//orc_program_append_str(p, "mulubw", "nx", "s1", "palpos");
		//orc_program_append_str(p, "mulubw", "pr", "s2", "vt");
		orc_program_append_str(p, "addb", "d1", "nx", "pr");
#else
		palp = orc_program_add_parameter(p, 2, "palpos");
		vt = orc_program_add_parameter(p, 2, "vt");
		orc_program_add_temporary(p, 2, "nx");
		orc_program_add_temporary(p, 2, "pr");

		orc_program_append_ds_str(p, "convubw", "nx", "s1");
		orc_program_append_ds_str(p, "convubw", "pr", "s2");
		orc_program_append_str(p, "mullw", "nx", "nx", "palpos");
		orc_program_append_str(p, "mullw", "pr", "pr", "vt");
		orc_program_append_str(p, "addw", "nx", "nx", "pr");
		orc_program_append_ds_str(p, "select1wb", "d1", "nx");
#endif
		orc_program_compile (p); //TODO: check return value here
	}
	orc_executor_set_program (ex, p);
	orc_executor_set_n (ex, 256*4);
	orc_executor_set_array (ex, ORC_VAR_S1, next);
	orc_executor_set_array (ex, ORC_VAR_S2, prev);
	orc_executor_set_array (ex, ORC_VAR_D1, active_pal);
	orc_executor_set_param (ex, palp, pos);
	orc_executor_set_param (ex, vt, 255-pos);

	orc_executor_run (ex);
}
#else
static void do_pallet_step(int pos, uint32_t * restrict active_pal, const uint8_t *restrict next, const uint8_t *restrict prev) {
	uint8_t *restrict d = (uint8_t *restrict)active_pal;
	for(int i=0; i<256*4; i++)
		d[i] = (uint8_t)((next[i]*pos + prev[i]*(255-pos))>>8);
}
#endif

