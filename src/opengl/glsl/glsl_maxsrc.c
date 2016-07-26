#include "common.h"
#include "audio/audio.h"
#include "opengl/glmisc.h"
#include "opengl/glmaxsrc.h"
#include "opengl/glscope.h"
#include "getsamp.h"

static const char *vtx_shader =
	"varying vec2 uv;\n"
	"void main() {\n"
	"	uv = gl_MultiTexCoord0.st*2.0f-1.0f;\n"
	"	gl_Position = gl_Vertex;\n"
	"}";

static const char *frag_src =
	"varying vec2 uv;\n"
	"uniform sampler2D prev;\n"
	"uniform mat3 R;\n"
	"#ifdef FLOAT_PACK_PIX\n"
	FLOAT_PACK_FUNCS
	"#endif\n"
	"void main() {\n"
	"	vec2 p;\n"
	"	{\n"
	"		vec3 t = vec3(0.5f);\n"
	"		t.yz = vec2(0.95f*0.5f + (0.05f*0.5f)*length(uv));\n"
	"		t = (uv.x*R[0] + uv.y*R[1])*t;\n"
	"		p = (t*R).xy + 0.5f;\n"
	"	}\n"
	"#ifdef FLOAT_PACK_PIX\n" //TODO: use this formula whenver we have extra prescision in the FBO
	"	gl_FragColor = encode(decode(texture2D(prev, p))*0.978f);\n"
	"#else\n"
//	"	gl_FragColor.r = texture2D(prev, p).r*0.978f;\n"
	"	gl_FragColor = texture2D(prev, p)*0.975f;\n" //TODO: use 0.978 if we have GL_R16 textures...
//	"	vec4 c = texture2D(prev, p);\n"
//	"	gl_FragColor = vec4(c.x - max(2/256.0f, c.x*(1.0f/100)));\n"
//	"	gl_FragColor = (c - max(vec4(2/256.0f), c*0.01f));\n"
	"#endif\n"
	"}\n";

struct priv_ctx {
	struct glmaxsrc_ctx pubctx;
	struct glscope_ctx *glscope;
	float tx, ty, tz;
	GLuint prog;
	GLint R_loc;
	uint32_t lastupdate;
};

static void update(struct glmaxsrc_ctx *ctx, const float *audio, int audiolen);

//TODO: teardown

struct glmaxsrc_ctx *maxsrc_new_glsl(int width, int height, GLboolean packed_intesity_pixels)
{
	printf("Compiling maxsrc shader:\n");
	const char *defs = packed_intesity_pixels?"#version 110\n#define FLOAT_PACK_PIX\n":"#version 110\n";
	GLuint prog = compile_program_defs(defs, vtx_shader, frag_src);
	GLint R_loc = -1;
	if(!prog) return NULL;

	printf("maxsrc shader compiled\n");
	glUseProgramObjectARB(prog);
	glUniform1iARB(glGetUniformLocationARB(prog, "prev"), 0);
	R_loc = glGetUniformLocationARB(prog, "R");
	glUseProgramObjectARB(0);
	CHECK_GL_ERR;

	int samp = MIN(MIN(width/2, height/2), 128);
	printf("maxsrc using %i points\n", samp);
	struct priv_ctx *priv = calloc(sizeof(*priv), 1);
	priv->pubctx.update = update;
	priv->pubctx.offscr = offscr_new(width, height, false, !packed_intesity_pixels);
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

	const float Rt[9] = {
		R[0][0], R[1][0], R[2][0],
		R[0][1], R[1][1], R[2][1],
		R[0][2], R[1][2], R[2][2],
	};

	GLint src_tex = offscr_start_render(ctx->offscr);

	glUseProgramObjectARB(priv->prog);
	glUniformMatrix3fvARB(priv->R_loc, 1, 0, Rt);
	glBindTexture(GL_TEXTURE_2D, src_tex);

	glBegin(GL_TRIANGLE_STRIP);
		glTexCoord2d(0,0); glVertex2d(-1, -1);
		glTexCoord2d(1,0); glVertex2d( 1, -1);
		glTexCoord2d(0,1); glVertex2d(-1,  1);
		glTexCoord2d(1,1); glVertex2d( 1,  1);
	glEnd();
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgramObjectARB(0);
	DEBUG_CHECK_GL_ERR;

	render_scope(priv->glscope, R, audio, audiolen);
	DEBUG_CHECK_GL_ERR;
	offscr_finish_render(ctx->offscr);

	priv->tx+=0.02f*dt; priv->ty+=0.01f*dt; priv->tz-=0.003f*dt;

	DEBUG_CHECK_GL_ERR;
}


