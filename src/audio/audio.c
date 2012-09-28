#include "common.h"
#include <fftw3.h>

#include "tribuf.h"
#include "audio.h"
#include "audio-private.h"
#include "beat.h"

#ifdef FFT_TRIBUF
#undef FFT_TRIBUF
#define FFT_RINGBUF 1
#endif

int buf_count = 0;
static int nr_samp = 0;
static float *fft_tmp = NULL;
static fftwf_plan p;

static tribuf *samp_tb = NULL;

#ifdef FFT_RINGBUF
#include "rb.h"
static rb_t *fft_rb = NULL;
#endif

static beat_ctx *gbl_beat_ctx = NULL;

typedef void (*audio_drv_shutdown_t)();
audio_drv_shutdown_t audio_drv_shutdown = NULL;

int audio_get_buf_count(void) {
	return buf_count;
}
int beat_get_count(void) {
	return beat_ctx_count(gbl_beat_ctx);
}

void audio_shutdown()
{
	if(audio_drv_shutdown != NULL) audio_drv_shutdown();

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
static float *do_fft(float *in1, float *in2)
{
	//memcpy(fft_tmp, in, sizeof(float)*nr_samp);
	for(int i=0; i<nr_samp;i++) { // window samples
		// Hanning window
		float w = 0.5f*(1.0f - cosf((2*(float)M_PI*i)/(nr_samp-1)));
		
		// Blackman
		//float w = (1.0f - 0.16f)/2 - 0.5f*cosf((2*(float)M_PI*i)/(nr_samp-1)) + 0.16f*0.5f*cosf((4*(float)M_PI*i)/(nr_samp-1));
		
		//Lanczos
		//float t = (2.0f*i/(nr_samp-1) - 1)*(float)M_PI;
		//float w = sin(t)/t;
		
		fft_tmp[i] = ((i < nr_samp/2)?in1[i]:in2[i-nr_samp/2])*w;
		
	}
	
	fftwf_execute(p);
	float *fft = fft_tmp;

	//const float scl = 1.0f/nr_samp;
	fft[0] = fabsf(fft_tmp[0])/nr_samp;
	for(int i=1; i < nr_samp/2; i++)
		fft[i] = sqrtf(fft_tmp[i]*fft_tmp[i] + fft_tmp[nr_samp-i]*fft_tmp[nr_samp-i])/nr_samp;
	fft[nr_samp/2] = fabsf(fft_tmp[nr_samp/2])/nr_samp;

#ifdef FFT_RINGBUF
	size_t fft_len = sizeof(float)*(nr_samp/2 + 1);
	if(rb_write_space(fft_rb) >=  fft_len)
		rb_write(fft_rb, (char*)fft, fft_len);
#endif

	return fft;
}

static int bufp = 0;
static float *cur_buf = NULL; ///< need to preserve the result of tb_get_write across calls
static float *prev_buf = NULL; 

// TODO: double check correctness
void audio_update(const float * __attribute__ ((aligned (16))) in, int n)
{
	float *samps = NULL;
	int remain = 0;

#if 0
	if(bufp == 0 && n == nr_samp) {
		samps  = tribuf_get_write(samp_tb);
		memcpy(samps, in, sizeof(float)*nr_samp);
		tribuf_finish_write(samp_tb);
	} else {
		if(bufp == 0) cur_buf = tribuf_get_write(samp_tb);

		samps = cur_buf;
	
		int cpy = MIN(n, nr_samp-bufp);
		memcpy(samps+bufp, in, sizeof(float)*cpy);
		remain = n - cpy;
		in += cpy;
		bufp = (bufp + cpy)%nr_samp;
		if(bufp == 0) {
			cur_buf = NULL;
			tribuf_finish_write(samp_tb);
		} else return;
	}
	
	//TODO: lapped transform?
	buf_count++;
	beat_ctx_update(gbl_beat_ctx, do_fft(samps, samps+nr_samp/2), nr_samp/2);
#else
	if(bufp == 0 && n == nr_samp/2) {
		samps  = tribuf_get_write(samp_tb);
		memcpy(samps, in, sizeof(float)*nr_samp/2);
		tribuf_finish_write(samp_tb);
	} else {
		if(bufp == 0) cur_buf = tribuf_get_write(samp_tb);

		samps = cur_buf;
	
		int cpy = MIN(n, nr_samp/2-bufp);
		memcpy(samps+bufp, in, sizeof(float)*cpy);
		remain = n - cpy;
		in += cpy;
		bufp = (bufp + cpy)%(nr_samp/2);
		if(bufp == 0) {
			cur_buf = NULL;
			tribuf_finish_write(samp_tb);
		} else return;
	}
	
	buf_count++;
	beat_ctx_update(gbl_beat_ctx, do_fft(samps, prev_buf), nr_samp/2);
	prev_buf = samps;
	
#endif

	if(remain > 0) audio_update(in, remain);
}

int audio_get_samples(audio_data *d) {
	d->len = nr_samp/2;
	d->data = tribuf_get_read(samp_tb);
	return 0;
}
void audio_finish_samples(void) { tribuf_finish_read(samp_tb); }

#define MAX_SAMP 2048

#ifdef FFT_RINGBUF
static float fft_data[MAX_SAMP];

int audio_get_fft(audio_data *d) {
	d->len = nr_samp/2+1;
	d->data = fft_data;
	size_t fft_len = sizeof(float)*(nr_samp/2 + 1);
	if(rb_read_space(fft_rb) >= fft_len) 
		rb_read(fft_rb, (void *)fft_data, fft_len);
	return 0;
}

void audio_fft_finsih_read(void) { }
#endif

static void *samp_data[3];

// never need more memory than we get here.
static float samp_bufs[MAX_SAMP*3] __attribute__ ((aligned (16)));

// sr is sample rate
int audio_setup(int sr)
{
	nr_samp = (sr<50000)?MAX_SAMP/2:MAX_SAMP;

	printf("Sample Rate %i\nUsing %i samples/buffer\n", sr, nr_samp/2);

	fft_tmp = fftwf_malloc(sizeof(float) * nr_samp); // do xform in place
	if(!fft_tmp) abort();

	p = fftwf_plan_r2r_1d(nr_samp, fft_tmp, fft_tmp, FFTW_R2HC, 0);

	samp_data[0] = samp_bufs;
	samp_data[1] = samp_data[0] + MAX_SAMP;
	samp_data[2] = samp_data[1] + MAX_SAMP;
	memset(samp_data[0], 0, sizeof(float) * MAX_SAMP * 3);

#ifdef FFT_RINGBUF
	fft_rb = rb_create(8*MAX_SAMP*sizeof(float));
#endif
	
	samp_tb = tribuf_new(samp_data, 0);
	
	prev_buf = tribuf_get_read_nolock(samp_tb);

	gbl_beat_ctx = beat_new();

	return 0;
}

int audio_init(const opt_data *od)
{
	printf("\nAudio input starting...\n");

	int rc;
	switch(od->audio_driver) {
	#ifdef HAVE_JACK
		case AUDIO_JACK:
			rc = jack_setup(od);
			audio_drv_shutdown = jack_shutdown;
			break;
	#endif
	#ifdef HAVE_PULSE
		case AUDIO_PULSE:
			audio_drv_shutdown = pulse_shutdown;
			rc = pulse_setup(od);
			break;
	#endif
	#ifdef HAVE_PORTAUDIO
		case AUDIO_PORTAUDIO:
			rc = audio_setup_pa(od);
			audio_drv_shutdown = audio_stop_pa;
			break;
	#endif
		default:
			printf("No Audio driver!\n");
			rc = audio_setup(48000);
			//rc = -1;
	}

	if(rc < 0) printf("Audio setup failed!\n");
	else printf("Finished audio setup\n\n");
	return rc;
}
