#ifndef JULIA_BEAT_H
#define JULIA_BEAT_H

typedef struct {
	int bands;
	int histlen;
	int hi;

	int *counts;
	float *means;
	float *stddev;
} beat_data;

typedef struct beat_ctx beat_ctx;

beat_ctx *beat_new(void);
int beat_ctx_count(beat_ctx *self);
int beat_ctx_bands(beat_ctx *self);
void beat_ctx_update(beat_ctx *self, const float *restrict fft, int fft_len);
void beat_ctx_get_data(beat_ctx *ctx, beat_data *ad);

#endif
