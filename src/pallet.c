#include <mmintrin.h>
#include <stdint.h>

// pallet must have 257 entries
static inline uint32_t lookup(uint16_t v, uint32_t *pal) 
{
	__m64 col1 = _mm_unpacklo_pi8(_mm_cvtsi32_si64(pal[(v>>8)]), _mm_cvtsi32_si64(0));
	__m64 col2 = _mm_unpacklo_pi8(_mm_cvtsi32_si64(pal[(v>>8)+1]), _mm_cvtsi32_si64(0));

	__m64 vt = _mm_set1_pi16(v & 0xff);
	__m64 t  = _mm_set1_pi16(0xff);
	
	//col1 = (col1*v + col2*(0xff-v))/256;
	t = _mm_sub_pi16(t, vt);
	col1 = _mm_mullo_pi16(col1, vt);
	col2 = _mm_mullo_pi16(col2, t);
	col1 = _mm_add_pi16(col1, col2);
	col1 = _mm_srli_pi16(col1, 8);
	col1 = _mm_packs_pu16(col1, col1);
	
	return _mm_cvtsi64_si32(col1);
}

void pallet_blit(uint32_t *dest, uint16_t *src, int w, int h, uint32_t *pal)
{
	for(int y = 0; y < h; y++) {
		for(int x = 0; x < w; x++) {
			uint16_t v = src[y*w + x];

			__m64 col1 = _mm_unpacklo_pi8(_mm_cvtsi32_si64(pal[(v>>8)]), _mm_cvtsi32_si64(0));
    		__m64 col2 = _mm_unpacklo_pi8(_mm_cvtsi32_si64(pal[(v>>8)+1]), _mm_cvtsi32_si64(0));

    		__m64 vt = _mm_set1_pi16(v & 0xff);
    		__m64 t  = _mm_set1_pi16(0xff);

		    //col1 = (col1*v + col2*(0xff-v))/256;
    		t = _mm_sub_pi16(t, vt);
    		col1 = _mm_mullo_pi16(col1, vt);
    		col2 = _mm_mullo_pi16(col2, t);
    		col1 = _mm_add_pi16(col1, col2);
    		col1 = _mm_srli_pi16(col1, 8);
    		col1 = _mm_packs_pu16(col1, col1);

			dest[y*w + x] = _mm_cvtsi64_si32(col1);
		}
	}
	_mm_empty();
}

//TODO load/generate pallets
