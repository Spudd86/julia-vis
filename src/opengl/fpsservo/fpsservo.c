
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
#define BASE_SLACK 500
#define MIN_SLACK 1500
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
	int64_t frame_start;
	int64_t last_swap_time;
	uint64_t msc;
	
	int interval; ///< how many swap periods between our swaps
	
	int count;
	
	struct runstat *workstat, *swapstat, *delaystat, *slackstat;
};

int fps_get_hist_len(struct fps_data *self) { (void)self;
	return FPS_HIST_LEN;
}

void fps_get_worktimes(struct fps_data *self, int64_t *total, const int **worktimes) {
	if(total) *total = self->workstat->sum;
	if(worktimes) *worktimes = self->workstat->data;
}

void fps_get_frametimes(struct fps_data *self, int64_t *total, const int **worktimes) {
	if(total) *total = self->swapstat->sum;
	if(worktimes) *worktimes = self->swapstat->data;
}

void fps_get_delays(struct fps_data *self, int64_t *total, const int **worktimes) {
	if(total) *total = self->delaystat->sum;
	if(worktimes) *worktimes = self->delaystat->data;
}

void fps_get_slacks(struct fps_data *self, int64_t *total, const int **worktimes) {
	if(total) *total = self->slackstat->sum;
	if(worktimes) *worktimes = self->slackstat->data;
}

int fps_get_cur_slack(struct fps_data *self) {
	return self->slack;
}

int64_t fps_get_target_msc(struct fps_data *self) {
	return self->msc;
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
	self->slack = MAX((self->period.n*15)/(100*self->period.d), MIN_SLACK);
	
	printf("freq %d (%d/%d) period %" PRId64 "/%" PRId64 "\n",
	       freq.n/freq.d, freq.n, freq.d, self->period.n, self->period.d);
	printf("slack = %d\n", self->slack);
	printf("init msc = %" PRId64 "\n", init_msc);
	
	self->last_swap_time = now;
	self->msc = init_msc;
	self->interval = 1;
	//self->interval = 2; self->period.n *=2;

	int worktime = (self->period.n)/(self->period.d*4);
	self->workstat = runstat_new(worktime, FPS_HIST_LEN);
	self->swapstat = runstat_new(0, FPS_HIST_LEN);
	self->delaystat = runstat_new(0, FPS_HIST_LEN);
	self->slackstat = runstat_new(self->slack, FPS_HIST_LEN);
}

struct fps_data *fps_data_new(struct fps_period rate, uint64_t init_msc, uint64_t now)
{
	struct fps_data *self = calloc(1,sizeof(*self));//malloc(sizeof(*self));
	if(!self) return NULL;
	init(self, rate, init_msc, now);
	return self;
}

//TODO: need to have some way to profile what value slack and delay have!
// ideally with a graph we can overly on top of whatever
// also would like to track when we miss a swap deadline, again profile, 
// possibly something like the audio-test does with beats
// maybe even show on a worktime graph a line where we think we'll end up 
// missing a deadline if we go over it

void frame_start(struct fps_data *self, int64_t now)
{
	self->frame_start = now;
}

/**
 * Call this just before XXXSwapBuffers()
 * 
 * @return target msc (for use with glXSwapBuffersMscOML()
 */
int64_t swap_begin(struct fps_data *self, int64_t now)
{
	// compute our target value of 'now'
	int64_t expected = ((self->last_swap_time - self->slack)*self->period.d + self->period.n)/self->period.d;
	self->slack_diff = expected - now;
	
	runstat_insert(self->workstat, self->count, now - (self->frame_start));
	
	//printf("now %" PRId64 " expected %" PRId64 "\n", now, expected);
	//printf("slackdiff %" PRId64 "\n", self->slack_diff);
	
	int period = self->period.n/self->period.d;
	int avgworktime = runstat_average(self->workstat);
	int wktime_stdev = runstat_stddev(self->workstat);
	
	int slack_target = BASE_SLACK;
	
	// if we assume worktime is expontially distributed with a shift then:
	//       want x s.t. p[t_wk > x] <= 0.002
	// x = -ln(0.002)/lambda = -ln(0.002)*wktime_stdev ~= 6.2*wktime_stdev
	// the distribution is shifted
	//slack_target += (wktime_stdev*60)/10;
	slack_target += (wktime_stdev*31)/10;
	
	slack_target += runstat_stddev(self->swapstat) + MAX(runstat_average(self->swapstat), 0);
	
	slack_target = MIN((int)((self->period.n*3)/(self->period.d*4)), MAX(slack_target, MIN_SLACK));
	
	if(self->slack > slack_target) self->slack = (self->slack*7 + slack_target)/8;
	else self->slack = slack_target;
	
	if(self->slack_diff < 0) {
		//self->delay += MIN(self->slack_diff, -MIN_SLACK/2);
		//self->delay = self->delay*3/4;
		self->delay = (self->delay/4 > -self->slack_diff/2) ? self->delay*3/4 : self->delay + self->slack_diff/2;
	} else {
		self->delay += MAX(self->slack_diff/4, 1);
	}
	
	self->delay = MAX(self->delay, 0);
	
	if(self->delay + avgworktime + self->slack > period) {
		int newdelay = period - (self->slack + avgworktime);
		self->delay = MAX(newdelay, 0);
	}
	
	self->count++;
	
	runstat_insert(self->slackstat, self->count, self->slack);
	
	self->msc += self->interval;

	return self->msc;
}

/**
 * @return delay from now to start drawing next frame
 */
int swap_complete(struct fps_data *self, int64_t now, uint64_t msc, uint64_t sbc)
{(void)sbc;
	//TODO: figure out why this seems to be insane
#if 1
	if(self->msc < msc) {
		printf("missed %" PRIu64 " swaps by %" PRId64 " slack = %d\n", msc - self->msc, self->slack_diff, self->slack);
		self->msc = msc;
	}
#endif
	
	//TODO: we need to do stuff to estimate work time, then use the esitimate
	// to keep delay + work + slack < period*interval
	
	//TODO: maybe track excpected swap_begin() here, so we can muck around with
	// returned delay without messing up our estimates for that and getting
	// worktime with jitter in it that isn't real
	//TODO: try to lock on to an estimate of when we'd really like to get the
	// swap event and if the actual timing isn't stable try to schedual relative
	// to the estimate, then let the estimate try to track the real times so
	// we don't drift off and end up thinking we are at frame start when we are
	// really somewhere in the middle
	
	int64_t expected = (self->last_swap_time*self->period.d + self->period.n)/self->period.d;
	int timediff = now - expected;
	runstat_insert(self->swapstat, self->count, timediff%(self->period.n/self->period.d));
	self->last_swap_time = now;

	if(timediff*self->period.d > self->period.n) printf("after expected swap by %d (we must have missed it!)\n", timediff);

	// compute our delay, update counters and stuff
	//TODO: be more useful about this
	if(timediff*self->period.d > self->period.n) {
		self->delay = 0;
	} else if(timediff > 0) {	
		self->delay = MAX(self->delay - timediff/2, 0);
	}
	
	runstat_insert(self->delaystat, self->count + 1, self->delay);
	
	if(self->count < FPS_HIST_LEN) return 0;
	return self->delay;
}


