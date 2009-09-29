
//TODO: test w/o mmx/sse
//TODO: test 8/16 bit code paths (32-bit seems to work)

#include "common.h"
#include "pixmisc.h"
#include "mymm.h"

struct pallet_colour {
	unsigned char r, g, b;
	unsigned char pos;
};

#define NUM_PALLETS 4
static const int num_pallets = NUM_PALLETS;
static const struct pallet_colour static_pallets[NUM_PALLETS][64] = {

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
		{   0,   0, 128,  60},
		{ 255, 128,  64, 137},
		{ 255, 255, 255, 240},
		{   0,   0, 255, 255}
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
		{ 196,  23,   0,  10},
		{ 255, 255, 255,  40},
		{  32,  96,   0,  70},
		{ 115, 230,   0, 240},
		{  45, 255, 255, 255}
	}

	// END
	//{   0,   0,   0,   0},
	//{   0,   0,   0,   0}
};


static uint32_t pallets32[NUM_PALLETS][256] __attribute__((aligned(16)));

static uint32_t active_pal[257] __attribute__((aligned(16)));

uint32_t *get_active_pal(void) {
	return active_pal;
}

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

static int pallet_changing = 0;
static int palpos = 0;
static int nextpal = 0;
static int curpal = 1;

void pallet_init(int bswap) {
	for(int p=0; p < num_pallets; p++)
		expand_pallet(static_pallets[p], pallets32[p], bswap);
	memcpy(active_pal, pallets32[curpal], sizeof(uint32_t)*256);
	active_pal[256] = active_pal[255];
}

void pallet_start_switch(int next) {
	next = next % num_pallets;
	if(next<0) return;
	if(pallet_changing) return; // haven't finished the last one
	if(next == curpal) next = (next+1) % num_pallets;

	nextpal = next;
	pallet_changing = 1;
}

#ifdef HAVE_ORC
#include <orc/orc.h>
static void do_pallet_step(void) {
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
	orc_executor_set_array (ex, ORC_VAR_S1, pallets32[nextpal]);
	orc_executor_set_array (ex, ORC_VAR_S2, pallets32[curpal]);
	orc_executor_set_array (ex, ORC_VAR_D1, active_pal);
	orc_executor_set_param (ex, palp, palpos);
	orc_executor_set_param (ex, vt, 255-palpos);

	orc_executor_run (ex);
}
#elif  defined(__SSE2__)
#warning Doing sse2 Compiled program will NOT work on system without it!
#include <emmintrin.h>
//TODO: decide if it's worth maintainig this... MMX version is probably plenty fast... and
//      it's not like 1k of uint8's is all that much work or data... so is sse really helping
//      (or for that matter is mmx?)
static void do_pallet_step(void) {
	__m128i zero = _mm_setzero_si128();
	__m128i mask = _mm_set1_epi16(0x00ff);
	__m128i vt = _mm_set1_epi16(palpos); // same for all i

	const __m128i * restrict const next = (__m128i *)pallets32[nextpal];
	const __m128i * restrict const old  = (__m128i *)pallets32[curpal];
	__m128i * restrict const dest = (__m128i *)active_pal;

	for(unsigned int i = 0; i < (256*4)/sizeof(__m128i); i+=2) {
		__m128i s1 = *(next+i);
		__m128i s2 = s1;
		s1 = _mm_unpacklo_epi8(s1, zero);
		s2 = _mm_unpackhi_epi8(s2, zero);

		__m128i d1 = *(old+i);
		__m128i d2 = d1;
		d1 = _mm_unpacklo_epi8(d1, zero);
		d2 = _mm_unpackhi_epi8(d2, zero);

		s1 = _mm_mullo_epi16(s1, vt);
		vt = _mm_andnot_si128(vt, mask); // vt = 255 - vt
		d1 = _mm_mullo_epi16(d1, vt);
		s1 = _mm_add_epi16(s1, d1);
		s1 = _mm_srli_epi16(s1, 8);

		d2 = _mm_mullo_epi16(d2, vt);
		vt = _mm_andnot_si128(vt, mask); // vt = 255 - vt
		s2 = _mm_mullo_epi16(s2, vt);
		s2 = _mm_add_epi16(s2, d2);
		s2 = _mm_srli_epi16(s2, 8);

		s1 = _mm_packs_epi16(s1, s2);
		*(dest + i) = s1;

		s1 = *(next+i+1);
		s2 = s1;
		s1 = _mm_unpacklo_epi8(s1, zero);
		s2 = _mm_unpackhi_epi8(s2, zero);

		d1 = *(old+i+1);
		d2 = d1;
		d1 = _mm_unpacklo_epi8(d1, zero);
		d2 = _mm_unpackhi_epi8(d2, zero);

		s1 = _mm_mullo_epi16(s1, vt);
		vt = _mm_andnot_si128(vt, mask); // vt = 255 - vt
		d1 = _mm_mullo_epi16(d1, vt);
		s1 = _mm_add_epi16(s1, d1);
		s1 = _mm_srli_epi16(s1, 8);

		d2 = _mm_mullo_epi16(d2, vt);
		vt = _mm_andnot_si128(vt, mask); // vt = 255 - vt
		s2 = _mm_mullo_epi16(s2, vt);
		s2 = _mm_add_epi16(s2, d2);
		s2 = _mm_srli_epi16(s2, 8);

		s1 = _mm_packs_epi16(s1, s2);
		*(dest + i + 1) = s1;
	}
}
#elif defined(__MMX__)
#warning Doing mmx Compiled program will NOT work on system without it!
static void do_pallet_step(void) {
	__m64 zero = _mm_cvtsi32_si64(0ll);
	__m64 mask = (__m64)(0x00ff00ff00ff);
	__m64 vt = _mm_set1_pi16(palpos); // same for all i

	for(int i = 0; i < 256; i+=4) {
		__m64 s1 = *(__m64 *)(pallets32[nextpal]+i);
		__m64 s2 = s1;
		s1 = _mm_unpacklo_pi8(s1, zero);
		s2 = _mm_unpackhi_pi8(s2, zero);

		__m64 d1 = *(__m64 *)(pallets32[curpal]+i);
		__m64 d2 = d1;
		d1 = _mm_unpacklo_pi8(d1, zero);
		d2 = _mm_unpackhi_pi8(d2, zero);

		s1 = _mm_mullo_pi16(s1, vt);
		vt = _mm_andnot_si64(vt, mask); // vt = 255 - vt
		d1 = _mm_mullo_pi16(d1, vt);
		s1 = _mm_add_pi16(s1, d1);
		s1 = _mm_srli_pi16(s1, 8);

		d2 = _mm_mullo_pi16(d2, vt);
		vt = _mm_andnot_si64(vt, mask); // vt = 255 - vt
		s2 = _mm_mullo_pi16(s2, vt);
		s2 = _mm_add_pi16(s2, d2);
		s2 = _mm_srli_pi16(s2, 8);

		s1 = _mm_packs_pu16(s1, s2);
		*(__m64 *)(active_pal+i) = s1;

		s1 = *(__m64 *)(pallets32[nextpal]+i+2);
		s2 = s1;
		s1 = _mm_unpacklo_pi8(s1, zero);
		s2 = _mm_unpackhi_pi8(s2, zero);

		d1 = *(__m64 *)(pallets32[curpal]+i+2);
		d2 = d1;
		d1 = _mm_unpacklo_pi8(d1, zero);
		d2 = _mm_unpackhi_pi8(d2, zero);

		s1 = _mm_mullo_pi16(s1, vt);
		vt = _mm_andnot_si64(vt, mask); // vt = 255 - vt
		d1 = _mm_mullo_pi16(d1, vt);
		s1 = _mm_add_pi16(s1, d1);
		s1 = _mm_srli_pi16(s1, 8);

		d2 = _mm_mullo_pi16(d2, vt);
		vt = _mm_andnot_si64(vt, mask); // vt = 255 - vt
		s2 = _mm_mullo_pi16(s2, vt);
		s2 = _mm_add_pi16(s2, d2);
		s2 = _mm_srli_pi16(s2, 8);

		s1 = _mm_packs_pu16(s1, s2);
		*(__m64 *)(active_pal+i+2) = s1;
	}
	_mm_empty();
}
#else
static void do_pallet_step(void) {
	const uint8_t *restrict next = (uint8_t *restrict)pallets32[nextpal], *restrict prev = (uint8_t *restrict)pallets32[curpal];
	uint8_t *restrict d = (uint8_t *restrict)active_pal;
	for(int i=0; i<256*4; i++)
		d[i] = (uint8_t)(next[i]*palpos + prev[i]*(255-palpos));
}
#endif

int get_pallet_changing(void) { return pallet_changing; }
void pallet_step(int step) {
	if(!pallet_changing) return;
	palpos += step;
	if(palpos >=256) {
		pallet_changing = palpos = 0;
		curpal = nextpal;
		memcpy(active_pal, pallets32[nextpal], 256);
		return;
	} else
		do_pallet_step();

	active_pal[256] = active_pal[255];
}

// pallet must have 257 entries (for easier interpolation on 16 bit indicies)
// output pix = (pallet[in/256]*(in%256) + pallet[in/256+1]*(255-in%256])/256
// for each colour component

// the above is not done in 16 bit modes they just do out = pallet[in/256] (and conver the pallet)

//TODO: load pallets from files of some sort

#ifdef __MMX__
static void pallet_blit32(uint32_t *restrict dest, unsigned int dst_stride, const uint16_t *restrict src, unsigned int src_stride, unsigned int w, unsigned int h, const uint32_t *restrict pal)
{
	const __m64 zero = _mm_cvtsi32_si64(0ll);
	const __m64 mask = (__m64)(0x00ff00ff00ff);
	dst_stride /= 4;

	for(unsigned int y = 0; y < h; y++) {
		for(unsigned int x = 0; x < w; x+=4) {
			int v = src[y*src_stride + x];
			__builtin_prefetch(src + y*src_stride + x + 4, 0, 0);
			__builtin_prefetch(dest + y*dst_stride + x + 4, 1, 0);

			__m64 col1 = *(__m64 *)(pal+(v/256));
			__m64 col2 = col1;
			col1 = _mm_unpacklo_pi8(col1, zero);
    		col2 = _mm_unpackhi_pi8(col2, zero);

		    //col1 = (col2*v + col1*(0xff-v))/256;
			__m64 vt = _mm_set1_pi16(v);
			vt = _mm_and_si64(vt, mask);
			col2 = _mm_mullo_pi16(col2, vt);
			vt = _mm_andnot_si64(vt, mask); // vt = 255 - vt
			col1 = _mm_mullo_pi16(col1, vt);
    		col1 = _mm_add_pi16(col1, col2);
    		col1 = _mm_srli_pi16(col1, 8);

			__m64 tmp = col1;

			v = src[y*src_stride + x + 1];
			col1 = *(__m64 *)(pal+(v/256));
			col2 = col1;
			col1 = _mm_unpacklo_pi8(col1, zero);
    		col2 = _mm_unpackhi_pi8(col2, zero);

			vt = _mm_set1_pi16(v);
			vt = _mm_and_si64(vt, mask);
			col2 = _mm_mullo_pi16(col2, vt);
			vt = _mm_andnot_si64(vt, mask); // vt = 255 - vt
			col1 = _mm_mullo_pi16(col1, vt);
    		col1 = _mm_add_pi16(col1, col2);
    		col1 = _mm_srli_pi16(col1, 8);

			tmp = _mm_packs_pu16(tmp, col1);
			//_mm_stream_pi((__m64 *)(dest + y*dst_stride + x), tmp);
			*(__m64 *)(dest + y*dst_stride + x) = tmp;

			v = src[y*src_stride + x + 2];
			col1 = *(__m64 *)(pal+(v/256));
			col2 = col1;
			col1 = _mm_unpacklo_pi8(col1, zero);
    		col2 = _mm_unpackhi_pi8(col2, zero);

			vt = _mm_set1_pi16(v);
			vt = _mm_and_si64(vt, mask);
			col2 = _mm_mullo_pi16(col2, vt);
			vt = _mm_andnot_si64(vt, mask); // vt = 255 - vt
			col1 = _mm_mullo_pi16(col1, vt);
    		col1 = _mm_add_pi16(col1, col2);
    		col1 = _mm_srli_pi16(col1, 8);

			tmp = col1;

			v = src[y*src_stride + x + 3];
			col1 = *(__m64 *)(pal+(v/256));
			col2 = col1;
			col1 = _mm_unpacklo_pi8(col1, zero);
    		col2 = _mm_unpackhi_pi8(col2, zero);

			vt = _mm_set1_pi16(v);
			vt = _mm_and_si64(vt, mask);
			col2 = _mm_mullo_pi16(col2, vt);
			vt = _mm_andnot_si64(vt, mask); // vt = 255 - vt
			col1 = _mm_mullo_pi16(col1, vt);
    		col1 = _mm_add_pi16(col1, col2);
    		col1 = _mm_srli_pi16(col1, 8);

			tmp = _mm_packs_pu16(tmp, col1);
			//_mm_stream_pi((__m64 *)(dest + y*dst_stride + x+2), tmp);
			*(__m64 *)(dest + y*dst_stride + x + 2) = tmp;
		}
	}
}
#else
void pallet_blit32(uint8_t *restrict dest, unsigned int dst_stride, const uint16_t *restrict src, unsigned int src_stride, unsigned int w, unsigned int h, const uint32_t *restrict pal)
{
	for(unsigned int y = 0; y < h; y++)
		for(unsigned int x = 0; x < w; x++)
			*(uint32_t *)(dest + y*dst_stride + x*4) = pal[src[y*src_stride + x]>>8];
}
#endif

// needs _mm_shuffle_pi16 no other sse/3dnow stuff used
#if defined(__SSE__) || defined(__3dNOW__)
static void pallet_blit565(uint8_t *restrict dest, unsigned int dst_stride, const uint16_t *pbattr src, unsigned int src_stride, unsigned int w, unsigned int h, uint32_t *restrict pal)
{
	for(unsigned int y = 0; y < h; y++) {
		for(unsigned int x = 0; x < w; x+=4) {
			__builtin_prefetch(src + y*src_stride + x + 4, 0, 0);
			__builtin_prefetch(dest + y*dst_stride + x + 4, 1, 0);

			__m64 r1, r2, g1, g2, b1, b2, c;
			int v = src[y*src_stride + x];
			c  = _mm_cvtsi32_si64(pal[v/256]);
			r1 = _mm_shuffle_pi16(c, 0xfd);
			g1 = _mm_shuffle_pi16(c, 0xfc);
			c  = _mm_slli_pi32(c, 8);
			b1 = _mm_shuffle_pi16(c, 0xfc);

			v=src[y*src_stride + x+1];
			c  = _mm_cvtsi32_si64(pal[v/256]);
			r2 = _mm_shuffle_pi16(c, 0xf7);
			g2 = _mm_shuffle_pi16(c, 0xf3);
			c  = _mm_slli_pi32(c, 8);
			b2 = _mm_shuffle_pi16(c, 0xf3);
			r1 = _mm_or_si64(r1, r2);
			g1 = _mm_or_si64(g1, g2);
			b1 = _mm_or_si64(b1, b2);

			v=src[y*src_stride + x+2];
			c  = _mm_cvtsi32_si64(pal[v/256]);
			r2 = _mm_shuffle_pi16(c, 0xdf);
			g2 = _mm_shuffle_pi16(c, 0xcf);
			c  = _mm_slli_pi32(c, 8);
			b2 = _mm_shuffle_pi16(c, 0xcf);
			r1 = _mm_or_si64(r1, r2);
			g1 = _mm_or_si64(g1, g2);
			b1 = _mm_or_si64(b1, b2);

			v=src[y*src_stride + x+3];
			c  = _mm_cvtsi32_si64(pal[v/256]);
			r2 = _mm_shuffle_pi16(c, 0x7f);
			g2 = _mm_shuffle_pi16(c, 0x3f);
			c  = _mm_slli_pi32(c, 8);
			b2 = _mm_shuffle_pi16(c, 0x3f);
			r1 = _mm_or_si64(r1, r2);
			g1 = _mm_or_si64(g1, g2);
			b1 = _mm_or_si64(b1, b2);

			r1 = _mm_srli_pi16(r1, 3);
			g1 = _mm_srli_pi16(g1, 10);
			b1 = _mm_srli_pi16(b1, 11);

			r1 = _mm_slli_pi16(r1, 11);
			g1 = _mm_slli_pi16(g1, 5);

			r1 = _mm_or_si64(r1, g1);
			r1 = _mm_or_si64(r1, b1);

			*(__m64 *)(dest + y*dst_stride + x*2) = r1;
		}
	}
}

static void pallet_blit555(uint8_t *restrict dest, unsigned int dst_stride, const uint16_t *pbattr src, unsigned int src_stride, unsigned int w, unsigned int h, uint32_t *restrict pal)
{
	for(unsigned int y = 0; y < h; y++) {
		for(unsigned int x = 0; x < w; x+=4) {
			__builtin_prefetch(src + y*src_stride + x + 4, 0, 0);
			__builtin_prefetch(dest + y*dst_stride + x + 4, 1, 0);
			int v;
			__m64 r1, r2, g1, g2, b1, b2, c;

			v  = src[y*src_stride + x];
			c  = _mm_cvtsi32_si64(pal[v/256]);
			r1 = _mm_shuffle_pi16(c, 0xfd);
			g1 = _mm_shuffle_pi16(c, 0xfc);
			c  = _mm_slli_pi32(c, 8);
			b1 = _mm_shuffle_pi16(c, 0xfc);

			v  = src[y*src_stride + x+1];
			c  = _mm_cvtsi32_si64(pal[v/256]);
			r2 = _mm_shuffle_pi16(c, 0xf7);
			g2 = _mm_shuffle_pi16(c, 0xf3);
			c  = _mm_slli_pi32(c, 8);
			b2 = _mm_shuffle_pi16(c, 0xf3);
			r1 = _mm_or_si64(r1, r2);
			g1 = _mm_or_si64(g1, g2);
			b1 = _mm_or_si64(b1, b2);

			v  = src[y*src_stride + x+2];
			c  = _mm_cvtsi32_si64(pal[v/256]);
			r2 = _mm_shuffle_pi16(c, 0xdf);
			g2 = _mm_shuffle_pi16(c, 0xcf);
			c  = _mm_slli_pi32(c, 8);
			b2 = _mm_shuffle_pi16(c, 0xcf);
			r1 = _mm_or_si64(r1, r2);
			g1 = _mm_or_si64(g1, g2);
			b1 = _mm_or_si64(b1, b2);

			v  = src[y*src_stride + x+3];
			c  = _mm_cvtsi32_si64(pal[v/256]);
			r2 = _mm_shuffle_pi16(c, 0x7f);
			g2 = _mm_shuffle_pi16(c, 0x3f);
			c  = _mm_slli_pi32(c, 8);
			b2 = _mm_shuffle_pi16(c, 0x3f);
			r1 = _mm_or_si64(r1, r2);
			g1 = _mm_or_si64(g1, g2);
			b1 = _mm_or_si64(b1, b2);

			r1 = _mm_srli_pi16(r1, 3);
			g1 = _mm_srli_pi16(g1, 11);
			b1 = _mm_srli_pi16(b1, 11);

			r1 = _mm_slli_pi16(r1, 10);
			g1 = _mm_slli_pi16(g1, 5);

			r1 = _mm_or_si64(r1, g1);
			r1 = _mm_or_si64(r1, b1);

			*(__m64 *)(dest + y*dst_stride + x*2) = r1;
		}
	}
}
#else //TODO: test these
#ifdef __MMX__
#warning no mmx for 16-bit modes (needs extras added in 3dnow or SSE)!
#endif
void pallet_blit565(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal)
{
	for(int y = 0; y < h; y++) {
		for(int x = 0; x < w; x++) {
			uint32_t cl = pal[src[y*src_stride + x]>>8];
			uint16_t px = (cl>>3)&0x1f;
			px = px | (((cl>>10)&0x3f)<<5);
			px = px | (((cl>>19)&0x1f)<<11);
			*(uint16_t *)(dest + y*dst_stride + x*4) = px;
		}
	}
}
void pallet_blit555(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h,
					const uint32_t *restrict pal)
{
	for(int y = 0; y < h; y++) {
		for(int x = 0; x < w; x++) {
			uint32_t cl = pal[src[y*src_stride + x]>>8];
			uint16_t px = (cl>>3)&0x1f;
			px = px | (((cl>>11)&0x1f)<<5);
			px = px | (((cl>>19)&0x1f)<<10);
			*(uint16_t *)(dest + y*dst_stride + x*4) = px;
		}
	}
}
#endif

#if defined(__MMX__)
static void pallet_blit8(uint8_t* restrict dest, unsigned int dst_stride,
		const uint16_t *pbattr src, unsigned int src_stride, unsigned int w, unsigned int h)
{
	for(unsigned int y = 0; y < h; y++) {
		for(unsigned int x = 0; x < w; x+=16) {
			__builtin_prefetch(src + y*src_stride + x + 16, 0, 0);
			__builtin_prefetch(dest + y*dst_stride + x + 16, 1, 0);

			__m64 p1, p2;

			p1 = *(__m64 *)(src + y*src_stride + x);
			p1  = _mm_srli_pi16(p1, 8);
			p2 = *(__m64 *)(src + y*src_stride + x+4);
			p2  = _mm_srli_pi16(p2, 8);

			p1 = _mm_packs_pu16(p1, p2);
			*(__m64 *)(dest + y*dst_stride + x) = p1;

			p1 = *(__m64 *)(src + y*src_stride + x+8);
			p1  = _mm_srli_pi16(p1, 8);
			p2 = *(__m64 *)(src + y*src_stride + x+12);
			p2  = _mm_srli_pi16(p2, 8);

			p1 = _mm_packs_pu16(p1, p2);
			*(__m64 *)(dest + y*dst_stride + x+ 8) = p1;
		}
	}
}
#else
void pallet_blit8(uint8_t * restrict dest, unsigned int dst_stride,
					const uint16_t *restrict src, unsigned int src_stride,
					unsigned int w, unsigned int h)
{
	for(int y = 0; y < h; y++)
		for(int x = 0; x < w; x++)
			*(dest + y*dst_stride + x*4) = src[y*src_stride + x]>>8;
}
#endif

void pallet_blit_Pixbuf(Pixbuf* dst, const uint16_t* restrict src, int w, int h)
{
	const int src_stride = w;
	w = IMIN(w, dst->w);
	h = IMIN(h, dst->h);

	void *pal = active_pal;

	if(dst->bpp == 32) pallet_blit32(dst->data, dst->pitch, src, src_stride, w, h, pal);
	else if(dst->bpp == 16) pallet_blit565(dst->data, dst->pitch, src, src_stride, w, h, pal);
	else if(dst->bpp == 15) pallet_blit555(dst->data, dst->pitch, src, src_stride, w, h, pal);
	else if(dst->bpp == 8) { // need to set surface's pallet
		pallet_blit8(dst->data, dst->pitch, src, src_stride, w, h);
	}
#ifdef __MMX__
	_mm_empty();
#endif
}

#ifdef USE_SDL
#include <SDL.h>
void pallet_blit_SDL(SDL_Surface *dst, const uint16_t* restrict src, int w, int h)
{
	const int src_stride = w;
	w = IMIN(w, dst->w);
	h = IMIN(h, dst->h);

	void *pal = active_pal;

	if((SDL_MUSTLOCK(dst) && SDL_LockSurface(dst) < 0) || w < 0 || h < 0) return;
	if(dst->format->BitsPerPixel == 32) pallet_blit32(dst->pixels, dst->pitch, src, src_stride, w, h, pal);
	else if(dst->format->BitsPerPixel == 16) pallet_blit565(dst->pixels, dst->pitch, src, src_stride, w, h, pal);
	else if(dst->format->BitsPerPixel == 15) pallet_blit555(dst->pixels, dst->pitch, src, src_stride, w, h, pal);
	else if(dst->format->BitsPerPixel == 8) { // need to set surface's pallet
		pallet_blit8(dst->pixels, dst->pitch, src, src_stride, w, h);
		SDL_SetColors(dst, pal, 0, 256);
	}
#ifdef __MMX__
	_mm_empty();
#endif
	if(SDL_MUSTLOCK(dst)) SDL_UnlockSurface(dst);
}
#endif

#ifdef HAVE_DIRECTFB
#include <directfb.h>
void pallet_blit_DFB(IDirectFBSurface *dst, uint16_t * restrict src, int w, int h, uint32_t *restrict pal)
{
	const int src_stride = w;
	DFBSurfacePixelFormat dst_format;
	void *dst_pixels = NULL;
	int dst_pitch, dst_w, dst_h;

	dst->GetSize(dst, &dst_w, &dst_h);
	dst->GetPixelFormat(dst, &dst_format);

	w = IMIN(w, dst_w);
	h = IMIN(h, dst_h);
	dst->Lock(dst, DSLF_WRITE, &dst_pixels, &dst_pitch); // TODO: error check
	if(dst_format == DSPF_RGB32) pallet_blit32(dst_pixels, dst_pitch, src, src_stride, w, h, pal);
	else if(dst_format == DSPF_RGB16) pallet_blit565(dst_pixels, dst_pitch, src, src_stride, w, h, pal);
	else if(dst_format == DSPF_RGB555) pallet_blit555(dst_pixels, dst_pitch, src, src_stride, w, h, pal);
#ifdef __MMX__
	_mm_empty();
#endif
	dst->Unlock(dst);
}
#endif

