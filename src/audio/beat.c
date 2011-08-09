#include "common.h"
#include "audio.h"
#include "audio-private.h"

#define BANDS (64)
// about a seconds worth
#define HIST 45

struct beat_ctx {
	int hi; // index in circular history buffer
	int count;
	int beat_count;
	int lastbeat;
	int beat_bands[BANDS];
	float Eh[BANDS][HIST*2];
	float V[BANDS];
	float E[BANDS];
};

beat_ctx *beat_new() {
	beat_ctx *self = malloc(sizeof(beat_ctx));
	memset(self, 0, sizeof(beat_ctx));
	return self;
}

int beat_ctx_count(beat_ctx *self) { return __sync_add_and_fetch(&self->beat_count, 0); }


static inline float sqr(float x) { return x*x; }

static inline float getsamp(const float *restrict data, int len, int i, int w) {
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
void beat_ctx_update(beat_ctx *self, const float *restrict fft, int fft_len)
{
	const int hi = self->hi;
	float *const restrict V = self->V;
	float *const restrict E = self->E;
	for(int b=0; b < BANDS; b++)
	{ // TODO: try to make a good beat detector based on slope of log2(fft + 1)/2
		// since log2(samp+1)/2 should give us a nice linear relation to percived volume
		//float tmp = log2f(getsamp(fft, fft_len, b*fft_len/(BANDS*2) , fft_len/(BANDS*4)) + 1.0f)/2;
		float tmp = getsamp(fft, fft_len, b*fft_len/(BANDS*2) , fft_len/(BANDS*4));
		
		float *const restrict Ehb = self->Eh[b];

		

		//float C = -0.0025714*V[b]+1.5142857;
		float C = -0.0025714*V[b]+1.5142857*2;
		//float C = -0.00025714*V[b]+1.5142857*2.5;
		if(tmp > C*E[b] && E[b]>0.002f) {
			if(self->count - self->lastbeat > 10) {
				__sync_add_and_fetch(&self->beat_count, 1);
				self->lastbeat = self->count;
			}
			//__sync_add_and_fetch(beat_bands + b, 1);
		}
		
		float v = 0;
		for(int i=0; i<HIST; i++) v += sqr(Ehb[(hi + HIST + i)%(HIST*2)]) - sqr(E[b]);
		self->V[b] = sqrtf(v);
		self->E[b] += (tmp - Ehb[(hi+HIST)%(HIST*2)])/HIST;

		Ehb[hi] = tmp;
	}

	self->hi = (hi + 1)%(HIST*2);
	self->count++;
}


static beat_ctx glbl_ctx;
void beat_setup() {
	memset(&glbl_ctx, 0, sizeof(beat_ctx));
}

int beat_get_count(void) { return __sync_add_and_fetch(&glbl_ctx.beat_count, 0); }
void beat_get_data(beat_data *ad) {
	ad->bands = BANDS;
	ad->histlen = HIST;
	ad->hi = glbl_ctx.hi;
	ad->counts = glbl_ctx.beat_bands;
	ad->stddev = glbl_ctx.V;
	ad->means  = glbl_ctx.E;
//	ad->df = dF;
	ad->hist = (void *)glbl_ctx.Eh;
}

/**
 * take 1024 frames of audio and do update beat detection
 * http://www.gamedev.net/reference/programming/features/beatdetection/
 */
void beat_update(const float *restrict fft, int fft_len)
{
	beat_ctx_update(&glbl_ctx, fft, fft_len);
}

