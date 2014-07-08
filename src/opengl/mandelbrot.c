/**
 * mandelbrot.c
 *
 */

#include "common.h"
#include "glmisc.h"
#include "points.h"

#include <complex.h>

static Pixbuf *mandel_surf;
static GLuint mandel_tex;


static inline __attribute__((always_inline)) double sqrd(double x) {return x*x;}
void init_mandel(void)
{
	mandel_surf = malloc(sizeof(Pixbuf));
	mandel_surf->w = mandel_surf->h = 1024;
	mandel_surf->pitch = mandel_surf->w*sizeof(uint8_t);
	mandel_surf->bpp  = 8; uint8_t *data = mandel_surf->data = malloc(mandel_surf->w * mandel_surf->h * sizeof(*data));

	for(int y=0; y < mandel_surf->h; y++) { // TODO: speed this up (marching squares?)
		for(int x=0; x < mandel_surf->w; x++) {
			double complex z0 = x*2.0f/mandel_surf->w - 1.5f + (y*2.0f/mandel_surf->h - 1.0f)*I;
			double complex z = z0;
//			int i=0; while(cabs(z) < 2 && i < 1024) {
			int i=0; while(sqrd(cimag(z))+sqrd(creal(z)) < 4 && i < 128) {
				i++;
				z = z*z + z0;
			}

			//n + log2ln(R) – log2ln|z|
			float mu = (i + 1 - log2f(logf(cabs(z))));
			data[y*mandel_surf->w + x] = 128*log2f(mu*(4.0f/128)+1);

			//data[y*mandel_surf->w + x] = (256*log2(i*32.0f/1024+1)/5);
		}
	}
	glPushAttrib(GL_TEXTURE_BIT);
	glGenTextures(1, &mandel_tex);
	glBindTexture(GL_TEXTURE_2D, mandel_tex);
	gluBuild2DMipmaps(GL_TEXTURE_2D, GL_LUMINANCE, mandel_surf->w, mandel_surf->h, GL_LUMINANCE, GL_UNSIGNED_BYTE, mandel_surf->data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glPopAttrib();
}

void render_mandel(struct point_data *pd)
{
	glBindTexture(GL_TEXTURE_2D, mandel_tex);
	glBegin(GL_TRIANGLE_STRIP);
		glTexCoord2d(0.0f, 1.0f); glVertex2f(0.0f,  1.0f);
		glTexCoord2d(1.0f, 1.0f); glVertex2f(1.0f,  1.0f);
		glTexCoord2d(0.0f, 0.0f); glVertex2f(0.0f,  0.0f);
		glTexCoord2d(1.0f, 0.0f); glVertex2f(1.0f,  0.0f);
	glEnd();
	glBindTexture(GL_TEXTURE_2D, 0);

	glPointSize(2);
	glBegin(GL_POINTS);
		glColor3f(0.0f, 1.0f, 0.0f);
		glVertex2d((pd->p[0]+1.0f)*0.5f, 1-(pd->p[1]+1)*0.5f);
		glColor3f(1.0f, 0.0f, 0.0f);
		glVertex2d((pd->t[0]+1.0f)*0.5f, 1-(pd->t[1]+1)*0.5f);
	glEnd();
	glColor3f(1.0f, 1.0f, 1.0f);
}

