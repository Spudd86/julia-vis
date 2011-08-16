#ifndef JULIA_AUDIO_H
#define JULIA_AUDIO_H

typedef struct {
	int len;
	float *data;
} audio_data;


/**
 * return total number of beats so far
 */
//TODO: rename
int beat_get_count(void);

int audio_init(const opt_data *);
void audio_shutdown();
void audio_update(const float *in, int n);
int audio_get_buf_count(void);

int audio_get_samples(audio_data *d);
void audio_finish_samples(void);
int audio_get_fft(audio_data *d);
void audio_fft_finsih_read(void);

#endif
