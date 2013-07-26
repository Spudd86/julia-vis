#ifndef GETSAMP_H
#define GETSAMP_H

static inline float getsamp(const float *data, int len, int i, int w) {
	float sum = 0, err = 0;
	int l = IMAX(i-w, 0);
	int u = IMIN(i+w, len);
	for(int i = l; i < u; i++) { // TODO: disable fast math
		float y = data[i] + err;
		float t = sum + y;
		err = (t - sum) - y;
		sum = t;
	}
	return sum / (2*w);
}

#endif
