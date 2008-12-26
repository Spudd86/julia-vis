#ifndef JULIA_AUDIO_H
#define JULIA_AUDIO_H

/**
 * return total number of beats so far
 */
int beat_get_count(void);
void beat_update(float *fft, int fft_len);

int audio_setup(int sr);
void audio_update(float *in, int n);

#endif
