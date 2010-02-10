#ifndef JULIA_AUDIO_H
#define JULIA_AUDIO_H

typedef struct {
	int len;
	float *data;
} audio_data;

typedef struct {
	int bands;
	int histlen;
	int hi;

	int *counts;
	float *means;
	float *stddev;
	float *hist;
	float *df;
} beat_data;

static inline float beat_gethist(beat_data *b, int band, int i) {
	if(i < 0 || i > b->histlen || band < 0 || band >= b->bands) return 0.0f;
	return b->hist[band*b->histlen*2 + (b->hi + b->histlen + i)%(b->histlen*2)];
}

/**
 * return total number of beats so far
 */
void beat_setup();
int beat_get_count(void);
void beat_get_data(beat_data *);
void beat_update(float *fft, int fft_len);

int audio_init(const opt_data *);
int audio_setup(int sr);
void audio_shutdown();
void audio_update(const float *in, int n);
int audio_get_buf_count(void);

int audio_get_samples(audio_data *d);
void audio_finish_samples(void);
int audio_get_fft(audio_data *d);
void audio_fft_finsih_read(void);

#endif
