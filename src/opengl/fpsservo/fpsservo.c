
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

#define MINFPS 20 // if period*interval goes over this number give up and run as fast as possible

#if USEC
#define ONE_SECOND INT64_C(1000000)
#define BASE_SLACK 500
#define MIN_SLACK 1500
#else
#define ONE_SECOND 1000
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
	int max_interval;
	
	int count;
	
	struct runstat *workstat, *swapstat, *delaystat, *slackstat, *tot_frametime;
};

int fps_get_hist_len(struct fps_data *self) { (void)self;
	return FPS_HIST_LEN;
}

void fps_get_total_frametimes(struct fps_data *self, int64_t *total, const int **worktimes) {
	if(total) *total = self->tot_frametime->sum;
	if(worktimes) *worktimes = self->tot_frametime->data;
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

int fps_get_cur_swap_interval(struct fps_data *self) {
	return self->interval;
}

int64_t fps_get_target_msc(struct fps_data *self) {
	return self->msc;
}


static uint64_t gcd(uint64_t a, uint64_t b) {
	while(b != 0) {
		uint64_t t = b;
		b = a % b;
		a = t;
	}
	return a;
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
	self->period.n = freq.d*ONE_SECOND;
	self->period.d = freq.n;

	uint64_t tmp = gcd(self->period.n, self->period.d);
	self->period.n /= tmp, self->period.d /= tmp;
	
	self->max_interval = MAX(freq.n/(freq.d*MINFPS), 1);

	// try about 15% of period for initial slack value
	self->slack = MAX((self->period.n*15)/(100*self->period.d), MIN_SLACK);
	
	printf("freq %d (%d/%d) period %" PRId64 "/%" PRId64 "\n",
	       freq.n/freq.d, freq.n, freq.d, self->period.n, self->period.d);
	printf("max interval %d\n", self->max_interval);
	printf("slack = %d\n", self->slack);
	printf("init msc = %" PRId64 "\n", init_msc);
	
	self->last_swap_time = now;
	self->msc = init_msc;
	self->interval = 1;
	//self->interval = 2;

	int worktime = (self->period.n)/(self->period.d*4);
	self->workstat = runstat_new(worktime, FPS_HIST_LEN);
	self->swapstat = runstat_new(0, FPS_HIST_LEN);
	self->delaystat = runstat_new(0, FPS_HIST_LEN);
	self->slackstat = runstat_new(self->slack, FPS_HIST_LEN);
	self->tot_frametime = runstat_new(self->period.n/self->period.d, FPS_HIST_LEN);
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
	int64_t expected = ((self->last_swap_time - self->slack)*self->period.d + self->interval*self->period.n)/self->period.d;
	self->slack_diff = expected - now;
	
	runstat_insert(self->workstat, self->count, now - (self->frame_start));
	
	//printf("now %" PRId64 " expected %" PRId64 "\n", now, expected);
	//printf("slackdiff %" PRId64 "\n", self->slack_diff);
	
	int period = (self->period.n*self->interval)/self->period.d;
	int avgworktime = runstat_average(self->workstat);
	int wktime_stdev = runstat_stddev(self->workstat);

#if 0
	if(self->count > FPS_HIST_LEN) {
		//TODO: after changing interval need a cooldown before it can change in the other direction to avoid hysteresis
		// (ie: after it goes from 1 to 2 it can go to 3 on the next frame but it can't go back to 1 for a while)

		uint64_t req_time = avgworktime + wktime_stdev*31/10 + abs(runstat_average(self->swapstat)) + MIN_SLACK;

		// if probably can't meet our deadlines, increment interval
		if(req_time*self->period.d > (self->interval*self->period.n)) {
			self->interval = (req_time*self->period.d)/self->period.n;
			self->delay = 0;
		}

		if(req_time*self->period.d < (self->interval*self->period.n*3)/4) {
			self->interval = (req_time*self->period.d)/self->period.n;
			//self->delay = 0;
		}

		self->interval = MIN(self->max_interval+1, MAX(self->interval, 1));
	}
#endif
	
	int slack_target = BASE_SLACK;
	
	// if we assume worktime is expontially distributed with a shift then:
	//       want x s.t. p[t_wk > x] <= 0.002
	// x = -ln(0.002)/lambda = -ln(0.002)*wktime_stdev ~= 6.2*wktime_stdev
	// the distribution is shifted
	//slack_target += (wktime_stdev*62)/10;
	slack_target += (wktime_stdev*31)/10;
	
	// even if swap is usually negative it represents uncertainty in our measurements and latency in the window system
	// so it should contribute to slack
	slack_target += runstat_stddev(self->swapstat) + abs(runstat_average(self->swapstat));
	
	slack_target = MIN((int)((self->period.n*self->interval*3)/(self->period.d*4)), MAX(slack_target, MIN_SLACK));
	
	if(self->slack > slack_target) self->slack = (self->slack*7 + slack_target)/8;
	else self->slack = slack_target;

	self->slack = MIN(self->slack, (self->period.n*self->interval*3)/(self->period.d*4));
	
	if(self->slack_diff < 0) {
		//self->delay += MIN(self->slack_diff, -MIN_SLACK/2);
		//self->delay = self->delay*3/4;
		//self->delay = (self->delay/4 > -self->slack_diff/2) ? self->delay*5/6 : self->delay + self->slack_diff/3;
		//self->delay += self->slack_diff/2;
		self->delay = self->delay*9/10 + self->slack_diff/2;
	} else {
		self->delay += self->slack_diff/4;
	}
	
	self->delay = MAX(self->delay, 0);
	
	if(self->delay + avgworktime + self->slack > period) {
		int newdelay = period - (self->slack + avgworktime);
		self->delay = MAX(newdelay, 0);
	}
	
	runstat_insert(self->slackstat, self->count, self->slack);

	if(self->interval > self->max_interval) return 0; // give up, run as fast as possible
	
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

	else runstat_insert(self->tot_frametime, self->count, now - self->last_swap_time);
	
	int64_t expected = (self->last_swap_time*self->period.d + self->interval*self->period.n)/self->period.d;
	int timediff = now - expected;
	runstat_insert(self->swapstat, self->count, timediff);//%((self->interval*self->period.n)/self->period.d));
	self->last_swap_time = now;

	if(timediff*self->period.d > self->period.n*self->interval) printf("after expected swap by %d (we must have missed it!)\n", timediff);

	// compute our delay, update counters and stuff
	//TODO: be more useful about this
	if(timediff*self->period.d > self->period.n*self->interval) {
		self->delay = 0;
	} else if(timediff > 0) {	
		self->delay = MAX(self->delay - timediff/3, 0);
	}
	
	int res = self->delay;
	if(self->count < FPS_HIST_LEN || self->interval > self->max_interval) {
		res = 0;
	}

	runstat_insert(self->delaystat, self->count, res);
	self->count++;
	return res;
}


