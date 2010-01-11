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
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB10_A2,  width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
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

static int samp = 0;
static float *sco_verts = NULL;
static float *sco_texco = NULL;
static GLboolean have_glsl = GL_FALSE;

static void fixed_init(void);
//TODO: clean up, only generate VBO's if we're going to use them
//TODO: add command line flag to use fixed function pipeline as much as possible
void gl_maxsrc_init(int width, int height) {
	iw=width, ih=height; samp = IMAX(iw,ih);

	if(GLEW_ARB_fragment_shader) {
		setup_max_fbo(width, height);
		shader_prog = compile_program(NULL, frag_src);
		sco_verts = malloc(sizeof(float)*samp*5*4);
		have_glsl = GL_TRUE;
	}
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glGenTextures(1, &pnt_tex);
	glBindTexture(GL_TEXTURE_2D, pnt_tex);

	Pixbuf *src = pnt = setup_point_16(64, 64);
	gluBuild2DMipmaps(GL_TEXTURE_2D, GL_LUMINANCE, src->w, src->h, GL_LUMINANCE, GL_UNSIGNED_SHORT, src->data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,  GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,  GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	fixed_init();

	glPopAttrib();
}

// render the old frame and distort it with GL shading language
static void render_bg_glsl(float R[3][3])
{
	float Rt[][3] = {
		{R[0][0], R[1][0], R[2][0]},
		{R[0][1], R[1][1], R[2][1]},
		{R[0][2], R[1][2], R[2][2]}
	};

	glUseProgramObjectARB(shader_prog);
	glUniform1iARB(glGetUniformLocationARB(shader_prog, "prev"), 0); //TODO: only need to call glGetUniformLocationARB once
	glUniformMatrix2x3fv(glGetUniformLocationARB(shader_prog, "R"), 1, 0, (float *)Rt);
	glBegin(GL_QUADS);
		glTexCoord2d(-1,-1); glVertex2d(-1, -1);
		glTexCoord2d( 1,-1); glVertex2d( 1, -1);
		glTexCoord2d( 1, 1); glVertex2d( 1,  1);
		glTexCoord2d(-1, 1); glVertex2d(-1,  1);
	glEnd();
	glUseProgramObjectARB(0);
}

#define GRID_SIZE 32
static GLint fixedind[GRID_SIZE][GRID_SIZE][4];
static float fixedvtx[GRID_SIZE+1][GRID_SIZE+1][2]; // TODO: this could be a const array filled in...

static union {
	struct { GLhandleARB ind, vtx, txco; };
	GLhandleARB handles[3];
}fixedH;

static GLboolean have_vbo = GL_FALSE;

static void fixed_init(void)
{
	for(int x=0; x<GRID_SIZE; x++) {
		for(int y=0; y<GRID_SIZE; y++) {
			fixedind[x][y][0] = x*(GRID_SIZE+1) + y;
			fixedind[x][y][1] = x*(GRID_SIZE+1) + y+1;
			fixedind[x][y][2] = (x+1)*(GRID_SIZE+1) + y+1;
			fixedind[x][y][3] = (x+1)*(GRID_SIZE+1) + y;
		}
	}
	const float step = 2.0f/GRID_SIZE;
	for(int xd=0; xd<=GRID_SIZE; xd++) {
		float u = xd*step - 1.0f;
		for(int yd=0; yd<=GRID_SIZE; yd++) {
			float v = yd*step - 1.0f;
			fixedvtx[xd][yd][0] = u; fixedvtx[xd][yd][1] = v;
		}
	}

	//TODO: command line option to disable this
	if(GLEW_ARB_vertex_buffer_object) {
		have_vbo = GL_TRUE;
		glGenBuffersARB(3, fixedH.handles);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, fixedH.ind);
		glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, sizeof(fixedind), fixedind, GL_STATIC_DRAW_ARB);

		glBindBufferARB(GL_ARRAY_BUFFER_ARB, fixedH.vtx);
		glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(fixedvtx), fixedvtx, GL_STATIC_DRAW_ARB);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, fixedH.txco);
		glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(fixedvtx), NULL, GL_STREAM_DRAW_ARB);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	}
}

typedef float fixed_txco_buf[GRID_SIZE+1][GRID_SIZE+1][2];

static void render_bg_fixed(float R[3][3])
{
	fixed_txco_buf texco_buf;
	fixed_txco_buf *texco = &texco_buf;

//	glBindBufferARB(GL_ARRAY_BUFFER_ARB, fixedH.txco);
//	float *ptr = glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
//	texco = &ptr;

	const float step = 2.0f/GRID_SIZE;
	for(int xd=0; xd<=GRID_SIZE; xd++) {
		float u = xd*step - 1.0f;
		for(int yd=0; yd<=GRID_SIZE; yd++) {
			float v = yd*step - 1.0f;

			float d = 0.95f + 0.05f*sqrtf(u*u + v*v);
			float p[] = { // first rotate our frame of reference, then do a zoom along 2 of the 3 axis
				(u*R[0][0] + v*R[0][1]),
				(u*R[1][0] + v*R[1][1])*d,
				(u*R[2][0] + v*R[2][1])*d
			};

			// rotate back and shift/scale to [0, GRID_SIZE]
			float x = (p[0]*R[0][0] + p[1]*R[1][0] + p[2]*R[2][0]+1.0f)*0.5f;
			float y = (p[0]*R[0][1] + p[1]*R[1][1] + p[2]*R[2][1]+1.0f)*0.5f;

			(*texco)[xd][yd][0] = x; (*texco)[xd][yd][1] = y;
		}
	}
//	glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);

	//TODO: try to come up with something that will run faster
	glClearColor(1.0f/256, 1.0f/256,1.0f/256, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glClearColor(0,0,0,1);

	glEnable(GL_BLEND);
	glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE);
	glBlendEquationEXT(GL_FUNC_SUBTRACT_EXT);
	glBlendColor(0, 0, 0, 63.0f/64);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, 0, *texco);
	if(have_vbo) {
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, fixedH.ind);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, fixedH.vtx);
		glVertexPointer(2, GL_FLOAT, 0, 0);
		glDrawElements(GL_QUADS, GRID_SIZE*GRID_SIZE*4, GL_UNSIGNED_INT, 0);
//		glBindBufferARB(GL_ARRAY_BUFFER_ARB, fixedH.txco);
//		glTexCoordPointer(2, GL_FLOAT, 0, 0);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	} else {
		glVertexPointer(2, GL_FLOAT, 0, fixedvtx);
		glDrawElements(GL_QUADS, GRID_SIZE*GRID_SIZE*4, GL_UNSIGNED_INT, fixedind);
	}
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
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

	if(have_glsl) render_bg_glsl(R);
	else render_bg_fixed(R);

	// ******************* normal
	float pw = 0.5f*fmaxf(1.0f/38, 10.0f/iw), ph = 0.5f*fmaxf(1.0f/38, 10.0f/ih);
	audio_data ad; audio_get_samples(&ad);
	for(int i=0; i<samp; i++) {
		float s = getsamp(&ad, i*ad.len/samp, ad.len/96);
		s=copysignf(log2f(fabsf(s)*3+1)/2, s);

		float xt = (i - samp/2)*1.0f/samp, yt = 0.1f*s, zt = 0.0f;
		float x = R[0][0]*xt + R[1][0]*yt + R[2][0]*zt;
		float y = R[0][1]*xt + R[1][1]*yt + R[2][1]*zt;
		float z = R[0][2]*xt + R[1][2]*yt + R[2][2]*zt;

		sco_verts[(i*4+0)*5+0] = 0; sco_verts[(i*4+0)*5+1] = 1; sco_verts[(i*4+0)*5+2] = x-pw; sco_verts[(i*4+0)*5+3] = y-ph; sco_verts[(i*4+0)*5+4] = z;
		sco_verts[(i*4+1)*5+0] = 1; sco_verts[(i*4+1)*5+1] = 1; sco_verts[(i*4+1)*5+2] = x+pw; sco_verts[(i*4+1)*5+3] = y-ph; sco_verts[(i*4+1)*5+4] = z;
		sco_verts[(i*4+2)*5+0] = 1; sco_verts[(i*4+2)*5+1] = 0; sco_verts[(i*4+2)*5+2] = x+pw; sco_verts[(i*4+2)*5+3] = y+ph; sco_verts[(i*4+2)*5+4] = z;
		sco_verts[(i*4+3)*5+0] = 0; sco_verts[(i*4+3)*5+1] = 0; sco_verts[(i*4+3)*5+2] = x-pw; sco_verts[(i*4+3)*5+3] = y+ph; sco_verts[(i*4+3)*5+4] = z;
	}
	audio_finish_samples();

	glEnable(GL_BLEND);
	glBlendEquationEXT(GL_MAX_EXT);
	glBindTexture(GL_TEXTURE_2D, pnt_tex);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glEnableClientState(GL_VERTEX_ARRAY); //FIXME
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glInterleavedArrays(GL_T2F_V3F, 0, sco_verts); //TODO: replace with seperate calls to glVertexPointer/glTexCoordPointer also maybe use VBOs and have a fixed VBO for tex-coords
	glDrawArrays(GL_QUADS, 0, samp*4);

// ********************** Point spirte -- broken in mesa? (tex co-ords are wrong, don't seem to maxblend)
//	glEnable(GL_BLEND);
//	glBlendEquationEXT(GL_MAX_EXT);
//	float pnt_size = fmaxf(iw/38.0, 10);
//	float quadratic[] =  { 1.0f, 0.0f, 0.01f };
//	float maxSize = 0.0f;
//	glEnable(GL_POINT_SPRITE_ARB);
////	glGetFloatv(GL_POINT_SIZE_MAX_ARB, &maxSize );
////	glPointParameterfvARB(GL_POINT_DISTANCE_ATTENUATION_ARB, quadratic);
////	glPointParameterf(GL_POINT_SIZE_MIN, 1.0f);
////	glPointParameterf(GL_POINT_SIZE_MAX, maxSize);
//	glPointSize(pnt_size);
//	glBindTexture(GL_TEXTURE_2D, pnt_tex);
//	glTexEnvf(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE);
////	glPointParameteri(GL_POINT_SPRITE_COORD_ORIGIN, GL_UPPER_LEFT);
//
//	float verts[samp][6]; // need to include colour or the points don't show (BLEH!)
//	audio_data ad; audio_get_samples(&ad); for(int i=0; i<samp; i++) {
//		float s = getsamp(&ad, i*ad.len/samp, ad.len/96);
//		s=copysignf(log2f(fabsf(s)*3+1)/2, s);
//
//		float xt = (i - samp/2)*1.0f/samp, yt = 0.1f*s, zt = 0.0f;
//		float x = R[0][0]*xt + R[1][0]*yt + R[2][0]*zt;
//		float y = R[0][1]*xt + R[1][1]*yt + R[2][1]*zt;
//		float z = R[0][2]*xt + R[1][2]*yt + R[2][2]*zt;
//		verts[i][0] = 1; verts[i][1] = 1; verts[i][2] = 1;
//		verts[i][3] = x; verts[i][4] = y; verts[i][5] = z;
//	} audio_finish_samples();
//	glEnableClientState(GL_VERTEX_ARRAY);
//	glEnableClientState(GL_COLOR_ARRAY);
////	glVertexPointer(3, GL_FLOAT, 0, verts);
//	glInterleavedArrays(GL_C3F_V3F, 0, verts);
//	glDrawArrays(GL_POINTS, 0, samp);
//	glDisableClientState(GL_COLOR_ARRAY);
//	glDisableClientState(GL_VERTEX_ARRAY);
//	glDisable( GL_POINT_SPRITE_ARB );

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	glPopClientAttrib();
	glPopAttrib();

	tx+=0.02*dt; ty+=0.01*dt; tz-=0.003*dt;
	cur_tex = next_tex;
}

GLuint gl_maxsrc_get() {
	return max_fbo_tex[cur_tex]; //TODO: why does using cur_tex here break?
}
