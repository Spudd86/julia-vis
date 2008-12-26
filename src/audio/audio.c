#include <unistd.h>
#include <glib.h>
#include <fftw3.h>
#include <math.h>

static float *fft = NULL;
static int nr_samp = 0;
static float *fft_tmp = NULL;
static fftwf_plan p;

static inline float sqr(float x) { return x*x; }

/**
 * take 1024 frames of audio and do update beat detection
 * http://www.gamedev.net/reference/programming/features/beatdetection/
 */
void do_fft(float *in)
{
	memcpy(fft_tmp, in, sizeof(float)*nr_samp);
	fftwf_execute(p);
	
	fft[0] = fft_tmp[0]/nr_samp;
	fft[nr_samp/2] = fft_tmp[nr_samp/2]/nr_samp;
	for(int i=1; i < nr_samp/2; i++)
		fft[i] = sqrtf(sqr(fft_tmp[i]) + sqr(fft_tmp[nr_samp-i]))/nr_samp;
}

void audio_update(float *in, int n)
{
	
}

// sr is sample rate
int audio_setup(int sr)
{
	nr_samp = (sr<50000)?1024:2048;


	fft_tmp = fftwf_malloc(sizeof(float) * nr_samp); // do xform in place
	fft = g_malloc(sizeof(float) * (nr_samp/2+1));

	p = fftwf_plan_r2r_1d(nr_samp, fft_tmp, fft_tmp, FFTW_R2HC, 0);
}

void audio_stop()
{
	g_assert(fft_tmp != NULL);
	g_assert(fft != NULL);
	fftwf_free(fft_tmp);
	g_free(fft);
	fftwf_destroy_plan(p);
	
	fft_tmp = fft = NULL;
}
