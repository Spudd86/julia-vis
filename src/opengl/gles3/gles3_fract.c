#include "common.h"
#include "points.h"

#include "gles3misc.h"
#include "gles_fract.h"


struct gles_ctx {
	struct glfract_ctx pubctx;
	GLint prog[GLES_MAP_FUNC_NUM_FUNCS];
	GLint c_loc[GLES_MAP_FUNC_NUM_FUNCS];
	bool maxsrc_map;
	gles_map_func func;
};

static const char *vtx_shader_src =
	"const vec2 texc[4] = vec2[4](vec2(0, 0), vec2(1, 0), vec2(0, 1), vec2(1, 1));\n"
	"const vec2 vert[4] = vec2[4](vec2(-1, -1), vec2( 1, -1), vec2(-1,  1), vec2( 1,  1));\n"
	"out vec2 uv;\n"
	"void main() {\n"
	"	uv = texc[gl_VertexID];\n"
	"	gl_Position = vec4(vert[gl_VertexID], 0. , 1. );\n"
	"}";

// TODO: split fragment shader into a co-ord calculations function/uniform declaration and actual shader

static const char *map_frag_shader =
	"uniform vec2 c;\n"
	"highp vec2 map(highp vec2 s) {\n"
	"	highp vec2 c_adj = vec2((c.x-0.5)*0.25 + 0.5, c.y*0.25 + 0.5);\n"
	"	highp vec2 t = s*s;\n"
	"	return vec2(t.x - t.y, 2.0*s.x*s.y) + c_adj;\n"
	"}\n\n"
	"in vec2 uv;\n"
	"out uvec4 fragColour;\n"
	"uniform highp usampler2D maxsrc;\n"
	"uniform mediump usampler2D prev;\n"
	// "out vec4 fragColour;\n"
	// "uniform highp sampler2D maxsrc;\n"
	// "uniform highp sampler2D prev;\n"
	"void main() {\n"
	"	vec2 s = uv*2.0-1.0;"
	"	vec2 p = (map(s*2.0)+1.0)*0.5*0.5;\n"
	"	uint v = texture(prev, p).r;\n"

	// "	p = max(p, vec2(0,0));\n" // Clamp
	// "	highp ivec2 pi = ivec2(vec2(textureSize(prev, 0)) * p * 256.0);\n"
	// "	ivec2 ps = ivec2(trunc(vec2(textureSize(prev, 0)) * p));\n"
	// "	uvec2 pf = uvec2(fract(vec2(textureSize(prev, 0)) * p) * 256.0);\n"
	// "	ivec2 bound = textureSize(prev, 0) - 1;\n"
	// "	highp uint p00 = texelFetch(prev, min(ps+ivec2(0,0), bound), 0).x;\n"
	// "	highp uint p01 = texelFetch(prev, min(ps+ivec2(0,1), bound), 0).x;\n"
	// "	highp uint p10 = texelFetch(prev, min(ps+ivec2(1,0), bound), 0).x;\n"
	// "	highp uint p11 = texelFetch(prev, min(ps+ivec2(1,1), bound), 0).x;\n"
	// "	highp uint v = ((p00*(256u - pf.x) + p01 * pf.x) * (256u - pf.y) \n"
	// "	              + (p10*(256u - pf.x) + p11 * pf.x) * pf.y) >> 16;\n"
	// "	v = (v * ((256u*97u)/100u)) >> 8u;\n"

	"	fragColour = max(\n"
	"			(uvec4(v)*(256u*97u)/100u)/256u,\n"
	"			texture(maxsrc, uv)\n"
	"	);\n"
	// "	fragColour = max(\n"
	// "			(texture(prev, p)*(97.0/100.0)),\n"
	// "			texture(maxsrc, uv)\n"
	// "	);\n"
	"}\n";
static void render(struct glfract_ctx *ctx, const struct point_data *pd, GLuint maxsrc_tex)
{
	struct gles_ctx *priv = (struct glsl_ctx *)ctx;

	GLint src_tex = offscr_start_render(ctx->offscr);
	glUseProgram(priv->prog[priv->func]);
	switch(priv->func)
	{
		case GLES_MAP_FUNC_NORMAL:
			glUniform2f(priv->c_loc[priv->func], pd->p[0], pd->p[1]);
			break;
		case GLES_MAP_FUNC_RATIONAL:
			glUniform4f(priv->c_loc[priv->func], pd->p[0], pd->p[1], pd->p[2], pd->p[3]);
			break;
		case GLES_MAP_FUNC_BUTTERFLY:
			break;
		case GLES_MAP_FUNC_POLY:
			break;
	}

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, src_tex);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, maxsrc_tex);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
	offscr_finish_render(ctx->offscr);
}

struct glfract_ctx *fractal_gles_init(gles_map_func f, int width, int height)
{
	struct gles_ctx *ctx = malloc(sizeof(*ctx));
	ctx->pubctx.render = render;
	ctx->func = f;

	// int quality = MIN(opts->quality, 4);
	// const char *map_defs = map_defs_list[packed_intesity_pixels][quality];

	//TODO: try to fall back to -q 0 if we fail to compile
	//TODO: might need to die if we don't get the packed pixels shader to compile...
	printf("Compiling map shaders:\n");

	ctx->prog[GLES_MAP_FUNC_NORMAL]    = compile_program_defs(NULL, vtx_shader_src, map_frag_shader);
	// ctx->prog[GLES_MAP_FUNC_RATIONAL]  = compile_program_defs(NULL, vtx_shader_src, map_frag_shader);
	// ctx->prog[GLES_MAP_FUNC_BUTTERFLY] = compile_program_defs(NULL, vtx_shader_src, map_frag_shader);
	// ctx->prog[GLES_MAP_FUNC_POLY]      = compile_program_defs(NULL, vtx_shader_src, map_frag_shader);

	for(int i=0; i < 1; i++) // GLES_MAP_FUNC_NUM_FUNCS
	{
		if(!ctx->prog[i]) {
			free(ctx);
			return NULL;
		}
		glUseProgram(ctx->prog[i]);
		ctx->c_loc[i] = glGetUniformLocation(ctx->prog[i], "c"); CHECK_GL_ERR;
		glUniform1i(glGetUniformLocation(ctx->prog[i], "maxsrc"), 0); CHECK_GL_ERR;
		glUniform1i(glGetUniformLocation(ctx->prog[i], "prev"), 1); CHECK_GL_ERR;
	}
	glUseProgram(0);
	printf("Map shaders compiled\n");

	ctx->pubctx.offscr = offscr_new(width, height, false, false);

	return (struct glfract_ctx *)ctx;
}
