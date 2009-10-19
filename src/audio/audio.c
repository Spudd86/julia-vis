#include "../common.h"
#include <fftw3.h>
#include <string.h>

#include "../tribuf.h"
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

void audio_shutdown()
{
	printf("audio shutting down\n");
	//tribuf_destroy(samp_tb);
	fftwf_free(fft_tmp);
	fftwf_destroy_plan(p);
	fft_tmp = NULL;
}

/**
 * take nr_samp frames of audio and do update beat detection
 * http://www.gamedev.net/reference/programming/features/beatdetection/
 */
static float *do_fft(float *in)
{
	memcpy(fft_tmp, in, sizeof(float)*nr_samp);
	fftwf_execute(p);
	float *fft = fft_tmp;
#ifdef FFT_TRIBUF
	fft = tribuf_get_write(fft_tb);
#endif

	//const float scl = 1.0f/nr_samp;
	fft[0] = fabsf(fft_tmp[0])/nr_samp;
	fft[nr_samp/2] = fabsf(fft_tmp[nr_samp/2])/nr_samp;
	for(int i=1; i < nr_samp/2; i++)
		fft[i] = sqrtf(fft_tmp[i]*fft_tmp[i] + fft_tmp[nr_samp-i]*fft_tmp[nr_samp-i])/nr_samp;

#ifdef FFT_TRIBUF
	tribuf_finish_write(fft_tb);
#endif
	return fft;
}

static int bufp = 0;

// TODO: double check correctness
void audio_update(const float * __attribute__ ((aligned (16))) in, int n)
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
	tribuf_finish_read(samp_tb); // TODO: fix interface so this doesn't need to be here
	return 0;
}
#ifdef FFT_TRIBUF
int audio_get_fft(audio_data *d) {
	d->len = nr_samp/2+1;
	d->data = tribuf_get_read(fft_tb);
	tribuf_finish_read(fft_tb); // TODO: fix interface so this doesn't need to be here
	return 0;
}

static void *fft_data[3];
#endif

static void *samp_data[3];

// never need more memory than we get here.
static float samp_bufs[2048*3] __attribute__ ((aligned (16)));

// sr is sample rate
int audio_setup(int sr)
{
	nr_samp = (sr<50000)?1024:2048;

	printf("Sample Rate %i\nUsing %i samples/buffer\n", sr, nr_samp);

	fft_tmp = fftwf_malloc(sizeof(float) * nr_samp); // do xform in place
	if(!fft_tmp) abort();

	p = fftwf_plan_r2r_1d(nr_samp, fft_tmp, fft_tmp, FFTW_R2HC, 0);

	//~ float *samp_bufs = fftwf_malloc(sizeof(float) * nr_samp * 3);
	//~ if(!samp_bufs) {fprintf(stderr, "Memory allocation failed"); exit(1); }
	samp_data[0] = samp_bufs;
	samp_data[1] = samp_data[0] + nr_samp;
	samp_data[2] = samp_data[1] + nr_samp;
	memset(samp_data[0], 0, sizeof(float) * nr_samp * 3);

	#ifdef FFT_TRIBUF
	for(int i=0;i<3;i++) {
		fft_data[i] = xmalloc(sizeof(float) * (nr_samp/2+1));
		memset(fft_data[i], 0, sizeof(float) * nr_samp/2+1);
	}
	fft_tb = tribuf_new(fft_data, 0);
	#endif
	samp_tb = tribuf_new(samp_data, 0);

	beat_setup();

	atexit(audio_shutdown);

	return 0;
}


int audio_setup_pa(opt_data *od);
int jack_setup(opt_data *);
int pulse_setup();

int audio_init(opt_data *od)
{
	int rc;
	#ifdef HAVE_JACK
	if(od->use_jack)
		rc = jack_setup(od);
	else
	#endif
	#ifdef HAVE_PULSE
	if(od->use_pulse)
		rc = pulse_setup(od);
	else
	#endif
		rc = audio_setup_pa(od);

	usleep(10000); // wait a bit so we have some audio in the buffer (instead of garbage)
	return rc;
}
