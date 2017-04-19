#include "common.h"
#include "beat.h"
#include "getsamp.h"

#define BANDS (64)
// about a seconds worth
//#define HIST 45
#define HIST 90

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

struct beat_ctx *beat_new(void)
{
	struct beat_ctx *self = malloc(sizeof(*self));
	self->hi = 0;
	self->count = 0;
	self->beat_count = 0;
	self->lastbeat = 0;
	for(size_t i = 0; i < BANDS; i++) {
		self->beat_bands[i] = 0;
		self->V[i] = 0.0f;
		self->E[i] = 0.0f;
		for(size_t j = 0; j < HIST*2; j++) {
			self->Eh[i][j] = 0.0f;
		}
	}
	return self;
}

void beat_delete(struct beat_ctx *self)
{
	free(self);
}

int beat_ctx_count(struct beat_ctx *self) { return self->beat_count; }
int beat_ctx_bands(struct beat_ctx *self) {(void)self; return BANDS; }

void beat_ctx_get_data(struct beat_ctx *ctx, struct beat_data *ad) {
	ad->bands = BANDS;
	ad->histlen = HIST;
	ad->hi = (ctx->hi + HIST*2 - 1)%(HIST*2);
	ad->counts = ctx->beat_bands;
	ad->stddev = ctx->V;
	ad->means  = ctx->E;
}


static inline float sqr(float x) { return x*x; }

/**
 * take 1024 frames of audio and do update beat detection
 * http://www.gamedev.net/reference/programming/features/beatdetection/
 */
void beat_ctx_update(struct beat_ctx *self, const float *restrict fft, int fft_len)
{
	const int hi = self->hi;
	float *const restrict V = self->V;
	float *const restrict E = self->E;
	for(int b=0; b < BANDS; b++)
	{ // TODO: try to make a good beat detector based on slope of log2(fft + 1)/2
		// since log2(samp+1)/2 should give us a nice linear relation to percived volume
		float tmp = log2f(getsamp(fft, fft_len, b*fft_len/(BANDS*2) , fft_len/(BANDS*4)) + 1.0f)/2;
		//float tmp = getsamp(fft, fft_len, b*fft_len/(BANDS*2) , fft_len/(BANDS*4));

		float *const restrict Ehb = self->Eh[b];



		//float C = -0.0025714f*V[b]+1.5142857f;
		//float C = -0.0025714f*V[b]+1.5142857f*2;
		float C = -0.00025714f*V[b]+1.5142857f*2.5f;
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

