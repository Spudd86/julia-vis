#include "config.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include "mymm.h"

#include "pixmisc.h"
#include "common.h"

struct pallet_colour {
	unsigned char r, g, b;
	unsigned char pos;
};

static const int num_pallets = 4;
static const struct pallet_colour static_pallets[4][64] = {
    
	{	//  r    g    b  pos
		{   0,  11, 138,   0},
		{ 255, 111, 255, 104},
		{ 111, 111, 255, 125},
		{ 111, 230, 255, 152},
		{ 255, 216, 111, 185},
		{ 255, 111, 111, 211},
		{ 255, 255, 255, 255}
	},

	{	//  r    g    b  pos
		{   0,   0,   0,   0},
		{   0,   0, 128,  60},
		{ 255, 128,  64, 137},
		{ 255, 255, 255, 230},
		{   0,   0, 255, 255}
	},

	{	//  r    g    b  pos
		{   0,   0,   0,   0},
		{ 255,   0, 128,  52},
		{ 196,   0,   0,  86},
		{   0,   0,   0, 175},
		{ 255,   0, 128, 231},
		{ 255, 255, 255, 255}
	},

	{	//  r    g    b  pos
		{   0,   0,   0,   0},
		{   0,   0,   0,  29},
		{ 115, 230,   0, 195},
		{  45, 255, 255, 255}
	}

	// END
	//{   0,   0,   0,   0},
	//{   0,   0,   0,   0}
};


//TODO: make sure these have good alignment
//~ static uint16_t pallets555[4][256+16] __attribute__((aligned)); // keep aligned
//~ static uint16_t pallets565[4][256+16] __attribute__((aligned));
static uint32_t pallets32[4][256+8] __attribute__((aligned));

static uint32_t active_pal[257] __attribute__((aligned)); 

void pallet_init(void) {
	for(int p=0; p < num_pallets; p++) {
		
		struct pallet_colour *curpal = static_pallets[p];
		int j = 0;
		do {
			j++;
			
			for(int i = curpal[j-1].pos; i < curpal[j].pos; i++) {
				int r = (curpal[j-1].r*(curpal[j].pos-i) + curpal[j].r*(i-curpal[j-1].pos))/(curpal[j].pos-curpal[j-1].pos);
				int g = (curpal[j-1].g*(curpal[j].pos-i) + curpal[j].g*(i-curpal[j-1].pos))/(curpal[j].pos-curpal[j-1].pos);
				int b = (curpal[j-1].b*(curpal[j].pos-i) + curpal[j].b*(i-curpal[j-1].pos))/(curpal[j].pos-curpal[j-1].pos);
				
				//~ pallets555[p][i] = ((r&0xfa)<<7)|((g&0xfa)<<2)|(b>>3); // not woring right, plus doesn't seem to offer any speedup
				//~ pallets565[p][i] = ((r&0xfa)<<8)|((g&0xfc)<<3)|(b>>3);
				pallets32[p][i] = (r<<16)|(g<<8)|b;
			}
			
		} while(static_pallets[p][j].pos != 255);
		//~ pallets555[p][256] = pallets555[p][255];
		//~ pallets565[p][256] = pallets565[p][255];
		pallets32[p][256]  = pallets32[p][255];
	}
	
	memcpy(active_pal, pallets32[1], sizeof(uint32_t)*257);
}

static int pallet_changing = 0;
static int palpos = 0;
static int nextpal = 0;
static int curpal = 1;

static void *pallet_getpal() {
	return active_pal;
}

void pallet_step(int step) {
	if(!pallet_changing) return;
	palpos += step;
	if(palpos >=256) {
		pallet_changing = palpos = 0;
		curpal = nextpal;
		memcpy(active_pal, pallets32[nextpal], sizeof(uint32_t)*257);
		return;
	}
	
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
	active_pal[256] = active_pal[255];
	_mm_empty();
	
}

void pallet_start_switch(int next) {
	if(pallet_changing || next == curpal) return; // haven't finished the last one
	
	if(next<0 || next>=num_pallets) return;
	nextpal = next;
	pallet_changing = 1;
}

// pallet must have 257 entries (for easier interpolation on 16 bit indicies)
// output pix = (pallet[in/256]*(in%256) + pallet[in/256+1]*(255-in%256])/256
// for each colour component

// the above is not done in 16 bit modes they just do out = pallet[in/256] (and conver the pallet)

//TODO: load pallets from files of some sort
//		pre-convert pallets to 565/555 if we're in a 16 bit mode (since we don't iterpolate there anyway)

static void pallet_blit_SDL32(uint32_t  * restrict dest, unsigned int dst_stride, uint16_t *restrict src, unsigned int src_stride, int w, int h, uint32_t *restrict pal)
{
	__m64 zero = _mm_cvtsi32_si64(0ll);
	__m64 mask = (__m64)(0x00ff00ff00ff);
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
			
		    //col1 = (col1*v + col2*(0xff-v))/256;
			__m64 vt = _mm_set1_pi16(v);
			vt = _mm_and_si64(vt, mask);
    		col1 = _mm_mullo_pi16(col1, vt);
			vt = _mm_andnot_si64(vt, mask); // vt = 255 - vt
    		col2 = _mm_mullo_pi16(col2, vt);
    		col1 = _mm_add_pi16(col1, col2);
    		col1 = _mm_srli_pi16(col1, 8);
    		
			__m64 tmp = col1;
			
			v = src[y*src_stride + x + 1];
			col1 = *(__m64 *)(pal+(v/256));
			col2 = col1;
			col1 = _mm_unpacklo_pi8(col1, zero);
    		col2 = _mm_unpackhi_pi8(col2, zero);
			
			// re-use vt from before, just do cols in opposite order
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
    		col1 = _mm_mullo_pi16(col1, vt);
			vt = _mm_andnot_si64(vt, mask); // vt = 255 - vt
    		col2 = _mm_mullo_pi16(col2, vt);
    		col1 = _mm_add_pi16(col1, col2);
    		col1 = _mm_srli_pi16(col1, 8);
    		
			tmp = col1;
			
			v = src[y*src_stride + x + 3];
			col1 = *(__m64 *)(pal+(v/256));
			col2 = col1;
			col1 = _mm_unpacklo_pi8(col1, zero);
    		col2 = _mm_unpackhi_pi8(col2, zero);
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

static void pallet_blit_SDL16(uint8_t  * restrict dest, unsigned int dst_stride, uint16_t *restrict src, unsigned int src_stride, int w, int h, uint16_t *restrict pal)
{
	for(unsigned int y = 0; y < h; y++) {
		for(unsigned int x = 0; x < w; x+=4) {
			__builtin_prefetch(src + y*src_stride + x + 4, 0, 0);
			__builtin_prefetch(dest + y*dst_stride + x + 4, 1, 0);
			
			int v = src[y*src_stride + x];
			*(uint16_t *)(dest + y*dst_stride + x*2) = pal[v/256];
			v = src[y*src_stride + x+1];
			*(uint16_t *)(dest + y*dst_stride + (x+1)*2) = pal[v/256];
			v = src[y*src_stride + x+2];
			*(uint16_t *)(dest + y*dst_stride + (x+2)*2) = pal[v/256];
			v = src[y*src_stride + x+3];
			*(uint16_t *)(dest + y*dst_stride + (x+3)*2) = pal[v/256];
		}
	}
}

static void pallet_blit_SDL565(uint8_t  * restrict dest, unsigned int dst_stride, uint16_t *restrict src, unsigned int src_stride, int w, int h, uint32_t *restrict pal)
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

static void pallet_blit_SDL555(uint8_t  * restrict dest, unsigned int dst_stride, uint16_t *restrict src, unsigned int src_stride, int w, int h, uint32_t *restrict pal)
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

#ifdef USE_SDL

#include <SDL.h>

void pallet_blit_SDL(SDL_Surface *dst, uint16_t * restrict src, int w, int h, int pi)
{
	const int src_stride = w;
	w = IMIN(w, dst->w);
	h = IMIN(h, dst->h);
	
	void *pal = pallet_getpal(pi, 32);//dst->format->BitsPerPixel);
	
	if((SDL_MUSTLOCK(dst) && SDL_LockSurface(dst) < 0) || w < 0 || h < 0) return;
	if(dst->format->BitsPerPixel == 32) pallet_blit_SDL32(dst->pixels, dst->pitch, src, src_stride, w, h, pal);
	//~ else if(dst->format->BitsPerPixel == 16 || dst->format->BitsPerPixel == 15)
		//~ pallet_blit_SDL16(dst->pixels, dst->pitch, src, src_stride, w, h, pal);
	else if(dst->format->BitsPerPixel == 16) pallet_blit_SDL565(dst->pixels, dst->pitch, src, src_stride, w, h, pal);
	else if(dst->format->BitsPerPixel == 15) pallet_blit_SDL555(dst->pixels, dst->pitch, src, src_stride, w, h, pal);
	_mm_empty();
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
	if(dst_format == DSPF_RGB32) pallet_blit_SDL32(dst_pixels, dst_pitch, src, src_stride, w, h, pal);
	else if(dst_format == DSPF_RGB16) pallet_blit_SDL565(dst_pixels, dst_pitch, src, src_stride, w, h, pal);
	else if(dst_format == DSPF_RGB555) pallet_blit_SDL555(dst_pixels, dst_pitch, src, src_stride, w, h, pal);
	_mm_empty();
	dst->Unlock(dst);
}
#endif

// stride is for dest
//~ void pallet_blit(void *dest, int dst_stride, uint16_t *src, int w, int h, uint32_t *pal)
//~ {
	//~ for(int y = 0; y < h; y++) 
		//~ for(int x = 0; x < w; x++) 
			//~ *(int32_t *)(dest + y*dst_stride + x*4) = pal[src[y*w + x]>>8];
//~ }

//TODO load/generate pallets

// this one is sort of what I'd like to do but there is no 8 bit mmx mul :(
//~ void pallet_blitSDL(SDL_Surface *dst, uint16_t *src, int w, int h, uint32_t *pal)
//~ {
	//~ if(SDL_MUSTLOCK(dst) && SDL_LockSurface(dst) < 0) return;

	//~ int dst_stride = dst->pitch/4;
	//~ uint32_t *dest = dst->pixels;
	//~ w = IMIN(w, dst->w);
	//~ h = IMIN(h, dst->h);
	
	//~ for(int y = 0; y < h; y++) {
		//~ for(int x = 0; x < w; x+=2) {
			//~ int v = src[y*src_stride + x];
			//~ int v2 = src[y*src_stride + x+1];
			//~ __m64 col1, col2;
			//~ col1 = col2 = (__m64)*(uint64_t *)(pal+(v>>8));
			//~ __m64 tmp   = (__m64)*(uint64_t *)(pal+(v2>>8));
			//~ col1 = _mm_unpacklo_pi32(col1, tmp);
			//~ col2 = _mm_unpackhi_pi32(col2, tmp);
			//~ tmp = _mm_unpacklo_pi32(_mm_set1_pi8(v & 0xff), _mm_set1_pi8(v2 & 0xff));
			//~ col1 = _mm_mulhi_pi8(col1, tmp);
			//~ tmp = _mm_andnot_si64(tmp, mask)
			//~ col2 = _mm_mulhi_pi8(col2, tmp);
    		//~ col1 = _mm_add_pi8(col1, col2);
			
			//~ _mm_stream_pi((__m64 *)(dest + y*dst_stride + x), tmp);
		//~ }
	//~ }
	//~ _mm_empty();
	//~ if(SDL_MUSTLOCK(dst)) SDL_UnlockSurface(dst);
	//~ SDL_UpdateRect(dst, 0, 0, w, h);
//~ }


