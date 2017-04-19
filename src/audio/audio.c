#include "common.h"

#include "tribuf.h"
#include "audio.h"
#include "audio-private.h"
#include "beat.h"

void split_radix_real_complex_fft(float *x, uint32_t n);

/*TODO: split this into a backend that does driver init and partial buffer management
 * (ie: makes sure we always get data in chunks that we exepct), and a second part
 * that has the callback that does the fft/triplebuffering management
 * preferably even more split up than that so we can run the audio test app
 * with just a ring buffer in the audio driver callback and re-use the
 * fft setup/windowing/calculation code and call to beat detection, but avoid
 * tying that to the triple buffer updates
 *
 * also need to convert to a context thing and stop using global variables
 *
 * want all that stuff so we can drive a software render at fixed framerate
 * from the audio clock in a gstreamer pipeline or something like that.
 *
 * basically we need to overhaul the structure of this code completely
 *
 * perhaps move fft calculation into the beat detector...
 *
 * need to make grabbing beat counts safer
 */

typedef void (*audio_drv_shutdown_t)(void);
static audio_drv_shutdown_t audio_drv_shutdown = NULL;

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
	#ifdef HAVE_SNDFILE
		case AUDIO_SNDFILE:
			rc = filedecode_setup(od);
			audio_drv_shutdown = filedecode_shutdown;
			break;
	#endif
		default:
			printf("No Audio driver!\n");
			//rc = audio_setup(48000);
			rc = -1;
	}

	if(rc < 0) printf("Audio setup failed!\n");
	else printf("Finished audio setup\n\n");
	return rc;
}

static int buf_count = 0;
static int nr_samp = 0;
static float *fft_tmp = NULL;

static tribuf *samp_tb = NULL;

static struct beat_ctx *gbl_beat_ctx = NULL;

int audio_get_buf_count(void) {
	return buf_count;
}
int beat_get_count(void) {
	//FIXME: not thread safe...
	return beat_ctx_count(gbl_beat_ctx);
}

void audio_shutdown()
{
	if(audio_drv_shutdown != NULL) audio_drv_shutdown();

	printf("audio shutting down\n");
	//tribuf_destroy(samp_tb);
	free(fft_tmp);
	fft_tmp = NULL;
}

/**
 * take nr_samp frames of audio and do update beat detection
 * http://www.gamedev.net/reference/programming/features/beatdetection/
 */
static float *do_fft(float *in1, float *in2)
{
	for(int i=0; i<nr_samp;i++) { // window samples
		// Hanning window
		float w = 0.5f*(1.0f - cosf((2*M_PI_F*i)/(nr_samp-1)));

		// Blackman
		//float w = (1.0f - 0.16f)/2 - 0.5f*cosf((2*M_PI_F*i)/(nr_samp-1)) + 0.16f*0.5f*cosf((4*M_PI_F*i)/(nr_samp-1));

		//Lanczos
		//float t = (2.0f*i/(nr_samp-1) - 1)*M_PI_F;
		//float w = sin(t)/t;

		fft_tmp[i] = ((i < nr_samp/2)?in1[i]:in2[i-nr_samp/2])*w;
	}

	split_radix_real_complex_fft(fft_tmp, nr_samp);
	float *fft = fft_tmp;

	const float scl = 2.0f/nr_samp;
	fft[0] = fabsf(fft_tmp[0])*scl;
	for(int i=1; i < nr_samp/2; i++)
		fft[i] = sqrtf(fft_tmp[i]*fft_tmp[i] + fft_tmp[nr_samp-i]*fft_tmp[nr_samp-i])*scl;
	fft[nr_samp/2] = fabsf(fft_tmp[nr_samp/2])*scl;

	return fft;
}

void audio_update(const float *in, int n)
{
	static int bufp = 0;
	static float *cur_buf = NULL; //need to preserve the result of tb_get_write across calls
	static float *prev_buf = NULL;

	if(prev_buf == NULL) prev_buf = tribuf_get_read_nolock(samp_tb);

	float *samps = NULL;
	int remain = 0;

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
		} else {
			return; // not enough samples yet
		}
	}

	buf_count++;
	float *fft = do_fft(samps, prev_buf);
	beat_ctx_update(gbl_beat_ctx, fft, nr_samp/2);
	prev_buf = samps;

	if(remain > 0) audio_update(in, remain);
}

int audio_get_samples(audio_data *d) {
	d->len = nr_samp/2;
	d->data = tribuf_get_read(samp_tb);
	return 0;
}
void audio_finish_samples(void) { tribuf_finish_read(samp_tb); }

#define MAX_SAMP 2048

// never need more memory than we get here.
static float samp_bufs[MAX_SAMP*3] __attribute__ ((aligned (16)));
static void *samp_data[3];

// sr is sample rate
int audio_setup(int sr)
{
	nr_samp = (sr<50000)?MAX_SAMP/2:MAX_SAMP;

	printf("Sample Rate %i\nUsing %i samples/buffer\n", sr, nr_samp/2);

	fft_tmp = malloc(sizeof(float) * nr_samp); // do xform in place
	if(!fft_tmp) abort();

	samp_data[0] = samp_bufs + MAX_SAMP*0;
	samp_data[1] = samp_bufs + MAX_SAMP*1;
	samp_data[2] = samp_bufs + MAX_SAMP*2;
	memset(samp_data[0], 0, sizeof(float) * MAX_SAMP * 3);
	samp_tb = tribuf_new(samp_data, 0);

	gbl_beat_ctx = beat_new();

	return 0;
}

