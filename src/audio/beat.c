#include "common.h"
#include "audio.h"

#define BANDS (64)
// about a seconds worth
#define HIST 45

static int hi = 0; // index in circular history buffer
static int count = 0;
static int beat_count = 0;
static int lastbeat = 0;
static int beat_bands[BANDS];
static float dF[BANDS];
static float Eh[BANDS][HIST*2];
static float V[BANDS];
static float E[BANDS];


void beat_setup() {
	memset(beat_bands, 0, sizeof(beat_bands));
	memset(Eh, 0, sizeof(Eh));
	memset(V, 0, sizeof(V));
	memset(E, 0, sizeof(E));
	memset(dF, 0, sizeof(dF));
}

int beat_get_count(void) { return __sync_add_and_fetch(&beat_count, 0); }
void beat_get_data(beat_data *ad) {
	ad->bands = BANDS;
	ad->histlen = HIST;
	ad->hi = hi;
	ad->counts = beat_bands;
	ad->stddev = V;
	ad->means  = E;
	ad->df = dF;
	ad->hist = (void *)Eh;
}

static inline float sqr(float x) { return x*x; }



static inline float getsamp(float *data, int len, int i, int w) {
	float res = 0;
	int l = IMAX(i-w, 1); // skip sample 0 it's average for energy for entire interval
	int u = IMIN(i+w, len);
	for(int i = l; i < u; i++) {
		res += data[i];
	}
	return res / (u-l);
}

/**
 * take 1024 frames of audio and do update beat detection
 * http://www.gamedev.net/reference/programming/features/beatdetection/
 */
void beat_update(float *fft, int fft_len)
{
	for(int b=0; b < BANDS; b++)
	{ // TODO: try to make a good beat detector based on slope of log2(fft + 1)/2
		// since log2(samp+1)/2 should give us a nice linear relation to percived volume 
		//float tmp = log2f(getsamp(fft, fft_len, b*fft_len/(BANDS*2) , fft_len/(BANDS*4)) + 1.0f)/2;
		float tmp = getsamp(fft, fft_len, b*fft_len/(BANDS*2) , fft_len/(BANDS*4));
		float v = 0;
		for(int i=0; i<HIST; i++) v += sqr(Eh[b][(hi + HIST + i)%(HIST*2)]) - sqr(E[b]);
		V[b] = sqrtf(v);
		E[b] += (tmp - Eh[b][(hi+HIST)%(HIST*2)])/HIST;
		
		Eh[b][hi] = tmp;
		

//		dF[b] = (3*log2f(1+31*Eh[b][hi])/5 - 4*log2f(1+31*Eh[b][(hi + 2*HIST - 1)%(HIST*2)])/5 +log2f(1+31* Eh[b][(hi + 2*HIST - 2)%(HIST*2)])/5)/2;

		dF[b] = log2f(1+31*Eh[b][hi])/5 - 2*log2f(1+31*Eh[b][(hi + 2*HIST - 1)%(HIST*2)])/5 + log2f(1+31*Eh[b][(hi + 2*HIST - 2)%(HIST*2)])/5;

		//float C = -0.0025714*V[b]+1.5142857;
		float C = -0.00025714*V[b]+1.5142857*2.5;
		if(tmp > C*E[b] && E[b]>0.002f) {
			if(count - lastbeat > 10) {
				__sync_add_and_fetch(&beat_count, 1);
				lastbeat = count;
			}
			//__sync_add_and_fetch(beat_bands + b, 1);
			
		}
	}
	
	hi = (hi + 1)%(HIST*2);
	count++;
}

