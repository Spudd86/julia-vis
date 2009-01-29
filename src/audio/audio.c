#include <unistd.h>
#include <fftw3.h>
#include <math.h>
#include <string.h>
#include <malloc.h>

#include "../tribuf.h"
#include "../common.h"
#include "audio.h"

static int nr_samp = 0;
static float *fft_tmp = NULL;
static fftwf_plan p;

static tribuf *samp_tb = NULL;
static tribuf *fft_tb = NULL;

static inline float sqr(float x) { return x*x; }

/**
 * take 1024 frames of audio and do update beat detection
 * http://www.gamedev.net/reference/programming/features/beatdetection/
 */
static float *do_fft(float *in)
{
	memcpy(fft_tmp, in, sizeof(float)*nr_samp);
	fftwf_execute(p);
	
	float *fft = tribuf_get_write(fft_tb);
	fft[0] = fabsf(fft_tmp[0])/nr_samp;
	fft[nr_samp/2] = fabsf(fft_tmp[nr_samp/2])/nr_samp;
	
	for(int i=1; i < nr_samp/2; i++)
		fft[i] = sqrtf(sqr(fft_tmp[i]) + sqr(fft_tmp[nr_samp-i]))/nr_samp;
	
	tribuf_finish_write(fft_tb);
	return fft;
}

void audio_update(float *in, int n)
{
	float *samps = tribuf_get_write(samp_tb);
	memcpy(samps, in, sizeof(float)*IMIN(n,nr_samp));
	tribuf_finish_write(samp_tb);
#ifdef DO_BEAT
	beat_update(do_fft(samps), nr_samp/2);
#endif
}

int audio_get_samples(audio_data *d) {
	d->len = nr_samp;
	d->data = tribuf_get_read(samp_tb);
	return 0;
}

int audio_get_fft(audio_data *d) {
	d->len = nr_samp/2+1;
	d->data = tribuf_get_read(fft_tb);
	return 0;
}

static void *fft_data[3];
static void *samp_data[3];
// sr is sample rate
int audio_setup(int sr)
{
	nr_samp = (sr<50000)?1024:2048;
	
	fft_tmp = fftwf_malloc(sizeof(float) * nr_samp); // do xform in place
	p = fftwf_plan_r2r_1d(nr_samp, fft_tmp, fft_tmp, FFTW_R2HC, 0);

	for(int i=0;i<3;i++) fft_data[i] = malloc(sizeof(float) * (nr_samp/2+1));
	for(int i=0;i<3;i++) samp_data[i] = malloc(sizeof(float) * nr_samp);
	for(int i=0;i<3;i++) {
		memset(samp_data[i], 0, sizeof(float) * nr_samp);
		memset(fft_data[i], 0, sizeof(float) * nr_samp/2+1);
	}
	
	fft_tb = tribuf_new(fft_data);
	samp_tb = tribuf_new(samp_data);
	
	
	return 0;
}

void audio_stop()
{
	fftwf_free(fft_tmp);
	fftwf_destroy_plan(p);
	fft_tmp = NULL;
}
