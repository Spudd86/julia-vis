#ifndef RUNSTAT_H
#define RUNSTAT_H

struct runstat {
	int n;
	int sum;
	int64_t sqrsum;
	int data[];
};

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

static int runstat_varience(struct runstat *self) {
	return (self->sqrsum - self->sum*(int64_t)self->sum/self->n)/self->n;
}

static int runstat_average(struct runstat *self) {
	return self->sum/self->n;
}

#endif

