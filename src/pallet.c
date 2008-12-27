#include <stdint.h>
#include <cairo.h>
#include <glib.h>

#include <mmintrin.h>
#include <xmmintrin.h>

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
// stride is for dest
void pallet_blit(void *dest, int dst_stride, uint16_t *src, int w, int h, uint32_t *pal)
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

			*(int32_t *)(dest + y*dst_stride + x*4) = _mm_cvtsi64_si32(col1);
		}
	}
	_mm_empty();
}

void pallet_blit_cairo(cairo_surface_t *dst, uint16_t *src, int w, int h, uint32_t *pal)
{
	int src_stride = w;
	int dst_stride = cairo_image_surface_get_stride(dst);
	guchar *dest = cairo_image_surface_get_data(dst);
	w = MIN(w, cairo_image_surface_get_width(dst));
	h = MIN(h, cairo_image_surface_get_height(dst));
	
	for(int y = 0; y < h; y++) {
		for(int x = 0; x < w; x++) {
			int v = src[y*src_stride + x];

			__m64 col1 = _mm_unpacklo_pi8(_mm_cvtsi32_si64(pal[(v>>8)]), _mm_cvtsi32_si64(0));
    		__m64 col2 = _mm_unpacklo_pi8(_mm_cvtsi32_si64(pal[(v>>8)+1]), _mm_cvtsi32_si64(0));

    		__m64 vt = _mm_set1_pi16(v & 0xff);
			__m64  t = _mm_set1_pi16(0xff-(v&0xff));

		    //col1 = (col1*v + col2*(0xff-v))/256;
    		col1 = _mm_mullo_pi16(col1, vt);
    		col2 = _mm_mullo_pi16(col2, t);
    		col1 = _mm_add_pi16(col1, col2);
    		col1 = _mm_srli_pi16(col1, 8);
    		col1 = _mm_packs_pu16(col1, col1);

			*(int32_t *)(dest + y*dst_stride + x*4) = _mm_cvtsi64_si32(col1);
		}
	}
	_mm_empty();
}

void pallet_blit_cairo_unroll(cairo_surface_t *dst, uint16_t *src, int w, int h, uint32_t *pal)
{
	int src_stride = w;
	int dst_stride = cairo_image_surface_get_stride(dst);
	guchar *dest = cairo_image_surface_get_data(dst);
	w = MIN(w, cairo_image_surface_get_width(dst));
	h = MIN(h, cairo_image_surface_get_height(dst));
	
	//static const __m64 z = (__m64)0ll;
	
	for(int y = 0; y < h; y++) {
		for(int x = 0; x < w; x+=2) 
		{
			int v = *(src++);
			int v2 = *(src++);
			__m64 tmp1 = (__m64)*(gint64 *)(pal+(v>>8));
			__m64 tmp2 = (__m64)*(gint64 *)(pal+(v2>>8));
			__m64 col1 = _mm_unpacklo_pi32(tmp1, tmp2);
			__m64 col2 = _mm_unpackhi_pi32(tmp1, tmp2);
			
			tmp1 = _mm_unpacklo_pi32(_mm_set1_pi8(v & 0xff), _mm_set1_pi8(v2 & 0xff));
			tmp2 = _mm_sub_pi8(_mm_set1_pi8(0xff), tmp1);
		    //col1 = (col1*v + col2*(0xff-v))/256;
    		col1 = _mm_mulhi_pi8(col1, tmp1);
    		col2 = _mm_mulhi_pi8(col2, tmp2);
    		col1 = _mm_add_pi8(col1, col2);
			
			_mm_stream_pi((__m64 *)(dest + y*dst_stride + x*4), tmp);
		}
	}
	_mm_empty();
}

//~ void pallet_blit_cairo_unroll(cairo_surface_t *dst, uint16_t *src, int w, int h, uint32_t *pal)
//~ {
	//~ int src_stride = w;
	//~ int dst_stride = cairo_image_surface_get_stride(dst);
	//~ guchar *dest = cairo_image_surface_get_data(dst);
	//~ w = MIN(w, cairo_image_surface_get_width(dst));
	//~ h = MIN(h, cairo_image_surface_get_height(dst));
	
	//~ //static const __m64 z = (__m64)0ll;
	
	//~ for(int y = 0; y < h; y++) {
		//~ for(int x = 0; x < w; x+=2) 
		//~ {
			//~ int v = *(src++);
			//~ __m64 col1 = (__m64)*(gint64 *)(pal+(v>>8));
			//~ __m64 col2 = col1;
			//~ col1 = _mm_unpacklo_pi8(col1, _mm_cvtsi32_si64(0));
    		//~ col2 = _mm_unpackhi_pi8(col2, _mm_cvtsi32_si64(0));

		    //~ //col1 = (col1*v + col2*(0xff-v))/256;
    		//~ col1 = _mm_mullo_pi16(col1, _mm_set1_pi16(v & 0xff));
    		//~ col2 = _mm_mullo_pi16(col2, _mm_set1_pi16(0xff-(v&0xff)));
    		//~ col1 = _mm_add_pi16(col1, col2);
    		//~ col1 = _mm_srli_pi16(col1, 8);
    		//~ col1 = _mm_packs_pu16(col1, col1);
			
			//~ __m64 tmp = col1;
			
			//~ v = *(src++);
			//~ col1 = (__m64)*(gint64 *)(pal+(v>>8));
			//~ col2 = col1;
			//~ col1 = _mm_unpacklo_pi8(col1, _mm_cvtsi32_si64(0));
    		//~ col2 = _mm_unpackhi_pi8(col2, _mm_cvtsi32_si64(0));

		    //~ //col1 = (col1*v + col2*(0xff-v))/256;
    		//~ col1 = _mm_mullo_pi16(col1, _mm_set1_pi16(v & 0xff));
    		//~ col2 = _mm_mullo_pi16(col2, _mm_set1_pi16(0xff-(v&0xff)));
    		//~ col1 = _mm_add_pi16(col1, col2);
    		//~ col1 = _mm_srli_pi16(col1, 8);
    		//~ col1 = _mm_packs_pu16(col1, col1);
			
			//~ tmp = _mm_unpacklo_pi32(tmp, col1);
			//~ _mm_stream_pi((__m64 *)(dest + y*dst_stride + x*4), tmp);
		//~ }
	//~ }
	//~ _mm_empty();
//~ }

//TODO load/generate pallets
