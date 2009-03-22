#ifndef POINTS_H_
#define POINTS_H_

#include "common.h"
#include <stdint.h>

struct point_data {
	float *p, *v, *t;
	uint64_t done_time;
	int dim;
	
};

struct point_data *new_point_data(int dim);
void update_points(struct point_data *pd, unsigned int passed_time, int retarget);

#endif