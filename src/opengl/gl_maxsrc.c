/**
 * gl_maxsrc.c
 *
 */

#include "common.h"

#include <SDL.h>
#include <GL/glew.h>
#include <GL/glut.h>

#include "audio/audio.h"
#include "glmisc.h"
#include "gl_maxsrc.h"

static Pixbuf *setup_point_16(int w, int h)
{
	Pixbuf *surf = malloc(sizeof(Pixbuf));
	uint16_t *buf = surf->data = malloc(w * h * sizeof(*buf)); surf->bpp  = 16;
	surf->w = w; surf->h = h; surf->pitch = surf->w*sizeof(*buf);

	for(int y=0; y < h; y++)  {
		for(int x=0; x < w; x++) {
//			float u = (2.0f*x)/w - 1, v = (2.0f*y)/h - 1;
			float u = 1-((float)x)/w, v = 1-((float)y)/h;
			buf[y*w + x] = (uint16_t)(expf(-4.5f*(u*u+v*v))*(UINT16_MAX));
		}
	}
	return surf;
}

static Pixbuf *setup_point_32(int w, int h)
{
	Pixbuf *surf = malloc(sizeof(Pixbuf));
	uint32_t *buf = surf->data = malloc(w * h * sizeof(*buf)); surf->bpp  = 32;
	surf->w = w; surf->h = h; surf->pitch = surf->w*sizeof(*buf);

	for(int y=0; y < h; y++)  {
		for(int x=0; x < w; x++) {
			float u = 1-((float)x)/w, v = 1-((float)y)/h;
			buf[y*w + x] = (uint32_t)(expf(-4.5f*(u*u+v*v))*(UINT32_MAX));
		}
	}
	return surf;
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

//TODO: use 2 basis vectors to find p instead of matrix op
// ie, uniform vec3 a,b; 
//     where a = <1,0,0>*R, b = <0, 1, 0>*R
// then vec3 p = (uv.x*a + uv.y*b)*vec3(1,d,d);
// and final co-ords are still R*p, except we can make R a mat3x2 and maybe save some MAD's

static const char *frag_src =
	"#version 120\n"
	"uniform sampler2D prev;\n"
	"invariant uniform mat2x3 R;\n"
	"void main() {\n"
	"	vec3 p = vec3(0.5f);\n"
	"	{\n"
	"		vec2 uv = gl_TexCoord[0].st;\n"
	"		float d = 0.96f*0.5f + (0.04f*0.5f)*log2(length(uv)*0.707106781186547f + 1);\n"
	"		p.yz = vec2(d); p = (uv.x*R[0] + uv.y*R[1])*p;\n" //NOTE: on mesa 7.6's compiler p*= generates more code for some reason
	"	}\n"
	"	vec4 c = texture2D(prev, p*R + 0.5f);\n"
	"	gl_FragData[0] = (c - max(vec4(2/256.0f), c*(1.0f/100)));\n" //TODO: only do this if tex format not precise enough
	"}\n";

static const char *frag_src_mix =
	"#version 120\n"
	FLOAT_PACK_FUNCS
	"uniform sampler2D prev;\n"
	"invariant uniform mat2x3 R;\n"
	"void main() {\n"
	"	vec3 p = vec3(0.5f);\n"
	"	{\n"
	"		vec2 uv = gl_TexCoord[0].st;\n"
//	"		float d = 0.96f*0.5f + (0.04f*0.5f)*log2(length(uv) + 1);\n"
	"		float d = 0.95f*0.5f + (0.05f*0.5f)*length(uv);"
	"		p.yz = vec2(d); p = (uv.x*R[0] + uv.y*R[1])*p;\n" //NOTE: on mesa 7.6's compiler p*= generates more code for some reason
	"	}\n"
//	"	float c = decode(texture2D(prev, p*R + 0.5f));\n"
//	"	gl_FragData[0] = encode(pow(pow(c,2.2f)*(97.0f/100), 1/2.2f));\n"
//	"	gl_FragData[0] = encode(pow(decode(texture2D(prev, p*R + 0.5f)), 1.03));\n"
//	"	gl_FragData[0] = encode(decode(texture2D(prev, p*R + 0.5f)) - 1.0f/100);\n"
	"	gl_FragData[0] = encode(decode(texture2D(prev, p*R + 0.5f))*(98.0f/100));\n"
	"}\n";

static void bg_vtx(float u, float v, vec2f *txco, void *cb_data) {
//	float d = 0.95f + 0.05f*sqrtf(u*u + v*v);
//	float p[] = { // first rotate our frame of reference, then do a zoom along 2 of the 3 axis
//		(u*(*R)[0][0] + v*(*R)[0][1]),
//		(u*(*R)[1][0] + v*(*R)[1][1])*d,
//		(u*(*R)[2][0] + v*(*R)[2][1])*d
//	};
//	txco->x = (p[0]*(*R)[0][0] + p[1]*(*R)[1][0] + p[2]*(*R)[2][0]+1.0f)*0.5f;
//	txco->y = (p[0]*(*R)[0][1] + p[1]*(*R)[1][1] + p[2]*(*R)[2][1]+1.0f)*0.5f;

	float *R = cb_data;
	float d = 0.95f + 0.05f*sqrtf(u*u + v*v);
	float p[] = { // first rotate our frame of reference, then do a zoom along 2 of the 3 axis
		(u*R[0*3+0] + v*R[0*3+1]),
		(u*R[1*3+0] + v*R[1*3+1])*d,
		(u*R[2*3+0] + v*R[2*3+1])*d
	};
	txco->x = (p[0]*R[0*3+0] + p[1]*R[1*3+0] + p[2]*R[2*3+0]+1.0f)*0.5f;
	txco->y = (p[0]*R[0*3+1] + p[1]*R[1*3+1] + p[2]*R[2*3+1]+1.0f)*0.5f;
//	txco->x = (u+1)/2; txco->y = (v+1)/2;
}

static GLuint pnt_tex = 0;
static GLhandleARB shader_prog = 0;
static GLuint max_fbo = 0, max_fbo_tex[2] = { 0, 0 };
static int cur_tex = 0;
static int iw, ih;
static int samp = 0;
static float pw = 0, ph = 0;
static float *sco_verts = NULL;
static GLboolean use_glsl = GL_FALSE;
static Map *fixed_map = NULL;
GEN_MAP_CB(fixed_map_cb, bg_vtx);

void gl_maxsrc_init(int width, int height, GLboolean packed_intesity_pixels, GLboolean force_fixed) {
	iw=width, ih=height;
	pw = 0.5f*fmaxf(1.0f/38, 10.0f/iw), ph = 0.5f*fmaxf(1.0f/38, 10.0f/ih);
	samp = (int)(10*fmaxf(1/pw,1/ph));
	printf("maxsrc using %n points\n", samp);
	sco_verts = malloc(sizeof(float)*samp*5*4);

	if(glewGetExtension("GL_ARB_shading_language_120") && !force_fixed) {
		use_glsl = GL_TRUE;
		printf("Compiling maxsrc shader:\n");
		if(packed_intesity_pixels)
			shader_prog = compile_program(NULL, frag_src_mix);
		else
			shader_prog = compile_program(NULL, frag_src);
		printf("maxsrc shader compiled\n");
	} else {
		use_glsl = GL_FALSE;
		fixed_map = map_new(32, fixed_map_cb);
	}

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glGenFramebuffersEXT(1, &max_fbo);
	glGenTextures(2, max_fbo_tex);
	for(int i=0; i<2; i++) {
		glBindTexture(GL_TEXTURE_2D, max_fbo_tex[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,  width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}

	Pixbuf *src = NULL;
	glGenTextures(1, &pnt_tex);
	glBindTexture(GL_TEXTURE_2D, pnt_tex);
	if(use_glsl && packed_intesity_pixels) {
		src = setup_point_32(32, 32);
		gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, src->w, src->h, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, src->data);
	} else {
		src = setup_point_16(32, 32);
		gluBuild2DMipmaps(GL_TEXTURE_2D, GL_LUMINANCE, src->w, src->h, GL_LUMINANCE, GL_UNSIGNED_SHORT, src->data);
	}
	free(src->data);
	free(src);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,  GL_MIRRORED_REPEAT_ARB);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,  GL_MIRRORED_REPEAT_ARB);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

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
	glBindTexture(GL_TEXTURE_2D, max_fbo_tex[cur_tex]);
	glBegin(GL_QUADS);
		glTexCoord2d(-1,-1); glVertex2d(-1, -1);
		glTexCoord2d( 1,-1); glVertex2d( 1, -1);
		glTexCoord2d( 1, 1); glVertex2d( 1,  1);
		glTexCoord2d(-1, 1); glVertex2d(-1,  1);
	glEnd();
	glUseProgramObjectARB(0);
}

static void render_bg_fixed(float R[3][3])
{
	//TODO: try to come up with something that will run faster
	glClearColor(1.0f/256, 1.0f/256,1.0f/256, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glClearColor(0,0,0,1);

	glEnable(GL_BLEND);
	glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE);
	glBlendEquationEXT(GL_FUNC_SUBTRACT_EXT);
	glBlendColor(0, 0, 0, 63.0f/64);

	glBindTexture(GL_TEXTURE_2D, max_fbo_tex[cur_tex]);
	map_render(fixed_map, R);
}

void gl_maxsrc_update(void)
{
	int next_tex = (cur_tex+1)%2;
	float dt = 1;
//	static static Uint32 lastupdate = -1;
//	if(lastupdate != -1) {
////		dt = (now - lastupdate)*0.05f;
//		if(now - lastupdate < 50) return; // limit FPS
//	}
//	lastupdate = now;

	float cx=cosf(tx), cy=cosf(ty), cz=cosf(tz);
	float sx=sinf(tx), sy=sinf(ty), sz=sinf(tz);

	float R[3][3] = {
		{cz*cy-sz*sx*sy, -sz*cx, -sy*cz-cy*sz*sx},
		{sz*cy+cz*sx*sy,  cz*cx, -sy*sz+cy*cz*sx},
		{cx*sy         ,    -sx,  cy*cx}
	};

	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);

	glActiveTexture(GL_TEXTURE0);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, max_fbo);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, max_fbo_tex[next_tex], 0);
	setup_viewport(iw, ih);

	if(use_glsl) render_bg_glsl(R);
	else render_bg_fixed(R);
	CHECK_GL_ERR;

	// ******************* normal
	audio_data ad; audio_get_samples(&ad);
	for(int i=0; i<samp; i++) {
		float s = getsamp(&ad, i*ad.len/samp, ad.len/96);
		s=copysignf(log2f(fabsf(s)*3+1)/2, s);

		float xt = (i - samp/2)*1.0f/samp, yt = 0.1f*s, zt = 0.0f;
		float x = R[0][0]*xt + R[1][0]*yt + R[2][0]*zt;
		float y = R[0][1]*xt + R[1][1]*yt + R[2][1]*zt;
		float z = R[0][2]*xt + R[1][2]*yt + R[2][2]*zt;

		sco_verts[(i*4+0)*5+0] = 0; sco_verts[(i*4+0)*5+1] = 2; sco_verts[(i*4+0)*5+2] = x-pw; sco_verts[(i*4+0)*5+3] = y-ph; sco_verts[(i*4+0)*5+4] = z;
		sco_verts[(i*4+1)*5+0] = 2; sco_verts[(i*4+1)*5+1] = 2; sco_verts[(i*4+1)*5+2] = x+pw; sco_verts[(i*4+1)*5+3] = y-ph; sco_verts[(i*4+1)*5+4] = z;
		sco_verts[(i*4+2)*5+0] = 2; sco_verts[(i*4+2)*5+1] = 0; sco_verts[(i*4+2)*5+2] = x+pw; sco_verts[(i*4+2)*5+3] = y+ph; sco_verts[(i*4+2)*5+4] = z;
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

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	glPopClientAttrib();
	glPopAttrib();

	tx+=0.02*dt; ty+=0.01*dt; tz-=0.003*dt;
	cur_tex = next_tex;

	CHECK_GL_ERR;
}

GLuint gl_maxsrc_get(void) {
	return max_fbo_tex[(cur_tex+1)%2];
//	return max_fbo_tex[(cur_tex)];
}
