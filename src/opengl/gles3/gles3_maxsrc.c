#include <SDL2/SDL.h>
#include <SDL2/SDL_opengles2.h>


#include "common.h"
#include "getsamp.h"
#include "audio/audio.h"

#include "gles3misc.h"
#include "opengl/glscope.h"
#include "opengl/glmaxsrc.h"

static const char *vtx_shader_src =
	"const vec2 vert[4] = vec2[4](vec2(-1, -1), vec2( 1, -1), vec2(-1,  1), vec2( 1,  1));\n"
	"out highp vec2 uv;\n"
	"void main() {\n"
	"	uv = vert[gl_VertexID];\n"
	"	gl_Position = vec4(vert[gl_VertexID], 0. , 1. );\n"
	"}";
static const char *frag_shader_src =
	"uniform highp usampler2D prev;\n"
	"uniform highp mat3 R;\n"
	"in highp vec2 uv;\n"
	"out uvec4 fragColour;\n"
	"void main() {\n"
	// "	fragColour = vec4(uv*0.5+0.5,0,1 );\n"
#if 1
	"	highp vec2 p;\n"
	"	highp float d = 0.95 + 0.053*sqrt(dot(uv,uv));\n"
	"	highp vec3 t = vec3((uv.x*R[0][0] + uv.y*R[0][1]),\n"
	"	                    (uv.x*R[1][0] + uv.y*R[1][1])*d,\n"
	"	                    (uv.x*R[2][0] + uv.y*R[2][1])*d);\n"
	"	p = vec2((t.x*R[0][0] + t.y*R[1][0] + t.z*R[2][0]+1.0)*0.5,\n"
	"	         (t.x*R[0][1] + t.y*R[1][1] + t.z*R[2][1]+1.0)*0.5);\n"
	// "	p = (t*R).xy*0.5 + 0.5;\n"
	// "		highp vec3 t = vec3(1, vec2(0.95 + 0.053*length(uv))) * 0.5;\n"
	// "		t = (uv.x*R[0] + uv.y*R[1])*t;\n"
	// "		t = vec3(uv, 0)*R*t;\n"
	// "		p = (t*R).xy + 0.5;\n"

	// "	uint v = ((texture(prev, p).r * 256u * 97u)/100u) >> 8;\n"
	// "	uint v = (texture(prev, p).r);\n"
	// TODO: need to filter ourselves because GLES doesn't have filtering for R16UI textures
	// TODO: force to black outside image (helper function something like uint fetchClamp(ivec2 p) {return mix(texelFetch(prev, p, 0).x, 0, (p < 0) || (p >= textureSize(prev, 0) - 1) ); } )

	// could use ivec2 p = trunc(textureSize(prev, 0) * p);vec2 pf = fract(textureSize(prev, 0) * p)
	// Should do the same thing as the software
	// "	p = max(p, vec2(0,0));\n" // Clamp
	// "	highp ivec2 pi = ivec2(vec2(textureSize(prev, 0)) * p * 256.0);\n"
	// "	highp uvec2 pf = uvec2(pi.x & 0xFF, pi.y & 0xFF);\n"
	// "	highp ivec2 ps = ivec2(pi.x >> 8, pi.y >> 8);\n"

	"	ivec2 ps = ivec2(trunc(vec2(textureSize(prev, 0)) * p));\n"
	"	uvec2 pf = uvec2(fract(vec2(textureSize(prev, 0)) * p) * 256.0);\n"
	"	ivec2 bound = textureSize(prev, 0) - 1;\n"
	"	highp uint p00 = texelFetch(prev, min(ps+ivec2(0,0), bound), 0).x;\n"
	"	highp uint p01 = texelFetch(prev, min(ps+ivec2(0,1), bound), 0).x;\n"
	"	highp uint p10 = texelFetch(prev, min(ps+ivec2(1,0), bound), 0).x;\n"
	"	highp uint p11 = texelFetch(prev, min(ps+ivec2(1,1), bound), 0).x;\n"
	"	highp uint v = ((p00*(256u - pf.x) + p01 * pf.x) * (256u - pf.y) \n"
	"	              + (p10*(256u - pf.x) + p11 * pf.x) * pf.y) >> 16;\n"
	"	v = (v * ((256u*97u)/100u)) >> 8u;\n"

	"	fragColour = uvec4(v);\n"
	"	gl_FragDepth = float(v)/65535.0;\n"
#endif
	"}";

struct priv_ctx {
	struct glmaxsrc_ctx pubctx;
	struct glscope_ctx *glscope;
	float tx, ty, tz;
	GLuint prog;
	GLint R_loc;
	uint32_t lastupdate;
};

static void update(struct glmaxsrc_ctx *ctx, const float *audio, int audiolen);

struct glmaxsrc_ctx *maxsrc_new_gles3(int width, int height)
{
	printf("Compiling maxsrc shader:\n");
	// const char *defs = packed_intesity_pixels?"#version 110\n#define FLOAT_PACK_PIX\n":"#version 110\n";
	GLuint prog = compile_program_defs(NULL, vtx_shader_src, frag_shader_src);
	GLint R_loc = -1;
	if(!prog) return NULL;

	printf("maxsrc shader compiled\n");
	glUseProgram(prog);
	glUniform1i(glGetUniformLocation(prog, "prev"), 0);
	R_loc = glGetUniformLocation(prog, "R");
	glUseProgram(0);
	CHECK_GL_ERR;

	int samp = MIN(MIN(width/8, height/8), 128);
	printf("maxsrc using %i points\n", samp);
	struct priv_ctx *priv = calloc(sizeof(*priv), 1);
	priv->pubctx.update = update;
	priv->pubctx.offscr = offscr_new(width, height, false, true);
	priv->glscope = gl_scope_init(width, height, samp, false);
	priv->prog = prog;
	priv->R_loc = R_loc;

	return &priv->pubctx;
}

static void update(struct glmaxsrc_ctx *ctx, const float *audio, int audiolen)
{DEBUG_CHECK_GL_ERR;
	struct priv_ctx *priv = (struct priv_ctx *)ctx;
	const uint32_t now = get_ticks();
	const float dt = (now - priv->lastupdate)*24/1000.0f;
	priv->lastupdate = now;

	float cx=cosf(priv->tx), cy=cosf(priv->ty), cz=cosf(priv->tz);
	float sx=sinf(priv->tx), sy=sinf(priv->ty), sz=sinf(priv->tz);

	float R[3][3] = {
		{cz*cy-sz*sx*sy, -sz*cx, -sy*cz-cy*sz*sx},
		{sz*cy+cz*sx*sy,  cz*cx, -sy*sz+cy*cz*sx},
		{cx*sy         ,    -sx,  cy*cx}
	};

	GLint src_tex = offscr_start_render(ctx->offscr);

	glClearDepthf(0.0f);
	// glClear(GL_COLOR_BUFFER_BIT);
	// glClear(GL_DEPTH_BUFFER_BIT);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	// glEnable(GL_DEPTH_TEST);
	// glDepthFunc(GL_ALWAYS);
	// glUseProgram(priv->prog);
	// glUniformMatrix3fv(priv->R_loc, 1, GL_TRUE, R);
	// glBindTexture(GL_TEXTURE_2D, src_tex);
	// glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	// glBindTexture(GL_TEXTURE_2D, 0);
	// glUseProgram(0);
	// CHECK_GL_ERR;

	render_scope(priv->glscope, R, audio, audiolen);
	DEBUG_CHECK_GL_ERR;
	offscr_finish_render(ctx->offscr);

	priv->tx+=0.02f*dt; priv->ty+=0.01f*dt; priv->tz-=0.003f*dt;

	DEBUG_CHECK_GL_ERR;
}
