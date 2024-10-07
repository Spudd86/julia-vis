#ifndef GETSAMP_H
#define GETSAMP_H

// TODO: write a stateful version, or one that just takes the total sample count and outputs an array

struct samp_state {
	const float *data;
	int len;
	int w;  // window width
	int pl; // lower edge of last sample window
	int pu; // upper edge of last sample window
	float sum;
	float err;
	float scale;
};

static void getsamp_step_init(struct samp_state *self, const float *data, int len, int w)
{
	self->data  = data;
	self->len   = len;
	self->w     = w;
	self->pl    = 0;
	self->pu    = 0;
	self->sum   = 0.0f;
	self->err   = 0.0f;
	self->scale = 1.0f/(2*w);
}

__attribute__((optimize("-fno-associative-math")))
static inline float getsamp_step(struct samp_state *self, int i)
{
#pragma clang fp reassociate(off)

	int l = IMAX(i - self->w, 0);
	int u = IMIN(i + self->w, self->len);

	// check if old window and new window overlap, and if so if it's enough to make the subracts worth doing
	if(i - self->w < self->pu && l - self->pl < self->w)
	{
		// windows overlap, subtract out old values
		for(int j = self->pl; j < l; j++) {
			float y = self->err - self->data[j];
			float t = self->sum + y;
			self->err = (t - self->sum) - y;
			self->sum = t;
		}
		for(int j = self->pu; j < u; j++) { // Add the new values from the current window
			float y = self->data[j] + self->err;
			float t = self->sum + y;
			self->err = (t - self->sum) - y;
			self->sum = t;
		}
	}
	else
	{
		// No window overlap, just zero out running sum
		self->err = 0;
		self->sum = 0;
		for(int j = l; j < u; j++) {
			float y = self->data[j] + self->err;
			float t = self->sum + y;
			self->err = (t - self->sum) - y;
			self->sum = t;
		}
	}

	self->pl = l;
	self->pu = u;

	return self->sum * self->scale;
}

__attribute__((optimize("-fno-associative-math")))
static inline float getsamp(const float *data, int len, int i, int w)
{
#pragma clang fp reassociate(off)
	float sum = 0, err = 0;
	int l = IMAX(i-w, 0);
	int u = IMIN(i+w, len);
	for(int j = l; j < u; j++) { // TODO: disable fast math
		float y = data[j] + err;
		float t = sum + y;
		err = (t - sum) - y;
		sum = t;
	}
	return sum / (2*w);
}

#endif
