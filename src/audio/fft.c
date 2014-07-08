// based on code from http://www.jjj.de/fxt/
// Copyright (C) 2010, 2012 Joerg Arndt
// License: GNU General Public License version 3 or later

#include "common.h"


// accurate on [-pi/4, pi/4] which is exactly the range we need
// adapted from cephes (zlib)
static inline float mysinf(float x)
{
	float z = x * x;
	float y = ((-1.9515295891E-4f * z
	           + 8.3321608736E-3f) * z
	           - 1.6666654611E-1f) * z * x;
	y += x;
	return y;
}

// accurate on [-pi/4, pi/4] which is exactly the range we need
// adapted from cephes (zlib)
static inline float mycosf(float x)
{
	float z = x * x;
	float y = ((2.443315711809948E-005f * z 
	           - 1.388731625493765E-003f) * z
	           + 4.166664568298827E-002f) * z * z;
	y -= 0.5f * z;
	y += 1.0f;
	return y;
}

static inline uint32_t revbin(uint32_t v)
// Return x with reversed bit order.
{

	v = ((v >> 1) & 0x55555555) | ((v & 0x55555555) << 1);
	v = ((v >> 2) & 0x33333333) | ((v & 0x33333333) << 2);
	v = ((v >> 4) & 0x0F0F0F0F) | ((v & 0x0F0F0F0F) << 4);
	v = ((v >> 8) & 0x00FF00FF) | ((v & 0x00FF00FF) << 8);
	v = ( v >> 16             ) | ( v               << 16);
	
	return v;
}

// computes floor(log2(v))
static inline uint32_t ld(uint32_t v) { // requires v to be a power of two
	return ((v & 0xAAAAAAAA) != 0)
	    | (((v & 0xFFFF0000) != 0) << 4)
	    | (((v & 0xFF00FF00) != 0) << 3)
	    | (((v & 0xF0F0F0F0) != 0) << 2)
	    | (((v & 0xCCCCCCCC) != 0) << 1);
}

static void revbin_permute(float *a, uint32_t n) {
	const uint32_t nh = n>>1, nm1 = n - 1;
	uint32_t x = 1, r = 0;
	
	//const int ldn = 31 - __builtin_clz(n);
	//const int rev_sft = __builtin_clz(n) + 1;
	const int rev_sft = 32 - ld(n);

	do {
		float t;
		r = r + nh;
		t = a[x]; a[x] = a[r]; a[r] = t;

		x++;
		
		r = revbin(x) >> rev_sft;
		//r = revbin(x) >> (32-ldn);
		//uint32_t ht=nh; while ( !((r^=ht)&ht) ) ht = ht >> 1; //r = revbin_upd(r, nh);

		if(r >= x) {
			t = a[x]; a[x] = a[r]; a[r] = t;
			t = a[nm1-x]; a[nm1-x] = a[nm1-r]; a[nm1-r] = t;
		}
		x++;
	} while(x < nh);
}

//
// Transform length is n=2**ldn
//
// Ordering of output:
//
// f[0]     = re[0] (==zero frequency, purely real)
// f[1]     = re[1]
//         ...
// f[n/2-1] = re[n/2-1]
// f[n/2]   = re[n/2]    (==nyquist frequency, purely real)
//
// f[n/2+1] = im[n/2-1]
// f[n/2+2] = im[n/2-2]
//         ...
// f[n-1]   = im[1]
//
// Corresponding real and imag parts (with the exception of
// zero and nyquist freq) are found in f[i] and f[n-i]
//
// The order of imaginary parts is the same as in fht_real_complex_fft
// (reversed wrt. easy_ordering_real_complex_fft())
//
// NOTE: conviently this is the same ordering as FFTW's "halfcomplex" format
void split_radix_real_complex_fft(float *x, uint32_t n)
{
	revbin_permute(x, n);

    for (unsigned int ix=0, id=4;  ix<n;  id*=4) {
        for (unsigned int i0=ix; i0<n; i0+=id) {
        	//sumdiff(x[i0], x[i0+1]); // {a, b}  <--| {a+b, a-b}
        	float st1 = x[i0] - x[i0+1]; x[i0] += x[i0+1]; x[i0+1] = st1;
        }
        ix = 2*(id-1);
    }

    unsigned int n2 = 2;
    unsigned int nn = n>>1;
    while ( nn>>=1 ) {
        uint32_t ix = 0;
        n2 <<= 1;
        uint32_t id = n2<<1;
        uint32_t n4 = n2>>2;
        uint32_t n8 = n2>>3;

        do { // ix
        	if(n4 != 1) {
		        for (uint32_t i0=ix; i0<n; i0+=id) {
		            uint32_t i1 = i0;
		            uint32_t i2 = i1 + n4;
		            uint32_t i3 = i2 + n4;
		            uint32_t i4 = i3 + n4;

		            float t1 , t2;
		            //diffsum3_r(x[i3], x[i4], t1);  // {a, b, s} <--| {a, b-a, a+b}
		            t1 = x[i3] + x[i4]; x[i4] -= x[i3];
		            //sumdiff3(x[i1], t1, x[i3]);   // {a, b, d} <--| {a+b, b, a-b}
		            x[i3] = x[i1] - t1; x[i1] += t1;

	                i1 += n8;
	                i2 += n8;
	                i3 += n8;
	                i4 += n8;

	                //sumdiff(x[i3], x[i4], t1, t2); // {s, d}  <--| {a+b, a-b}
	                t1 = x[i3] + x[i4]; t2 = x[i3] - x[i4];
	                t1 = -t1 * (float)M_SQRT1_2;
	                t2 *= (float)M_SQRT1_2;

					// sumdiff(t1, x[i2], x[i4], x[i3]); // {s, d}  <--| {a+b, a-b}
	                float st1 = x[i2]; x[i4] = t1 + st1; x[i3] = t1 - st1;
	                //sumdiff3(x[i1], t2, x[i2]);  // {a, b, d} <--| {a+b, b, a-b}
	                x[i2] = x[i1] - t2; x[i1] += t2;
		        }
			} else {
				for (uint32_t i0=ix; i0<n; i0+=id) {
		            uint32_t i1 = i0;
		            uint32_t i2 = i1 + n4;
		            uint32_t i3 = i2 + n4;
		            uint32_t i4 = i3 + n4;

		            float t1;
		            //diffsum3_r(x[i3], x[i4], t1);  // {a, b, s} <--| {a, b-a, a+b}
		            t1 = x[i3] + x[i4]; x[i4] -= x[i3];
		            //sumdiff3(x[i1], t1, x[i3]);   // {a, b, d} <--| {a+b, b, a-b}
		            x[i3] = x[i1] - t1; x[i1] += t1;
		    	}
			}
            ix = (id<<1) - n2;
            id <<= 2;
        } while(ix < n);

        float e = 2.0f*(float)M_PI/n2;
        for (uint32_t j=1; j<n8; j++) {
            float a = j * e;
            float cc1, ss1, cc3, ss3;
            ss1 = mysinf(a);   cc1 = mycosf(a);
            //ss3 = sinf(3*a); cc3 = cosf(3*a);
            cc3 = 4*cc1*(cc1*cc1-0.75f); ss3 = 4*ss1*(0.75f-ss1*ss1);

            ix = 0;
            id = n2<<1;
            do {
                for (uint32_t i0=ix; i0<n; i0+=id) {
                    uint32_t i1 = i0 + j;
                    uint32_t i2 = i1 + n4;
                    uint32_t i3 = i2 + n4;
                    uint32_t i4 = i3 + n4;

                    uint32_t i5 = i0 + n4 - j;
                    uint32_t i6 = i5 + n4;
					uint32_t i7 = i6 + n4;
                    uint32_t i8 = i7 + n4;

                    float t1, t2, t3, t4, st1;
                    //cmult(c, s, x, y, &u, &v)
            		//cmult(cc1, ss1, x[i7], x[i3], t2, t1); // {u,v} <--| {x*c-y*s, x*s+y*c}
                    t2 = x[i7]*cc1 - x[i3]*ss1; t1 = x[i7]*ss1 + x[i3]*cc1;
                    //cmult(cc3, ss3, x[i8], x[i4], t4, t3);
                    t4 = x[i8]*cc3 - x[i4]*ss3; t3 = x[i8]*ss3 + x[i4]*cc3;

                    //sumdiff(t2, t4);   // {a, b} <--| {a+b, a-b}
                    st1 = t2 - t4; t2 += t4; t4 = st1;
                    //sumdiff(t2, x[i6], x[i8], x[i3]);
                    x[i8] = t2 + x[i6]; x[i3] = t2 - x[i6];

                    //sumdiff_r(t1, t3);  // {a, b} <--| {a+b, b-a}
                    st1 = t3 - t1; t1 += t3; t3 = st1;
                    //sumdiff(t3, x[i2], x[i4], x[i7]);
                    x[i4] = t3 + x[i2]; x[i7] = t3 - x[i2];

                    //sumdiff3(x[i1], t1, x[i6]);   // {a, b, d} <--| {a+b, b, a-b}
                    x[i6] = x[i1] - t1; x[i1] += t1;
                    //diffsum3_r(t4, x[i5], x[i2]);  // {a, b, s} <--| {a, b-a, a+b}
                    x[i2] = t4 + x[i5]; x[i5] -= t4;
                }
                ix = (id<<1) - n2;
                id <<= 2;
            } while(ix < n);
        }
    }
}

