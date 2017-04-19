#ifndef JULIA_BEAT_H
#define JULIA_BEAT_H

typedef struct beat_data {
	int bands;
	int histlen;
	int hi;

	int *counts;
	float *means;
	float *stddev;
} beat_data;

struct beat_ctx *beat_new(void);
void beat_delete(struct beat_ctx *self);
int beat_ctx_count(struct beat_ctx *self);
int beat_ctx_bands(struct beat_ctx *self);
void beat_ctx_update(struct beat_ctx *self, const float *restrict fft, int fft_len);
void beat_ctx_get_data(struct beat_ctx *ctx, struct beat_data *ad);

#endif
