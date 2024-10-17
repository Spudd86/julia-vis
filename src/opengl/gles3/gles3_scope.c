#include <GLES3/gl3.h>

#include "common.h"
#include "getsamp.h"
#include "gles3misc.h"
#include "opengl/glscope.h"

#define PNT_RADIUS 1.0f

// Use shader with vertex index so we can just upload a list of filtered audio samples and transform them on the gpu

#define xstr(s) str(s)
#define str(s) #s

static const char *pnt_vtx_shader =
	"layout (location = 0) in vec2 s;\n"

	"uniform mat3 R;\n"
	"uniform uint num_samp;\n"
	"uniform vec2 pnt_size;\n"

	"out vec2 uv;\n"

	// TODO: projection matrix?

	"const lowp vec2 texc[8] = vec2[8](\n"
	"	vec2(-1,  1),\n"
	"	vec2(-1, -1),\n"
	"	vec2( 0,  1),\n"
	"	vec2( 0, -1),\n"
	"	vec2( 0,  1),\n"
	"	vec2( 0, -1),\n"
	"	vec2( 1,  1),\n"
	"	vec2( 1, -1)\n"
	");\n"
	"const lowp ivec2 tns_arr[8] = ivec2[8](\n"
	"	ivec2(-1, -1), \n"
	"	ivec2( 1, -1), \n"
	"	ivec2(-1,  0), \n"
	"	ivec2( 1,  0), \n"
	"	ivec2(-1,  0), \n"
	"	ivec2( 1,  0), \n"
	"	ivec2(-1,  1), \n"
	"	ivec2( 1,  1)\n"
	");\n"

	"void main() {\n"
	"	uv = texc[gl_VertexID];\n"

	"	vec3 p1 = R * vec3(float(gl_InstanceID+0)/float(num_samp-1u) - 0.5, s.x*0.2, 0);\n" // calculate position based on instance
	"	vec3 p2 = R * vec3(float(gl_InstanceID+1)/float(num_samp-1u) - 0.5, s.y*0.2, 0);\n"
	"	vec2 p[2] = vec2[2](\n"
	"		(p1.xy/(p1.z + 2.0))*4.0/3.0,\n"
	"		(p2.xy/(p2.z + 2.0))*4.0/3.0\n"
	"	);\n"
	"	vec2 t = normalize(p[1] - p[0]) * pnt_size;\n"
	"	vec2 n = vec2(-t.y, t.x);\n" // Normal to t
	"	vec2 tns = vec2(tns_arr[gl_VertexID]);\n"
	"	vec2 pos = mix(p[0], p[1], bvec2(gl_VertexID >= 4));\n"
	"	gl_Position = vec4(pos + n*tns.x+ t*tns.y, 0.0 , 1.0 );\n"
	"}";

static const char *pnt_shader_src =
	"in vec2 uv;\n"
	"out highp uvec4 fragColour;\n"
	// "out vec4 fragColour;\n"
	"const uint colmax = (1u<<16) - 1u;\n"
	"void main() {\n"
	"	float v = exp( -4.5*0.5*log2( dot(uv,uv)+1.0 ) );\n"
	"	v = clamp(v, 0.0, 1.0);\n"
	"	gl_FragDepth = v;\n"
	// "	fragColour = vec4(v);\n"
	"	fragColour = uvec4( v*float(colmax) );\n"
	"}";

struct glscope_ctx {
	GLuint shader_prog ;
	GLint R_loc;
	GLuint vbo ;
	int samp;
	float pw, ph;

	float data[];
};

struct glscope_ctx *gl_scope_init(int width, int height, int num_samp, GLboolean force_fixed)
{
	struct glscope_ctx *ctx = calloc(sizeof(*ctx) + sizeof(float)*num_samp, 1);
	ctx->pw = PNT_RADIUS*fmaxf(1.0f/24, 8.0f/width), ctx->pw  = PNT_RADIUS*fmaxf(1.0f/24, 8.0f/height);
	ctx->samp = num_samp;

	glGenBuffers(1, &ctx->vbo);

	printf("Compiling scope shaders\n");
	ctx->shader_prog = compile_program_defs(NULL, pnt_vtx_shader, pnt_shader_src);
	glUseProgram(ctx->shader_prog); CHECK_GL_ERR;
	ctx->R_loc = glGetUniformLocation(ctx->shader_prog, "R"); CHECK_GL_ERR;
	glUniform2f(glGetUniformLocation(ctx->shader_prog, "pnt_size"), ctx->pw, ctx->pw ); CHECK_GL_ERR;
	glUniform1ui(glGetUniformLocation(ctx->shader_prog, "num_samp"), ctx->samp); CHECK_GL_ERR;
	glUseProgram(0); CHECK_GL_ERR;
	printf("Scope shaders done\n");

	CHECK_GL_ERR;

	return ctx;
}

void render_scope(struct glscope_ctx *ctx, float R[3][3], const float *data, int len)
{
	int samp = ctx->samp;


	struct samp_state samp_state;
	getsamp_step_init(&samp_state, data, len, len/96);

	//TODO: double check that this is correct, and that shader is doing correct side multiply
	// const float Rt[9] = {
	// 	R[0][0], R[1][0], R[2][0],
	// 	R[0][1], R[1][1], R[2][1],
	// 	R[0][2], R[1][2], R[2][2],
	// };


	GLboolean blend_was_enabled = glIsEnabled(GL_BLEND);
	GLint old_blend_eq, alpha_blend_eq;
	glGetIntegerv(GL_BLEND_EQUATION_RGB, &old_blend_eq);
	glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &alpha_blend_eq);

	// glBlendFunc(GL_ONE, GL_ONE);
	// glBlendEquationSeparate(GL_FUNC_ADD, alpha_blend_eq); CHECK_GL_ERR;

	glEnable(GL_BLEND); CHECK_GL_ERR;
	glEnable(GL_DEPTH_TEST); CHECK_GL_ERR; glDepthFunc(GL_GREATER);

	// glBlendEquationSeparate(GL_MAX, alpha_blend_eq); CHECK_GL_ERR;

	glUseProgram(ctx->shader_prog); CHECK_GL_ERR;
	glUniformMatrix3fv(ctx->R_loc, 1, GL_TRUE, R); CHECK_GL_ERR;
	glEnableVertexAttribArray(0); CHECK_GL_ERR;

	glBindBuffer(GL_ARRAY_BUFFER, ctx->vbo); CHECK_GL_ERR;
	glBufferData(GL_ARRAY_BUFFER, sizeof(*ctx->data)*samp, NULL, GL_STREAM_DRAW); CHECK_GL_ERR;
	float *dst_buf = glMapBufferRange(GL_ARRAY_BUFFER, 0, sizeof(float)*samp, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT); CHECK_GL_ERR;
	if(dst_buf) {
		for(int i=0; i<samp; i++) {
			dst_buf[i] = getsamp_step(&samp_state, i*len/(samp-1));
		}
		glUnmapBuffer(GL_ARRAY_BUFFER);
		CHECK_GL_ERR;
	}
	else {
		CHECK_GL_ERR;
		glUnmapBuffer(GL_ARRAY_BUFFER);
		for(int i=0; i<samp; i++) {
			ctx->data[i] = getsamp_step(&samp_state, i*len/(samp-1));
		}
		glBufferData(GL_ARRAY_BUFFER, sizeof(*ctx->data)*samp, ctx->data, GL_STREAM_DRAW);
		CHECK_GL_ERR;
	}
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float), 0); // stride is only one float since we want to step by line segments
	glVertexAttribDivisor(0, 1);

	glDrawArraysInstanced( GL_TRIANGLE_STRIP, 0, 8, samp-1);

	glDisableVertexAttribArray(0);
	glUseProgram(0);

	glDisable(GL_DEPTH_TEST);

	glBlendEquationSeparate(old_blend_eq, alpha_blend_eq);
	if(!blend_was_enabled) glDisable(GL_BLEND);
	CHECK_GL_ERR;
}
