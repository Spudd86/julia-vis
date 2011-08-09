
#include "common.h"
#include <stdint.h>
#include <inttypes.h>
#include <math.h>

#include "fpsservo.h"
#include "runstat.h"

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


#define FPS_HIST_LEN 128
#define USEC 1

#if USEC
#define BASE_SLACK 1500
#define MIN_SLACK 2000
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
	
	struct runstat *workstat;
};

void fps_get_worktimes(struct fps_data *self, int *total, int *len, const int **worktimes) {
	if(total) *total = self->workstat->sum;
	if(len) *len = self->workstat->n;
	if(worktimes) *worktimes = self->workstat->data;
}

int fps_get_cur_slack(struct fps_data *self) {
	return self->slack;
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
	self->workstat = runstat_new(worktime, FPS_HIST_LEN);
}

struct fps_data *fps_data_new(struct fps_period rate, uint64_t init_msc, uint64_t now)
{
	struct fps_data *self = calloc(1,sizeof(*self));//malloc(sizeof(*self));
	if(!self) return NULL;
	init(self, rate, init_msc, now);
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

//TODO: need to have some way to profile what value slack and delay have!
// ideally with a graph we can overly on top of whatever
// also would like to track when we miss a swap deadline, again profile, 
// possibly something like the audio-test does with beats
// maybe even show on a worktime graph a line where we think we'll end up 
// missing a deadline if we go over it

/**
 * Call this just before XXXSwapBuffers()
 * 
 * @return target msc (for use with glXSwapBuffersMscOML()
 */
int64_t swap_begin(struct fps_data *self, int64_t now)
{
	int period = self->period.n/self->period.d;

	runstat_insert(self->workstat, self->count, now - (self->last_swap_time + self->delay));

	int avgworktime = runstat_average(self->workstat);
	int varience = runstat_varience(self->workstat);
	int wktime_stdev = (int)isqrt(varience);
	
	// if we assume worktime is expontially distributed then:
	//       want x s.t. p[t_wk > x] <= 0.002
	// x = -ln(0.002)/lambda = -ln(0.002)*wktime_stdev ~= 6.2*wktime_stdev
	// need to add the smallest worktime seen to compensate for the fact that 
	// the distribution is shifted
	//int min_wkt = self->worktimes[0]; for(int i=1; i<WORK_HIST_LEN; i++) min_wkt = MIN(min_wkt, self->worktimes[i]);
	//int slack_target = MIN(self->period.n*2/(self->period.d*3), MAX(wktime_stdev*63/10 + min_wkt + BASE_SLACK, MIN_SLACK));
	
	// if we assume normal distribution then we want this instead
	int slack_target = MIN(self->period.n*2/(self->period.d*3), MAX(wktime_stdev*6 + avgworktime + BASE_SLACK, MIN_SLACK));
	// however neither is right, but normal is probably more wrong, however we 
	// don't need to do a pass over the worktimes to get an average
	// also trying to use same probability as above is not big enough...
	
	//TODO: maybe make slack decrease in an interpolated way...
	if(self->slack > slack_target) self->slack = (self->slack*3 + slack_target)/4;
	else self->slack = slack_target;

	// compute our target value of 'now'
	int64_t expected = (self->last_swap_time*self->period.d + self->period.n)/self->period.d - self->slack;
	self->slack_diff = expected - now;
	//self->delay += self->slack_diff/2;
	if(self->slack_diff < 0) self->delay += MIN(self->slack_diff/2, -MIN_SLACK);
	//else self->delay += MAX(period/32, 1);
	else self->delay += MAX(self->slack_diff/4, 1);
	
	//printf("now %" PRId64 " expected %" PRId64 "\n", now, expected);
	//printf("slackdiff %" PRId64 "\n", self->slack_diff);
	
	if(self->delay + avgworktime + self->slack > period) {
		int newdelay = period - (self->slack + avgworktime);
		//printf("DELAY CAPPED! reduced by %d\n", self->delay - newdelay);
		self->delay = newdelay;
	}
	
	if(self->delay < 0) {
		//printf("DELAY NEGATIVE\n");
		self->delay = 0;
	} 
	
	self->count++;
	
	return self->msc + self->interval;
}

/**
 * @return delay from now to start drawing next frame
 */
int swap_complete(struct fps_data *self, int64_t now, uint64_t msc, uint64_t sbc)
{

	//TODO: figure out why this seems to be insane
#if 0
	self->msc += self->interval;
	if(self->msc < msc) {
		//printf("missed %d swaps by %d slack = %d\n", msc - self->msc, self->slack_diff, self->slack);
		self->msc = msc;
		
		
	}
#endif
	
	//TODO: we need to do stuff to estimate work time, then use the esitimate
	// to keep delay + work + slack < period*interval
	
	int64_t expected = (self->last_swap_time*self->period.d + self->period.n)/self->period.d;
	int timediff = now - expected;
	
	self->last_swap_time = now;
	
	// compute our delay, update counters and stuff
	
	if(timediff > self->period.n/self->period.d)
		printf("after expected swap by %d (we must have missed it!)\n", timediff);
		
	//TODO: mabey track stddev of timediff%period and use that in slack calculation?

	//TODO: be more useful about this	
	if(timediff*self->period.d > self->period.n) self->delay = 0;
	else if(timediff > 0) self->delay = MAX(self->delay - timediff, 0);
	
	//self->delay = MAX(self->delay - self->slack/500, 0);
	
	//TODO: maybe include an estimator for winsys delay and jitter, like we have
	// for worktime

	return self->delay;
}


