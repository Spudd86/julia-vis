#ifndef POINTS_H_
#define POINTS_H_

struct point_data {
	void *rng;
	float *p, *v, *t;
	unsigned int done_time;
	int dim;
};

struct point_data *new_point_data(int dim);
void update_points(struct point_data *pd, unsigned int passed_time, int retarget);

#endif
