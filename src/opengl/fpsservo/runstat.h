#ifndef RUNSTAT_H
#define RUNSTAT_H

struct runstat {
	int n;
	int64_t sum;
	int64_t sqrsum;
	int data[];
};

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

#undef iter1

static struct runstat *runstat_new(int def, int len) {
	struct runstat *self = malloc(sizeof(*self) + sizeof(self->data[0])*len);
	self->n = len;
	
	self->sum = self->n*def;
	self->sqrsum = self->n*(int64_t)def*def;
	for(int i=0; i < self->n; i++) {
		self->data[i] = def;
	}
	
	return self;
}

static void runstat_insert(struct runstat *self, int i, int v) {
	i = i % self->n;
	int oldv = self->data[i];
	self->sum -= oldv;
	self->sum += v;
	self->sqrsum -= oldv*oldv;
	self->sqrsum += v*v;
	self->data[i] = v;
}

static int runstat_varience(const struct runstat *self) {
	return (self->sqrsum - self->sum*(int64_t)self->sum/self->n)/self->n;
}

static int runstat_stddev(const struct runstat *self) {
	return isqrt(runstat_varience(self));
}

static int runstat_average(const struct runstat *self) {
	return self->sum/self->n;
}

#endif

