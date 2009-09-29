/**
 * drawscope.c
 *
 */

#include "common.h"

#include <SDL.h>
#include <GL/glew.h>
#include <GL/glut.h>

#include "glmisc.h"
#include "audio/audio.h"
#include "tribuf.h"

static float pntw, pnth;
static int pnt_size;
static int samp;
static Pixbuf *src;
static GLuint pnt_tex;

static int scope_init_done = 0;

//#define TEX16

static Pixbuf *setup_point_16(int w, int h)
{
	Pixbuf *surf = malloc(sizeof(Pixbuf));
#ifdef TEX16
	uint16_t *buf = surf->data = malloc(w * h * sizeof(*buf)); surf->bpp  = 16;
#else
	uint32_t *buf = surf->data = malloc(w * h * sizeof(*buf)); surf->bpp  = 32;
#endif
	surf->w = w; surf->h = h;
	surf->pitch = surf->w*sizeof(*buf);
	memset(buf, 0, w*h*sizeof(*buf));

	for(int y=0; y < h; y++)  {
		for(int x=0; x < w; x++) {
			float u = (2.0f*x)/w - 1, v = (2.0f*y)/h - 1;
			float z = expf(-4.5f*(u*u+v*v));
#ifdef TEX16
			buf[y*w + x] = (uint16_t)(z*(UINT16_MAX));
#else
			uint8_t b = (uint8_t)(z*(UINT8_MAX));
			buf[y*w + x] = (b<<24)|(b<<16)|(b<<8)|b;
#endif
		}
	}
	return surf;
}

int scope_init(int width, int height)
{
	//check extenions
	if(!GLEW_EXT_blend_minmax)
		return -1;

	samp = IMAX(width,height)/4;
	pntw = 0.5f*fmaxf(1.0f/32, 10.0f/width); pnth = 0.5f*fmaxf(1.0f/32, 10.0f/height);
	pnt_size = IMAX(width/32, 10);

	src = setup_point_16(32, 32);
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glGenTextures(1, &pnt_tex);
	glBindTexture(GL_TEXTURE_2D, pnt_tex);
#ifdef TEX16
	gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, src->w, src->h, GL_LUMINANCE, GL_UNSIGNED_SHORT, src->data);
//	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,  src->w, src->h, 0, GL_LUMINANCE, GL_UNSIGNED_SHORT, src->data);
#else
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,  src->w, src->h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, src->data);
//	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,  src->w, src->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, src->data);
#endif
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,  GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,  GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glPopAttrib();

	scope_init_done= 1;
}


static float tx=0, ty=0, tz=0;
static inline float getsamp(audio_data *d, int i, int w) {
	float res = 0;
	int l = IMAX(i-w, 0);
	int u = IMIN(i+w, d->len);
	for(int i = l; i < u; i++) {
		res += d->data[i];
	}
	res = res / (2*w);
	return res;
}


void render_scope()
{
	if(!scope_init_done) abort();
	GLenum err = GL_NONE;

	audio_data ad;
	audio_get_samples(&ad);

	//TODO: use GL_MODELVIEW matrix to do transform
	//TODO: use vertex array or VBO
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
	glEnable(GL_BLEND);
	glBlendEquation(GL_MAX);
	float pw = pntw, ph = pnth;
	glBindTexture(GL_TEXTURE_2D, pnt_tex);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glBegin(GL_QUADS); for(int i=0; i<samp; i++) {
		float s = getsamp(&ad, i*ad.len/samp, ad.len/96);
		s=copysignf(log2f(fabsf(s)*3+1)/2, s);

		//TODO: work out how to do this...
		float x = (i - samp/2)*1.0f/samp, y = 0.15f*s;
		glTexCoord2d(0.0,1.0); glVertex3f(x-pw, y-ph, 0);
		glTexCoord2d(1.0,1.0); glVertex3f(x+pw, y-ph, 0);
		glTexCoord2d(1.0,0.0); glVertex3f(x+pw, y+ph, 0);
		glTexCoord2d(0.0,0.0); glVertex3f(x-pw, y+ph, 0);
	} glEnd();
//	glBegin(GL_QUAD_STRIP);
//	float s = getsamp(&ad, 0, ad.len/96);
//	s=copysignf(log2f(fabsf(s)*3+1)/2, s);
//	float x = (0 - samp/2)*1.0f/samp, y = 0.15f*s;
//	glTexCoord2d(0.0,1.0); glVertex3f(x-pw, y+ph, 0);
//	glTexCoord2d(0.0,0.0); glVertex3f(x-pw, y-ph, 0);
//	for(int i=1; i<samp; i++) {
//		s = getsamp(&ad, i*ad.len/samp, ad.len/96);
//		s=copysignf(log2f(fabsf(s)*3+1)/2, s);
//
//		//TODO: work out how to do this...
//		x = (i - samp/2)*1.0f/samp, y = 0.15f*s;
//		glTexCoord2d(0.5,1.0); glVertex3f(x, y+ph, 0);
//		glTexCoord2d(0.5,0.0); glVertex3f(x, y-ph, 0);
//	}
//	x = (samp - samp/2)*1.0f/samp, y = 0.15f*s;
//	glTexCoord2d(1.0,1.0); glVertex3f(x+pw, y+ph, 0);
//	glTexCoord2d(1.0,0.0); glVertex3f(x+pw, y-ph, 0);
//	glEnd();

	glPopClientAttrib();
	glPopAttrib();


//	samp = 4;
//
//	float verts[samp][2];
//	GLshort index[samp];
//	for(int i=0; i<samp; i++) {
//		float s = getsamp(&ad, i*ad.len/samp, ad.len/96);
//		s=copysignf(logf(fabsf(s)*3+1)/2, s);
//		float x = (i - samp/2)*1.0f/samp, y = 0.2f*s;
//		verts[i][0] = x;
//		verts[i][1] = y;
//		index[i] = i;
//	}
//
//	glPushAttrib(GL_ALL_ATTRIB_BITS);
//	glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
//	glEnable( GL_BLEND );
//	glBlendFunc( GL_ONE, GL_SRC_ALPHA ); if((err=glGetError()) != GL_NONE) printf("Error\n");
////	glBlendEquation(GL_MAX);
//
//	glBindTexture(GL_TEXTURE_2D, pnt_tex); if((err=glGetError()) != GL_NONE) printf("Error\n");
//	if((err=glGetError()) != GL_NONE) printf("Error\n");
////	float quadratic[] =  { 1.0f, 0.0f, 0.01f };
////	glPointParameterfvARB( GL_POINT_DISTANCE_ATTENUATION_ARB, quadratic );
////	glPointParameterfARB( GL_POINT_FADE_THRESHOLD_SIZE_ARB, 0.0f );
//	glTexEnvi(GL_POINT_SPRITE_ARB, GL_COORD_REPLACE_ARB, GL_TRUE);
//	if((err=glGetError()) != GL_NONE) printf("Error\n");
////	glPointParameterfARB(GL_POINT_SIZE_MIN_ARB, 1); if((err=glGetError()) != GL_NONE) printf("Error\n");
////	glPointParameterfARB(GL_POINT_SIZE_MAX_ARB, 16); if((err=glGetError()) != GL_NONE) printf("Error\n");
//	float maxSize = 0.0f; glGetFloatv( GL_POINT_SIZE_MAX_ARB, &maxSize ); if( maxSize > 32.0f ) maxSize = 32.0f;
////	glPointSize( maxSize );
//	glPointSize(8);
//	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
//	glBindTexture(GL_TEXTURE_2D, pnt_tex); if((err=glGetError()) != GL_NONE) printf("Error\n");
//	glEnable(GL_POINT_SPRITE_ARB);
//	glBegin(GL_POINTS); for(int i=0; i<samp; i++) {
//		glVertex2f(verts[i][0], verts[i][1]);
//	} glEnd(); if((err=glGetError()) != GL_NONE) printf("Error\n");
////	glEnableClientState(GL_VERTEX_ARRAY);
////	glVertexPointer(2, GL_FLOAT, 2*sizeof(float), verts);
////	glDrawElements(GL_POINTS, samp, GL_UNSIGNED_SHORT, index);
//	if((err=glGetError()) != GL_NONE) printf("Error\n");
//	glPopClientAttrib();
//	glPopAttrib();
}
