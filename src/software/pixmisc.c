
#include "common.h"
#include "pixmisc.h"

#ifdef HAVE_ORC
#include <orc/orc.h>
void fade_pix(void *restrict __attribute__((aligned (16))) buf, int w, int h, uint8_t fade)
{
	static OrcProgram *p = NULL;
	static int fd;
	if (p == NULL) {
		p = orc_program_new();
		fd = orc_program_add_parameter(p, 2, "fade");
		orc_program_add_destination(p, 2, "d1");
		orc_program_append_str(p, "mulhuw", "d1", "d1", "fade");
		orc_program_compile (p); //TODO: check return value here
	}
	
	OrcExecutor _ex;
	OrcExecutor *ex = &_ex;
	orc_executor_set_program (ex, p);
	orc_executor_set_n (ex, w*h);
	orc_executor_set_array (ex, ORC_VAR_D1, buf);
	orc_executor_set_param (ex, fd, fade<<8);
	
	orc_executor_run (ex);
}
void maxblend(void *restrict dest, void *restrict src, int w, int h)
{
	static OrcProgram *p = NULL;
	if (p == NULL) {
		p = orc_program_new_ds(2,2);
		orc_program_append_str(p, "maxuw", "d1", "d1", "s1");
		orc_program_compile (p);  //TODO: check return value here
	}
	
	OrcExecutor _ex;
	OrcExecutor *ex = &_ex;
	orc_executor_set_program (ex, p);
	orc_executor_set_n (ex, w*h);
	orc_executor_set_array (ex, ORC_VAR_S1, src);
	orc_executor_set_array (ex, ORC_VAR_D1, dest);
	orc_executor_run (ex);
}

#elif defined(__SSE2__)
#include "mymm.h"
#include <emmintrin.h>
#warning Doing sse2 Compiled program will NOT work on system without it!

// requires w%16 == 0
void maxblend(void *restrict dest, void *restrict src, int w, int h)
{
	__m128i * restrict mbdst = dest; const __m128i * restrict mbsrc = src;
	const __m128i off = _mm_set1_epi16(0x8000);
	for(unsigned int i=0; i < 2*w*h/sizeof(__m128); i+=2) { // TODO see if the prefeting is helping
		__builtin_prefetch(&(mbsrc[i+2]), 0, 0); __builtin_prefetch(&(mbdst[i+2]), 1, 0);

		__m128i v1, v2, t1, t2;

		v1 = mbdst[i], t1 = mbsrc[i];
		v1 = _mm_add_epi16(v1, off); t1 = _mm_add_epi16(t1, off);
		v1 = _mm_max_epi16(v1, t1);
		v1 = _mm_sub_epi16(v1, off);
		mbdst[i]=v1;

		v2 = mbdst[i+1], t2 = mbsrc[i+1];
		v2 = _mm_add_epi16(v2, off); t2 = _mm_add_epi16(t2, off);
		v2 = _mm_max_epi16(v2, t2);
		v2 = _mm_sub_epi16(v2, off);
		mbdst[i+1]=v2;
	}
}

void fade_pix(void *restrict buf, int w, int h, uint8_t fade)
{
	__m128i * const restrict mbbuf = buf;
	const __m128i fd = _mm_set1_epi16(fade<<8);
	const unsigned int n = 2*w*h/sizeof(__m128i);
	unsigned int i=0;

	switch(n%4){
		case 0: mbbuf[i] = _mm_mulhi_epu16(mbbuf[i], fd); i++;
		case 3: mbbuf[i] = _mm_mulhi_epu16(mbbuf[i], fd); i++;
		case 2: mbbuf[i] = _mm_mulhi_epu16(mbbuf[i], fd); i++;
		case 1: mbbuf[i] = _mm_mulhi_epu16(mbbuf[i], fd); i++;
	}
	do { 
		__builtin_prefetch(mbbuf+i+4, 1, 0);
	
		mbbuf[i] = _mm_mulhi_epu16(mbbuf[i], fd);
		mbbuf[i+1] = _mm_mulhi_epu16(mbbuf[i+1], fd);
		mbbuf[i+2] = _mm_mulhi_epu16(mbbuf[i+2], fd);
		mbbuf[i+3] = _mm_mulhi_epu16(mbbuf[i+3], fd);
		i+=4; 
	} while(i < n);
}
#else

#ifdef __MMX__
#include "mymm.h"
void fade_pix(void *restrict __attribute__((aligned (16))) buf, int w, int h, uint8_t fade)
{
	__m64 *mbbuf = buf;
	const __m64 fd = _mm_set1_pi16(fade<<7);
	//const __m64 fd = _mm_set1_pi16(fade);
	for(unsigned int i=0; i < 2*w*h/sizeof(__m64); i+=4) { // TODO see if the prefeting is helping
		__builtin_prefetch(mbbuf+i+4, 1, 0);
		__m64 v1, v2, v3, v4;//,t;

		v1 = mbbuf[i];
		v1 = _mm_srli_pi16(v1, 1);
		v1 = _mm_mulhi_pi16(v1, fd);
		v1 = _mm_slli_pi16(v1, 2);
		mbbuf[i]=v1;

		v2 = mbbuf[i+1];
		v2 = _mm_srli_pi16(v2, 1);
		v2 = _mm_mulhi_pi16(v2, fd);
		v2 = _mm_slli_pi16(v2, 2);
		mbbuf[i+1]=v2;

		v3 = mbbuf[i+2];
		v3 = _mm_srli_pi16(v3, 1);
		v3 = _mm_mulhi_pi16(v3, fd);
		v3 = _mm_slli_pi16(v3, 2);
		mbbuf[i+2]=v3;

		v4 = mbbuf[i+3];
		v4 = _mm_srli_pi16(v4, 1);
		v4 = _mm_mulhi_pi16(v4, fd);
		v4 = _mm_slli_pi16(v4, 2);
		mbbuf[i+3]=v4;
	}
	_mm_empty();
}
#else
void fade_pix(void *restrict buf, int w, int h, uint8_t fade)
{
	const int n = w*h;
	uint16_t *restrict d=buf;
	for(int i=0; i<n; i++)
		d[i] = (((uint32_t)d[i])*fade)>>8;
}
#endif

#if defined(__SSE__) || defined(__3dNOW__)
void maxblend(void *restrict dest, void *restrict src, int w, int h)
{
	__m64 *mbdst = dest, *mbsrc = src;
	const __m64 off = _mm_set1_pi16(0x8000);
	for(unsigned int i=0; i < 2*w*h/sizeof(__m64); i+=4) { // TODO see if the prefeting is helping
		__builtin_prefetch(&(mbsrc[i+4]), 0, 0); __builtin_prefetch(&(mbdst[i+4]), 1, 0);

		__m64 v1, v2, v3, v4, t1, t2, t3, t4;

		v1 = mbdst[i], t1 = mbsrc[i];
		v1 = _mm_add_pi16(v1, off); t1 = _mm_add_pi16(t1, off);
		v1 = _mm_max_pi16(v1, t1);
		v1 = _mm_sub_pi16(v1, off);
		mbdst[i]=v1;

		v2 = mbdst[i+1], t2 = mbsrc[i+1];
		v2 = _mm_add_pi16(v2, off); t2 = _mm_add_pi16(t2, off);
		v2 = _mm_max_pi16(v2, t2);
		v2 = _mm_sub_pi16(v2, off);
		mbdst[i+1]=v2;

		v3 = mbdst[i+2], t3 = mbsrc[i+2];
		v3 = _mm_add_pi16(v3, off); t3 = _mm_add_pi16(t3, off);
		v3 = _mm_max_pi16(v3, t3);
		v3 = _mm_sub_pi16(v3, off);
		mbdst[i+2]=v3;

		v4 = mbdst[i+3], t4 = mbsrc[i+3];
		v4 = _mm_add_pi16(v4, off); t4 = _mm_add_pi16(t4, off);
		v4 = _mm_max_pi16(v4, t4);
		v4 = _mm_sub_pi16(v4, off);
		mbdst[i+3]=v4;
	}
	_mm_empty();
}
#else
void maxblend(void *restrict dest, void *restrict src, int w, int h)
{
	const int n = w*h;
	uint16_t *restrict d=dest, *restrict s=src;
	for(int i=0; i<n; i++) 
		d[i] = MAX(d[i], s[i]);
}
#endif

#endif



