#include <unistd.h>
#include <fftw3.h>
#include <math.h>
#include <string.h>
#include <malloc.h>

#include "../tribuf.h"
#include "../common.h"
#include "audio.h"

int buf_count = 0;
static int nr_samp = 0;
static float *fft_tmp = NULL;
static fftwf_plan p;

static tribuf *samp_tb = NULL;
#ifdef FFT_TRIBUF
static tribuf *fft_tb = NULL;
#endif

int audio_get_buf_count(void) {
	return buf_count;
}

static inline float sqr(float x) { return x*x; }

/* TODO:
 *  handle n != nr_samp in audio_update
 *  add a setup that takes opts from main and calls pa or jack setups
 */

/**
 * take nr_samp frames of audio and do update beat detection
 * http://www.gamedev.net/reference/programming/features/beatdetection/
 */
static float *do_fft(float *in)
{
	memcpy(fft_tmp, in, sizeof(float)*nr_samp);
	fftwf_execute(p);
	
	float *fft;
	#ifdef FFT_TRIBUF
	fft = tribuf_get_write(fft_tb);
	#else
	fft = fft_tmp;
	#endif
	fft[0] = fabsf(fft_tmp[0])/nr_samp;
	fft[nr_samp/2] = fabsf(fft_tmp[nr_samp/2])/nr_samp;
	
	for(int i=1; i < nr_samp/2; i++)
		fft[i] = sqrtf(sqr(fft_tmp[i]) + sqr(fft_tmp[nr_samp-i]))/nr_samp;
	
	#ifdef FFT_TRIBUF
	tribuf_finish_write(fft_tb);
	#endif
	return fft;
}

static int bufp = 0;

// TODO: double check correctness
void audio_update(const float *in, int n)
{
	float *samps = tribuf_get_write(samp_tb);
	int remain = 0;
	
	if(bufp == 0 && n == nr_samp) {
		memcpy(samps, in, sizeof(float)*nr_samp);
	} else {
		int cpy = IMIN(n, nr_samp-bufp);
		memcpy(samps+bufp, in, sizeof(float)*cpy);
		remain = n - cpy;
		in += cpy;
		bufp = (bufp + cpy)%nr_samp;
	}
	if(bufp != 0) return;
	
	tribuf_finish_write(samp_tb);
	buf_count++;
	beat_update(do_fft(samps), nr_samp/2);
	
	if(remain > 0) audio_update(in, remain);
}

int audio_get_samples(audio_data *d) {
	d->len = nr_samp;
	d->data = tribuf_get_read(samp_tb);
	return 0;
}
#ifdef FFT_TRIBUF
int audio_get_fft(audio_data *d) {
	d->len = nr_samp/2+1;
	d->data = tribuf_get_read(fft_tb);
	return 0;
}
#endif

static void *fft_data[3];
static void *samp_data[3];
// sr is sample rate
int audio_setup(int sr)
{
	nr_samp = (sr<50000)?1024:2048;
	
	printf("Sample Rate %i\nUsing %i samples/buffer\n", sr, nr_samp);
	
	fft_tmp = fftwf_malloc(sizeof(float) * nr_samp); // do xform in place
	if(!fft_tmp) abort();
	
	p = fftwf_plan_r2r_1d(nr_samp, fft_tmp, fft_tmp, FFTW_R2HC, 0);
	
	for(int i=0;i<3;i++) {
		samp_data[i] = xmalloc(sizeof(float) * nr_samp);
		memset(samp_data[i], 0, sizeof(float) * nr_samp);
	}
	
	#ifdef FFT_TRIBUF
	for(int i=0;i<3;i++) {
		fft_data[i] = xmalloc(sizeof(float) * (nr_samp/2+1));
		memset(fft_data[i], 0, sizeof(float) * nr_samp/2+1);
	}
	fft_tb = tribuf_new(fft_data);
	#endif
	samp_tb = tribuf_new(samp_data);
	
	beat_setup();
	
	return 0;
}

void audio_shutdown()
{
	fftwf_free(fft_tmp);
	fftwf_destroy_plan(p);
	fft_tmp = NULL;
}

int audio_setup_pa();
int jack_setup(opt_data *);

int audio_init(opt_data *od) 
{
	#ifdef HAVE_JACK
	if(od->use_jack) {
		printf("Starting jack\n");
		return jack_setup(od);
	} else
#endif
		return audio_setup_pa();
}
