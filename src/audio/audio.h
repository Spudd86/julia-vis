#ifndef JULIA_AUDIO_H
#define JULIA_AUDIO_H

typedef struct {
	int len;
	float *data;
} audio_data;

/**
 * return total number of beats so far
 */
int beat_get_count(void);
void beat_band_get_counts(audio_data *ad);
void beat_update(float *fft, int fft_len);

int audio_setup(int sr);
void audio_update(const float *in, int n);

int audio_get_samples(audio_data *d);
int audio_get_fft(audio_data *d);

#endif
