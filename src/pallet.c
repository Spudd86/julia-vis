
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

static void do_pallet_step(int pos, uint32_t * restrict active_pal, const uint8_t *restrict next, const uint8_t *restrict prev);

static int pallet_changing = 0;
static int palpos = 0;
static int nextpal = 0;
static int curpal = 0;

static uint32_t active_pal[257] __attribute__((aligned(16)));


void pallet_init(int bswap) {
	for(int p=0; p < num_pallets; p++)
		expand_pallet(static_pallets[p], pallets32[p], bswap);
	memcpy(active_pal, pallets32[curpal], sizeof(uint32_t)*256);
	active_pal[256] = active_pal[255];
}

const uint32_t *get_active_pal(void) { return active_pal; }
int get_pallet_changing(void) { return pallet_changing; }

void pallet_start_switch(int next) {
	next = next % num_pallets;
	if(next<0) return;
	if(pallet_changing) return; // haven't finished the last one
	if(next == curpal) next = (next+1) % num_pallets;

//	nextpal = 4;
	nextpal = next;
	pallet_changing = 1;
}

int pallet_step(int step) {
	if(!pallet_changing) return 0;
	palpos += step;
	if(palpos >=256) {
		pallet_changing = palpos = 0;
		curpal = nextpal;
		memcpy(active_pal, pallets32[nextpal], 256);
	} else
		do_pallet_step(palpos, active_pal, (uint8_t *restrict)pallets32[nextpal], (uint8_t *restrict)pallets32[curpal]);

	active_pal[256] = active_pal[255];
	return 1;
}

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

