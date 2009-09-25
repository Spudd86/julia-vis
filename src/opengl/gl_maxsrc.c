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

#include "gl_maxsrc.h"

static GLuint max_fbo, max_fbo_tex[2];
static int cur_tex = 0;
static int iw, ih;

static Uint32 lastupdate = -1;

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

static void setup_max_fbo(int width, int height) {
	glGenFramebuffersEXT(1, &max_fbo);
	glGenTextures(2, max_fbo_tex);
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, max_fbo);

	for(int i=0; i<2; i++) {
		glBindTexture(GL_TEXTURE_2D, max_fbo_tex[i]);
//		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,  width, height, 0, GL_LUMINANCE, GL_UNSIGNED_SHORT, NULL);
//		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,  width, height, 0, GL_RGBA, GL_UNSIGNED_INT_10_10_10_2, NULL);
		if(GLEW_ARB_half_float_pixel) { printf("using half float pixels in max fbo\n");
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F_ARB,  width, height, 0, GL_RGB, GL_HALF_FLOAT_ARB, NULL);
		}if(GLEW_ARB_color_buffer_float)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F_ARB,  width, height, 0, GL_RGB, GL_FLOAT, NULL);
		else
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,  width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
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
	res = res / (2*w);
	return res;
}

static Pixbuf *pnt;
static GLuint pnt_tex;
static GLhandleARB shader_prog;
//TODO: ARB_fragment_program version
static const char *frag_src =
	"uniform sampler2D prev;"
	"uniform mat3 R;" //TODO: change R into one mat3x2 and one mat2x3
	"void main() {"
	"	vec2 uv = gl_TexCoord[0].st;"
	"	float d = 0.97f + 0.03f*length(uv);"
//	"	vec3 p=vec3((uv.x*R[0][0] + uv.y*R[0][1])," //TODO: optimize this to use real matrix, move to vertex shader
//	"				(uv.x*R[1][0] + uv.y*R[1][1])*d,"
//	"				(uv.x*R[2][0] + uv.y*R[2][1])*d);"
//	"	uv = vec2(	p[0]*R[0][0] + p[1]*R[1][0] + p[2]*R[2][0],"
//	"				p[0]*R[0][1] + p[1]*R[1][1] + p[2]*R[2][1]);"
//	"	uv = (uv + 1)*0.5f;"
	"	vec3 p = R*(vec3(uv, 0)*R*-vec3(1, d, d));" //TODO: figure out if this is actually faster...
	"	uv = (vec2(p) + 1)*0.5f;"
	"	gl_FragData[0] = texture2D(prev, uv)*(126/128.0f);"
	"}";

void gl_maxsrc_init(int width, int height) {
	iw=width, ih=height;
	setup_max_fbo(width, height);
	shader_prog = compile_program(NULL, frag_src);
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glGenTextures(1, &pnt_tex);
	glBindTexture(GL_TEXTURE_2D, pnt_tex);

	Pixbuf *src = pnt = setup_point_16(64, 64);
	gluBuild2DMipmaps(GL_TEXTURE_2D, GL_LUMINANCE, src->w, src->h, GL_LUMINANCE, GL_UNSIGNED_SHORT, src->data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,  GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,  GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glPopAttrib();
}

void gl_maxsrc_update(Uint32 now) {
	int next_tex = (cur_tex+1)%2;
	float dt = 1;
	if(lastupdate != -1) {
		dt = (now - lastupdate)*0.05f;
		if(now - lastupdate < 20) return; // limit to 100FPS
	}
	lastupdate = now;

	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
	glActiveTexture(GL_TEXTURE0);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, max_fbo);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, max_fbo_tex[next_tex], 0);
	glViewport(0,0, iw, ih);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	float cx=cosf(tx), cy=cosf(ty), cz=cosf(tz);
	float sx=sinf(tx), sy=sinf(ty), sz=sinf(tz);

	float R[][3] = {
		{cz*cy-sz*sx*sy, -sz*cx, -sy*cz-cy*sz*sx},
		{sz*cy+cz*sx*sy,  cz*cx, -sy*sz+cy*cz*sx},
		{cx*sy         ,    -sx,  cy*cx}
	};

	glBindTexture(GL_TEXTURE_2D, max_fbo_tex[cur_tex]);
	glUseProgramObjectARB(shader_prog);
	glUniform1iARB(glGetUniformLocationARB(shader_prog, "prev"), 0);
	glUniformMatrix3fvARB(glGetUniformLocationARB(shader_prog, "R"), 1, 0, R);
	glBegin(GL_QUADS);
		glTexCoord2d(-1,-1); glVertex2d( 1,  1);
		glTexCoord2d( 1,-1); glVertex2d(-1,  1);
		glTexCoord2d( 1, 1); glVertex2d(-1, -1);
		glTexCoord2d(-1, 1); glVertex2d( 1, -1);
	glEnd();
	glUseProgramObjectARB(0);

	audio_data ad;
	audio_get_samples(&ad);
	int samp = IMAX(iw,ih)/2;

	glEnable(GL_BLEND);
	glBlendEquation(GL_MAX);

	//TODO: use GL_MODELVIEW matrix to do transform
	//TODO: use vertex array or VBO
	float pw = 0.5f*fmaxf(1.0f/38, 10.0f/iw), ph = 0.5f*fmaxf(1.0f/38, 10.0f/ih);
	glBindTexture(GL_TEXTURE_2D, pnt_tex);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glBegin(GL_QUADS); for(int i=0; i<samp; i++) {
		float s = getsamp(&ad, i*ad.len/samp, ad.len/96);
		s=copysignf(log2f(fabsf(s)*3+1)/2, s);

		float xt = (i - samp/2)*1.0f/samp, yt = 0.15f*s, zt = 0.0f;
		float x = R[0][0]*xt + R[1][0]*yt + R[2][0]*zt;
		float y = R[0][1]*xt + R[1][1]*yt + R[2][1]*zt;
		float z = R[0][2]*xt + R[1][2]*yt + R[2][2]*zt;
		glTexCoord2d(0.0,1.0); glVertex3f(x-pw, y-ph, z);
		glTexCoord2d(1.0,1.0); glVertex3f(x+pw, y-ph, z);
		glTexCoord2d(1.0,0.0); glVertex3f(x+pw, y+ph, z);
		glTexCoord2d(0.0,0.0); glVertex3f(x-pw, y+ph, z);
	} glEnd();

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	glPopClientAttrib();
	glPopAttrib();

	tx+=0.02*dt; ty+=0.01*dt; tz-=0.003*dt;
	cur_tex = next_tex;
}

GLuint gl_maxsrc_get() {
	return max_fbo_tex[cur_tex];
}
