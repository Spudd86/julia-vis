#ifndef COLOURSPACE_H__
#define COLOURSPACE_H__ 1

static inline float linearize(float c)
{
	c = c/255.0f; // normalise range
	if(c <= 0.04045f) c = c/12.92f;
	else c = powf((c+0.055f)/1.055f, 2.4f);
	//c = c*255.0f;
	return c;
}

static inline float gamma_curve(float c)
{
	//c = c/255.0f; // normalise range
	if(c <= 0.0031308f) c *= 12.92f;
	else c = 1.055f*powf(c, 1.0f/2.4f) - 0.055f;
	return c*255.0f;
}

typedef union {
	struct {
		float x, y, z;
	};
	struct {
		float r, g, b;
	};
	struct {
		float L, A, B;
	};
	struct {
		float l, u, V;
	};
	float v[3];
} colourf;

static inline colourf rgb2xyz(colourf col)
{
	static const float m[3][3] = {
		{0.412383f, 0.357585f, 0.18048f},
		{0.212635f, 0.71517f,  0.072192f},
		{0.01933f,  0.119195f, 0.950528f}
	};
	const float c[] = {col.r*100, col.g*100, col.b*100};
	colourf r;

	r.x = m[0][0]*c[0] + m[0][1]*c[1] + m[0][2]*c[2];
	r.y = m[1][0]*c[0] + m[1][1]*c[1] + m[1][2]*c[2];
	r.z = m[2][0]*c[0] + m[2][1]*c[1] + m[2][2]*c[2];

	return r;
}

static inline colourf xyz2rgb(colourf col)
{
	static const float m[3][3] = {
		{ 3.24103f,  -1.53741f, -0.49862f},
		{-0.969242f,  1.87596f,  0.041555f},
		{ 0.055632f, -0.203979f, 1.05698f}
	};

	const float c[] = {col.x/100, col.y/100, col.z/100};
	colourf r;

	r.r = m[0][0]*c[0] + m[0][1]*c[1] + m[0][2]*c[2];
	r.g = m[1][0]*c[0] + m[1][1]*c[1] + m[1][2]*c[2];
	r.b = m[2][0]*c[0] + m[2][1]*c[1] + m[2][2]*c[2];

	return r;
}

static inline colourf xyz2Lab(colourf col)
{
	colourf r;
	float X = col.x / 95.047f;
	float Y = col.y / 100;
	float Z = col.z / 108.883f;

	if(X > 216.0f/24389.0f) X = cbrtf(X);
	else              X = ((24389.0f/27.0f)/116.0f) * X + 16.0f/116.0f;
	if(Y > 216.0f/24389.0f) Y = cbrtf(Y);
	else              Y = ((24389.0f/27.0f)/116.0f) * Y + 16.0f/116.0f;
	if(Z > 216.0f/24389.0f) Z = cbrtf(Z);
	else              Z = ((24389.0f/27.0f)/116.0f) * Z + 16.0f/116.0f;

	r.L = 116 * Y - 16;
	r.A = 500 * (X - Y);
	r.B = 200 * (Y - Z);

	return r;
}

static inline colourf Lab2xyz(colourf col)
{
	colourf r;
	float Y = (col.L + 16.0f)/116.0f;
	float X = col.A/500.0f + Y;
	float Z = Y - col.B/200.0f;

	if(Y*Y*Y > 216.0f/24389.0f) Y = Y*Y*Y;
	else                  Y = (Y - 16.0f/116.0f)/((24389.0f/27.0f)/116.0f);
	if(X*X*X > 216.0f/24389.0f) X = X*X*X;
	else                  X = (X - 16.0f/116.0f)/((24389.0f/27.0f)/116.0f);
	if(Z*Z*Z > 216.0f/24389.0f) Z = Z*Z*Z;
	else                  Z = (Z - 16.0f/116.0f)/((24389.0f/27.0f)/116.0f);

	r.x = X * 95.047f;
	r.y = Y * 100;
	r.z = Z * 108.883f;

	return r;
}

static inline colourf xyz2luv(colourf col)
{
	//const float uref = 4*0.31271f/(-2*0.31271f - 12*0.32902f + 3);
	//const float vref = 9*0.32902f/(-2*0.31271f - 12*0.32902f + 3);
	const float uref = 4*95.047f/(95.047f + 15*100 + 3*108.883f);
	const float vref = 9*100/(95.047f + 15*100 + 3*108.883f);
	colourf r;

	float Y = col.y;
	float l;
	if(Y/100 <= 216.0f/24389.0f) l = (24389.0f/27.0f)*Y/100;
	else                         l = 116*cbrtf(Y/100) - 16;

	float ut = 0, vt = 0;

	if(col.x + 15*col.y + 3*col.z > FLT_EPSILON) { // avoid divide by zero, only using FLT_EPSILON because it's convenient
		ut = 4*col.x/(col.x + 15*col.y + 3*col.z);
		vt = 9*col.y/(col.x + 15*col.y + 3*col.z);
	}

	r.l = l;
	r.u = 13*l*(ut - uref);
	r.V = 13*l*(vt - vref);

	return r;
}

static inline colourf luv2xyz(colourf col)
{
	//const float uref = 4*0.31271f/(-2*0.31271f - 12*0.32902f + 3);
	//const float vref = 9*0.32902f/(-2*0.31271f - 12*0.32902f + 3);
	const float uref = 4*95.047f/(95.047f + 15*100 + 3*108.883f);
	const float vref = 9*100/(95.047f + 15*100 + 3*108.883f);
	colourf r;

	float Y;
	if(col.l <= 8) Y = 100*col.l*(27.0f/24389.0f); //powf(3.0f/29.0f, 3.0f);
	else           Y = 100*powf((col.l + 16)/116.0f, 3.0f);

	r.y = Y;
	r.x = r.z = 0.0f;
	if(col.l > FLT_EPSILON) { // avoid divide by zero, only using FLT_EPSILON because it's convenient
		float ut = col.u/(col.l*13) + uref;
		float vt = col.V/(col.l*13) + vref;

		r.x = Y*(9*ut)/(4*vt);
		r.z = Y*(12 - 3*ut - 20*vt)/(4*vt);
	}

	return r;
}

static inline colourf rgb2luv(colourf c)
{
	c = rgb2xyz(c);
	c = xyz2luv(c);
	return c;
}

static inline colourf luv2rgb(colourf c)
{
	c = luv2xyz(c);
	c = xyz2rgb(c);
	return c;
}

static inline colourf rgb2Lab(colourf c)
{
	c = rgb2xyz(c);
	c = xyz2Lab(c);
	return c;
}

static inline colourf Lab2rgb(colourf c)
{
	c = Lab2xyz(c);
	c = xyz2rgb(c);
	return c;
}

static inline colourf make_linear(uint8_t r, uint8_t g, uint8_t b)
{
	colourf c;
	c.r = linearize(r);
	c.g = linearize(g);
	c.b = linearize(b);
	return c;
}

#endif
