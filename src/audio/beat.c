#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

#include "audio.h"

#define BANDS (32)
// about a seconds worth
#define HIST 45

static int beat_count;
static int beat_bands[BANDS];
static float Eh[BANDS*HIST];


int beat_get_count(void) { return __sync_add_and_fetch(&beat_count, 0); }
void beat_band_get_counts(audio_data *ad) {
	if(ad->data == NULL) { 
		ad->len = BANDS;
		ad->data = malloc(sizeof(int)*BANDS);
	}
	memcpy(ad->data, beat_bands, sizeof(int)*BANDS);
}

static inline float sqr(float x) { return x*x; }

// numerically stable, one pass
static float std_dev(float a[], int n)
{
    float M = a[0];
    float Q = 0.0f;
    for(int i = 1; i < n; ++i) {
       Q += Q + i*sqr(a[i]-M)/(i+1);
       M += (a[i] - M)/(i+1);
    }
    return sqrt(Q/n);
}

/**
 * take 1024 frames of audio and do update beat detection
 * http://www.gamedev.net/reference/programming/features/beatdetection/
 */
void beat_update(float *fft, int fft_len)
{
	static int hi = 0; // index in circular history buffer

	float V[BANDS];
	float E[BANDS];
	
	for(int b = 0; b < BANDS; b++) {
		E[b] = V[b] = 0.0f;
		for(int i = 0; i < HIST; i++) {
			E[b] += Eh[b*HIST + i];
			V[b] += sqr(E[b*HIST + i] - E[i]);
		}
		E[b] /= HIST;  V[b] /= HIST;
	}

	for(int b=0; b < BANDS; b++)
	{
		float tmp = 0;
		for(int i = 0; i < fft_len/BANDS; i++)
			tmp += fft[b*BANDS + i];
		float C = -0.0025714*V[b]+1.5142857;
		if(tmp > C*E[b] && V[b]>150) {
			__sync_add_and_fetch(&beat_count, 1);
			__sync_add_and_fetch(beat_bands + b, 1);
		}
		Eh[b*HIST + hi] = tmp;
	}
	
	hi = (hi + 1)%BANDS;
}

