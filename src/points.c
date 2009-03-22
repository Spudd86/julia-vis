
#include "common.h"
#include "points.h"

#include "mtwist/mtwist.h"

#include <math.h>

struct point_data *new_point_data(int dim) 
{
	struct point_data *pd = xmalloc(sizeof(struct point_data));
	
	pd->dim = dim;
	pd->done_time = 0;
	
	pd->v = xmalloc(sizeof(float)*pd->dim);
	pd->p = xmalloc(sizeof(float)*pd->dim);
	pd->t = xmalloc(sizeof(float)*pd->dim);
	
	mt_goodseed(); // seed our PSRNG
	for(int i=0; i<pd->dim; i++) {
		pd->t[i] = 0.5f*((mt_lrand()%2048)*2.0f/2048 - 1.0f);
		pd->p[i] = 0.5f*((mt_lrand()%2048)*2.0f/2048 - 1.0f);
		pd->v[i] = 0.0f;
	}
	
	return pd;
}

/**
 * @param pd 
 * @param del time since last update in milliseconds
 */
void update_points(struct point_data *pd, unsigned int passed_time, int retarget)
{
	if(retarget) {
		for(int i=0; i<pd->dim; i++) 
			pd->t[i] = 0.5f*((mt_lrand()%2048)*2.0f/2048 - 1.0f);
	}
	
	const float tsped = 0.002f;
	const int steps_ps = 150;
	
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
		while(dt > 1000/steps_ps) dt = dt/2;
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