
#include "common.h"
#include "points.h"

//#include "mtwist/mtwist.h"
#include "isaac/isaac.h"

#ifdef _WIN32
#undef WIN32
#define WIN32
#endif

// for seeding
#ifdef WIN32
#include <sys/timeb.h>
#else /* WIN32 */
#include <sys/time.h>
#endif

static void seed(struct isaac_ctx *ctx)
{
#ifdef WIN32
    struct _timeb	tb;		/* Time of day (Windows mode) */
    (void) _ftime (&tb);
    isaac_init(ctx, (void *)&tb, sizeof(tb));
#else /* WIN32 */
    struct timeval	tv;		/* Time of day */
    (void) gettimeofday (&tv, NULL);
    isaac_init(ctx, (void *)&tv, sizeof(tv));
#endif /* WIN32 */
}

void destroy_point_data(struct point_data *pd)
{
	free(pd->rng);
	free(pd->v);
	free(pd->p);
	free(pd->t);
	free(pd);
}

struct point_data *new_point_data(int dim)
{
	struct point_data *pd = xmalloc(sizeof(struct point_data));
	pd->rng = xmalloc(sizeof(isaac_ctx));
	seed(pd->rng);

	pd->dim = dim;
	pd->done_time = 0;

	pd->v = xmalloc(sizeof(float)*pd->dim);
	pd->p = xmalloc(sizeof(float)*pd->dim);
	pd->t = xmalloc(sizeof(float)*pd->dim);

	for(int i=0; i<pd->dim; i++) {
/*		pd->t[i] = 0.5f*((isaac_next_uint32(pd->rng)%2048)*2.0f/2048 - 1.0f);*/
/*		pd->p[i] = 0.5f*((isaac_next_uint32(pd->rng)%2048)*2.0f/2048 - 1.0f);*/
		pd->t[i] = 0.5f*isaac_next_signed_float(pd->rng);
		pd->p[i] = 0.5f*isaac_next_signed_float(pd->rng);
		pd->v[i] = 0.0f;
	}

	return pd;
}

void update_points(struct point_data *pd, unsigned int passed_time, int retarget)
{
	if(retarget) {
		for(int i=0; i<pd->dim; i++)
			pd->t[i] = 0.5f*((isaac_next_uint32(pd->rng)%2048)*2.0f/2048 - 1.0f);
/*			pd->t[i] = 0.5f*isaac_next_signed_float(pd->rng);*/
	}

	const float tsped = 0.002f;
	const unsigned int steps_ps = 150;

	//TODO change to use the fact that:
	// we can properly step an exponential with
	// blend = 1.0f - pow(<constant>, delta_t*speed)
	// lerp(pos, target, blend)
	// Need to review what this code is actually doing to figure out how to apply this
	// applying it should get us a small speed up on the main thread, probably too small to measure
	// but it's also cleaner and we should be clean.
	// It might be easier to unpack what the code is computing and work out the integral
	// See this video https://www.youtube.com/watch?v=yGhfUcPjXuE&t=502s for starting thoughts on the integration

	// go through this mess here to make sure we're running the loop some integer multiple of the framerate times
	// (assuming constant framerate that is)
	// this seems to give nice smooth movement at ALL framerates so we'll
	// stick with it (getting smooth motion with some kind of framerate
	//   independance took some time)
	// also suposed to do at least steps_ps every second
	unsigned int del = (passed_time - pd->done_time);
	unsigned int dt = del;
	int steps = 1;
	if(del > 0) {
		//while(dt > (1000/steps_ps)) dt = dt/2;
		while(dt*steps_ps > 1000) dt = dt/2;
		steps = del/dt;
	} else dt=1;

	const float delt  = 30*dt/1000.0f;
	float tmp[pd->dim];
	for(int j=0; j<steps; j++) {
		for(int i=0; i < pd->dim; i++) tmp[i] = pd->t[i] - pd->p[i];

		float mag = 0;
		for(int i=0; i < pd->dim; i++) mag += tmp[i]*tmp[i];
		mag=(mag>0)?delt*0.4f/sqrtf(mag):0;

		for(int i=0; i < pd->dim; i++) pd->v[i] = (pd->v[i] + tmp[i]*mag*tsped)/(tsped+1);
		for(int i=0; i < pd->dim; i++) pd->p[i] = pd->p[i] + pd->v[i]*delt;
	}

	pd->done_time=passed_time;
}
