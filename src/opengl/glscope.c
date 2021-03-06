/**
 * glscope.c
 *
 * functions for drawing the wiggly line in the middle, the name
 * reflects where this whole thing came from (the "superscope" from AVS)
 */

//TODO: maybe pass through a Polyline Decimation before generating vertices http://geomalgorithms.com/a16-_decimate-1.html

#include "common.h"
#include "audio/audio.h"
#include "glmisc.h"
#include "glscope.h"
#include "getsamp.h"

#define xstr(s) str(s)
#define str(s) #s

#define PNT_RADIUS 1.0f

//#define USE_VBO 1

static uint16_t *setup_point_16(int w, int h)
{
	uint16_t *buf = malloc(w * h * sizeof(*buf));
	for(int y=0; y < h; y++)  {
		for(int x=0; x < w; x++) {
			float u = PNT_RADIUS*(1-((float)x)/(w-0.5f)), v = PNT_RADIUS*(1-((float)y)/(h-0.5f)); // for mirrored repeat
			//float u = (2.0f*x)/(w-1) - 1, v = (2.0f*y)/(h-1) - 1;
			buf[y*w + x] = (uint16_t)(expf(-4.5f*log2f(u*u+v*v + 1)/2)*(UINT16_MAX));
		}
	}
	return buf;
}

static const char *pnt_vtx_shader =
	"varying vec2 uv;\n"
	"void main() {\n"
	"	uv = (gl_MultiTexCoord0.st - 1.0f) * " xstr(PNT_RADIUS) ";\n"
	"	gl_Position = gl_Vertex;\n"
	"}";

static const char *pnt_shader_src =
	"varying vec2 uv;\n"
	"void main() {\n"
	"	gl_FragColor = vec4(clamp(exp(-4.5f*0.5f*log2(dot(uv,uv)+1.0f)), 0.0f, 1.0f-1.0f/255.0f));\n"
	"}";

struct glscope_ctx {
	GLuint shader_prog ;
	GLuint pnt_tex;
	int samp;
	float pw, ph;

	GLfloat *sco_verts;

	size_t nidx;
	uint16_t *sco_ind;
};

struct glscope_ctx *gl_scope_init(int width, int height, int num_samp, GLboolean force_fixed)
{
	struct glscope_ctx *ctx = calloc(sizeof(*ctx), 1);
	//point_init(&self->pnt_src, IMAX(w/24, 8), IMAX(h/24, 8));
	ctx->pw = PNT_RADIUS*fmaxf(1.0f/24, 8.0f/width), ctx->ph = PNT_RADIUS*fmaxf(1.0f/24, 8.0f/height);
	ctx->samp = num_samp;

	if(!force_fixed) { // compile succeed
		ctx->shader_prog = compile_program_defs("#version 110\n", pnt_vtx_shader, pnt_shader_src);
		printf("scope shader compiled\n");
	}

	if(!ctx->shader_prog) {
		glGenTextures(1, &ctx->pnt_tex);
		glBindTexture(GL_TEXTURE_2D, ctx->pnt_tex);

		int pnt_size = 32;
		uint16_t *data = setup_point_16(pnt_size, pnt_size);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, pnt_size, pnt_size, 0, GL_LUMINANCE, GL_UNSIGNED_SHORT, data);
		free(data);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glBindTexture(GL_TEXTURE_2D, 0);
		CHECK_GL_ERR;
	}

	GLfloat *sco_verts = ctx->sco_verts = malloc(sizeof(*sco_verts)*ctx->samp*8*4);
	for(int i=0; i<ctx->samp; i++) {
		sco_verts[(i*8+0)*4+0] = 0; sco_verts[(i*8+0)*4+1] = 2;
		sco_verts[(i*8+1)*4+0] = 0; sco_verts[(i*8+1)*4+1] = 0;
		sco_verts[(i*8+2)*4+0] = 1; sco_verts[(i*8+2)*4+1] = 2;
		sco_verts[(i*8+3)*4+0] = 1; sco_verts[(i*8+3)*4+1] = 0;
		sco_verts[(i*8+4)*4+0] = 1; sco_verts[(i*8+4)*4+1] = 2;
		sco_verts[(i*8+5)*4+0] = 1; sco_verts[(i*8+5)*4+1] = 0;
		sco_verts[(i*8+6)*4+0] = 2; sco_verts[(i*8+6)*4+1] = 2;
		sco_verts[(i*8+7)*4+0] = 2; sco_verts[(i*8+7)*4+1] = 0;
	}

	// Now build the index data
	size_t nstrips = ctx->samp;
	size_t ndegenerate = 2 * (nstrips - 1);
	size_t verts_per_strip = 8;
	size_t total_verts = ctx->nidx = verts_per_strip*nstrips + ndegenerate;

 	uint16_t *idx_buf = ctx->sco_ind = malloc(sizeof(*idx_buf)*total_verts);
	for(size_t i = 0, offset = 0; i < nstrips; i++) {
		if(i > 0) { // Degenerate begin: repeat first vertex
			idx_buf[offset++] = i*verts_per_strip;
		}
		for(size_t j = 0; j < verts_per_strip; j++) { // One part of the strip
			idx_buf[offset++] = i*verts_per_strip + j;
		}
		if(i < nstrips-1) { // Degenerate end: repeat last vertex
			idx_buf[offset++] = i*verts_per_strip + (verts_per_strip - 1);
		}
	}
	CHECK_GL_ERR;

	return ctx;
}

void render_scope(struct glscope_ctx *ctx, float R[3][3], const float *data, int len)
{
	// do the rotate/project ourselves because the GL matrix won't do the right
	// thing if we just send it our vertices, we want our tris to always be
	// parallel to the view plane, because we're actually drawing a fuzzy line
	// not a 3D object
	// also it makes it easier to match the software implementation

	GLfloat *sco_verts = ctx->sco_verts;
	const float pw = ctx->pw, ph = ctx->ph;
	const int samp = ctx->samp;

	float px, py;
	{
		float s = getsamp(data, len, 0, len/96);
		s=copysignf(log2f(fabsf(s)*3+1)/2, s);
		float xt = -0.5f, yt = 0.2f*s, zt = 0.0f;
		float x = R[0][0]*xt + R[1][0]*yt + R[2][0]*zt;
		float y = R[0][1]*xt + R[1][1]*yt + R[2][1]*zt;
		float z = R[0][2]*xt + R[1][2]*yt + R[2][2]*zt;
		const float zvd = 1/(z+2);
		px=x*zvd*4/3; py=y*zvd*4/3;
	}

	for(int i=0; i<samp; i++) {
		float s = getsamp(data, len, (i+1)*len/(samp), len/96);
		s=copysignf(log2f(fabsf(s)*3+1)/2, s);

		float xt = (i+1 - (samp)/2.0f)*(1.0f/(samp)), yt = 0.2f*s, zt = 0.0f;
		float x = R[0][0]*xt + R[1][0]*yt + R[2][0]*zt;
		float y = R[0][1]*xt + R[1][1]*yt + R[2][1]*zt;
		float z = R[0][2]*xt + R[1][2]*yt + R[2][2]*zt;
		const float zvd = 1/(z+2);
		x=x*zvd*4/3; y=y*zvd*4/3;

		const float dx=x-px, dy=y-py;
		const float d = 1/hypotf(dx, dy);
		const float tx=dx*d*pw, ty=dy*d*pw;
		const float nx=-dy*d*pw, ny=dx*d*ph;

		sco_verts[(i*8+0)*4+2] = px-nx-tx; sco_verts[(i*8+0)*4+3] = py-ny-ty;
		sco_verts[(i*8+1)*4+2] = px+nx-tx; sco_verts[(i*8+1)*4+3] = py+ny-ty;
		sco_verts[(i*8+2)*4+2] = px-nx   ; sco_verts[(i*8+2)*4+3] = py-ny;
		sco_verts[(i*8+3)*4+2] = px+nx   ; sco_verts[(i*8+3)*4+3] = py+ny;
		sco_verts[(i*8+4)*4+2] =  x-nx   ; sco_verts[(i*8+4)*4+3] =  y-ny;
		sco_verts[(i*8+5)*4+2] =  x+nx   ; sco_verts[(i*8+5)*4+3] =  y+ny;
		sco_verts[(i*8+6)*4+2] =  x-nx+tx; sco_verts[(i*8+6)*4+3] =  y-ny+ty;
		sco_verts[(i*8+7)*4+2] =  x+nx+tx; sco_verts[(i*8+7)*4+3] =  y+ny+ty;
		px=x,py=y;
	}

	//TODO: save/restore GL state
	GLboolean blend_was_enabled = glIsEnabled(GL_BLEND);
	GLint old_blend_eq; glGetIntegerv(GL_BLEND_EQUATION, &old_blend_eq);

	glEnable(GL_BLEND);
	glBlendEquation(GL_MAX);
	if(ctx->shader_prog) glUseProgramObjectARB(ctx->shader_prog);
	if(ctx->pnt_tex) glBindTexture(GL_TEXTURE_2D, ctx->pnt_tex);

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, sizeof(float)*4, sco_verts);
	glVertexPointer(2, GL_FLOAT, sizeof(float)*4, sco_verts + 2);
	glDrawRangeElements(GL_TRIANGLE_STRIP, 0, samp*8, ctx->nidx, GL_UNSIGNED_SHORT, ctx->sco_ind); // core since GL 1.2
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	if(ctx->shader_prog) glUseProgramObjectARB(0);

	glBlendEquation(old_blend_eq);
	if(!blend_was_enabled) glDisable(GL_BLEND);

	CHECK_GL_ERR;
}

