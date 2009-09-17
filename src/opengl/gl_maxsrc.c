/**
 * gl_maxsrc.c
 *
 */

#include "common.h"

#include <SDL.h>
#include <GL/glew.h>
#include <GL/glut.h>

#include "glmisc.h"
#include "audio/audio.h"
#include "tribuf.h"

static GLuint max_fbo, max_fbo_tex[2], depthbuffer;
static int cur_tex = 0;
static int iw, ih;

static Pixbuf *setup_point_16(int w, int h)
{
	Pixbuf *surf = malloc(sizeof(Pixbuf));
	uint16_t *buf = surf->data = malloc(w * h * sizeof(*buf)); surf->bpp  = 16;
	surf->w = w; surf->h = h;
	surf->pitch = surf->w*sizeof(*buf);
	memset(buf, 0, w*h*sizeof(*buf));

	for(int y=0; y < h; y++)  {
		for(int x=0; x < w; x++) {
			float u = (2.0f*x)/w - 1, v = (2.0f*y)/h - 1;
			buf[y*w + x] = (uint16_t)(expf(-4.5f*(u*u+v*v))*(UINT16_MAX));
		}
	}
	return surf;
}

static Pixbuf *setup_point_32(int w, int h)
{
	Pixbuf *surf = malloc(sizeof(Pixbuf));
	uint32_t *buf = surf->data = malloc(w * h * sizeof(*buf)); surf->bpp  = 32;;
	surf->w = w; surf->h = h;
	surf->pitch = surf->w*sizeof(*buf);
	memset(buf, 0, w*h*sizeof(*buf));

	for(int y=0; y < h; y++)  {
		for(int x=0; x < w; x++) {
			float u = (2.0f*x)/w - 1, v = (2.0f*y)/h - 1;
			float t = expf(-4.5f*(u*u+v*v));
			uint8_t tmp = (uint8_t)(t*(UINT8_MAX));
			buf[y*w + x] = (tmp<<24)|(tmp<<16)|(tmp<<8)|tmp;
		}
	}
	return surf;
}

static void setup_max_fbo(int width, int height) {
	glGenFramebuffersEXT(1, &max_fbo);
	glGenRenderbuffersEXT(1, &depthbuffer);
	glGenTextures(2, max_fbo_tex);
	glPushAttrib(GL_VIEWPORT_BIT);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, max_fbo);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, depthbuffer);
	glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT, width, height);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, depthbuffer);

	for(int i=0; i<2; i++) {
		glBindTexture(GL_TEXTURE_2D, max_fbo_tex[i]);
//		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,  width, height, 0, GL_LUMINANCE, GL_UNSIGNED_SHORT, NULL);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, max_fbo_tex[i], 0);
		glViewport(0,0,width, height);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	glPopAttrib();
}

static float tx=0, ty=0, tz=0;
static inline float getsamp(audio_data *d, int i, int w) {
	float res = 0;
	int l = IMAX(i-w, 0);
	int u = IMIN(i+w, d->len);
	for(int i = l; i < u; i++) {
		res += d->data[i];
	}
	return res / (2*w);
}

static Pixbuf *pnt;
static GLuint pnt_tex;
static GLhandleARB shader_prog;
static const char *frag_src =
	"uniform sampler2D prev;"
	"uniform mat3 R;"
	"void main() {"
	"	vec2 uv = gl_TexCoord[0].st*0.98;"
	"	float d = 0.95f + 0.05*length(uv*2-1);"
//	"	vec2 uv = gl_TexCoord[0].st*2-1;"
//	"	vec3 p=vec3((uv.x*R[0][0] + uv.y*R[0][1])," //TODO: optimize this to use real matrix, move to vertex shader
//	"				(uv.x*R[1][0] + uv.y*R[1][1])*d,"
//	"				(uv.x*R[2][0] + uv.y*R[2][1])*d);"
//	"	uv = vec2(	(p[0]*R[0][0] + p[1]*R[1][0] + p[2]*R[2][0]+1.0f)*0.5f,"
//	"				(p[0]*R[0][1] + p[1]*R[1][1] + p[2]*R[2][1]+1.0f)*0.5f);"
	"	gl_FragData[0] = texture2D(prev, uv)*0.98;"
	"	gl_FragDepth = gl_FragData[0].x;"
	"}";

void gl_maxsrc_init(int width, int height) {
	iw=width, ih=height;
	setup_max_fbo(width, height);
	shader_prog = compile_program(NULL, frag_src);
	glPushAttrib(GL_TEXTURE_BIT);
	glGenTextures(1, &pnt_tex);
	glBindTexture(GL_TEXTURE_2D, pnt_tex);

//	Pixbuf *src = pnt = setup_point_32(64, 64);
//	gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, src->w, src->h, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, src->data);
	Pixbuf *src = pnt = setup_point_16(64, 64);
	gluBuild2DMipmaps(GL_TEXTURE_2D, GL_LUMINANCE, src->w, src->h, GL_LUMINANCE, GL_UNSIGNED_SHORT, src->data);
//	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, src->w, src->h, 0, GL_LUMINANCE, GL_UNSIGNED_SHORT, src->data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,  GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,  GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glPopAttrib();
//	pixbuf_to_texture(pnt, &pnt_tex, GL_CLAMP, 0);
}

void gl_maxsrc_update() {
	int next_tex = (cur_tex+1)%2;
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
	glActiveTexture(GL_TEXTURE0);
//	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, max_fbo);
//	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, max_fbo_tex[next_tex], 0);
//	glViewport(0,0, iw, ih);
//	glMatrixMode(GL_PROJECTION);
//	glLoadIdentity();
//	glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
////	gluPerspective(45, 1, -2, 2);
//	glMatrixMode(GL_MODELVIEW);
//	glLoadIdentity();

//	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
//	glUseProgramObjectARB(shader_prog);
//	glBindTexture(GL_TEXTURE_2D, max_fbo_tex[cur_tex]);
////	glBindTexture(GL_TEXTURE_2D, pnt_tex);
//	glUniform1iARB(glGetUniformLocationARB(shader_prog, "prev"), 0);
//	glBegin(GL_QUADS);
//		glTexCoord2d(0.0,1.0); glVertex2d(-1, -1);
//		glTexCoord2d(1.0,1.0); glVertex2d( 1, -1);
//		glTexCoord2d(1.0,0.0); glVertex2d( 1,  1);
//		glTexCoord2d(0.0,0.0); glVertex2d(-1,  1);
//	glEnd();
//	glUseProgramObjectARB(0);

	audio_data ad;
	audio_get_samples(&ad);
	int samp = IMAX(IMAX(iw,ih)/4, 1023);
//	samp = 4;
//	float cx=cosf(tx), cy=cosf(ty), cz=cosf(tz);
//	float sx=sinf(tx), sy=sinf(ty), sz=sinf(tz);
//
//	float R[][3] = {
//		{cz*cy-sz*sx*sy, -sz*cx, -sy*cz-cy*sz*sx},
//		{sz*cy+cz*sx*sy,  cz*cx, -sy*sz+cy*cz*sx},
//		{cx*sy         ,    -sx,  cy*cx}
//	};
//	glLoadMatrixf(R);
//	glEnable(GL_DEPTH_TEST);
	int pnt_size = IMAX(iw/24, 16);

	float verts[samp][2];
	GLshort index[samp];
	for(int i=0; i<samp; i++) {
		float s = getsamp(&ad, i*ad.len/samp, ad.len/96);
		s=copysignf(logf(fabsf(s)*3+1)/2, s);
		verts[i][0] = (i - samp/2)*1.0f/samp;
		verts[i][1] = 0.2f*s;
		index[i] = i;
	}
	float pw = 0.5f*(float)pnt_size/iw, ph = 0.5f*(float)pnt_size/ih;
//	glBindTexture(GL_TEXTURE_2D, pnt_tex);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glBegin(GL_QUADS); for(int i=0; i<samp; i++) {
		float x = verts[i][0], y = verts[i][1];
		glTexCoord2d(0.0,1.0); glVertex2d(x-pw, y-ph);
		glTexCoord2d(1.0,1.0); glVertex2d(x+pw, y-ph);
		glTexCoord2d(1.0,0.0); glVertex2d(x+pw, y+ph);
		glTexCoord2d(0.0,0.0); glVertex2d(x-pw, y+ph);
	} glEnd();

//	glEnableClientState(GL_VERTEX_ARRAY);
//	glEnable(GL_POINT_SPRITE_ARB);
//	glBindTexture(GL_TEXTURE_2D, pnt_tex);
//	float quadratic[] =  { 1.0f, 0.0f, 0.01f };
//	glPointParameterfvARB( GL_POINT_DISTANCE_ATTENUATION_ARB, quadratic );
//	glPointParameterfARB( GL_POINT_FADE_THRESHOLD_SIZE_ARB, 0.0f );
//	glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE);
//	glPointParameterf( GL_POINT_SIZE_MIN, 1.0f );
//	glPointParameterf( GL_POINT_SIZE_MAX, 64);
//	glPointSize(pnt_size);
//	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
////	glBegin(GL_POINTS); for(int i=0; i<samp; i++) {
////		glVertex2f(verts[i][0], verts[i][1]);
////	} glEnd();
//	glVertexPointer(2, GL_FLOAT, 2*sizeof(float), verts);
//	glDrawElements(GL_POINTS, samp, GL_UNSIGNED_SHORT, index);

//	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	glPopClientAttrib();
	glPopAttrib();
	tx+=0.02; ty+=0.01; tz-=0.003;
	cur_tex = next_tex;
}

GLuint gl_maxsrc_get() {
	return cur_tex;
}
