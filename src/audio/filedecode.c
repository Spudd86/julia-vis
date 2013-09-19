#include "common.h"

#include "audio.h"
#include "audio-private.h"

#if __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
#	include <time.h>
#	include <threads.h>
#else
#	include "tinycthread.h"
#endif

#include <sndfile.h>

#define	BUFFER_LEN 1024

// need to downmix to mono

static SNDFILE *infile = NULL;
static SF_INFO sfinfo;
static thrd_t decode_tid;

// TODO: use an atomic here, this works but the C11 spec says it doesn't have to
// we can use a relaxed atomic for this though, which should be really cheap
static int running = 1; 

static int decode_thread(void *parm);

int filedecode_setup(const opt_data *od)
{
	const char *filename = od->audio_opts;
	printf("Using %s for audio input\n", filename);
	
	if (!(infile = sf_open (filename, SFM_READ, &sfinfo))) {
    	printf ("Unable to open input file %s: %s\n", filename, sf_strerror (NULL)) ;
        return -1;
    }
	audio_setup(sfinfo.samplerate);
	
	//TODO: stop using globals for the thread, pass in a struct
	if(thrd_create(&decode_tid, decode_thread, NULL) != thrd_success) {
		sf_close(infile);
		printf ("Failed to start decode thread") ;
		return -1;
	}
	
	return 0;
}

void filedecode_shutdown(void)
{
	//TODO: stop the decode thread and join it
	running = 0;
	int r;
	// failed to join, just give up on shutting down, process will exit soon
	if(thrd_join(decode_tid, &r) != thrd_success) return;
	sf_close(infile);
}

//TODO: check if we are on Linux and use CLOCK_MONOTONIC if we are

// timespec_get doesn't seem to be defined by glibc yet
// so we must use clock_gettime
// overall we would prefer using CLOCK_MONOTONIC anyway... but that's not portably
// an option
#define timespec_get(tsptr, clk) clock_gettime(clk, tsptr)

//TODO: more complex buffering?
static int decode_thread(void *parm)
{
	float data[BUFFER_LEN*sfinfo.channels];
	struct timespec t0;
	uint64_t frms_sent = 0;
	timespec_get(&t0, TIME_UTC);
	while(running) {
		sf_count_t readcount = sf_readf_float(infile, data, BUFFER_LEN);
		if(readcount == 0) {
			//TODO: loop
			return 1;
		}
		
		frms_sent += readcount;
		
		// downmix to mono if needed
		if(sfinfo.channels != 1) { //TODO: improve downmix to use channel map when possible
			for(int j = 1; j < sfinfo.channels; j++) { //TODO: Kahan sum?
				data[0] += data[j];
			}
			for(int i = 1; i < readcount; i++) {
				data[i] = 0.0f; // TODO: mix into our data array
				for(int j = 0; j < sfinfo.channels; j++) { //TODO: Kahan sum?
					data[i] += data[i*sfinfo.channels + j];
				}
			}
		}
		
	
		//TODO: use a better sleep function on Linux, tinycthread uses usleep so it can't quite get the timing exactly right
		// not much of a problem since the error won't ever grow
		uint64_t ns = (frms_sent * 1000000000L / sfinfo.samplerate);
		struct timespec wake_time = { t0.tv_sec + (time_t)(ns/1000000000L),  t0.tv_nsec + ns%1000000000L};
		thrd_sleep(&wake_time, NULL); // sleep until it is time to send the data in
		
		audio_update(data, readcount);
    }
    
    return 0;
}

