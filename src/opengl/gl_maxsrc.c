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

#include <assert.h>

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

	for(int i=0; i<2; i++) {
		glBindTexture(GL_TEXTURE_2D, max_fbo_tex[i]);
//		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,  width, height, 0, GL_LUMINANCE, GL_UNSIGNED_SHORT, NULL);
//		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,  width, height, 0, GL_RGBA, GL_UNSIGNED_INT_10_10_10_2, NULL);
		if(GLEW_ARB_half_float_pixel && GLEW_ARB_texture_float)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F_ARB,  width, height, 0, GL_RGB, GL_HALF_FLOAT_ARB, NULL);
		else if(GLEW_ARB_color_buffer_float && GLEW_ARB_texture_float)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F_ARB,  width, height, 0, GL_RGB, GL_FLOAT, NULL);
		else
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB10,  width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
		//TODO: check for errors to be sure we can actually use GL_RGB10 here (possibly also testing that we can render to it)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
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
//TODO: ARB_vertex_program version (fallback if no GLSL)
//TODO: fallback to fixed_map if no shaders at all

//TODO: use 2 basis vectors to find p instead of matrix op
// ie, uniform vec3 a,b; 
//     where a = <1,0,0>*R, b = <0, 1, 0>*R
// then vec3 p = (uv.x*a + uv.y*b)*vec3(1,d,d);
// and final co-ords are still R*p, except we can make R a mat3x2 and maybe save some MAD's

static const char *frag_src =
	"#version 120\n"
	"uniform sampler2D prev;"
	"invariant uniform mat2x3 R;"
	"void main() {"
	"	vec3 p = vec3(0.5f);"
	"	{"
	"		vec2 uv = gl_TexCoord[0].st;"
	"		float d = 0.96f*0.5f + (0.04f*0.5f)*log2(length(uv)*0.707106781186547 + 1);"
	"		p.yz = vec2(d); p = (uv.x*R[0] + uv.y*R[1])*p;" //NOTE: on mesa 7.6's compiler p*= generates more code for some reason
	"	}"
//	"	vec2 tmp = texture2D(prev, gl_TexCoord[0].st*0.5+0.5).rb;"
//	"	float c = dot(tmp, vec2(256,1-1.0/256)/256) - 0.25/256;"
//	"	vec2 c = texture2D(prev, p*R + 0.5f).xy*(98.0/100);"
//	"	c.y += fract(c.x*255);"
//	"	gl_FragData[0].xy = c;"
//	"	gl_FragData[0].r = c; gl_FragData[0].b = fract(c*256);"
	"	vec4 c = texture2D(prev, p*R + 0.5f);"
	"	gl_FragData[0].x = (c.x - max(2/256.0f, c.x*(1.0f/100)));" //TODO: only do this if tex format not precise enough
//	"	gl_FragData[0].x = (c.x - c.x*(2/128.0f));"
//	"	gl_FragData[0] = texture2D(prev, vec2(p*R)*0.5f + 0.5f)*(63/64.0f);"
//	"	gl_FragData[0] = (texture2D(prev, vec2(p*R)*0.5f + 0.5f) - (2/256.0f));"
	"}";

//TODO: fallback to glCopyTexImage2D/glCopyTexSubImage2D if no FBO's

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
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glPopAttrib();
}

void gl_maxsrc_update(Uint32 now) {
	int next_tex = (cur_tex+1)%2;
	float dt = 1;
	if(lastupdate != -1) {
//		dt = (now - lastupdate)*0.05f;
		if(now - lastupdate < 20) return; // limit to 100FPS
	}
	lastupdate = now;

	float cx=cosf(tx), cy=cosf(ty), cz=cosf(tz);
	float sx=sinf(tx), sy=sinf(ty), sz=sinf(tz);

	float R[][3] = {
		{cz*cy-sz*sx*sy, -sz*cx, -sy*cz-cy*sz*sx},
		{sz*cy+cz*sx*sy,  cz*cx, -sy*sz+cy*cz*sx},
		{cx*sy         ,    -sx,  cy*cx}
	};

	float Rt[][3] = {
		{R[0][0], R[1][0], R[2][0]},
		{R[0][1], R[1][1], R[2][1]},
		{R[0][2], R[1][2], R[2][2]}
	};

	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, max_fbo);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, max_fbo_tex[next_tex], 0);
	glViewport(0,0, iw, ih);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, max_fbo_tex[cur_tex]);
	glUseProgramObjectARB(shader_prog);
	glUniform1iARB(glGetUniformLocationARB(shader_prog, "prev"), 0);
	glUniformMatrix2x3fv(glGetUniformLocationARB(shader_prog, "R"), 1, 0, (float *)Rt);
	glBegin(GL_QUADS);
		glTexCoord2d(-1,-1); glVertex2d(-1, -1);
		glTexCoord2d( 1,-1); glVertex2d( 1, -1);
		glTexCoord2d( 1, 1); glVertex2d( 1,  1);
		glTexCoord2d(-1, 1); glVertex2d(-1,  1);
	glEnd();
	glUseProgramObjectARB(0);

	audio_data ad;
	audio_get_samples(&ad);
	int samp = IMAX(iw,ih);

	glEnable(GL_BLEND);
	glBlendEquationEXT(GL_MAX_EXT);

	//TODO: use vertex array or VBO
	float pw = 0.5f*fmaxf(1.0f/38, 10.0f/iw), ph = 0.5f*fmaxf(1.0f/38, 10.0f/ih);
	glBindTexture(GL_TEXTURE_2D, pnt_tex);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	glBegin(GL_QUADS); for(int i=0; i<samp; i++) {
		float s = getsamp(&ad, i*ad.len/samp, ad.len/96);
		s=copysignf(log2f(fabsf(s)*3+1)/2, s);

		float xt = (i - samp/2)*1.0f/samp, yt = 0.1f*s, zt = 0.0f;
		float x = R[0][0]*xt + R[1][0]*yt + R[2][0]*zt;
		float y = R[0][1]*xt + R[1][1]*yt + R[2][1]*zt;
		float z = R[0][2]*xt + R[1][2]*yt + R[2][2]*zt;
		glTexCoord2d(0.0,1.0); glVertex3f(x-pw, y-ph, z);
		glTexCoord2d(1.0,1.0); glVertex3f(x+pw, y-ph, z);
		glTexCoord2d(1.0,0.0); glVertex3f(x+pw, y+ph, z);
		glTexCoord2d(0.0,0.0); glVertex3f(x-pw, y+ph, z);
	} glEnd();

	audio_finish_samples();
//	glFlush();
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, 0, 0);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	glPopClientAttrib();
	glPopAttrib();

	tx+=0.02*dt; ty+=0.01*dt; tz-=0.003*dt;
	cur_tex = next_tex;
}

GLuint gl_maxsrc_get() {
	return max_fbo_tex[cur_tex]; //TODO: why does using cur_tex here break?
}
