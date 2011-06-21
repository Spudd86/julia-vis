
#include "common.h"
#include <stdint.h>
#include <inttypes.h>
#include <math.h>

#include "fpsservo.h"

/* From GLX_OML_sync_control

    This extension incorporates the use of three counters that provide
    the necessary synchronization. The Unadjusted System Time (or UST)
    is a 64-bit monotonically increasing counter that is available
    throughout the system. UST is not a resource that is controlled
    by OpenGL, so it is not defined further as part of this extension.
    The graphics Media Stream Counter (or graphics MSC) is a counter
    that is unique to the graphics subsystem and increments for each
    vertical retrace that occurs. The Swap Buffer Counter (SBC) is an
    attribute of a GLXDrawable and is incremented each time a swap
    buffer action is performed on the associated drawable.
*/

/*
	if we don't have INTEL_swap_event can use glXWaitForSbcOML() to wait for 
	the swap to actually happen, since glXSwapBuffersMscOML() returns the sbc we
	would have to wait for. The only problem is we can't do something else in
	the mean time with that...
 */


#define WORK_HIST_LEN 16
#define USEC 1

#if USEC
#define BASE_SLACK 500
#define MIN_SLACK 1000
#else
#define BASE_SLACK 1
#define MIN_SLACK 2
#endif

struct fps_data {
	int delay, slack;
	
	struct {
		int64_t n, d;
	} period; ///< how often swaps happen (hardware) as a fraction in ms
	
	int64_t slack_diff; ///< difference between when we swapped and when we would have liked to
	int64_t last_swap_time;
	uint64_t msc;
	
	int interval; ///< how many swap periods between our swaps
	
	int count;
	
	int totworktime;
	int64_t work_powsumavg_n;
	int worktimes[WORK_HIST_LEN];
};

void fps_get_worktimes(struct fps_data *self, int *total, int *len, const int **worktimes) {
	if(total) *total = self->totworktime;
	if(len) *len = WORK_HIST_LEN;
	if(worktimes) *worktimes = self->worktimes;
}

/**
 * fill in the fps_data struct
 * 
 * @self the struct to fill in
 * @rate the rate at which swaps happen in Hz
 */
static void init(struct fps_data *self, struct fps_period freq, uint64_t init_msc, uint64_t now)
{
	memset(self, 0, sizeof(*self));
#if USEC
	self->period.n = freq.d*INT64_C(1000000);
	self->period.d = freq.n;
#else
	self->period.n = freq.d*1000;
	self->period.d = freq.n;
#endif

	// try about 15% of period for initial slack value
	self->slack = MAX(self->period.n*15/(100*self->period.d), MIN_SLACK);
	
	printf("freq %d (%d/%d) period %" PRId64 "/%" PRId64 "\n", 
	       freq.n/freq.d, freq.n, freq.d, self->period.n, self->period.d);
	printf("slack = %d\n", self->slack);
	
	self->last_swap_time = now;
	self->msc = init_msc;
	self->interval = 1;
	
	int worktime = (self->period.n - self->slack*self->period.d)/self->period.d;
	self->totworktime = WORK_HIST_LEN*worktime;
	self->work_powsumavg_n = WORK_HIST_LEN*(int64_t)worktime*worktime;
	for(int i=0; i < WORK_HIST_LEN; i++) {
		self->worktimes[i] = worktime;
	}
	
	printf("powsum %" PRId64 "\n", self->work_powsumavg_n/WORK_HIST_LEN);
}

struct fps_data *fps_data_new(struct fps_period rate, uint64_t init_msc, uint64_t now)
{
	struct fps_data *self = calloc(1,sizeof(*self));//malloc(sizeof(*self));
	printf("1 fps_data %p\n", self);
	if(!self) return NULL;
	init(self, rate, init_msc, now);
	printf("2 fps_data %p\n", self);
	return self;
}

#define iter1(N) \
    try = root + (1 << (N)); \
    if (n >= try << (N))   \
    {   n -= try << (N);   \
        root |= 2 << (N); \
    }

static int32_t isqrt(uint32_t n)
{
    uint32_t root = 0, try;
    iter1 (15);    iter1 (14);    iter1 (13);    iter1 (12);
    iter1 (11);    iter1 (10);    iter1 ( 9);    iter1 ( 8);
    iter1 ( 7);    iter1 ( 6);    iter1 ( 5);    iter1 ( 4);
    iter1 ( 3);    iter1 ( 2);    iter1 ( 1);    iter1 ( 0);
    return root >> 1;
}

/**
 * Call this just before XXXSwapBuffers()
 * 
 * @return target msc (for use with glXSwapBuffersMscOML()
 */ 
int64_t swap_begin(struct fps_data *self, int64_t now)
{
	// if we've run in to our slack time we need to shorten delay
#if 1
	int worktime = now - (self->last_swap_time + self->delay);
	int oldwt = self->worktimes[self->count % WORK_HIST_LEN];
	self->totworktime -= oldwt;
	self->totworktime += worktime;
	self->work_powsumavg_n += worktime*worktime - oldwt*oldwt;
	self->worktimes[self->count % WORK_HIST_LEN] = worktime;
	
	int varience = (self->work_powsumavg_n - self->totworktime*(int64_t)(self->totworktime/WORK_HIST_LEN))/WORK_HIST_LEN;
	int wktime_stdev = (int)isqrt(varience);
	
	//TODO: need to have some way to profile what value slack and delay have!
	// ideally with a graph we can overly on top of whatever
	// also would like to track when we miss a swap deadline, again profile, 
	// possibly something like the audio-test does with beats
	// maybe even show on a worktime graph a line where we think we'll end up 
	// missing a deadline if we go over it
	self->slack = MAX(wktime_stdev*2 + BASE_SLACK, MIN_SLACK);
	//printf("powsum %d varience %d stdev %d\n", self->work_powsumavg_n/WORK_HIST_LEN, varience, wktime_stdev);
#endif
	// compute our target value of 'now'
	int64_t expected = (self->last_swap_time*self->period.d + self->period.n)/self->period.d - self->slack;
	self->slack_diff = expected - now;
	self->delay += self->slack_diff/4;
	
	//printf("now %" PRId64 " expected %" PRId64 "\n", now, expected);
	//printf("slackdiff %" PRId64 "\n", self->slack_diff);
	
	int avgworktime = self->totworktime/WORK_HIST_LEN;
	int64_t period = self->period.n/self->period.d;
	if(self->delay + avgworktime + MIN_SLACK > period) {
		int newdelay = period - (self->slack + avgworktime);
		//printf("DELAY CAPPED! reduced by %d\n", self->delay - newdelay);
		self->delay = newdelay;
	}
	
	if(self->delay < 0) self->delay = 0;
	if((self->delay+self->slack+1)*self->period.d > self->period.n) self->delay = 0;
	
	return self->msc + self->interval;
}

/**
 * @return delay from now to start drawing next frame
 */
int swap_complete(struct fps_data *self, int64_t now, uint64_t msc, uint64_t sbc)
{

	//TODO: figure out why this seems to be insane
#if 1
	self->msc += self->interval;
	if(self->msc < msc) {
		//printf("missed %d swaps by %d slack = %d\n", msc - self->msc, self->slack_diff, self->slack);
		self->msc = msc;
	}
#endif
	
	//TODO: we need to do stuff to estimate work time, then use the esitimate
	// to keep delay + work + slack < period*interval
	
	self->last_swap_time = now;
	
	// compute our delay, update counters and stuff
	self->count++;
	return self->delay;
}


